// =================================================================
// src/gui/widgets/WorldTexture.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see WorldTexture.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-09-02 — Photo style (Muted/NightWash/Crisp) added, von Martin
//                 Fischer, KI-gestuetzt ueber Anthropic Claude
//                 (Cowork). Begruendung im Header.
// =================================================================

#include "WorldTexture.h"

#include "gui/widgets/WorldMapCatalog.h"

#include "core/AppSettings.h"

#include <algorithm>

namespace Longpath {
namespace WorldTexture {

namespace {

struct Cache {
    QString path;
    QImage  image;
    bool    tried{false};
};

Cache& cache()
{
    static Cache c;
    return c;
}

struct StyledCache {
    QString path;
    Style   style{Style::Muted};
    QImage  image;
    bool    have{false};
};

StyledCache& styledCache()
{
    static StyledCache c;
    return c;
}

// Tonwertkurve je Stil: erst Helligkeit (Multiplikator), dann Kontrast
// (Drehpunkt bei Mittelgrau) -- dieselbe Reihenfolge wie CSS' eigenes
// filter: brightness() vor contrast(), weil genau diese Werte am
// gezeigten Entwurf (Blue-Marble-Entwuerfe, 2026-09-02) abgenommen
// wurden. Kein Thetis-Bezug: reine NereusSDR-Bildoberflaeche, editoriell
// gewaehlt, nicht gemessen.
struct Curve {
    bool   grey;
    double brightness;
    double contrast;
};

Curve curveFor(Style s)
{
    switch (s) {
    case Style::Muted:     return Curve{true,  0.50, 1.20};
    case Style::NightWash: return Curve{false, 0.62, 1.08};
    case Style::Crisp:     return Curve{false, 0.84, 1.05};
    }
    return Curve{true, 0.50, 1.20};
}

QImage applyStyle(const QImage& src, Style style)
{
    if (src.isNull()) { return src; }

    QImage out = src.convertToFormat(QImage::Format_RGB32);
    const Curve curve = curveFor(style);

    const auto adjust = [&](int v) {
        double vv = v * curve.brightness;
        vv = (vv - 128.0) * curve.contrast + 128.0;
        return std::clamp(static_cast<int>(vv), 0, 255);
    };

    const int h = out.height();
    const int w = out.width();
    for (int y = 0; y < h; ++y) {
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            int r = qRed(px);
            int g = qGreen(px);
            int b = qBlue(px);
            if (curve.grey) {
                r = g = b = qGray(px);
            }
            line[x] = qRgb(adjust(r), adjust(g), adjust(b));
        }
    }
    return out;
}

} // namespace

Notifier& Notifier::instance()
{
    static Notifier n;
    return n;
}

QString settingsKey()
{
    return QStringLiteral("GlobeWorldImagePath");
}

QString currentPath()
{
    return AppSettings::instance().value(settingsKey(), QString{}).toString();
}

void reload()
{
    cache() = Cache{};
    styledCache() = StyledCache{};
    emit Notifier::instance().changed();
}

QImage image()
{
    Cache& c = cache();
    const QString want = currentPath();

    if (c.tried && c.path == want) { return c.image; }

    c.path  = want;
    c.tried = true;
    c.image = QImage{};

    if (want.isEmpty()) { return c.image; }

    QImage img(want);
    if (img.isNull()) { return c.image; }

    // Converted once here rather than per pixel at render time.
    c.image = img.convertToFormat(QImage::Format_RGB32);
    return c.image;
}

QString styleSettingsKey()
{
    return QStringLiteral("GlobeWorldImageStyle");
}

Style style()
{
    const QString v = AppSettings::instance()
        .value(styleSettingsKey(), QStringLiteral("Muted")).toString();
    if (v == QStringLiteral("NightWash")) { return Style::NightWash; }
    if (v == QStringLiteral("Crisp"))     { return Style::Crisp; }
    return Style::Muted;
}

void setStyle(Style s)
{
    if (s == style()) { return; }

    const QString v = s == Style::NightWash ? QStringLiteral("NightWash")
                     : s == Style::Crisp     ? QStringLiteral("Crisp")
                                              : QStringLiteral("Muted");
    AppSettings::instance().setValue(styleSettingsKey(), v);
    emit Notifier::instance().changed();
}

QImage styledImage()
{
    const QImage base = image();
    if (base.isNull()) { return base; }

    const QString path = currentPath();
    const Style   want  = style();

    StyledCache& sc = styledCache();
    if (sc.have && sc.path == path && sc.style == want) { return sc.image; }

    sc.path  = path;
    sc.style = want;
    sc.image = applyStyle(base, want);
    sc.have  = true;
    return sc.image;
}

QString requiredAttribution()
{
    const QString path = currentPath();
    if (path.isEmpty()) { return QString{}; }
    const WorldMapCatalog::Entry e = WorldMapCatalog::describe(path);
    return e.attributionRequired ? e.attribution : QString{};
}

bool setPath(const QString& path)
{
    QImage img(path);
    // Check before committing: a file that cannot be read must not blank
    // a map that was working.
    if (img.isNull()) { return false; }

    AppSettings::instance().setValue(settingsKey(), path);
    Cache& c = cache();
    c.path  = path;
    c.tried = true;
    c.image = img.convertToFormat(QImage::Format_RGB32);
    emit Notifier::instance().changed();
    return true;
}

void clearPath()
{
    AppSettings::instance().setValue(settingsKey(), QString{});
    cache() = Cache{};
    emit Notifier::instance().changed();
}

} // namespace WorldTexture
} // namespace Longpath
