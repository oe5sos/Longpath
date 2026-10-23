// =================================================================
// tests/tst_flex_wasserfall_kachel.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Die FLEX-8400 fuehrt ZWEI Bildstroeme, und bis zum 2026-10-23 zog
// Longpath nur den ersten: der Wasserfall wurde aus den FFT-Bins
// abgeleitet, mit eigenem Zeitgeber und eigenen Schwellen. Der
// Betreiber am 2026-10-22: „das rot ist eigenartig gewesen, auch die
// zeitliche laenge, bis der wasserfall anfaengt".
//
// Der Weg steht in docs/protocols/flex-wasserfall-aethersdr.md,
// nachgelesen bei AetherSDR @d58e2b8a. Dieser Stand prueft die drei
// Dinge, an denen es scheitern kann:
//
//   1. Die Kachelwerte sind KEINE dBm. Sie sind eine Intensitaet
//      (qint16/128, am Geraet +88 bis +120). In den dBm-Weg gegeben
//      stuenden sie sofort am oberen Anschlag — genau das Rot, das
//      behoben werden soll. (Erst gebaut, dann herausgenommen,
//      2026-10-22.)
//   2. Ein Traeger muss an SEINER Frequenz landen, auch wenn der
//      Ausschnitt der Kachel und der des Fensters nicht
//      uebereinstimmen (gemessen 230,6 kHz gegen 200 kHz).
//   3. Solange Kacheln kommen, darf der abgeleitete Weg nicht
//      mitschreiben — zwei Schreiber aus zwei Takten geben ein
//      Flackern.
// =================================================================

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"
#include "core/FlexVitaReader.h"

#include <algorithm>
#include <cmath>

using namespace Longpath;

namespace {

// Eine Kachel, wie die Maschine sie schickt: Rauschteppich um 96,
// ein Traeger bei 115 an bekannter Stelle. Zahlen aus AetherSDRs
// eigenem Kommentar („typical noise floor ~96-106, signal peaks
// ~110-115").
QVector<float> kachel(int bins, int traegerBin, float traeger = 115.0f)
{
    QVector<float> v(bins);
    for (int i = 0; i < bins; ++i) {
        v[i] = 96.0f + 1.5f * std::sin(0.37f * i);   // Teppich mit Textur
    }
    if (traegerBin >= 0 && traegerBin < bins) { v[traegerBin] = traeger; }
    return v;
}

int hoechsteStufe(const QVector<quint8>& stufen, int* wo)
{
    int best = -1, idx = -1;
    for (int i = 0; i < stufen.size(); ++i) {
        if (stufen[i] > best) { best = stufen[i]; idx = i; }
    }
    if (wo) { *wo = idx; }
    return best;
}

} // namespace

class TstFlexWasserfallKachel : public QObject {
    Q_OBJECT

private:
    // Ein Fenster, das wirklich ein Wasserfallbild hat: ohne Groesze
    // ist `m_waterfall` leer und jede Zeile kehrt sofort um.
    void richteEin(SpectrumWidget& sw, double mitteHz, double breiteHz)
    {
        sw.setKiwiDisplaySource(true);
        sw.setFrequencyRange(mitteHz, breiteHz);
        sw.resize(1200, 600);
        sw.show();
        QTest::qWait(300);
    }

private slots:

    // ── 1. Der Teppich darf NICHT am Anschlag stehen ─────────────────
    void derTeppichStehtNichtAmAnschlag()
    {
        SpectrumWidget sw;
        richteEin(sw, 14.100e6, 200.0e3);

        sw.zeigeFremdeWasserfallzeile(kachel(2048, -1), 14.000e6, 14.200e6);
        const auto stufen = sw.wfZeilenstufenForTest();
        QVERIFY(!stufen.isEmpty());

        int wo = -1;
        const int hoch = hoechsteStufe(stufen, &wo);
        double summe = 0.0;
        for (quint8 v : stufen) { summe += v; }
        const double mittel = summe / stufen.size();
        qInfo().nospace() << "nur Teppich: hoechste Stufe " << hoch
                          << ", Mittel " << mittel;

        // Waere die Intensitaet als dBm gelesen worden, stuende JEDER
        // Punkt auf 255 — das ist das einfarbige Rot.
        QVERIFY2(mittel < 200.0,
                 qPrintable(QStringLiteral("Der Rauschteppich steht im "
                     "Mittel auf %1 von 255 — das Bild ist einfarbig.")
                     .arg(mittel, 0, 'f', 1)));
    }

    // ── 2. Der Traeger landet an seiner Frequenz ─────────────────────
    void derTraegerLandetAnSeinerFrequenz()
    {
        SpectrumWidget sw;
        // Fenster 200 kHz um 14.100 — die Kachel ist BREITER (230,6
        // kHz), genau wie am Geraet gemessen.
        richteEin(sw, 14.100e6, 200.0e3);

        const int bins = 2048;
        const double kachelTief = 13.9846e6, kachelHoch = 14.2152e6;
        // Traeger auf 14.125 MHz setzen.
        const double zielHz = 14.125e6;
        const int traegerBin = static_cast<int>(
            (zielHz - kachelTief) / ((kachelHoch - kachelTief) / bins));

        sw.zeigeFremdeWasserfallzeile(kachel(bins, traegerBin),
                                      kachelTief, kachelHoch);
        const auto stufen = sw.wfZeilenstufenForTest();
        QVERIFY(!stufen.isEmpty());

        int wo = -1;
        const int hoch = hoechsteStufe(stufen, &wo);
        // Erwartete Stelle im Fenster.
        const double fensterTief = 14.100e6 - 100.0e3;
        const int erwartet = static_cast<int>(
            (zielHz - fensterTief) / (200.0e3 / stufen.size()));
        qInfo().nospace() << "Traeger bei 14.125 MHz: hoechste Stufe "
                          << hoch << " an Punkt " << wo
                          << ", erwartet " << erwartet;

        QVERIFY2(hoch > 0, "der Traeger ist gar nicht angekommen");
        QVERIFY2(std::abs(wo - erwartet) <= 3,
                 qPrintable(QStringLiteral("Der Traeger steht an Punkt %1 "
                     "statt %2 — das sind %3 Bildpunkte daneben.")
                     .arg(wo).arg(erwartet).arg(std::abs(wo - erwartet))));
    }

    // ── 3. Nur EIN Schreiber ─────────────────────────────────────────
    void dieKachelUebernimmtDasBild()
    {
        SpectrumWidget sw;
        richteEin(sw, 14.100e6, 200.0e3);
        QVERIFY(!sw.fremdeWasserfallquelleForTest());

        sw.zeigeFremdeWasserfallzeile(kachel(2048, 500), 14.000e6, 14.200e6);
        QVERIFY2(sw.fremdeWasserfallquelleForTest(),
                 "nach einer Kachel muss der abgeleitete Weg schweigen");
    }

    // ── Die Umrechnung selbst, als Beleg fuer die Einheit ────────────
    void kachelwerteSindIntensitaetKeineDbm()
    {
        // Echte Werte vom Geraet liegen bei ~11300 -> ~88.
        QCOMPARE(FlexVita::tileColourValue(11438), 11438.0f / 128.0f);
        QVERIFY(FlexVita::tileColourValue(11438) > 85.0f);
        QVERIFY(FlexVita::tileColourValue(11438) < 95.0f);
        // POSITIV. Waeren es dBm, muessten sie negativ sein — und in
        // Longpaths dBm-Wasserfall stuende alles am oberen Anschlag.
        QVERIFY(FlexVita::tileColourValue(11438) > 0.0f);
    }
};

QTEST_MAIN(TstFlexWasserfallKachel)
#include "tst_flex_wasserfall_kachel.moc"
