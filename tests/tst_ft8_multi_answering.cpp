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

#include "core/Ft8Decoder.h"
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
};

QTEST_MAIN(TstFt8MultiAnswering)
#include "tst_ft8_multi_answering.moc"
