// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf, kein Port.
//
// ── Die Tondiagnose gab es nur unter Linux ──────────────────────────────
//
// Am 2026-10-22 hoerte der Betreiber auf Windows ein gleichmaessiges
// Knistern — „als wuerde ein Maschinengewehr schiessen". Jede Zahl, die
// die Ursache eingrenzt (Geraet, Schnittstelle, ausgehandelte Rate,
// Latenz, Aussetzerzaehler), steht im Programm bereit. Sichtbar war sie
// nicht: der Eintrag „Diagnose audio backend…" stand hinter
//
//     #if defined(Q_OS_LINUX)
//
// Also holte er sie stundenlang mit PowerShell aus dem Protokoll, und
// dreimal kam dabei ein Lauf OHNE Verbindung, in dem der Ton nie offen
// war und keine der Zeilen stand. Am Ende: „das ist mir zu umstaendlich
// mit 2 computern".
//
// Der Lauf haelt zwei Dinge fest:
//   1. den Menueeintrag, auf JEDER Plattform
//   2. dass AudioEngine die beiden Baender herausgibt, aus denen der
//      Bericht gebaut wird — ohne Pruef-Naht, denn er gehoert zur
//      Bedienung und nicht zum Pruefstand
//
// Gegenprobe (2026-10-22): mit dem #if defined(Q_OS_LINUX) faellt
// derEintragStehtImHilfemenue auf macOS und Windows um; unter Linux
// bliebe er gruen, was genau die Luecke war.

#include <QtTest>
#include <QMenu>
#include <QMenuBar>

#include "core/AudioEngine.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstTondiagnoseUeberall : public QObject
{
    Q_OBJECT

private:
    // NICHT mw->menuBar() nehmen: MainWindow haengt seine Leiste per
    // setMenuWidget() in eine eigene TitleBar um, und menuBar() legt
    // danach eine NEUE, LEERE an (Fund vom 2026-09-02).
    static QMenu* hilfeMenue(MainWindow* mw)
    {
        for (QMenuBar* bar : mw->findChildren<QMenuBar*>()) {
            for (QAction* a : bar->actions()) {
                if (!a->menu()) { continue; }
                if (a->text().contains(QLatin1String("Help"))) { return a->menu(); }
            }
        }
        return nullptr;
    }

private slots:

    void derEintragStehtImHilfemenue()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        QMenu* hilfe = hilfeMenue(mw);
        QVERIFY2(hilfe != nullptr, "kein Hilfemenue gefunden");

        bool da = false;
        QStringList gesehen;
        for (QAction* a : hilfe->actions()) {
            if (a->isSeparator()) { continue; }
            gesehen << a->text();
            if (a->text().contains(QLatin1String("Diagnose audio backend"))) {
                da = true;
            }
        }
        QVERIFY2(da, qPrintable(QStringLiteral(
                     "kein Eintrag 'Diagnose audio backend' im Hilfemenue; "
                     "gefunden: %1").arg(gesehen.join(QLatin1String(" | ")))));
        delete mw;
    }

    // Der Bericht wird aus diesen beiden Baendern gebaut. Im Pruefmodus
    // oeffnet makeBus() bewusst KEIN echtes Geraet (sonst nimmt sich ein
    // Lauf das Mikrofon des Betreibers), also steht hier nullptr — der
    // Fall, den der Dialog mit "not configured" beantworten muss. Geprueft
    // wird darum die ERREICHBARKEIT, nicht der Inhalt.
    void dieBaenderSindOhnePruefnahtErreichbar()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model != nullptr);
        AudioEngine* eng = model->audioEngine();
        QVERIFY2(eng != nullptr, "keine Tonmaschine am Modell");

        // Beide Aufrufe duerfen jederzeit gefahrlos gehen, auch wenn kein
        // Geraet offen ist. Dass sie uebersetzen und nicht abstuerzen, ist
        // die Aussage.
        const IAudioBus* lautsprecher = eng->speakersBus();
        const IAudioBus* mikrofon     = eng->txInputBus();
        QCOMPARE(lautsprecher, static_cast<const IAudioBus*>(nullptr));
        QCOMPARE(mikrofon,     static_cast<const IAudioBus*>(nullptr));

        delete mw;
    }
};

QTEST_MAIN(TstTondiagnoseUeberall)
#include "tst_tondiagnose_ueberall.moc"
