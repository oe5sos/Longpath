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
#include <QComboBox>
#include <QSlider>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/AsrApplet.h"
#include "gui/applets/FrequencyApplet.h"
#include "gui/applets/InstrumentApplet.h"
#include "gui/meters/MeterPoller.h"

using namespace Longpath;

namespace {
// TxMeterApplet ist am 2026-08-30 entfernt worden (Betreiber: das
// SWR/Leistung-Fenster soll es nur noch als Zusatzzeile im
// Frequenzfenster geben, nicht mehr als eigenes Applet). Diese Pruefung
// wollte Roehre/Segmente nur an IRGENDEINEM BarInstrument sehen, das
// diese beiden Formen kennt und sie speichert -- SwrInstrument (ein
// InstrumentApplet auf Balkenform) leistet genau das.
InstrumentApplet* swrInstrument(MainWindow* mw)
{
    for (InstrumentApplet* ia : mw->findChildren<InstrumentApplet*>()) {
        if (ia && ia->appletId() == QStringLiteral("SwrInstrument")) {
            return ia;
        }
    }
    return nullptr;
}
} // namespace

class TstSettingsAreRemembered : public QObject
{
    Q_OBJECT

private slots:
    // Die 3D-Ansicht des Spektrums (Display-Flyout, Combo "Spectrum:
    // 2D / 3D") muss den Neustart ueberleben -- und zwar an beiden
    // Enden: das Widget zeichnet wieder 3D, UND der Combo zeigt "3D".
    // Der zweite Teil fehlte: das Widget laedt seine Einstellungen,
    // bevor das Panel existiert, und niemand sagte es dem Combo.
    void dreiDAnsichtUeberlebtDenNeustart()
    {
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* combo = mw->findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"));
            QVERIFY2(combo, "Combo 'Spectrum: 2D / 3D' nicht gefunden");
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            QCOMPARE(combo->currentIndex(), 0);
            QCOMPARE(sw->spectrumRenderMode(), SpectrumRenderMode::Mode2D);
            combo->setCurrentIndex(1);
            QCOMPARE(sw->spectrumRenderMode(), SpectrumRenderMode::Mode3D);
            mw->close();
            delete mw;
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            auto* combo = mw->findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"));
            QVERIFY(combo);
            QVERIFY2(sw->spectrumRenderMode() == SpectrumRenderMode::Mode3D,
                     "Das Widget hat die 3D-Ansicht vergessen");
            QVERIFY2(combo->currentIndex() == 1,
                     "Der Combo zeigt 2D, obwohl das Widget 3D zeichnet");
            mw->close();
            delete mw;
        }
    }

    // Gleicher Fehler wie bei der 3D-Ansicht, ein Regler weiter: die
    // WF-Gain-Slider im Display-Flyout hatte ihren eigenen fest
    // verdrahteten Vorgabewert (50) und wusste nichts vom Wert, den
    // SpectrumWidget schon geladen hatte.
    void wfGainUeberlebtDenNeustart()
    {
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* slider = mw->findChild<QSlider*>(QStringLiteral("wfGainSlider"));
            QVERIFY2(slider, "Regler 'WF Gain' nicht gefunden");
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            QCOMPARE(slider->value(), sw->wfColorGain());
            slider->setValue(77);
            QCOMPARE(sw->wfColorGain(), 77);
            mw->close();
            delete mw;
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            auto* slider = mw->findChild<QSlider*>(QStringLiteral("wfGainSlider"));
            QVERIFY(slider);
            QVERIFY2(sw->wfColorGain() == 77, "Das Widget hat den WF-Gain-Wert vergessen");
            QVERIFY2(slider->value() == 77,
                     "Der Regler zeigt seine Vorgabe, obwohl das Widget 77 geladen hat");
            mw->close();
            delete mw;
        }
    }

    // Gleicher Fehler wie WF Gain, plus der Regler-Bereich (0..100), der
    // zu eng fuer das Widget (0..125) war. 110 liegt bewusst ueber der
    // alten Deckelung -- waere der Bereich nicht wirklich erweitert
    // worden, wuerde setValue(110) hier still auf 100 gekappt und der
    // Test faellt sichtbar durch, statt zufaellig grün zu bleiben.
    void blackLvlUeberlebtDenNeustart()
    {
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* slider = mw->findChild<QSlider*>(QStringLiteral("wfBlackSlider"));
            QVERIFY2(slider, "Regler 'Black Lvl' nicht gefunden");
            QCOMPARE(slider->maximum(), 125);
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            QCOMPARE(slider->value(), sw->wfBlackLevel());
            slider->setValue(110);
            QCOMPARE(sw->wfBlackLevel(), 110);
            mw->close();
            delete mw;
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            auto* slider = mw->findChild<QSlider*>(QStringLiteral("wfBlackSlider"));
            QVERIFY(slider);
            QVERIFY2(sw->wfBlackLevel() == 110, "Das Widget hat den Black-Lvl-Wert vergessen");
            QVERIFY2(slider->value() == 110,
                     "Der Regler zeigt seine Vorgabe, obwohl das Widget 110 geladen hat");
            mw->close();
            delete mw;
        }
    }

    // Gleicher Fehler wie WF Gain, plus vier erfundene Combo-Eintraege,
    // die keinem WfColorScheme-Wert entsprachen. Index 7 ("Clarity Blue")
    // liegt bewusst ausserhalb der alten Vier -- war die Liste nicht
    // wirklich erweitert worden, gaebe es diesen Eintrag im Combo gar
    // nicht und der Test faellt beim Setzen schon durch.
    void farbschemaUeberlebtDenNeustart()
    {
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* combo = mw->findChild<QComboBox*>(QStringLiteral("colorSchemeCmb"));
            QVERIFY2(combo, "Combo 'Farbschema' nicht gefunden");
            QVERIFY2(combo->count() >= 9, "Der Combo hat nicht alle Schemata");
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            combo->setCurrentIndex(7);
            QCOMPARE(static_cast<int>(sw->wfColorScheme()), 7);
            mw->close();
            delete mw;
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* sw = mw->findChild<SpectrumWidget*>();
            QVERIFY(sw);
            auto* combo = mw->findChild<QComboBox*>(QStringLiteral("colorSchemeCmb"));
            QVERIFY(combo);
            QVERIFY2(static_cast<int>(sw->wfColorScheme()) == 7,
                     "Das Widget hat das Farbschema vergessen");
            QVERIFY2(combo->currentIndex() == 7,
                     "Der Combo zeigt seine Vorgabe, obwohl das Widget Schema 7 geladen hat");
            mw->close();
            delete mw;
        }
    }

    void balkenformUeberlebtDenNeustart()
    {
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* swr = swrInstrument(mw);
            QVERIFY(swr);
            swr->setTube(true);
            swr->setSegmented(true);
            swr->saveState();
            mw->close();
            QCoreApplication::processEvents();
        }
        {
            auto* mw = new MainWindow();
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            auto* swr = swrInstrument(mw);
            QVERIFY(swr);
            qInfo() << "nach Neustart — Roehre:" << swr->isTube()
                    << "Segmente:" << swr->isSegmented();
            QVERIFY2(swr->isTube(), "Die Roehre ist vergessen");
            QVERIFY2(swr->isSegmented(), "Die Segmente sind vergessen");
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
