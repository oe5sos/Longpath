// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Sendeseite — geschlossene Schleife, ohne Funkgeraet.
//
// Was hier geprueft wird, ist der Weg, den eine FT8-Nachricht bis zum
// Sender nimmt: Text -> ft8_lib-Modulation (12 kHz) -> Umtastung auf
// die Mikrofonrate (48 kHz) -> `pullSamples()` in Haeppchen, so wie
// der Tonfaden sie abholt. Was dabei herauskommt, wird wieder auf
// 12 kHz gebracht und DEKODIERT: steht dieselbe Nachricht da, hat der
// ganze Weg getragen.
//
// Ohne diese Schleife waere „es sendet" eine Behauptung ueber Code,
// den niemand gehoert hat.
//
// ── Was hier bewusst NICHT steht ────────────────────────────────────
//
// Das Tasten. Ft8TxSource weiss, WAS gesendet wuerde und wie lang das
// Fenster ist; ob der Sender angeht, entscheidet jemand anderes.
// Dieselbe Trennung wie beim Sprachspeicher (audio/VoiceKeyer.h).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>

#include "core/Ft8Decoder.h"
#include "core/Ft8Tx.h"
#include "core/Ft8Ablauf.h"
#include "models/LogEntry.h"
#include "core/AppSettings.h"
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QMouseEvent>
#include "core/MoxController.h"
#include "core/safety/BandPlanGuard.h"
#include "models/RadioModel.h"
#include "gui/applets/Ft8Applet.h"
#include "core/Resampler.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/PcMicSource.h"

using namespace Longpath;

class TstFt8Senden : public QObject
{
    Q_OBJECT

private:
    /// Holt das ganze Fenster so ab, wie der Tonfaden es tut: in
    /// gleich grossen Haeppchen.
    static QVector<float> abholen(Ft8TxSource& quelle, int haeppchen,
                                  int hoechstens)
    {
        QVector<float> alles;
        QVector<float> puffer(haeppchen);
        while (alles.size() < hoechstens) {
            const int n = quelle.pullSamples(puffer.data(), haeppchen);
            if (n != haeppchen) { break; }
            for (int i = 0; i < n; ++i) { alles.append(puffer[i]); }
            if (!quelle.laeuft()) { break; }
        }
        return alles;
    }

private:
    /// Ein Doppelklick auf die erste Zeile der Fundliste. Per
    /// sendEvent, weil QTest-Mausklicks ohne Schirm verpuffen.
    static void doppelklickInDieListe(Ft8Applet& applet)
    {
        auto* vp = applet.listForTest()->viewport();
        QMouseEvent ev(QEvent::MouseButtonDblClick, QPointF(10, 5),
                       vp->mapToGlobal(QPointF(10, 5)),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(vp, &ev);
    }

private slots:

    // ── Die Schleife ────────────────────────────────────────────────

    void wasGesendetWuerdeLaesstSichZurueckLesen()
    {
        const QString text = QStringLiteral("CQ OE5SOS JN78");
        Ft8TxSource quelle(48000);
        QVERIFY2(quelle.vorbereiten(text, 1500.0f,
                                    Ft8Decoder::Protocol::Ft8),
                 "ft8_lib konnte die Nachricht nicht packen");
        QCOMPARE(quelle.nachricht(), text);

        // Ein ganzes FT8-Fenster bei 48 kHz: 15 s = 720 000 Proben.
        qInfo() << "Fenster am Sendeweg:" << quelle.probenImFenster()
                << "Proben";
        QVERIFY2(quelle.probenImFenster() > 700000,
                 qPrintable(QString::number(quelle.probenImFenster())));

        // Vor dem Start ist Stille — und zwar ohne Ausnahme.
        QVector<float> stille(1024);
        QCOMPARE(quelle.pullSamples(stille.data(), 1024), 1024);
        for (float v : stille) { QCOMPARE(v, 0.0f); }
        QVERIFY(!quelle.laeuft());

        quelle.starten();
        QVERIFY(quelle.laeuft());

        QVector<float> alles;
        QVector<float> puffer(1024);
        int runden = 0;
        while (quelle.laeuft() && runden < 2000) {
            const int n = quelle.pullSamples(puffer.data(), 1024);
            QCOMPARE(n, 1024);
            for (int i = 0; i < n; ++i) { alles.append(puffer[i]); }
            ++runden;
        }
        qInfo() << "abgeholt:" << alles.size() << "Proben in" << runden
                << "Haeppchen";
        QVERIFY2(!quelle.laeuft(), "die Quelle hoert nicht von selbst auf");

        // Zurueck auf 12 kHz und dekodieren.
        Resampler zurueck(48000.0, double(Ft8Decoder::kSampleRate), 4096);
        QVector<float> bei12k;
        QVector<float> aus(4096 + 64);
        for (int pos = 0; pos < alles.size(); pos += 4096) {
            const int n = qMin(4096, alles.size() - pos);
            const int g = zurueck.processInto(alles.constData() + pos, n,
                                              aus.data(), aus.size());
            for (int i = 0; i < g; ++i) { bei12k.append(aus[i]); }
        }
        qInfo() << "zurueck bei 12 kHz:" << bei12k.size() << "Proben";

        const auto funde = Ft8Decoder::decodeBlock(bei12k.constData(),
                                                   bei12k.size());
        QStringList wortlaute;
        for (const Ft8Decode& d : funde) { wortlaute << d.text; }
        qInfo().noquote() << "zurueckgelesen:" << wortlaute.join(QLatin1String(" | "));
        QVERIFY2(wortlaute.contains(text),
                 qPrintable(QStringLiteral("gesendet „%1\", gelesen: %2")
                                .arg(text, wortlaute.join(QLatin1String(" | ")))));
    }

    /// Abbrechen heisst sofort still — nicht am Ende des Fensters.
    void abbrechenWirktSofort()
    {
        Ft8TxSource quelle(48000);
        QVERIFY(quelle.vorbereiten(QStringLiteral("CQ OE5SOS JN78"),
                                   1500.0f, Ft8Decoder::Protocol::Ft8));
        quelle.starten();

        // ── Ein Fenster beginnt mit STILLE ──────────────────────
        //
        // Gemessen am 2026-10-15: encodeBlock legt die Daten mittig
        // ins Fenster, davor liegen rund 1,18 s Ruhe (ft8_lib
        // demo/gen_ft8.c rechnet `stille = (ganz - nutz) / 2`). Die
        // ersten 1024 Proben — 21 ms — sind darum still, und der
        // erste Entwurf dieses Laufs erklaerte das zum Fehler.
        //
        // Fuer das Tasten heisst das: der Sender muss zu
        // FENSTERBEGINN an, nicht erst, wenn man etwas hoert.
        QVector<float> puffer(1024);
        bool etwasGehoert = false;
        int gelesen = 0;
        while (gelesen < 48000 * 3 && !etwasGehoert) {
            QCOMPARE(quelle.pullSamples(puffer.data(), 1024), 1024);
            gelesen += 1024;
            for (float v : puffer) {
                if (v != 0.0f) { etwasGehoert = true; break; }
            }
        }
        qInfo() << "erster Ton nach" << gelesen << "Proben ="
                << QString::number(gelesen / 48000.0, 'f', 2) << "s";
        QVERIFY2(etwasGehoert, "in den ersten drei Sekunden kein Ton");

        quelle.abbrechen();
        QVERIFY(!quelle.laeuft());
        QCOMPARE(quelle.pullSamples(puffer.data(), 1024), 1024);
        for (float v : puffer) { QCOMPARE(v, 0.0f); }
    }

    // ── Die Gegenperiode ────────────────────────────────────────────

    void dieGegenperiodeLiegtRichtig()
    {
        // 1 000 000 000 000 ms ist nicht durch 60 000 teilbar; die
        // Rechnung geht von der vollen Minute aus, also wird hier eine
        // genommen: 999 999 960 000 ms.
        const qint64 minute = 999999960000LL;
        QCOMPARE(minute % 60000, 0LL);

        const auto ft8 = Ft8Decoder::Protocol::Ft8;
        QCOMPARE(Ft8Tx::fensterMs(ft8), 15000);
        QCOMPARE(Ft8Tx::fensterNummer(minute, ft8), 0);
        QCOMPARE(Ft8Tx::fensterNummer(minute + 15000, ft8), 1);
        QCOMPARE(Ft8Tx::fensterNummer(minute + 45000, ft8), 3);

        QVERIFY(Ft8Tx::istSendefenster(minute, ft8, Ft8Periode::Gerade));
        QVERIFY(!Ft8Tx::istSendefenster(minute, ft8, Ft8Periode::Ungerade));
        QVERIFY(Ft8Tx::istSendefenster(minute + 15000, ft8,
                                       Ft8Periode::Ungerade));

        // FT4: halb so lange Fenster, also acht in der Minute.
        const auto ft4 = Ft8Decoder::Protocol::Ft4;
        QCOMPARE(Ft8Tx::fensterMs(ft4), 7500);
        QCOMPARE(Ft8Tx::fensterNummer(minute + 7500, ft4), 1);

        // Und der Weg bis zum naechsten eigenen Fenster.
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute, ft8, Ft8Periode::Gerade), 0LL);
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute, ft8, Ft8Periode::Ungerade),
                 15000LL);
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute + 1000, ft8,
                                        Ft8Periode::Gerade), 29000LL);
    }

    /// FT4 geht denselben Weg — kuerzeres Fenster, gleiche Schleife.
    void ft4SendetAuch()
    {
        Ft8TxSource quelle(48000);
        QVERIFY(quelle.vorbereiten(QStringLiteral("CQ OE5SOS JN78"),
                                   1500.0f, Ft8Decoder::Protocol::Ft4));
        // 7,5 s bei 48 kHz = 360 000 Proben.
        qInfo() << "FT4-Fenster:" << quelle.probenImFenster() << "Proben";
        QVERIFY2(quelle.probenImFenster() > 340000,
                 qPrintable(QString::number(quelle.probenImFenster())));
        QVERIFY2(quelle.probenImFenster() < 400000,
                 "ein FT4-Fenster ist halb so lang wie ein FT8-Fenster");
    }

    /// Eine Nachricht, die ft8_lib nicht packen kann, darf NICHT
    /// stillschweigend ein leeres Fenster senden.
    void einUnmoeglicherTextSendetNichts()
    {
        Ft8TxSource quelle(48000);
        const bool ok = quelle.vorbereiten(
            QStringLiteral("DIES IST VIEL ZU LANG FUER EINE FT8 NACHRICHT 123"),
            1500.0f, Ft8Decoder::Protocol::Ft8);
        qInfo() << "zu langer Text angenommen:" << ok;
        if (!ok) {
            QCOMPARE(quelle.probenImFenster(), 0);
            quelle.starten();
            QVERIFY2(!quelle.laeuft(),
                     "ohne vorbereitete Nachricht darf nichts starten");
        }
    }

    // ── Der Weg in den Sender ───────────────────────────────────────

    /// Am Mikrofon-Router angemeldet — und ohne laufende Quelle still.
    ///
    /// Das ist die Stelle, an der FT8 bisher endete: gebaut, aber
    /// nirgends angeschlossen. Der Sprachspeicher steht bis heute so
    /// da (WavTxSource, niemand ruft es).
    // ── Sagt der Bandplan nein, wird nicht gesendet ─────────────────
    //
    // MoxController::setMox(true) darf abgelehnt werden (falsche
    // Betriebsart, Frequenz ausserhalb des Bandes, Cross-Band-TX). Die
    // Ablehnung kommt NUR als moxRejected(reason) — setMox() gibt
    // nichts zurueck.
    //
    // Bis zum 2026-10-16 hoerte im Ft8Applet niemand darauf: der Ton
    // lief danach das ganze Fenster weiter, der Sendeweg blieb auf
    // FT8 (das Mikrofon also abgeklemmt), und in der Zeile stand
    // „transmitting", waehrend nichts hinausging.
    //
    // FT4 statt FT8, damit der Lauf hoechstens ein 7,5-s-Fenster
    // abwartet statt eines von 15 s.
    void einAbgelehntesTastenBrichtDasSendenAb()
    {
        RadioModel radio;
        radio.installFt8TxForTest();   // sonst: „no transmit path"
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);

        auto* mox = radio.moxController();
        QVERIFY2(mox, "ohne MoxController hat dieser Lauf keinen Gegenstand");

        // Eine Pruefung, die immer ablehnt — wie der Bandplan an einer
        // Stelle, an der nicht gesendet werden darf.
        // MoxCheckFn nimmt nichts entgegen und wird nur vor dem
        // EINschalten gefragt — siehe MoxController::setMoxCheck.
        int gefragt = 0;
        mox->setMoxCheck([&gefragt]() {
            ++gefragt;
            return safety::BandPlanGuard::MoxCheckResult{
                false, QStringLiteral("ausserhalb des Bandes")};
        });

        applet.sendeNachricht(QStringLiteral("CQ OE5SOS JN78"));
        QVERIFY2(applet.sendetGerade(),
                 "der Versuch muss erst einmal anlaufen — er wartet auf "
                 "sein Fenster");

        // Sobald das Fenster beginnt, wird getastet, abgelehnt und
        // abgebrochen.
        //
        // Die Frist nicht raten, sondern rechnen: gesendet wird in der
        // GEGENperiode, die kommt nur jedes zweite Fenster. Die
        // Wartezeit ist also bis zu 2 x fensterMs — bei FT4 bis zu
        // 15 s, nicht 7,5. Mit geschaetzten 12 s fielen am 2026-10-16
        // zwei von drei Laeufen um (13100 und 13150 ms haetten
        // gereicht), waehrend der erste gruen war: genau die Art
        // Wette, die dieser Pruefstand heute an vier Stellen
        // losgeworden ist.
        const int maxWarten = 2 * Ft8Tx::fensterMs(Ft8Decoder::Protocol::Ft4);
        QTRY_VERIFY_WITH_TIMEOUT(!applet.sendetGerade(), maxWarten + 5000);

        QVERIFY2(gefragt > 0, "es wurde nie zu tasten versucht");
        QVERIFY2(!mox->isMox(), "abgelehnt heisst: der Sender bleibt aus");

        // Und der Sendeweg liegt wieder, wo er war — sonst waere das
        // Mikrofon nach einem abgelehnten Versuch stumm.
        if (auto* router = radio.micRouter()) {
            QVERIFY2(router->activeSource() != MicSource::Ft8,
                     "nach dem Abbruch darf der Sendeweg nicht auf FT8 "
                     "stehenbleiben");
        }

        // Die Zeile darf nicht BEHAUPTEN zu senden — aber sie darf
        // (und soll) „not transmitting: <grund>" sagen. Ein
        // `contains("transmitting")` trifft genau diesen richtigen
        // Fall mit: im Durchgang vom 2026-10-16 fiel der Lauf damit
        // um, und zwar unstet, weil updateStatus() die Zeile mal
        // rechtzeitig ueberschreibt und mal nicht. Das ist dieselbe
        // Wette aufs Timing, die dieser Pruefstand an vier anderen
        // Stellen losgeworden ist.
        //
        // Geprueft wird darum, was wirklich zaehlt: die Zeile faengt
        // nicht mit „transmitting" an, und wenn der Grund noch steht,
        // nennt sie ihn.
        const QString zeile = applet.statusForTest()
                                  ? applet.statusForTest()->text() : QString();
        QVERIFY2(!zeile.startsWith(QStringLiteral("transmitting")),
                 qPrintable(QStringLiteral("die Zeile behauptet zu senden: %1")
                                .arg(zeile)));
    }

    // ── Auf eine gehoerte Station antworten ─────────────────────────
    //
    // Ein Doppelklick in die Fundliste soll den Standardanruf ins
    // Eingabefeld legen. Geprueft wird die REGEL, nicht die Maus:
    // antwortAuf() ist statisch und braucht weder Fenster noch
    // Zeigegeraet.
    void einKlickAufEineZeileWirdZurAntwort_data()
    {
        QTest::addColumn<QString>("zeile");
        QTest::addColumn<QString>("erwartet");

        // So, wie onDecoded() die Zeile baut: HHMMSS, dB auf 4,
        // DT auf 5, Hz auf 5, dann zwei Leerzeichen und die Nachricht.
        QTest::newRow("CQ mit Locator")
            << QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31")
            << QStringLiteral("DL1ABC OE5SOS JN67");
        QTest::newRow("CQ DX")
            << QStringLiteral("120000  -14  -0.3   780  CQ DX VK3ABC QF22")
            << QStringLiteral("VK3ABC OE5SOS JN67");
        // Ein laufendes QSO zweier anderer: geantwortet wird dem, der
        // sendet — senderCallsign() liest das zweite Rufzeichen.
        QTest::newRow("fremdes QSO")
            << QStringLiteral("120000   -3   0.0  2100  OE1XYZ DL1ABC -12")
            << QStringLiteral("DL1ABC OE5SOS JN67");
        // Sich selbst anrufen ergibt nichts.
        QTest::newRow("das bin ich")
            << QStringLiteral("120000   -3   0.0  2100  DL1ABC OE5SOS -12")
            << QString();
        // Kein Rufzeichen in der Zeile.
        QTest::newRow("nur Grusswort")
            << QStringLiteral("120000   -3   0.0  2100  73")
            << QString();
        // Eine Zeile ohne Nachrichtenspalte.
        QTest::newRow("abgeschnitten")
            << QStringLiteral("120000   -3")
            << QString();
    }

    void einKlickAufEineZeileWirdZurAntwort()
    {
        QFETCH(QString, zeile);
        QFETCH(QString, erwartet);
        QCOMPARE(Ft8Applet::antwortAuf(zeile, QStringLiteral("OE5SOS"),
                                       QStringLiteral("JN67VV")),
                 erwartet);
    }

    // Ohne eigenes Rufzeichen gibt es keine Antwort — sonst stuende
    // eine halbe Nachricht im Feld, die beim Senden nicht packbar ist.
    void ohneEigenesRufzeichenKeineAntwort()
    {
        const QString zeile =
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31");
        QVERIFY(Ft8Applet::antwortAuf(zeile, QString(),
                                      QStringLiteral("JN67VV")).isEmpty());
        QVERIFY(Ft8Applet::antwortAuf(zeile, QStringLiteral("   "),
                                      QStringLiteral("JN67VV")).isEmpty());

        // Ohne Locator aber schon: der ist in einer FT8-Antwort
        // ueblich, aber nicht zwingend.
        QCOMPARE(Ft8Applet::antwortAuf(zeile, QStringLiteral("OE5SOS"),
                                       QString()),
                 QStringLiteral("DL1ABC OE5SOS"));
    }

    // ── Der Schalter „Auto" entscheidet, ob von selbst gesendet wird ─
    //
    // Ohne ihn legt ein Doppelklick die Antwort nur ins Feld. Mit ihm
    // faengt der Ablauf an. Das ist die Grenze zwischen „das Programm
    // schlaegt vor" und „das Programm sendet", und sie wird hier
    // beidseitig festgehalten.
    //
    // Der Doppelklick geht per sendEvent an den viewport: QTest-Klicks
    // verpuffen ohne Schirm, und dieser Lauf hat keinen.
    void ohneAutoFuelltDerDoppelklickNurDasFeld()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        auto* liste = applet.listForTest();
        QVERIFY(liste);
        liste->setPlainText(
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31"));

        QVERIFY2(applet.autoButtonForTest()
                     && !applet.autoButtonForTest()->isChecked(),
                 "Auto muss AUS voreingestellt sein");

        doppelklickInDieListe(applet);

        QCOMPARE(applet.textFeldForTest()->text(),
                 QStringLiteral("DL1ABC OE5SOS JN67"));
        QVERIFY2(!applet.ablaufForTest().laeuft(),
                 "ohne Auto darf kein Ablauf anlaufen");
        QVERIFY2(!applet.sendetGerade(),
                 "ohne Auto darf nichts gesendet werden");
    }

    void mitAutoFaengtDerAblaufAn()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        applet.listForTest()->setPlainText(
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31"));
        applet.autoButtonForTest()->setChecked(true);

        doppelklickInDieListe(applet);

        QVERIFY2(applet.ablaufForTest().laeuft(),
                 "mit Auto muss der Ablauf anlaufen");
        QCOMPARE(applet.ablaufForTest().partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::RufeAn);
        QVERIFY2(applet.sendetGerade(),
                 "der erste Zug muss auf sein Fenster warten");

        // Und der Schalter haelt ihn auch wieder an — sonst gaebe es
        // keinen Weg zurueck ausser dem Stop-Knopf.
        applet.autoButtonForTest()->setChecked(false);
        QVERIFY(!applet.ablaufForTest().laeuft());
        applet.abbrechen();
    }

    // ── Der Logbucheintrag eines fertigen QSOs ──────────────────────
    //
    // Geprueft werden die FELDER, nicht die Zustellung: logEintrag()
    // ist statisch, das Signal ist nur der Weg nach draussen
    // (MainWindow -> RotorLogbookPanel::logExternalQso, derselbe wie
    // bei einem QSO aus WSJT-X).
    void derLogbucheintragStimmt()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67VV"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC R-15"), -7));
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::Fertig);

        // 14,074 MHz Wahlfrequenz, oberes Seitenband: der Sendeton
        // liegt darueber.
        const LogEntry e = Ft8Applet::logEintrag(
            a, 14074000.0, /*unteresSeitenband=*/false,
            Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67VV"));

        QCOMPARE(e.call, QStringLiteral("DL1ABC"));
        QCOMPARE(e.mode, QStringLiteral("FT8"));
        QCOMPARE(e.band, QStringLiteral("20m"));
        QCOMPARE(e.rstSent, QStringLiteral("-07"));
        QCOMPARE(e.rstRcvd, QStringLiteral("-15"));
        QCOMPARE(e.gridSquare, QStringLiteral("JO31"));
        QCOMPARE(e.myGridSquare, QStringLiteral("JN67VV"));
        QVERIFY(e.timeOn.isValid());
        QCOMPARE(e.timeOn.timeSpec(), Qt::UTC);
        // 14,074 MHz + 1500 Hz Sendeton.
        QVERIFY2(qAbs(e.freqMHz - 14.0755) < 1e-6,
                 qPrintable(QStringLiteral("Frequenz %1").arg(e.freqMHz, 0, 'f', 6)));
    }

    // In DIGL liegt der Ton UNTER der Wahlfrequenz — ohne das stuende
    // die Frequenz im Logbuch um bis zu 3 kHz daneben.
    void inDiglLiegtDerTonDarunter()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));

        const LogEntry oben = Ft8Applet::logEintrag(
            a, 14074000.0, false, Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67"));
        const LogEntry unten = Ft8Applet::logEintrag(
            a, 14074000.0, true, Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67"));
        QVERIFY2(unten.freqMHz < oben.freqMHz,
                 "in DIGL muss die Frequenz unter der in DIGU liegen");
        QVERIFY(qAbs((oben.freqMHz - unten.freqMHz) - 0.003) < 1e-6);
    }

    // FT4 ist eine eigene ADIF-Betriebsart, kein SUBMODE von FT8.
    void ft4StehtAlsEigeneBetriebsart()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        const LogEntry e = Ft8Applet::logEintrag(
            a, 7074000.0, false, Ft8Decoder::Protocol::Ft4, QStringLiteral("JN67"));
        QCOMPARE(e.mode, QStringLiteral("FT4"));
        QVERIFY2(e.submode.isEmpty(), "FT4 ist kein SUBMODE");
        QCOMPARE(e.band, QStringLiteral("40m"));
    }

    // Kam nie ein Feldkenner, bleibt das Feld leer statt geraten.
    void ohneFeldkennerBleibtDasFeldLeer()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC"), -7));
        const LogEntry e = Ft8Applet::logEintrag(
            a, 14074000.0, false, Ft8Decoder::Protocol::Ft8, QStringLiteral("JN67"));
        QVERIFY(e.gridSquare.isEmpty());
    }

    // ── Der ganze Weg: Doppelklick bis Logbucheintrag ───────────────
    //
    // Und die Wache dabei: zeigeAblauf() laeuft bei jedem Schritt, und
    // der Zustand bleibt auf Fertig stehen. Ohne m_qsoGemeldet stuende
    // dasselbe QSO mehrfach im Logbuch — darum wird hier GEZAEHLT,
    // nicht nur geschaut, ob ueberhaupt etwas kommt.
    void einQsoWirdGenauEinmalGemeldet()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);

        applet.listForTest()->setPlainText(
            QStringLiteral("120000   -8   0.2  1500  CQ DL1ABC JO31"));
        applet.autoButtonForTest()->setChecked(true);
        doppelklickInDieListe(applet);
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeAn);
        QCOMPARE(applet.ablaufForTest().partnerLocator(),
                 QStringLiteral("JO31"));

        const auto fund = [&applet](const char* text, double snr) {
            Ft8Decode d;
            d.text    = QString::fromLatin1(text);
            d.snrDb   = snr;
            d.freqHz  = 1500.0;
            applet.fundForTest(d);
        };

        fund("OE5SOS DL1ABC -09", -12.0);
        QCOMPARE(applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::SendeRReport);

        fund("OE5SOS DL1ABC RR73", -12.0);
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::Sende73);
        QCOMPARE(gemeldet.count(), 0);   // noch nicht fertig

        fund("OE5SOS DL1ABC 73", -12.0);
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::Fertig);
        QCOMPARE(gemeldet.count(), 1);

        // Weitere Funde derselben Station aendern nichts mehr — und
        // melden vor allem nicht noch einmal.
        fund("OE5SOS DL1ABC 73", -12.0);
        fund("OE5SOS DL1ABC RR73", -12.0);
        QCOMPARE(gemeldet.count(), 1);

        // Und der Weg, auf dem die zweite Meldung wirklich entstuende:
        // das zuletzt gesendete Fenster laeuft ab, fensterZuEnde()
        // sieht einen Ablauf, der nicht mehr laeuft, und zeigt ihn an.
        // Ohne die Wache stuende das QSO hier ein zweites Mal im
        // Logbuch — im Betrieb bei JEDEM Durchgang.
        applet.fensterZuEndeForTest();
        applet.fensterZuEndeForTest();
        QCOMPARE(gemeldet.count(), 1);

        const auto e = gemeldet.first().first().value<LogEntry>();
        QCOMPARE(e.call, QStringLiteral("DL1ABC"));
        QCOMPARE(e.mode, QStringLiteral("FT4"));
        QCOMPARE(e.gridSquare, QStringLiteral("JO31"));
        QCOMPARE(e.rstSent, QStringLiteral("-08"));   // SNR der CQ-Zeile
        QCOMPARE(e.rstRcvd, QStringLiteral("-09"));

        applet.abbrechen();
    }

    void derRouterLiefertDieFt8QuelleDurch()
    {
        PcMicSource* keinPc = nullptr;
        CompositeTxMicRouter router(keinPc, nullptr, false);

        Ft8TxSource quelle(48000);
        router.setFt8Source(&quelle);
        QCOMPARE(router.ft8Source(), static_cast<TxMicRouter*>(&quelle));

        router.setActiveSource(MicSource::Ft8);

        // Ohne gestartete Quelle: Stille, kein Rauschen, kein Muell.
        QVector<float> puffer(512);
        QCOMPARE(router.pullSamples(puffer.data(), 512), 512);
        for (float v : puffer) { QCOMPARE(v, 0.0f); }

        // Mit gestarteter Quelle kommt das Fenster durch den Router —
        // dieselben Proben, die die Quelle selbst liefert.
        QVERIFY(quelle.vorbereiten(QStringLiteral("CQ OE5SOS JN78"),
                                   1500.0f, Ft8Decoder::Protocol::Ft8));
        quelle.starten();

        bool etwas = false;
        int gelesen = 0;
        while (gelesen < 48000 * 3 && !etwas) {
            QCOMPARE(router.pullSamples(puffer.data(), 512), 512);
            gelesen += 512;
            for (float v : puffer) { if (v != 0.0f) { etwas = true; break; } }
        }
        QVERIFY2(etwas, "durch den Router kommt kein Ton");

        // Und die Gegenprobe: ohne angemeldete Quelle faellt der
        // Router auf Pc zurueck (hier null) und liefert Stille statt
        // zu stuerzen.
        router.setFt8Source(nullptr);
        QCOMPARE(router.pullSamples(puffer.data(), 512), 512);
        for (float v : puffer) { QCOMPARE(v, 0.0f); }
    }
};

QTEST_MAIN(TstFt8Senden)
#include "tst_ft8_senden.moc"
