// SPDX-License-Identifier: GPL-3.0-or-later
//
// Was der Betreiber einstellt, ist beim naechsten Start noch da.
//
// Anlass, 2026-08-23: "einstellungen, format, und design sowie
// anordnung der widget sollte sich gemerkt werden".
//
// Diese Pruefung liest nicht im Code nach, ob irgendwo setValue steht
// — sie stellt etwas um, baut das Fenster ab, baut es neu auf und
// sieht nach. Nur so faellt der Fall auf, in dem gespeichert wird und
// das Wiederherstellen fehlt (oder umgekehrt).
//
// Sie deckt bewusst die Dinge ab, die am 2026-08-23 dazugekommen
// sind: bei denen ist die Wahrscheinlichkeit am groessten, dass das
// Merken vergessen wurde.

#include <QtTest>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/AsrApplet.h"
#include "gui/applets/FrequencyApplet.h"
#include "gui/applets/TxMeterApplet.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/meters/MeterPoller.h"

using namespace Longpath;

class TstSettingsAreRemembered : public QObject
{
    Q_OBJECT

private slots:
    void balkenformUeberlebtDenNeustart()
    {
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* txm = mw->findChild<TxMeterApplet*>();
            QVERIFY(txm);
            txm->bar()->setTube(true);
            txm->bar()->setSegmented(true);
            txm->saveState();
            mw->close();
            QCoreApplication::processEvents();
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* txm = mw->findChild<TxMeterApplet*>();
            QVERIFY(txm);
            qInfo() << "nach Neustart — Roehre:" << txm->bar()->isTube()
                    << "Segmente:" << txm->bar()->isSegmented();
            QVERIFY2(txm->bar()->isTube(), "Die Roehre ist vergessen");
            QVERIFY2(txm->bar()->isSegmented(), "Die Segmente sind vergessen");
            mw->close();
        }
    }

    void zusatzzeilenUeberlebenDenNeustart()
    {
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* f = mw->findChild<FrequencyApplet*>();
            QVERIFY(f);
            f->setShowPower(true);
            f->setShowSwr(true);
            mw->close();
            QCoreApplication::processEvents();
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* f = mw->findChild<FrequencyApplet*>();
            QVERIFY(f);
            qInfo() << "nach Neustart — Stehwelle:" << f->showsPower()
                    << "SWR:" << f->showsSwr();
            QVERIFY2(f->showsPower(), "Die Stehwellenzeile ist vergessen");
            QVERIFY2(f->showsSwr(), "Die SWR-Zeile ist vergessen");
            mw->close();
        }
    }

    void dieAnzeigequelleDesPanadaptersUeberlebt()
    {
        // ── Erst aufraeumen ─────────────────────────────────────────
        //
        // Ohne das besteht diese Pruefung aus einem VORIGEN Lauf
        // heraus: der Schluessel liegt noch in der Ablage, das
        // Wiederherstellen findet ihn, und ob das Speichern in DIESEM
        // Lauf funktioniert hat, bleibt offen. Genau so haette sie am
        // 2026-08-23 beinahe faelschlich bestanden.
        for (const QString& k : AppSettings::instance().allKeys()) {
            if (k.contains(QStringLiteral("KiwiDisplay"))) {
                AppSettings::instance().setValue(k, QStringLiteral("False"));
            }
        }

        // Der Verdacht, mit dem ich hier angefangen habe: beim Bauen
        // der Stufe 6 habe ich das Merken NICHT eingebaut. Wenn diese
        // Pruefung faellt, ist das der Beleg.
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            sw->setKiwiDisplaySource(true);
            qInfo() << "gesetzt an Panadapter, Schluessel siehe Ablage";
            for (const QString& k : AppSettings::instance().allKeys()) {
                if (k.contains(QStringLiteral("KiwiDisplay"))) {
                    qInfo().noquote() << "  gespeichert:" << k;
                }
            }
            mw->close();
            QCoreApplication::processEvents();
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            for (const QString& k : AppSettings::instance().allKeys()) {
                if (k.contains(QStringLiteral("KiwiDisplay"))) {
                    qInfo().noquote() << "  vorhanden:" << k;
                }
            }
            qInfo() << "nach Neustart — Anzeige vom KiwiSDR:"
                    << sw->kiwiDisplaySource();
            QVERIFY2(sw->kiwiDisplaySource(),
                     "Die Anzeigequelle des Panadapters ist vergessen");
            mw->close();
        }
    }
};

QTEST_MAIN(TstSettingsAreRemembered)
#include "tst_settings_are_remembered.moc"
