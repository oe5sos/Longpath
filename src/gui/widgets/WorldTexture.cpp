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
// =================================================================

#include "WorldTexture.h"

#include "core/AppSettings.h"

namespace NereusSDR {
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

} // namespace

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
    return true;
}

} // namespace WorldTexture
} // namespace NereusSDR
