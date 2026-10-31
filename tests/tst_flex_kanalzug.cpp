// =================================================================
// tests/tst_flex_kanalzug.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Der Befund vom 2026-10-31: der Sendeton-Kanalzug lief an genau einer
// Stelle, `TxWorkerThread.cpp:1131` — dem eigenen Sendeweg, auf dem
// Longpath selbst rechnet. Die Flex rechnet selbst und hat darum
// keinen TxWorkerThread; ihre Mikrofonpumpe holte den Ton direkt aus
// `AudioEngine::pullTxMic` und schickte ihn, wie der Kopf dort sagte,
// „roh an die Maschine".
//
// Damit war die ganze Kette ausser Betrieb, sobald am Flex gesendet
// wurde — Entrauschen, Tor, Entzerrer, De-Esser, Kompressor, Roehre,
// PUDU, Hall, Clipper, Begrenzer. Der Betreiber hatte sie in AetherSDR
// eingestellt, im Kanalzugfenster stehen seine Werte, und nichts davon
// ging in die Luft.
//
// Gegenprobe gegen die alte Fassung: `dieAlteFassungLiesseDenTonRoh`
// faehrt denselben Block mit `kette = nullptr` — also genau so, wie es
// vor dem 2026-10-31 war — und haelt fest, dass dabei NICHTS geschieht
// ausser Aussteuerung und Riegel. Faellt der neue Weg wieder heraus,
// wird dieser Fall gruen bleiben und der danebenstehende
// `dieKetteFasstDenTonAn` rot.
// =================================================================

#include <QtTest/QtTest>

#include "gui/MainWindow.h"
#include "core/strip/StripChain.h"
#include "core/strip/ClientComp.h"

#include <cmath>
#include <vector>

using Longpath::MainWindow;
using Longpath::StripChain;

namespace {

constexpr double kRate   = 48000.0;
// Was die Pumpe wirklich holt: `offen * 2` Eingangsrahmen bei 48 kHz,
// die danach auf 24 kHz halbiert werden. 480 sind 10 ms davon.
constexpr int    kRahmen = 480;
// −1,0 dBFS, derselbe Riegel wie in der Pumpe.
constexpr float  kGrenze = 0.891f;

// Etwas mit Struktur, damit eine Stufe, die ueberhaupt arbeitet, als
// Unterschied sichtbar wird. Dieselbe Form wie in tst_strip_chain.
std::vector<float> stimmartig(int rahmen, float amp = 0.3f)
{
    std::vector<float> v(static_cast<size_t>(rahmen), 0.0f);
    for (int i = 0; i < rahmen; ++i) {
        const double t = double(i) / kRate;
        v[static_cast<size_t>(i)] = amp * float(
            0.7 * std::sin(2.0 * M_PI * 220.0 * t)
          + 0.3 * std::sin(2.0 * M_PI * 1400.0 * t)
          + 0.1 * std::sin(2.0 * M_PI * 4200.0 * t));
    }
    return v;
}

bool gleich(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) { return false; }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) { return false; }
    }
    return true;
}

float spitze(const std::vector<float>& v)
{
    float p = 0.0f;
    for (float s : v) { p = std::max(p, std::fabs(s)); }
    return p;
}

// Eine Kette, die hoerbar etwas tut: Kompressor mit tiefer Schwelle
// und Makeup, alles andere aus. Ein Entzerrer mit flacher Kurve wuerde
// den Block unveraendert lassen und den Prueffall wertlos machen.
void stelleKompressorScharf(StripChain& k)
{
    k.prepare(kRate);
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        k.setStageEnabled(static_cast<StripChain::Stage>(i), false);
    }
    k.setStageEnabled(StripChain::Stage::Comp, true);
    k.comp().setThresholdDb(-30.0f);
    k.comp().setRatio(4.0f);
    k.comp().setMakeupDb(6.0f);
    k.setEnabled(true);
}

} // namespace

class TstFlexKanalzug : public QObject
{
    Q_OBJECT

private slots:

    // ── Die Gegenprobe: so war es vorher ─────────────────────────────
    //
    // Ohne Kette bleibt genau das uebrig, was die Pumpe vor dem
    // 2026-10-31 konnte: ein Faktor und ein Riegel.
    void dieAlteFassungLiesseDenTonRoh()
    {
        std::vector<float> ton = stimmartig(kRahmen);
        const std::vector<float> vorher = ton;

        const MainWindow::FlexMikBlock e = MainWindow::flexMikrofonblock(
            ton.data(), kRahmen, 1.0f, nullptr, kGrenze);

        QVERIFY(!e.ketteLief);
        QVERIFY2(gleich(ton, vorher),
                 "ohne Kette und mit 0 dB darf kein Wert sich aendern");
        QCOMPARE(e.quelleSpitze, e.ausgangSpitze);
    }

    // ── Und so ist es jetzt ──────────────────────────────────────────
    void dieKetteFasstDenTonAn()
    {
        StripChain kette;
        stelleKompressorScharf(kette);

        std::vector<float> ton = stimmartig(kRahmen);
        const std::vector<float> vorher = ton;

        const MainWindow::FlexMikBlock e = MainWindow::flexMikrofonblock(
            ton.data(), kRahmen, 1.0f, &kette, kGrenze);

        QVERIFY2(e.ketteLief, "die Kette war an und muss gelaufen sein");
        QVERIFY2(!gleich(ton, vorher),
                 "eine laufende Kette muss den Block veraendern — sonst "
                 "haengt sie wieder nicht im Sendeweg");
        for (float s : ton) { QVERIFY(std::isfinite(s)); }
    }

    // ── Der Hauptschalter gilt auch hier ─────────────────────────────
    //
    // Dieselbe Zusage wie im eigenen Sendeweg: aus ist bitgenau aus,
    // nicht „jede Stufe neutral".
    void ausgeschalteteKetteLaesstDenTonInRuhe()
    {
        StripChain kette;
        stelleKompressorScharf(kette);
        kette.setEnabled(false);

        std::vector<float> ton = stimmartig(kRahmen);
        const std::vector<float> vorher = ton;

        const MainWindow::FlexMikBlock e = MainWindow::flexMikrofonblock(
            ton.data(), kRahmen, 1.0f, &kette, kGrenze);

        QVERIFY(!e.ketteLief);
        QVERIFY2(gleich(ton, vorher),
                 "ausgeschaltet heiszt bitgenau unveraendert");
    }

    // ── Die Reihenfolge: Aussteuerung VOR der Kette ──────────────────
    //
    // Die Schwellen des Kompressors sind absolut. Laege die
    // Aussteuerung dahinter, arbeitete die Kette jedes Mal an einem
    // anderen Pegel und dieselbe Einstellung klaenge bei jedem
    // Reglerstand anders.
    void dieAussteuerungLiegtVorDerKette()
    {
        std::vector<float> ton = stimmartig(kRahmen, 0.1f);
        const float roh = spitze(ton);

        // +6 dB, ohne Kette: die Quellspitze muss das sehen.
        const float faktor = std::pow(10.0f, 6.0f / 20.0f);
        const MainWindow::FlexMikBlock e = MainWindow::flexMikrofonblock(
            ton.data(), kRahmen, faktor, nullptr, kGrenze);

        QVERIFY2(e.quelleSpitze > roh * 1.9f,
                 "die Quellspitze wird NACH der Aussteuerung gemessen");
    }

    // ── Der Riegel steht auch ohne Kette ─────────────────────────────
    void derRiegelGreiftImmer()
    {
        std::vector<float> ton(static_cast<size_t>(kRahmen), 0.99f);
        MainWindow::flexMikrofonblock(ton.data(), kRahmen, 1.0f,
                                      nullptr, kGrenze);
        QVERIFY2(spitze(ton) <= kGrenze + 1.0e-6f,
                 "nichts darf ueber den Riegel hinaus zur Maschine");
    }

    // ── Die Quellspitze meint die QUELLE ─────────────────────────────
    //
    // Nicht Haarspalterei: an dieser Zahl haengt die Warnung „das
    // Mikrofon steht zu hoch". Haenge sie hinter die Kette, dann
    // beschuldigt sie bei jedem kraeftigen Makeup das Mikrofon.
    void quellspitzeUndAusgangSpitzeSindZweiZahlen()
    {
        StripChain kette;
        stelleKompressorScharf(kette);

        std::vector<float> ton = stimmartig(kRahmen, 0.05f);
        const MainWindow::FlexMikBlock e = MainWindow::flexMikrofonblock(
            ton.data(), kRahmen, 1.0f, &kette, kGrenze);

        QVERIFY(e.ketteLief);
        QVERIFY2(e.quelleSpitze < 0.1f,
                 "die Quelle war leise und bleibt es in dieser Zahl");
        QVERIFY2(e.ausgangSpitze > e.quelleSpitze,
                 "der Kompressor hebt mit 6 dB Makeup an — das gehoert "
                 "in die ANDERE Zahl");
    }

    // ── Kein Block, kein Unfall ──────────────────────────────────────
    void leererBlockUndNullzeigerSindKeinAbsturz()
    {
        StripChain kette;
        stelleKompressorScharf(kette);

        MainWindow::FlexMikBlock e = MainWindow::flexMikrofonblock(
            nullptr, kRahmen, 1.0f, &kette, kGrenze);
        QVERIFY(!e.ketteLief);

        std::vector<float> ton = stimmartig(kRahmen);
        e = MainWindow::flexMikrofonblock(ton.data(), 0, 1.0f,
                                          &kette, kGrenze);
        QVERIFY(!e.ketteLief);
    }
};

QTEST_MAIN(TstFlexKanalzug)
#include "tst_flex_kanalzug.moc"
