// SPDX-License-Identifier: GPL-3.0-or-later
//
// Alles, was am 2026-08-23 dazugekommen ist — ZUSAMMEN, im echten
// Hauptfenster.
//
// Der Anlass ist kein Fehlerbericht, sondern eine Luecke im Vorgehen:
// an diesem Tag sind viele Bausteine entstanden — KiwiSDR mit Ton,
// Wasserfall und Sendesperre, die Roehre, das SWR/Leistung-Widget, die
// Empfaengerkacheln, die PureSignal-Stabilitaetsregel, die
// Streckenerkennung, die knappen Balken — und JEDER wurde einzeln
// geprueft. Keiner zusammen mit den anderen.
//
// Genau dort sitzen die Fehler, die einzeln nicht auffallen. An diesem
// Tag ist das zweimal passiert: die knappen Zusatzzeilen zeichneten
// nichts mehr, weil sie kuerzer wurden als die festen Raender es
// zuliessen — beide Aenderungen fuer sich richtig. Und die
// Kiwi-Zuordnung lief ins Leere, weil ein Fenster ohne Funkgeraet gar
// keine Scheibe hat.
//
// Diese Pruefung baut ein VOLLSTAENDIGES Hauptfenster, ruehrt alles
// Neue an und baut es wieder ab. Sie sucht keinen bestimmten Fehler —
// sie sucht den, den ich noch nicht kenne.

#include <QtTest>
#include <QAction>
#include <QLabel>
#include <QMenu>

#include "core/AudioEngine.h"
#include "core/KiwiSdrManager.h"
#include "core/RadioLinkKind.h"
#include "gui/MainWindow.h"
#include "gui/PsaIndicatorWidget.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/FrequencyApplet.h"
#include "gui/applets/KiwiSdrApplet.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/meters/MeterPoller.h"
#include "gui/widgets/VfoTileRow.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TstTodaysWorkTogether : public QObject
{
    Q_OBJECT

private slots:
    void allesZusammenUndWiederWeg()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        // ── 1. Die neuen Applets sind ueberhaupt da ─────────────────
        QVERIFY2(mw->findChild<FrequencyApplet*>(),
                 "Das Frequenzfenster fehlt");
        QVERIFY2(mw->findChild<KiwiSdrApplet*>(),
                 "Das KiwiSDR-Applet fehlt");
        QVERIFY2(mw->findChild<VfoTileRow*>(),
                 "Die Empfaengerkacheln fehlen");
        qInfo() << "Applets vorhanden";

        // ── 2. Ein KiwiSDR aufnehmen ────────────────────────────────
        KiwiSdrManager* kiwi = mw->kiwiSdrManagerForTest();
        QVERIFY(kiwi);
        for (const auto& p : kiwi->profiles()) { kiwi->removeProfile(p.id); }
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Pruefling"),
                                      QStringLiteral("kiwi.example.at:8073"));
        QVERIFY2(!kiwi->profiles().isEmpty(), "KiwiSDR nicht angelegt");
        const int sliceId =
            kiwi->assignedSliceForProfile(kiwi->profiles().first().id);
        QVERIFY2(sliceId >= 0, "KiwiSDR keiner Scheibe zugeordnet");
        qInfo() << "KiwiSDR aufgenommen, Scheibe" << sliceId;

        // ── 3. Sendesperre greift auch hier ─────────────────────────
        RadioModel* model = mw->radioModelForTest();
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        audio->setKiwiSdrAudioSourceEnabled(sliceId, true);
        model->transmitModel().setMox(true);
        QVERIFY2(!audio->kiwiSdrAudioEnabled(sliceId),
                 "Sendesperre greift im vollen Fenster nicht");
        model->transmitModel().setMox(false);
        QVERIFY(audio->kiwiSdrAudioEnabled(sliceId));
        qInfo() << "Sendesperre greift";

        // ── 4. SWR/Leistung im Frequenzfenster ───────────────────────
        //
        // Bis 2026-08-30 lebte diese Pruefung im eigenen SWR/Leistung-
        // Applet (TxMeterApplet). Der Betreiber wollte SWR/Leistung
        // seither NUR noch als Zusatzzeile im Frequenzfenster, nicht
        // mehr als eigenes Fenster ("nur zusaetzlich im Bereich des
        // Frequenzfenster, nicht alle") -- TxMeterApplet ist entfernt.
        // Was bleibt zu pruefen: die beiden Zeilen schalten sich ein
        // und bleiben es, waehrend Abstimmen/Senden den Zustand
        // wechseln.
        auto* freq = mw->findChild<FrequencyApplet*>();
        QVERIFY(freq);
        freq->setShowPower(true);
        freq->setShowSwr(true);
        QVERIFY(freq->showsPower());
        QVERIFY(freq->showsSwr());
        model->transmitModel().setTune(true);
        QVERIFY(freq->showsPower());
        QVERIFY(freq->showsSwr());
        model->transmitModel().setTune(false);
        model->transmitModel().setMox(true);
        QVERIFY(freq->showsPower());
        QVERIFY(freq->showsSwr());
        model->transmitModel().setMox(false);
        qInfo() << "SWR/Leistung im Frequenzfenster bleiben eingeschaltet";

        // ── 5. Roehre an und aus, waehrend alles laeuft ─────────────
        for (BarInstrument* b : mw->findChildren<BarInstrument*>()) {
            b->setTube(true);
            b->setSegmented(true);
        }
        QCoreApplication::processEvents();
        for (BarInstrument* b : mw->findChildren<BarInstrument*>()) {
            b->setTube(false);
        }
        QCoreApplication::processEvents();
        qInfo() << "Roehre geschaltet an"
                << mw->findChildren<BarInstrument*>().size() << "Balken";

        // ── 6. Panadapter auf KiwiSDR und zurueck ───────────────────
        int switched = 0;
        for (SpectrumWidget* sw : mw->findChildren<SpectrumWidget*>()) {
            sw->setKiwiDisplaySource(true);
            QVERIFY(sw->kiwiDisplaySource());
            sw->setKiwiDisplaySource(false);
            ++switched;
        }
        qInfo() << "Anzeigequelle geschaltet an" << switched << "Panadaptern";

        // ── 7. Die PS-Stabilitaetsanzeige ───────────────────────────
        if (auto* psa = mw->findChild<PsaIndicatorWidget*>()) {
            psa->setPsEnabled(true);
            psa->setMox(true);
            psa->setCorrectionsBeingApplied(true);
            psa->setStabilityAction(PsCorrectionAction::Withhold);
            QVERIFY2(!psa->psText().contains(QStringLiteral("orrect")),
                     qPrintable(psa->psText()));
            psa->setStabilityAction(PsCorrectionAction::Run);
            qInfo() << "PS-Anzeige folgt der Regel";
        }

        // ── 8. Und alles wieder abbauen ─────────────────────────────
        //
        // Der teuerste Teil. Am 2026-08-22 hat genau hier ein
        // abgeloester Panadapter das Programm zum Absturz gebracht,
        // und die vier Schutzmethoden dagegen mussten erst aus Aether
        // nachportiert werden.
        mw->close();
        QCoreApplication::processEvents();
        qInfo() << "sauber abgebaut";
    }

    void zweimalHintereinander()
    {
        // Ein zweiter Durchgang faengt, was beim ersten liegen blieb —
        // gemerkte Einstellungen, nicht geloeste Verbindungen, ein
        // Zeitgeber, der noch laeuft.
        for (int round = 0; round < 2; ++round) {
            auto* mw = new MainWindow();
            mw->resize(1200, 800);
            mw->show();
            QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
            QVERIFY(mw->findChild<FrequencyApplet*>());
            mw->close();
            QCoreApplication::processEvents();
            qInfo() << "Durchgang" << (round + 1) << "in Ordnung";
        }
    }
};

QTEST_MAIN(TstTodaysWorkTogether)
#include "tst_todays_work_together.moc"
