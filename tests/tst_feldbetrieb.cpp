// SPDX-License-Identifier: GPL-3.0-or-later
//
// „wenn man wenig platz hat und sota macht waere es besser um mehr am
//  wasserfall sehen zu koennen ... sonst ist alles sehr eng ... die
//  unwichtigen dinge wuerden dann weg bleiben." (Betreiber,
//  2026-10-14)
//
// Der Weg dorthin gab es schon — Layoutprofile koennen jedes Feld
// ausblenden —, nur musste man ihn vierzehn Mal klicken. Jetzt ein
// Eintrag.
//
// Dieser Lauf haelt DREI Dinge nach, und das dritte ist das
// wichtigste:
//
//   1. Der Eintrag ist im Menue und ausloesbar.
//   2. Danach stehen genau die Felder aus der Liste, sonst keines.
//   3. Er schreibt in ein EIGENES Profil. Ein „Feldbetrieb", der die
//      Einrichtung von Wochen ueberschreibt, waere das Gegenteil von
//      hilfreich — und LayoutProfiles::activate() sichert das
//      bisherige Profil, bevor es wechselt.
//
// Gegenprobe (2026-10-15): ohne den Profilwechsel bleibt current() auf
// dem Ausgangsprofil stehen und derSchritt...InEinEigenesProfil faellt.

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QImage>

#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "gui/applets/AppletVisibilityController.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstFeldbetrieb : public QObject
{
    Q_OBJECT

private:
    static QAction* eintrag(MainWindow* mw)
    {
        for (QMenuBar* bar : mw->findChildren<QMenuBar*>()) {
            for (QAction* a : bar->actions()) {
                if (!a->menu()) { continue; }
                for (QAction* e : a->menu()->actions()) {
                    if (!e->menu()) { continue; }
                    for (QAction* f : e->menu()->actions()) {
                        if (f->text().contains(QLatin1String("Field layout"))) {
                            return f;
                        }
                    }
                }
                for (QAction* e : a->menu()->actions()) {
                    if (e->text().contains(QLatin1String("Field layout"))) {
                        return e;
                    }
                }
            }
        }
        return nullptr;
    }

private slots:
    void derEintragIstDaUndRaeumtAuf()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QAction* a = eintrag(mw);
        QVERIFY2(a, "kein Eintrag fuer den Feldbetrieb im Menue");
        QVERIFY2(a->isEnabled(), "ausgegraut");

        auto* vis = mw->findChild<AppletVisibilityController*>();
        QVERIFY(vis);

        a->trigger();
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        const QStringList bleiben = MainWindow::fieldLayoutKeepIds();
        QVERIFY2(!bleiben.isEmpty(), "die Liste ist leer");

        QStringList falschAn, falschAus;
        for (const QString& id : vis->registeredIds()) {
            // Rahmenteile zaehlen nicht mit; sie sind keine Felder.
            if (id.startsWith(QLatin1String("Chrome"))) { continue; }
            const bool sollte = bleiben.contains(id);
            const bool ist    = vis->isVisible(id);
            if (ist && !sollte) { falschAn << id; }
            if (!ist && sollte) { falschAus << id; }
        }
        qInfo() << "sichtbar bleiben:" << bleiben;

        QVERIFY2(falschAus.isEmpty(),
                 qPrintable(QStringLiteral("fehlen nach dem Aufraeumen: %1")
                                .arg(falschAus.join(QLatin1String(", ")))));
        QVERIFY2(falschAn.isEmpty(),
                 qPrintable(QStringLiteral("stehen noch da: %1")
                                .arg(falschAn.join(QLatin1String(", ")))));

        // Ein Blatt zum Ansehen, wenn jemand eines will. Der Betreiber
        // beurteilt Gestaltung an Bildern in wirklicher Groesse
        // („ein PNG je Variante"), und das Feldbetrieb-Bild laesst sich
        // sonst nur durch Klicken herstellen. Ohne LONGPATH_SHOT_DIR
        // kostet die Zeile nichts.
        const QString ablage =
            qEnvironmentVariable("LONGPATH_SHOT_DIR").trimmed();
        if (!ablage.isEmpty()) {
            QImage blatt(mw->size(), QImage::Format_RGB32);
            blatt.fill(Qt::black);
            mw->render(&blatt);
            const QString datei =
                ablage + QStringLiteral("/feldbetrieb.png");
            QVERIFY2(blatt.save(datei), qPrintable(datei));
            qInfo().noquote() << "Blatt:" << datei;
        }

        delete mw;
    }

    /// Und er tut es in EINEM EIGENEN Profil.
    void derSchrittGehtInEinEigenesProfil()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        auto* profile = mw->findChild<LayoutProfiles*>();
        QVERIFY(profile);
        const QString vorher = profile->current();

        QAction* a = eintrag(mw);
        QVERIFY(a);
        a->trigger();
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        qInfo() << "Profil vorher" << vorher << "nachher"
                << profile->current();

        QCOMPARE(profile->current(), QStringLiteral("Field"));
        QVERIFY2(profile->current() != vorher,
                 "der Feldbetrieb hat in das laufende Profil geschrieben — "
                 "damit waere die Einrichtung des Betreibers weg");
        QVERIFY2(profile->names().contains(vorher),
                 "das bisherige Profil ist verschwunden");

        delete mw;
    }
};

QTEST_MAIN(TstFeldbetrieb)
#include "tst_feldbetrieb.moc"
