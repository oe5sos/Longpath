// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_rtty_senden_flaeche.cpp  (Longpath)
// =================================================================
// Die Bedienung zum RTTY-Senden.
//
// Der Sender selbst (core/RttyTx.h) ist in tst_rtty_senden geprueft,
// samt Rundlauf durch den eigenen Dekoder. Hier geht es nur um das,
// was das Fenster damit macht: Sendeweg umlegen, tasten, und beides
// in der richtigen Reihenfolge wieder zurueck.
//
// Die Reihenfolge ist der Grund, warum es diesen Lauf gibt: erst den
// Weg legen und dann tasten (sonst ginge der Sender mit dem Mikrofon
// auf, nicht mit dem Ton), und beim Aufhoeren umgekehrt, damit zu
// keinem Zeitpunkt das Mikrofon auf Sendung ist.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "core/MoxController.h"
#include "core/RttyTx.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/safety/BandPlanGuard.h"
#include "gui/applets/RttyDecoderApplet.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstRttySendenFlaeche : public QObject
{
    Q_OBJECT

private slots:
    // Ohne Sendeweg (also vor dem Verbinden) sagt es das, statt still
    // nichts zu tun.
    void ohneSendewegSagtEsDas()
    {
        RadioModel radio;
        RttyDecoderApplet applet(&radio);
        applet.sendeText(QStringLiteral("CQ"));
        QVERIFY(!applet.sendetGerade());
        QVERIFY2(applet.sendeStatusForTest()
                     && !applet.sendeStatusForTest()->text().isEmpty(),
                 "ein Versuch ohne Sendeweg muss sichtbar scheitern");
    }

    void leererTextSendetNichts()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);
        applet.sendeText(QStringLiteral("   "));
        QVERIFY(!applet.sendetGerade());
    }

    // Der Kern: der Sendeweg liegt waehrend der Sendung auf Rtty, und
    // danach wieder dort, wo er war.
    void derSendewegGehtHinUndZurueck()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        auto* router = radio.micRouter();
        QVERIFY(router);
        router->setActiveSource(MicSource::Pc);

        applet.sendeText(QStringLiteral("CQ DE OE5SOS"));
        QVERIFY2(applet.sendetGerade(), "der Versuch muss anlaufen");
        QCOMPARE(router->activeSource(), MicSource::Rtty);
        QVERIFY2(radio.rttyTxSource()->laeuft(),
                 "die Tonquelle muss laufen, sonst geht Stille hinaus");

        applet.sendenAbbrechen();
        QVERIFY(!applet.sendetGerade());
        QCOMPARE(router->activeSource(), MicSource::Pc);
        QVERIFY2(!radio.rttyTxSource()->laeuft(),
                 "nach dem Abbruch darf kein Ton mehr nachkommen");
    }

    // Sagt der Bandplan nein, wird nicht weitergesendet — dieselbe
    // Luecke, die bei FT8 am 2026-10-16 gefunden wurde.
    void einAbgelehntesTastenBrichtAb()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        auto* mox = radio.moxController();
        QVERIFY(mox);
        int gefragt = 0;
        mox->setMoxCheck([&gefragt]() {
            ++gefragt;
            return safety::BandPlanGuard::MoxCheckResult{
                false, QStringLiteral("ausserhalb des Bandes")};
        });

        auto* router = radio.micRouter();
        router->setActiveSource(MicSource::Pc);

        applet.sendeText(QStringLiteral("CQ DE OE5SOS"));

        QVERIFY2(gefragt > 0, "es wurde nie zu tasten versucht");
        QVERIFY2(!applet.sendetGerade(), "abgelehnt heisst: nicht senden");
        QVERIFY2(!mox->isMox(), "der Sender muss aus bleiben");
        QVERIFY2(router->activeSource() == MicSource::Pc,
                 "der Sendeweg darf nicht auf Rtty stehenbleiben — sonst "
                 "ist das Mikrofon nach einem abgelehnten Versuch stumm");
        // Die Zeile muss den GRUND nennen und stehen lassen. Sie
        // enthaelt dabei das Wort „transmitting" — als „not
        // transmitting: …", und genau so soll es sein. Anders als bei
        // FT8, wo updateStatus() im naechsten Takt ohnehin
        // drueberschreibt, bleibt sie hier stehen: ein abgelehnter
        // Sendeversuch, der kommentarlos verschwindet, ist schlimmer
        // als einer, der sagt warum.
        const QString zeile = applet.sendeStatusForTest()
                                  ? applet.sendeStatusForTest()->text()
                                  : QString();
        QVERIFY2(zeile.contains(QStringLiteral("ausserhalb des Bandes")),
                 qPrintable(QStringLiteral("die Zeile nennt den Grund "
                                           "nicht: %1").arg(zeile)));
        QVERIFY2(!zeile.startsWith(QStringLiteral("transmitting")),
                 qPrintable(QStringLiteral("die Zeile behauptet zu "
                                           "senden: %1").arg(zeile)));
    }

    // Zwei Druecke hintereinander: der zweite prallt ab, solange der
    // erste laeuft. Sonst stuende mitten im Text ein neuer Anfang.
    void einZweiterDruckPralltAb()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        applet.sendeText(QStringLiteral("ERSTER"));
        QVERIFY(applet.sendetGerade());
        const QString ersterText = radio.rttyTxSource()->text();

        applet.sendeText(QStringLiteral("ZWEITER"));
        QCOMPARE(radio.rttyTxSource()->text(), ersterText);

        applet.sendenAbbrechen();
    }

    // Der Router liefert waehrend der Sendung wirklich die
    // RTTY-Toene durch — nicht Stille und nicht das Mikrofon.
    void durchDenRouterKommtDerTon()
    {
        RadioModel radio;
        radio.installRttyTxForTest();
        RttyDecoderApplet applet(&radio);

        applet.sendeText(QStringLiteral("CQ"));
        QVERIFY(applet.sendetGerade());

        QVector<float> puffer(4096);
        bool etwas = false;
        for (int runde = 0; runde < 40 && !etwas; ++runde) {
            QCOMPARE(radio.micRouter()->pullSamples(puffer.data(), 4096), 4096);
            for (float v : puffer) { if (v != 0.0f) { etwas = true; break; } }
        }
        QVERIFY2(etwas, "durch den Router kommt kein Ton");

        applet.sendenAbbrechen();
    }
};

QTEST_MAIN(TstRttySendenFlaeche)
#include "tst_rtty_senden_flaeche.moc"
