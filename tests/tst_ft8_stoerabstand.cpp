// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_ft8_stoerabstand.cpp  (Longpath)
// =================================================================
//
// Stimmt die dB-Spalte? Gegen ein Signal mit BEKANNTEM Stoerabstand.
//
// ── Warum es diese Pruefung gibt ────────────────────────────────────
//
// Betreiber am 2026-10-31, am Geraet: „bei der spalte sehe ich DB.
// aber nur posivitv, keine negativen DB, teilweise steht am ende dann
// auch ein minus DB" -- bei einer Zeile mit `19` in der Spalte und
// `-11` im Text derselben Zeile.
//
// Er hatte recht: dort stand `kand->score * 0.5f`, der Sync-Score von
// ft8_lib. Ein Kontrastmass, kein Stoerabstand, und nie negativ.
//
// Eine neue Rechnung darf nicht wieder „plausibel aussehen" -- sie
// muss gegen eine bekannte Groesse stimmen. Also wird hier ein Signal
// erzeugt, Rauschen mit gerechneter Leistung daraufgelegt und
// nachgesehen, was der Dekoder meldet.
//
// Der Bezug ist 2500 Hz wie bei WSJT-X. Das Rauschen ist weiss ueber
// die ganze Bandbreite (12000/2 = 6000 Hz), also traegt der
// 2500-Hz-Ausschnitt den Anteil 2500/6000.
//
// Modification history (Longpath):
//   2026-10-31 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QVector>
#include <cmath>
#include <random>

#include "core/Ft8Decoder.h"

using namespace Longpath;

namespace {

constexpr double kBezugBandbreiteHz = 2500.0;
constexpr double kHalbeAbtastrate   = Ft8Decoder::kSampleRate / 2.0;

/// Ein FT8-Signal mit genau diesem Stoerabstand (dB in 2500 Hz).
QVector<float> mitRauschen(const QVector<float>& rein, double zielDb,
                           uint32_t saat)
{
    double leistung = 0.0;
    for (float v : rein) { leistung += double(v) * double(v); }
    leistung /= double(rein.size());

    // Rauschleistung IM BEZUGSBAND, daraus die Leistung je Probe.
    const double imBand = leistung / std::pow(10.0, zielDb / 10.0);
    const double jeProbe = imBand * (kHalbeAbtastrate / kBezugBandbreiteHz);
    const double sigma  = std::sqrt(jeProbe);

    std::mt19937 wuerfel(saat);
    std::normal_distribution<double> glocke(0.0, sigma);

    QVector<float> aus = rein;
    for (float& v : aus) { v = float(double(v) + glocke(wuerfel)); }
    return aus;
}

/// Das Signal dorthin legen, wo es am Band WIRKLICH liegt: 500 ms
/// nach der Fenstergrenze, wie `Ft8Tx::sendeplan` es sendet.
///
/// ── Warum das noetig ist (2026-10-31 gemessen) ─────────────
///
/// `encodeBlock` MITTET das Signal im Fenster -- treu nach
/// `ft8_lib demo/gen_ft8.c`, das Dateien erzeugt und keine Sendung
/// plant. Bei FT8 sind das 1,18 s, bei FT4 1,23 s. Und ft8_lib sucht
/// den Zeitversatz nur ueber −10…+19 Symbole: bei FT8 sind das
/// 3,04 s (passt), bei FT4 nur 0,91 s (passt NICHT).
///
/// Gemessen, mit dem Signal an verschiedene Stellen gelegt:
///
///     FT4 ab 0,00 s -> gelesen      FT4 ab 1,00 s -> nichts
///     FT4 ab 0,50 s -> gelesen      FT4 ab 1,23 s -> nichts
///     FT4 ab 0,75 s -> gelesen      FT4 ab 1,50 s -> nichts
///
/// Darum konnte FT4 nie einen Rundlauf haben. **Am Band ist nichts
/// kaputt**: gesendet wird nicht aus der Mitte, `Ft8Tx::sendeplan`
/// ueberspringt die Stille und setzt den Ton auf 500 ms wie WSJT-X.
QVector<float> anDieRichtigeStelle(const QVector<float>& gemittet,
                                   Ft8Decoder::Protocol p)
{
    const bool ft4 = (p == Ft8Decoder::Protocol::Ft4);
    const double periode = ft4 ? 0.048 : 0.160;
    const int    symbole = ft4 ? 105 : 79;
    const int nutz = int(0.5 + symbole * periode * Ft8Decoder::kSampleRate);
    const int ganz = gemittet.size();
    const int mitte = (ganz - nutz) / 2;
    const int ziel  = int(0.5 * Ft8Decoder::kSampleRate);
    if (mitte < 0 || ziel + nutz > ganz) { return gemittet; }

    QVector<float> aus(ganz, 0.0f);
    for (int i = 0; i < nutz; ++i) { aus[ziel + i] = gemittet[mitte + i]; }
    return aus;
}

}  // namespace

class TstFt8Stoerabstand : public QObject
{
    Q_OBJECT

private slots:
    void dieSpalteStimmtGegenEinBekanntesSignal_data()
    {
        QTest::addColumn<double>("zielDb");
        QTest::newRow("-5 dB")  << -5.0;
        QTest::newRow("-10 dB") << -10.0;
        QTest::newRow("-15 dB") << -15.0;
        QTest::newRow("-18 dB") << -18.0;
        QTest::newRow("-19 dB") << -19.0;
        QTest::newRow("0 dB")   <<   0.0;
    }

    void dieSpalteStimmtGegenEinBekanntesSignal()
    {
        QFETCH(double, zielDb);

        QVector<float> rein;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("CQ OE5SOS JN67"),
                                        1500.0f,
                                        Ft8Decoder::Protocol::Ft8, &rein));

        const QVector<float> ton = mitRauschen(rein, zielDb, 12345u);
        const auto funde = Ft8Decoder::decodeBlock(ton.constData(),
                                                   ton.size(),
                                                   Ft8Decoder::Protocol::Ft8);
        QVERIFY2(!funde.isEmpty(),
                 qPrintable(QStringLiteral("bei %1 dB nichts dekodiert")
                                .arg(zielDb)));

        const Ft8Decode& d = funde.first();
        qInfo().noquote()
            << QStringLiteral("Ziel %1 dB -> gemeldet %2 dB")
                   .arg(zielDb, 0, 'f', 1).arg(double(d.snrDb), 0, 'f', 1);

        // Zwei dB Toleranz. Gemessen am 2026-10-31 lag der groesste
        // Fehler bei 0,4 dB (Tabelle im Kopf von `stoerabstandDb`);
        // die zwei dB lassen Luft fuer den Rauschwuerfel, sind aber
        // eng genug, dass eine Zahl mit der falschen Groessenordnung
        // — der Fehler, den dieser Lauf verhindern soll — nicht
        // durchkommt.
        //
        // Bei −20 und −21 dB dekodiert dieser Lauf gar nichts mehr
        // (mit dieser Saat). Das ist die Grenze des Dekoders, nicht
        // der Schaetzung, und eine eigene Frage: WSJT-X kommt dort
        // noch herunter.
        QVERIFY2(std::abs(double(d.snrDb) - zielDb) <= 2.0,
                 qPrintable(QStringLiteral("Ziel %1 dB, gemeldet %2 dB")
                                .arg(zielDb, 0, 'f', 1)
                                .arg(double(d.snrDb), 0, 'f', 1)));
    }

    // Und die Richtung muss stimmen: ein staerkeres Signal bekommt die
    // groessere Zahl. Ein Kontrastmass tut das auch -- aber zusammen
    // mit dem Fall oben ist beides belegt.
    void lauterIstGroesser()
    {
        QVector<float> rein;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("CQ OE5SOS JN67"),
                                        1500.0f,
                                        Ft8Decoder::Protocol::Ft8, &rein));

        const auto laut  = Ft8Decoder::decodeBlock(
            mitRauschen(rein, -5.0, 7u).constData(), rein.size(),
            Ft8Decoder::Protocol::Ft8);
        const auto leise = Ft8Decoder::decodeBlock(
            mitRauschen(rein, -18.0, 7u).constData(), rein.size(),
            Ft8Decoder::Protocol::Ft8);
        QVERIFY(!laut.isEmpty() && !leise.isEmpty());
        QVERIFY2(laut.first().snrDb > leise.first().snrDb + 5.0f,
                 qPrintable(QStringLiteral("laut %1, leise %2")
                                .arg(double(laut.first().snrDb))
                                .arg(double(leise.first().snrDb))));
    }

    // Und dasselbe fuer FT4 — mit EIGENEM Bezugswert. Die FT8-Zahl
    // umzurechnen waere geraten: FT4 hat die Symbolperiode 0,048 s
    // statt 0,160 s, also eine kuerzere FFT und breitere Bins.
    void ft4StimmtAuch_data()
    {
        QTest::addColumn<double>("zielDb");
        QTest::newRow("0 dB")   <<   0.0;
        QTest::newRow("-5 dB")  <<  -5.0;
        QTest::newRow("-10 dB") << -10.0;
        QTest::newRow("-13 dB") << -13.0;
    }

    void ft4StimmtAuch()
    {
        QFETCH(double, zielDb);

        QVector<float> rein;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("CQ OE5SOS JN67"),
                                        1500.0f,
                                        Ft8Decoder::Protocol::Ft4, &rein));
        const QVector<float> gelegt =
            anDieRichtigeStelle(rein, Ft8Decoder::Protocol::Ft4);

        const QVector<float> ton = mitRauschen(gelegt, zielDb, 2026u);
        const auto funde = Ft8Decoder::decodeBlock(ton.constData(),
                                                   ton.size(),
                                                   Ft8Decoder::Protocol::Ft4);
        QVERIFY2(!funde.isEmpty(),
                 qPrintable(QStringLiteral("FT4 bei %1 dB nichts dekodiert")
                                .arg(zielDb)));

        const Ft8Decode& d = funde.first();
        qInfo().noquote()
            << QStringLiteral("FT4 Ziel %1 dB -> gemeldet %2 dB")
                   .arg(zielDb, 0, 'f', 1).arg(double(d.snrDb), 0, 'f', 1);
        QVERIFY2(std::abs(double(d.snrDb) - zielDb) <= 2.0,
                 qPrintable(QStringLiteral("FT4 Ziel %1 dB, gemeldet %2 dB")
                                .arg(zielDb, 0, 'f', 1)
                                .arg(double(d.snrDb), 0, 'f', 1)));
    }

    // Ein schwaches Signal MUSS eine negative Zahl bekommen. Das war
    // der sichtbare Teil des Befunds: die alte Spalte konnte das gar
    // nicht.
    void einSchwachesSignalBekommtEinMinus()
    {
        QVector<float> rein;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("CQ OE5SOS JN67"),
                                        1500.0f,
                                        Ft8Decoder::Protocol::Ft8, &rein));
        const auto funde = Ft8Decoder::decodeBlock(
            mitRauschen(rein, -15.0, 99u).constData(), rein.size(),
            Ft8Decoder::Protocol::Ft8);
        QVERIFY(!funde.isEmpty());
        QVERIFY2(funde.first().snrDb < 0.0f,
                 qPrintable(QStringLiteral("bei −15 dB meldet er %1")
                                .arg(double(funde.first().snrDb))));
    }
};

QTEST_MAIN(TstFt8Stoerabstand)
#include "tst_ft8_stoerabstand.moc"
