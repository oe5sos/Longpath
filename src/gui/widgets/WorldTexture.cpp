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

#include "gui/widgets/WorldMapCatalog.h"

#include "core/AppSettings.h"

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

} // namespace WorldTexture
} // namespace Longpath
