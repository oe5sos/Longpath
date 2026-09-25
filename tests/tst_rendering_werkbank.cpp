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
#include <QDir>
#include <QImage>

#include <cmath>
#include <random>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"

#include <QFile>
#include <QXmlStreamReader>

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
            w.setDbmRange(-150.0f, -60.0f);
            // Clarity wie im Betrieb: Fenster Boden-5 .. Boden+55
            w.setClarityActive(true);
            w.setClarityWaterfallThresholds(-140.0f, -80.0f, -135.0f);
            v.apply(w);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));

            Scene scene(20260925);
            // ~12 s Verlauf bei 30 Bildern/s, damit der Wasserfall voll ist
            const bool echoProbe = v.id.startsWith(QLatin1String("g_echo"));
            for (int n = 0; n < 360; ++n) {
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
            const QImage img = grabSpectrum(w);
            QVERIFY2(!img.isNull(), "leeres Bild");
            const QString path = dir + QLatin1Char('/') + v.id + QStringLiteral(".png");
            QVERIFY(img.save(path));
            qInfo().noquote() << "geschrieben:" << path << img.size();
        }
    }
};

QTEST_MAIN(TestRenderingWerkbank)
#include "tst_rendering_werkbank.moc"
