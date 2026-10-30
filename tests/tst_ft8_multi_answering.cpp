// no-port-check: Longpath-eigener Pruefstand. Das VERFAHREN stammt aus
// MSHV (Herkunft im Kopf von core/Ft8Tx.h und in
// docs/attribution/MSHV-PROVENANCE.md); hier steht kein fremder Code.
// =================================================================
// tests/tst_ft8_multi_answering.cpp  (Longpath)
// =================================================================
//
// Mehrere Stationen in EINEM FT8-Fenster — MSHVs „Multi Answering".
//
// Betreiber am 2026-10-22, nach dem Vergleich der drei FT8-Programme:
// „mshv wäre meine erste wahl, sofern möglich."
//
// ── Warum dieser Prueflauf gegen den eigenen Dekoder faehrt ─────────
//
// Ein Sender laesst sich nicht daran pruefen, dass er Proben liefert —
// die liefert auch Rauschen. Der Rundlauf fasst dagegen Packung,
// Tonlage, Summierung und Pegel in einem Lauf: was hineingegeben
// wurde, muss wieder herauskommen. Dieselbe Begruendung wie beim
// RTTY-Rundlauf (PR #257).
//
// ── Was festgenagelt wird ───────────────────────────────────────────
//
//   * aus DREI Nachrichten in einem Fenster findet der eigene Dekoder
//     alle drei wieder — das ist der eigentliche Beweis
//   * die Tonleiter hat MSHVs 60 Hz Abstand und dessen Grenzen
//   * das Fenster wird NICHT laenger, nur voller
//   * der Pegel sinkt mit der Zahl der Plaetze (sonst uebersteuert es)
//   * eine unpackbare Nachricht laesst KEIN halbes Fenster stehen
//
// Modification history (Longpath):
//   2026-10-22 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QSignalSpy>
#include <QPushButton>
#include <QSpinBox>

#include "core/Ft8Decoder.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"
#include "models/LogEntry.h"
#include "core/AppSettings.h"
#include "core/Ft8Tx.h"

using namespace Longpath;

class TstFt8MultiAnswering : public QObject
{
    Q_OBJECT

private:
    static constexpr auto kFt8 = Ft8Decoder::Protocol::Ft8;

    /// Das 12-kHz-Fenster aus N Nachrichten, so wie
    /// vorbereitenMehrere() es intern bildet — summiert und geteilt.
    static QVector<float> summiere(const QVector<QPair<QString, float>>& n)
    {
        QVector<float> summe;
        for (const auto& [text, ton] : n) {
            QVector<float> eine;
            if (!Ft8Decoder::encodeBlock(text, ton, kFt8, &eine)) { return {}; }
            if (summe.size() < eine.size()) { summe.resize(eine.size()); }
            for (int i = 0; i < eine.size(); ++i) { summe[i] += eine[i]; }
        }
        for (float& v : summe) { v /= static_cast<float>(n.size()); }
        return summe;
    }

    static float spitze(const QVector<float>& v)
    {
        float m = 0.0f;
        for (float x : v) { m = std::max(m, std::abs(x)); }
        return m;
    }

private slots:

    void dieTonleiterFolgtMshv()
    {
        // MSHV gen_ft8.cpp:262 — 60 Hz je Platz.
        const QVector<float> l = Ft8TxSource::tonleiter(1500.0f, 4);
        QCOMPARE(l.size(), 4);
        QCOMPARE(l.at(0), 1500.0f);
        QCOMPARE(l.at(1), 1560.0f);
        QCOMPARE(l.at(2), 1620.0f);
        QCOMPARE(l.at(3), 1680.0f);
    }

    void dieTonleiterHaeltSichAnDieGrenzen()
    {
        // Unten 100, oben 5000 — ebenda. Ohne die Klammer liefe ein
        // Platz aus dem Durchlassbereich und waere still.
        const QVector<float> tief = Ft8TxSource::tonleiter(50.0f, 2);
        QCOMPARE(tief.at(0), 100.0f);

        const QVector<float> hoch = Ft8TxSource::tonleiter(4950.0f, 3);
        QCOMPARE(hoch.at(0), 4950.0f);
        QCOMPARE(hoch.at(1), 5000.0f);
        QCOMPARE(hoch.at(2), 5000.0f);
    }

    void dreiStationenKommenAlleWiederHeraus()
    {
        // DER Beweis: drei Nachrichten hinein, drei heraus.
        const QVector<QPair<QString, float>> plaetze = {
            {QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
            {QStringLiteral("DL2DEF OE5SOS JN67"), 1560.0f},
            {QStringLiteral("DL3GHI OE5SOS JN67"), 1620.0f},
        };

        const QVector<float> fenster = summiere(plaetze);
        QVERIFY2(!fenster.isEmpty(), "das Fenster liess sich nicht bilden");

        const QVector<Ft8Decode> funde =
            Ft8Decoder::decodeBlock(fenster.constData(), fenster.size(), kFt8);

        QStringList gefunden;
        for (const Ft8Decode& f : funde) { gefunden << f.text.trimmed(); }

        for (const auto& [text, ton] : plaetze) {
            QVERIFY2(gefunden.contains(text),
                     qPrintable(QStringLiteral("fehlt: %1 — gefunden: %2")
                                    .arg(text, gefunden.join(QStringLiteral(" | ")))));
        }
    }

    void dasFensterWirdNichtLaengerNurVoller()
    {
        // Drei Stationen duerfen nicht die dreifache Sendezeit kosten —
        // sie teilen sich DASSELBE Fenster. Waere es laenger, liefe der
        // Sender in das Empfangsfenster hinein.
        Ft8TxSource eine(48000);
        QVERIFY(eine.vorbereiten(QStringLiteral("DL1ABC OE5SOS JN67"),
                                 1500.0f, kFt8));
        const int allein = eine.probenImFenster();

        Ft8TxSource drei(48000);
        QVERIFY(drei.vorbereitenMehrere(
            {{QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
             {QStringLiteral("DL2DEF OE5SOS JN67"), 1560.0f},
             {QStringLiteral("DL3GHI OE5SOS JN67"), 1620.0f}},
            kFt8));

        QCOMPARE(drei.probenImFenster(), allein);
    }

    void derPegelSinktMitDerZahlDerPlaetze()
    {
        // MSHV teilt durch die Zahl der Plaetze (gen_ft8.cpp:303). Ohne
        // das uebersteuert jedes Fenster mit mehr als einer Station.
        // Also: die Summe aus zweien darf nicht lauter sein als eine.
        QVector<float> eine;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("DL1ABC OE5SOS JN67"),
                                        1500.0f, kFt8, &eine));
        const float spitzeEine = spitze(eine);

        const QVector<float> zwei = summiere(
            {{QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
             {QStringLiteral("DL2DEF OE5SOS JN67"), 1560.0f}});
        QVERIFY(!zwei.isEmpty());

        QVERIFY2(spitze(zwei) <= spitzeEine * 1.01f,
                 qPrintable(QStringLiteral("zwei Plaetze sind lauter als einer: "
                                           "%1 gegen %2")
                                .arg(spitze(zwei)).arg(spitzeEine)));
    }

    void eineUnpackbareNachrichtLaesstKeinHalbesFenster()
    {
        // Die dritte ist Unsinn. Dann darf NICHTS vorbereitet sein --
        // ein Fenster, in dem stillschweigend eine Station fehlt, ist
        // schlimmer als keines.
        Ft8TxSource quelle(48000);
        QVERIFY(quelle.vorbereiten(QStringLiteral("DL1ABC OE5SOS JN67"),
                                   1500.0f, kFt8));
        const int vorher = quelle.probenImFenster();
        QVERIFY(vorher > 0);

        const bool ging = quelle.vorbereitenMehrere(
            {{QStringLiteral("DL1ABC OE5SOS JN67"), 1500.0f},
             {QStringLiteral("!!! kein gueltiges FT8 !!! viel zu lang und krumm"), 1560.0f}},
            kFt8);

        QVERIFY2(!ging, "eine unpackbare Nachricht ging durch");
        QVERIFY2(!quelle.laeuft(), "es laeuft trotz Fehlschlag");
    }

    // ── Und jetzt ist es auch ERREICHBAR (2026-10-30) ───────────────
    //
    // Die Faelle darueber pruefen `Ft8Mehrfach` und
    // `Ft8TxSource::vorbereitenMehrere` fuer sich. Beide waren seit
    // dem Bau (#323/#333/#335) vollstaendig und gruen -- und wurden
    // von NIEMANDEM gerufen: sie kamen nur in ihrer eigenen Datei und
    // in diesen Prueflaeufen vor. In der Bilanz gegen MSHV stand
    // trotzdem „gebaut".
    //
    // Betreiber am 2026-10-30: „wie funktioniert mshv nun bei mir" --
    // und beim Nachsehen war die Antwort: gar nicht. Diese Faelle
    // nageln den Anschluss fest, damit das nicht noch einmal
    // unbemerkt abreiszt.

    void dasAppletHatEinenSchalterDafuer()
    {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY2(applet.plaetzeSpinForTest(), "der Schalter fehlt");
        QCOMPARE(applet.plaetzeSpinForTest()->minimum(), 1);
        QCOMPARE(applet.plaetzeSpinForTest()->maximum(),
                 Ft8Mehrfach::kMaxPlaetze);
        // Ab Werk AUS: die Sendeleistung wird geteilt, das waehlt
        // niemand versehentlich.
        QCOMPARE(applet.plaetzeSpinForTest()->value(), 1);
    }

    void derSchalterStelltDieZahlDerPlaetzeDurch()
    {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.mehrfachForTest().plaetze(), 1);
        applet.plaetzeSpinForTest()->setValue(3);
        QCOMPARE(applet.mehrfachForTest().plaetze(), 3);
    }

    void zweiAnruferLandenAufZweiPlaetzen()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.mehrfachForTest().plaetze(), 3);

        const auto fund = [&applet](const char* text, double snr) {
            Ft8Decode d;
            d.text   = QString::fromLatin1(text);
            d.snrDb  = snr;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };

        fund("OE5SOS DL1ABC JO31", -7.0);
        fund("OE5SOS SP9XYZ JO90", -12.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 2);
        QVERIFY(applet.mehrfachForTest().kennt(QStringLiteral("DL1ABC")));
        QVERIFY(applet.mehrfachForTest().kennt(QStringLiteral("SP9XYZ")));

        // GEGENPROBE: derselbe Ruf noch einmal legt keinen zweiten
        // Platz an -- sonst antwortete man demselben zweimal in
        // demselben Fenster, auf zwei Toenen.
        fund("OE5SOS DL1ABC JO31", -7.0);
        QCOMPARE(applet.mehrfachForTest().aktive(), 2);
    }

    // Und die Gegenprobe zum Schalter: mit EINEM Platz bleibt alles
    // beim einzelnen Ablauf. Ohne diesen Lauf belegt der Fall
    // darueber nicht, dass der Schalter etwas aendert.
    void mitEinemPlatzLaeuftDerGewoehnlicheWeg()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("OE5SOS DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);

        QCOMPARE(applet.mehrfachForTest().aktive(), 0);
    }


    // ── Vier Fehler aus der Durchsicht vom 2026-10-30 ───────────────
    //
    // Alle vier wurden eine Stunde nach dem Anschluss gefunden, beim
    // ruhigen Durchlesen. Sie haben hier ihren Prueffall, damit
    // keiner davon still zurueckkommt.

    // 1. Der schwerste: der Block in `fensterZuEnde` war NIE
    //    geschrieben worden (eine Ersetzung war still
    //    fehlgeschlagen). Multi Answering sendete, aber kein Zug kam
    //    je voran und kein QSO landete im Logbuch.
    void einPlatzKommtUeberDasFensterVoran()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("OE5SOS DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);

        // Ein Fenster vergeht, ohne dass etwas kam: der Zaehler des
        // Platzes muss laufen. Ohne den fehlenden Block geschah hier
        // GAR NICHTS.
        const auto vorher = applet.mehrfachForTest().belegung();
        QCOMPARE(vorher.size(), 1);
        for (int i = 0; i < 6; ++i) { applet.fensterZuEndeForTest(); }
        QVERIFY2(applet.mehrfachForTest().aktive() == 0,
                 "nach sechs leeren Fenstern muss der Platz aufgeben");
    }

    void einFertigesQsoAufEinemPlatzLandetImLogbuch()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);

        const auto fund = [&applet](const char* t, double snr) {
            Ft8Decode d;
            d.text   = QString::fromLatin1(t);
            d.snrDb  = snr;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };

        fund("OE5SOS DL1ABC JO31", -7.0);     // er ruft mich
        fund("OE5SOS DL1ABC R-15", -7.0);     // er bestaetigt
        fund("OE5SOS DL1ABC RR73", -7.0);     // und ist fertig
        applet.fensterZuEndeForTest();

        QCOMPARE(gemeldet.count(), 1);
        const auto e = gemeldet.first().first().value<LogEntry>();
        QCOMPARE(e.call, QStringLiteral("DL1ABC"));
    }

    // 2. Stop hielt die Plaetze nicht an.
    void stopHaeltAuchDiePlaetzeAn()
    {
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("3"));
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("OE5SOS DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QCOMPARE(applet.mehrfachForTest().aktive(), 1);

        applet.stopButtonForTest()->click();
        QCOMPARE(applet.mehrfachForTest().aktive(), 0);
    }

    // 3. Der Protokollwechsel liess die gehoerten Tonlagen stehen.
    void derProtokollwechselVergisstDieTonlagen()
    {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
        RadioModel radio;
        Ft8Applet applet(&radio);

        Ft8Decode d;
        d.text   = QStringLiteral("CQ DL1ABC JO31");
        d.snrDb  = -7.0;
        d.freqHz = 1500.0;
        applet.fundForTest(d);
        QVERIFY(!applet.belegteToeneForTest().isEmpty());

        // FT4 liegt auf einer anderen Frequenz -- 1500 Hz dort ist
        // eine andere Stelle im Band.
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        QVERIFY2(applet.belegteToeneForTest().isEmpty(),
                 "die alten Tonlagen gelten auf der neuen Frequenz nicht");
    }

};

QTEST_MAIN(TstFt8MultiAnswering)
#include "tst_ft8_multi_answering.moc"
