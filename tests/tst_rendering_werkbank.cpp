// no-port-check: Longpath-original test tool, no Thetis logic.
//
// ── Rendering-Werkbank (2026-09-25) ─────────────────────────────────────
//
// Betreiber: "setze dich intensiv mit rendering auseinander, was wir
// besser, klarer, designen und gestalten koennen." Um Varianten
// vergleichen zu koennen, braucht es eine Szene, die sich nicht aendert:
// dieselben Signale, dasselbe Rauschen, derselbe Ablauf -- und den
// ECHTEN Panadapter (SpectrumWidget, GPU-Pfad), nicht einen Nachbau.
//
// Die Szene (48 kHz Spanne wie die QRP, 20 m):
//   - Rauschboden -135 dBm je Bin, exponentialverteilt (Rauschleistung)
//   - S9-Traeger (-73 dBm), dauernd
//   - schwaches CW (-118 dBm), getastet
//   - SSB-Sprache (2,4 kHz, Formanten, Silbenhuellkurve, -95 dBm)
//   - FT8-Gruppe: sechs 50-Hz-Toene, 15-s-Takt (hier verkuerzt), -108..-116
//
// Ohne LONGPATH_GRAB_DIR ueberspringt sich der Test (CI). Mit ihm
// entsteht je Variante ein PNG. GPU-Flaeche -> nicht offscreen laufen
// lassen, das Fenster erscheint kurz.
//
//   LONGPATH_GRAB_DIR=/pfad ./build/tests/tst_rendering_werkbank
//
// Varianten waehlt LONGPATH_RENDER_VARIANTS (Komma-Liste), sonst alle.

#include <QtTest/QtTest>
#include <QApplication>
#include <QGuiApplication>
#include <QDir>
#include <QImage>
#include <QMouseEvent>

#include <array>
#include <cmath>
#include <random>

#include "core/AppSettings.h"
#include "core/TransverterSetup.h"
#include "gui/SpectrumWidget.h"
#include "models/BandPlanManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include "FensterFrist.h"

using namespace Longpath;

namespace {

constexpr int    kFftSize   = 4096;
constexpr double kSampleHz  = 48000.0;
constexpr double kCenterHz  = 14100000.0;
constexpr double kHzPerBin  = kSampleHz / kFftSize;

double db2p(double db) { return std::pow(10.0, db / 10.0); }

int binFor(double hz) { return static_cast<int>(std::lround((hz - kCenterHz) / kHzPerBin)) + kFftSize / 2; }

// Eine Szene, Bild fuer Bild erzeugt -- deterministisch (fester Samen).
class Scene {
public:
    explicit Scene(quint32 seed) : m_rng(seed) {}

    QVector<float> frame(int n)
    {
        QVector<float> bins(kFftSize);
        std::exponential_distribution<double> expo(1.0);
        const double floorP = db2p(-135.0);
        for (int i = 0; i < kFftSize; ++i) {
            bins[i] = static_cast<float>(floorP * expo(m_rng));
        }
        // S9-Traeger bei 14,0985 MHz, mit Fensterkeule (3 Bins)
        addTone(bins, 14098500.0, -73.0);
        // Schwaches CW bei 14,0915, getastet (Morse-artig)
        const bool keyed = ((n / 4) % 7) < 4 && ((n / 28) % 3) != 2;
        if (keyed) { addTone(bins, 14091500.0, -118.0); }
        // SSB bei 14,1100-14,1124
        addSsb(bins, 14110000.0, -95.0, n);
        // FT8-Gruppe um 14,1180 (Takt 180 Bilder an, 60 aus)
        if ((n % 240) < 180) {
            const double base = 14117600.0;
            const double lv[6] = {-108, -112, -110, -116, -109, -114};
            for (int k = 0; k < 6; ++k) {
                const double f = base + k * 310.0 + 6.25 * ((n + 13 * k) % 8);
                addTone(bins, f, lv[k]);
            }
        }
        return bins;
    }

private:
    void addTone(QVector<float>& bins, double hz, double dbm)
    {
        // Hauptkeule eines Blackman-Harris-Fensters grob: Mitte 0 dB,
        // Nachbarn -6 dB, dann -20 dB.
        const double exact = (hz - kCenterHz) / kHzPerBin + kFftSize / 2;
        const int c = static_cast<int>(std::lround(exact));
        const double p = db2p(dbm);
        const double w[5] = {db2p(-20), db2p(-6), 1.0, db2p(-6), db2p(-20)};
        for (int k = -2; k <= 2; ++k) {
            const int b = c + k;
            if (b >= 0 && b < kFftSize) { bins[b] += static_cast<float>(p * w[k + 2]); }
        }
    }

    void addSsb(QVector<float>& bins, double lowHz, double dbm, int n)
    {
        // Silbenhuellkurve: an/aus in 3..9 Bildern, Formanten 500/1500 Hz
        const double env = (std::sin(n * 0.37) > -0.2 ? 1.0 : 0.05)
                         * (0.6 + 0.4 * std::sin(n * 1.7));
        std::exponential_distribution<double> expo(1.0);
        const int b0 = binFor(lowHz + 250.0), b1 = binFor(lowHz + 2650.0);
        for (int b = b0; b < b1; ++b) {
            const double f = (b - b0) * kHzPerBin;
            const double shape = 0.3 + std::exp(-std::pow((f - 500.0) / 300.0, 2))
                               + 0.6 * std::exp(-std::pow((f - 1500.0) / 400.0, 2));
            bins[b] += static_cast<float>(db2p(dbm) * env * shape * expo(m_rng));
        }
    }

    std::mt19937 m_rng;
};

// ── 2-m-Szene fuer das IFo2-Bandbild (2026-11-08) ─────────────────────
//
// Betreiber: das SDR an IFo2 des MV-2 als breites Bandbild neben dem
// TS-590. Die Szene liegt auf der ANTENNE (144 MHz); das Widget sieht,
// wie im Betrieb, die ZF (28 MHz) und rechnet ueber den Transverter um.
// Belegung erfunden, aber nach dem IARU-R1-Plan: CW unten, SSB-Gedraenge
// um 144,300, FT8 bei 144,174 (Gewohnheit, nicht im Plan), Baken ab
// 144,400, FM-Anruf 145,500, Relais-Ausgaenge ab 145,600.
class ZweiMeterSzene {
public:
    ZweiMeterSzene(double mitteRfHz, double rateHz, int bins, quint32 seed,
                   bool gedraenge = false)
        : m_mitte(mitteRfHz), m_rate(rateHz), m_bins(bins), m_rng(seed),
          m_gedraenge(gedraenge)
    {
        // Contest-Gedraenge (Entwurf Lueckenfinder, 2026-11-08): SSB alle
        // 3-6 kHz, unregelmaessig, mit drei echten Luecken -- so sieht ein
        // 2-m-Contestsamstag aus, nicht wie ein Raster alle 15 kHz.
        if (m_gedraenge) {
            std::mt19937 r(4711);
            std::uniform_real_distribution<double> schritt(2900.0, 6000.0);
            std::uniform_real_distribution<double> pegel(-118.0, -88.0);
            const double frei[][2] = {{144212000, 144221000}, {144262000, 144268500},
                                      {144334000, 144346000}};
            for (double f = 144162000.0; f < 144396000.0; f += schritt(r)) {
                bool inLuecke = false;
                for (const auto& l : frei) { if (f + 2700.0 > l[0] && f < l[1]) { inLuecke = true; } }
                if (inLuecke) { continue; }
                m_stationen.push_back({f, pegel(r)});
            }
        }
    }

    QVector<float> frame(int n)
    {
        QVector<float> b(m_bins);
        std::exponential_distribution<double> expo(1.0);
        const double boden = db2p(-135.0);
        for (int i = 0; i < m_bins; ++i) { b[i] = static_cast<float>(boden * expo(m_rng)); }
        // LONGPATH_2M_RASTER=1: nur Pruefton alle 250 kHz (-90 dBm), um die
        // Abbildung Frequenz -> Bildpunkt ueber die ganze Breite zu messen.
        if (qEnvironmentVariableIntValue("LONGPATH_2M_RASTER") == 1) {
            for (double f = 144000000.0; f <= 148000000.0; f += 250000.0) { ton(b, f, -90.0); }
            return b;
        }

        // CW
        ton(b, 144050000.0, -96.0);
        if (((n / 5) % 6) < 4) { ton(b, 144062500.0, -117.0); }
        if (((n / 3) % 5) < 3) { ton(b, 144083000.0, -112.0); }
        // FT8 um 144,174 (Takt wie oben)
        if ((n % 240) < 180) {
            const double lv[5] = {-112, -118, -109, -121, -115};
            for (int k = 0; k < 5; ++k) { ton(b, 144174300.0 + k * 420.0, lv[k]); }
        }
        // SSB-Gedraenge
        const double ssb[][2] = {
            {144195000, -108}, {144210000, -96}, {144226000, -113}, {144241000, -101},
            {144256000, -110}, {144270000, -90}, {144285000, -104}, {144300000, -82},
            {144318000, -99},  {144333000, -111}, {144349000, -94}, {144366000, -107},
            {144381000, -115},
        };
        if (m_gedraenge) {
            int s = 0;
            for (const auto& st : m_stationen) {
                if (std::sin(n * (0.17 + 0.029 * s) + s) > -0.6) { sprache(b, st[0], st[1], n, s); }
                ++s;
            }
            for (const auto& bk : {144408000.0, 144418000.0, 144437000.0, 144462000.0}) { ton(b, bk, -114.0); }
            ton(b, 144050000.0, -100.0);
            return b;
        }
        int k = 0;
        for (const auto& st : ssb) {
            const bool spricht = std::sin(n * (0.21 + 0.037 * k) + k) > -0.3;
            if (spricht) { sprache(b, st[0], st[1], n, k); }
            ++k;
        }
        // Baken
        const double bake[][2] = {
            {144408000, -118}, {144418000, -106}, {144425000, -121}, {144437000, -112},
            {144451000, -116}, {144462000, -109}, {144475000, -120},
        };
        for (const auto& bk : bake) { ton(b, bk[0], bk[1]); }
        // FM: Anruf und Relaisausgaenge (12,5-kHz-Raster), 12 kHz breit
        if ((n / 40) % 3 != 2) { band(b, 145500000.0 - 6000.0, 12000.0, -88.0); }
        const double rpt[] = {145600000.0, 145625000.0, 145687500.0, 145737500.0, 145775000.0};
        int r = 0;
        for (double f : rpt) {
            if (((n / 30) + r) % 4 != 0) { band(b, f - 6000.0, 12000.0, -80.0 - 6.0 * r); }
            ++r;
        }
        // APRS-Pakete
        if ((n % 90) < 8) { band(b, 144800000.0 - 6000.0, 12000.0, -92.0); }
        return b;
    }

private:
    int binFuer(double hz) const
    {
        return static_cast<int>(std::lround((hz - m_mitte) / (m_rate / m_bins))) + m_bins / 2;
    }
    void ton(QVector<float>& b, double hz, double dbm)
    {
        const int c = binFuer(hz);
        const double w[5] = {db2p(-20), db2p(-6), 1.0, db2p(-6), db2p(-20)};
        for (int i = -2; i <= 2; ++i) {
            const int x = c + i;
            if (x >= 0 && x < m_bins) { b[x] += static_cast<float>(db2p(dbm) * w[i + 2]); }
        }
    }
    // SSB wie die 20-m-Szene oben: Formanten 500/1500 Hz, 300-2700 Hz,
    // Silbenhuellkurve -- ein flacher Block sieht in der Lupe nach nichts aus.
    void sprache(QVector<float>& b, double traeger, double dbm, int n, int k)
    {
        std::exponential_distribution<double> expo(1.0);
        const double env = 0.55 + 0.45 * std::sin(n * (0.9 + 0.13 * k) + 2.0 * k);
        const int b0 = binFuer(traeger + 300.0), b1 = std::max(b0 + 1, binFuer(traeger + 2700.0));
        const double hzJeBin = m_rate / m_bins;
        for (int x = b0; x < b1; ++x) {
            if (x < 0 || x >= m_bins) { continue; }
            const double f = (x - b0) * hzJeBin;
            // Sprache faellt zu den Hoehen hin ab (etwa -12 dB von 300 auf
            // 2500 Hz), mit einem zweiten Formanten um 1400 Hz.
            const double fa = f + 300.0;
            const double form = (1.0 / (1.0 + (fa / 700.0) * (fa / 700.0)))
                              * (1.0 + 0.8 * std::exp(-std::pow((fa - 1400.0) / 300.0, 2)))
                              + 0.02;
            b[x] += static_cast<float>(db2p(dbm) * env * form * expo(m_rng));
        }
    }
    void band(QVector<float>& b, double unten, double breite, double dbm)
    {
        std::exponential_distribution<double> expo(1.0);
        const int b0 = binFuer(unten), b1 = std::max(b0 + 1, binFuer(unten + breite));
        for (int x = b0; x < b1; ++x) {
            if (x >= 0 && x < m_bins) {
                b[x] += static_cast<float>(db2p(dbm) * (0.4 + expo(m_rng)));
            }
        }
    }
    double m_mitte, m_rate;
    int m_bins;
    std::mt19937 m_rng;
    bool m_gedraenge{false};
    QVector<std::array<double, 2>> m_stationen;
};

// Die Anzeige-Einstellungen eines echten Longpath.settings uebernehmen
// (LONGPATH_RENDER_SETTINGS=<Datei>): alle Schluessel, die mit "Display"
// oder "Clarity" beginnen. So sehen die Bilder aus wie beim Betreiber und
// nicht wie der leere Pruefsandkasten. Liest nur, schreibt die Datei nie.
int importDisplaySettings(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { return 0; }
    QXmlStreamReader x(&f);
    int n = 0;
    int depth = 0;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement()) {
            ++depth;
            const QString key = x.name().toString();
            if (depth == 2 && (key.startsWith(QLatin1String("Display"))
                               || key.startsWith(QLatin1String("Clarity")))) {
                const QString val = x.readElementText();
                --depth;
                AppSettings::instance().setValue(key, val);
                ++n;
            }
        } else if (x.isEndElement()) {
            --depth;
        }
    }
    return n;
}

struct Variant {
    QString id;
    std::function<void(SpectrumWidget&)> apply;
};

QImage grabSpectrum(SpectrumWidget& w)
{
#ifdef LONGPATH_GPU_SPECTRUM
    return w.grabFramebuffer();
#else
    return w.grab().toImage();
#endif
}

} // namespace

class TestRenderingWerkbank : public QObject {
    Q_OBJECT
private slots:
    void render()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        // Der Linux-CI setzt LONGPATH_GRAB_DIR fuer seine Haenger-Diagnose
        // global (ci.yml) und laeuft offscreen -- ohne QRhi gibt es kein
        // Bild, der Pruefstand hat dort nichts zu tun (2026-09-26).
        if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
            QSKIP("Offscreen-Plattform: kein GPU-Framebuffer.");
        }
        QDir().mkpath(dir);

        const QString settingsFile = qEnvironmentVariable("LONGPATH_RENDER_SETTINGS");
        if (!settingsFile.isEmpty()) {
            qInfo() << "Anzeige-Einstellungen uebernommen:"
                    << importDisplaySettings(settingsFile);
        }

        const QStringList wanted = qEnvironmentVariable("LONGPATH_RENDER_VARIANTS")
                                       .split(QLatin1Char(','), Qt::SkipEmptyParts);

        // Martins Stand (Longpath.settings 2026-09-25): 3D, Pan Average,
        // WF Sample, beide LogRecursive, Clarity an.
        auto martin = [](SpectrumWidget& w) {
            w.setSpectrumRenderMode(SpectrumRenderMode::Mode3D);
            w.setSpectrumDetector(SpectrumDetector::Average);
            w.setWaterfallDetector(SpectrumDetector::Sample);
            w.setSpectrumAveraging(SpectrumAveraging::LogRecursive);
            w.setWaterfallAveraging(SpectrumAveraging::LogRecursive);
        };
        const QList<Variant> variants = {
            {QStringLiteral("a_martin_3d_avg_sample"), martin},
            {QStringLiteral("b_2d_avg_sample"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
             }},
            {QStringLiteral("c_2d_peak_peak_thetis"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
                 w.setSpectrumDetector(SpectrumDetector::Peak);
                 w.setWaterfallDetector(SpectrumDetector::Peak);
             }},
            {QStringLiteral("e_2d_avg_wfpeak"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
                 w.setWaterfallDetector(SpectrumDetector::Peak);
             }},
            {QStringLiteral("f_2d_avg_wfaverage"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
                 w.setWaterfallDetector(SpectrumDetector::Average);
             }},
            {QStringLiteral("g_echo_probe"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
                 w.setWaterfallDetector(SpectrumDetector::Peak);
             }},
            {QStringLiteral("d_3d_peak_peak"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumDetector(SpectrumDetector::Peak);
                 w.setWaterfallDetector(SpectrumDetector::Peak);
             }},
            // "Kurven mit Hof" (Glas & Tiefe): Martins 2D-Stand in drei
            // Fassungen -- heute (beide Schalter aus, also der
            // Werkszustand) / weiche Kante / weiche Kante + Hof. Die
            // drei Bilder sind die Vorlage, an der er entscheidet.
            {QStringLiteral("k_kurve_heute"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
             }},
            {QStringLiteral("l_kurve_weich"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
                 w.setTraceSoftEdge(true);
             }},
            {QStringLiteral("m_kurve_weich_hof"), [martin](SpectrumWidget& w) {
                 martin(w);
                 w.setSpectrumRenderMode(SpectrumRenderMode::Mode2D);
                 w.setTraceSoftEdge(true);
                 w.setTraceHalo(true);
             }},
            // ── Schaerfe-Entwuerfe (2026-11-08) ─────────────────────────
            // Betreiber: „was kannst du graphisch beim Panadapter noch
            // verbessern, auch in Hinsicht auf schaerfere Grafik". Grund
            // ist DIESMAL sein ganzer Anzeige-Stand: loadSettings() liest
            // die mit LONGPATH_RENDER_SETTINGS uebernommenen Schluessel
            // (die Varianten oben setzten nur einzelne Werte von Hand,
            // loadSettings lief dort nie). Jede Variante aendert genau
            // EINE Sache gegen s0, s6 nimmt alle zusammen.
            {QStringLiteral("s0_heute"), [](SpectrumWidget& w) {
                 w.loadSettings();
             }},
            {QStringLiteral("s1_wasserfall_voll"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setWaterfallDevicePixels(true);
             }},
            {QStringLiteral("s2_wf_detektor_peak"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setWaterfallDetector(SpectrumDetector::Peak);
             }},
            {QStringLiteral("s3_haarlinien"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setGridHairline(true);
             }},
            {QStringLiteral("s4_kante_kurz"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setTraceEdgePx(0.5f);
             }},
            {QStringLiteral("s5_verlauf"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setGradientEnabled(true);
             }},
            {QStringLiteral("s6_alles"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setWaterfallDevicePixels(true);
                 w.setWaterfallDetector(SpectrumDetector::Peak);
                 w.setGridHairline(true);
                 w.setTraceEdgePx(0.5f);
                 w.setGradientEnabled(true);
             }},
            {QStringLiteral("s7_wasserfall_voll_peak"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setWaterfallDevicePixels(true);
                 w.setWaterfallDetector(SpectrumDetector::Peak);
             }},
            // Durchlass und Bandbreite (2026-11-08), vom echten Bau: d1 in
            // Ruhe, d2 beim Ziehen der oberen Kante (echte Mausereignisse,
            // siehe unten nach der Einspielung).
            {QStringLiteral("d1_bandbreite_20m"), [](SpectrumWidget& w) {
                 w.loadSettings();
             }},
            {QStringLiteral("d2_kante_ziehen"), [](SpectrumWidget& w) {
                 w.loadSettings();
             }},
            // Seit der Wahl („Alles zusammen") ist s0 der NEUE Stand ab
            // Werk. s8 zeigt zum Vergleich den alten: alle drei aus.
            {QStringLiteral("s8_vorher"), [](SpectrumWidget& w) {
                 w.loadSettings();
                 w.setWaterfallDevicePixels(false);
                 w.setGridHairline(false);
                 w.setTraceEdgePx(1.0f);
             }},
        };

        for (const Variant& v : variants) {
            if (!wanted.isEmpty() && !wanted.contains(v.id)) { continue; }
            SpectrumWidget w;
            w.resize(1170, 620);
            w.setSampleRate(kSampleHz);
            w.setDdcCenterFrequency(kCenterHz);
            w.setFrequencyRange(kCenterHz, kSampleHz);
            w.setVfoFrequency(14111500.0);
            w.setFilterOffset(150, 2850);
            v.apply(w);
            // loadSettings (s_*) liest auch die gespeicherte Zoombreite
            // (DisplayBandwidth) -- die Szene ist 48 kHz breit, also hier
            // noch einmal setzen, fuer alle Varianten gleich.
            w.setFrequencyRange(kCenterHz, kSampleHz);
            // Nach apply: loadSettings (s_*) bringt den Bereich des
            // Betreibers fuer SEIN Band mit; die Szene liegt aber bei
            // -135 dBm Rauschen. Bereich und Clarity-Fenster gehoeren zur
            // Szene, nicht zum Entwurf -- fuer alle Varianten gleich.
            w.setDbmRange(-150.0f, -60.0f);
            // Clarity wie im Betrieb: Fenster Boden-5 .. Boden+55
            w.setClarityActive(true);
            w.setClarityWaterfallThresholds(-140.0f, -80.0f, -135.0f);
            // LONGPATH_RENDER_HIDDEN=1: waehrend der Einspielung versteckt.
            // Bei schlafendem Bildschirm drosselt macOS ein sichtbares
            // GPU-Fenster auf wenige Bilder/s, der GUI-Thread steht bis
            // 1 s, und der Wasserfall-Takt holt verpasste Zeilen mit
            // Kopien nach -- Bloecke statt Zeilen. Versteckt rechnet das
            // Widget weiter, gezeichnet wird erst am Ende.
            // =2: erst zeigen (Textur anlegen), dann verstecken -- wie ein
            // schwebendes Panadapter-Fenster, das sich versteckt, solange
            // Longpath nicht die aktive App ist (hidesOnDeactivate).
            // =3: nie zeigen. QRhiWidget::grabFramebuffer() zeichnet auch
            // ohne Fenster auf dem Schirm (Qt-Doku) -- dann erscheint auf
            // dem Bildschirm des Betreibers gar nichts (2026-11-08).
            const int hiddenMode = qEnvironmentVariableIntValue("LONGPATH_RENDER_HIDDEN");
            const bool hidden = hiddenMode != 0;
            const bool neverShow = hiddenMode == 3;
            if (!hidden || hiddenMode == 2) {
                w.show();
                QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
            }
            if (hiddenMode == 2) {
                QTest::qWait(300);
                w.hide();
            }

            Scene scene(20260925);
            // ~12 s Verlauf bei 30 Bildern/s, damit der Wasserfall voll ist
            const bool echoProbe = v.id.startsWith(QLatin1String("g_echo"));
            // LONGPATH_RENDER_FRAMES: mehr Bilder fuellen den Wasserfall
            // auch bei langsamem Zeilentakt (Betreiber: 101 ms je Zeile).
            const int frames = qMax(1, qEnvironmentVariableIntValue("LONGPATH_RENDER_FRAMES") > 0
                                           ? qEnvironmentVariableIntValue("LONGPATH_RENDER_FRAMES")
                                           : 360);
            for (int n = 0; n < frames; ++n) {
                QVector<float> f = scene.frame(n);
                // Echo-Probe: ein starker Traeger nur in den letzten Bildern.
                // Oben muss er stehen, an der Unterkante darf er nicht
                // auftauchen (Ringumbruch, waterfall.frag).
                if (echoProbe && n >= 350) {
                    const int b = binFor(14105300.0);
                    for (int k = -1; k <= 1; ++k) { f[b + k] += static_cast<float>(db2p(-60.0)); }
                }
                w.updateSpectrumLinear(0, f, 1.0, 0.0);
                QTest::qWait(33);
            }
            if (v.id.startsWith(QLatin1String("d2_"))) {
                // Die obere Kante greifen und 7 Punkte nach rechts ziehen --
                // nicht loslassen, damit der Wert an der Kante im Bild steht.
                const QRect r = w.spectrumRect();
                const double links = w.centerFrequency() - w.bandwidth() / 2.0;
                const double x = r.left() + (14111500.0 + 2850.0 - links) / w.bandwidth() * r.width();
                const QPointF a(x, r.top() + 0.5 * r.height());
                const QPointF b(x + 7.0, a.y());
                QMouseEvent druck(QEvent::MouseButtonPress, a, w.mapToGlobal(a),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(&w, &druck);
                QMouseEvent zug(QEvent::MouseMove, b, w.mapToGlobal(b),
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(&w, &zug);
            }
            if (hidden && !neverShow) {
                w.show();
                QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
                QTest::qWait(300);
            }
            const QImage img = grabSpectrum(w);
            if (img.isNull()) {
                QSKIP("Kein QRhi/GPU-Backend: grabFramebuffer() liefert kein Bild.");
            }
            const QString path = dir + QLatin1Char('/') + v.id + QStringLiteral(".png");
            QVERIFY(img.save(path));
            qInfo().noquote() << "geschrieben:" << path << img.size();
        }
    }

    // Das IFo2-Bandbild (2026-11-08): z1 = Contest-Ausschnitt 144,0-144,5,
    // z2 = das ganze Fenster des MV-2, 144-148. Senden duerfte das Geraet
    // dort ohnehin nicht (Region 1 endet bei 146) -- es geht nur ums Bild.
    void zweiMeter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
            QSKIP("Offscreen-Plattform: kein GPU-Framebuffer.");
        }
        QDir().mkpath(dir);
        const QString settingsFile = qEnvironmentVariable("LONGPATH_RENDER_SETTINGS");
        if (!settingsFile.isEmpty()) { importDisplaySettings(settingsFile); }
        const QStringList wanted = qEnvironmentVariable("LONGPATH_RENDER_VARIANTS")
                                       .split(QLatin1Char(','), Qt::SkipEmptyParts);

        TransverterSetup tv;
        tv.enabled  = true;
        tv.ifLowHz  = 28000000;
        tv.ifHighHz = 32000000;
        tv.offsetHz = 116000000;
        TransverterSetup::setCurrent(tv);

        BandPlanManager plaene;
        plaene.loadPlans();
        plaene.setActivePlan(QStringLiteral("IARU Region 1"));

        struct Fall { QString id; double mitteRf; double rate; int bins; QSize groesse;
                      bool gedraenge = false; };
        const QList<Fall> faelle = {
            {QStringLiteral("z1_contest_144_0_bis_144_5"), 144250000.0, 500000.0, 16384, {1170, 620}},
            {QStringLiteral("z2_voll_144_bis_148"),        146000000.0, 4000000.0, 65536, {1170, 620}},
            // Grund fuer den Lupen-Entwurf (2026-11-08): +/-10 kHz um den
            // VFO, fein aufgeloest, als schmaler Streifen.
            {QStringLiteral("z3_lupe_144300_20k"),         144300000.0, 20000.0, 4096, {560, 300}},
            {QStringLiteral("z4_contest_gedraenge"),       144250000.0, 500000.0, 16384, {1170, 620}, true},
        };
        for (const Fall& fall : faelle) {
            if (!wanted.isEmpty() && !wanted.contains(fall.id)) { continue; }
            SpectrumWidget w;
            w.resize(fall.groesse);
            w.loadSettings();
            w.setBandPlanManager(&plaene);
            w.setBandPlanFontSize(qMax(6, AppSettings::instance()
                                              .value(QStringLiteral("BandPlanFontSize"), 6).toInt()));
            const double mitteZf = fall.mitteRf - tv.offsetHz;
            // VFO ZUERST: ohne CTUN scrollt setVfoFrequency das Bild nach,
            // sobald der VFO in den aeusseren 10 % steht (Thetis
            // console.cs:31371). Bei 144-148 tut 144,300 das -- am Geraet
            // stimmt die Scheibe dann nach, hier gibt es keine, und das Bild
            // ragte 100 kHz ueber die gelieferten Bins hinaus (gestreckt um
            // 2,6 %, gemessen 2026-11-08). Erst danach das Fenster setzen.
            w.setVfoFrequency(144300000.0 - tv.offsetHz);
            w.setSampleRate(fall.rate);
            w.setDdcCenterFrequency(mitteZf);
            w.setFrequencyRange(mitteZf, fall.rate);
            w.setFilterOffset(150, 2850);
            const bool lupe = fall.id.startsWith(QLatin1String("z3_"));
            w.setCompanionMarker(!lupe, 144300000 - tv.offsetHz, QStringLiteral("TS-590"));
            if (lupe) { w.setShowPeakValueOverlay(false); }
            // LONGPATH_S_STUFEN=1: S-Stufen statt dBm an der Skala (2026-11-08).
            if (qEnvironmentVariableIntValue("LONGPATH_S_STUFEN") == 1) {
                w.setSUnitsScaleShown(true);
            }
            w.setDbmRange(-150.0f, -60.0f);
            w.setClarityActive(true);
            w.setClarityWaterfallThresholds(-140.0f, -80.0f, -135.0f);

            ZweiMeterSzene szene(fall.mitteRf, fall.rate, fall.bins, 20261108, fall.gedraenge);
            const int frames = qMax(1, qEnvironmentVariableIntValue("LONGPATH_RENDER_FRAMES") > 0
                                           ? qEnvironmentVariableIntValue("LONGPATH_RENDER_FRAMES")
                                           : 360);
            for (int n = 0; n < frames; ++n) {
                w.updateSpectrumLinear(0, szene.frame(n), 1.0, 0.0);
                QTest::qWait(33);
            }
            qInfo().noquote() << fall.id << "Mitte" << w.centerFrequency() << "Breite" << w.bandwidth()
                              << "Rate" << w.sampleRate() << "DDC" << w.ddcCenterFrequency();
            const QImage img = grabSpectrum(w);
            if (img.isNull()) {
                QSKIP("Kein QRhi/GPU-Backend: grabFramebuffer() liefert kein Bild.");
            }
            const QString path = dir + QLatin1Char('/') + fall.id + QStringLiteral(".png");
            QVERIFY(img.save(path));
            qInfo().noquote() << "geschrieben:" << path << img.size();
            // Fuer Entwurfsblaetter: was die Kurve WIRKLICH zeigt (dBm je
            // Anzeigepunkt) und die Geometrie -- damit ein Entwurf nach den
            // angezeigten Werten zeichnet, nicht nach Augenmass.
            {
                QJsonObject o;
                QJsonArray px;
                for (float v : w.renderedPixels()) { px.append(std::round(v * 10.0f) / 10.0); }
                const QRect r = w.spectrumRect();
                o[QStringLiteral("pixels")]     = px;
                o[QStringLiteral("specRect")]   = QJsonArray{r.x(), r.y(), r.width(), r.height()};
                o[QStringLiteral("dpr")]        = img.width() / static_cast<double>(w.width());
                o[QStringLiteral("centerHz")]   = w.centerFrequency();
                o[QStringLiteral("bandwidthHz")] = w.bandwidth();
                o[QStringLiteral("offsetHz")]   = static_cast<double>(tv.offsetHz);
                o[QStringLiteral("vfoRfHz")]    = 144300000.0;
                o[QStringLiteral("filterLoHz")] = 150;
                o[QStringLiteral("filterHiHz")] = 2850;
                QFile jf(dir + QLatin1Char('/') + fall.id + QStringLiteral(".json"));
                QVERIFY(jf.open(QIODevice::WriteOnly));
                jf.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
            }
            w.setBandPlanManager(nullptr);
        }
        TransverterSetup::setCurrent(TransverterSetup{});
    }
};

QTEST_MAIN(TestRenderingWerkbank)
#include "tst_rendering_werkbank.moc"
