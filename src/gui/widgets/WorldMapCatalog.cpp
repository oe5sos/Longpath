// =================================================================
// src/gui/widgets/WorldMapCatalog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Siehe WorldMapCatalog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "gui/widgets/WorldMapCatalog.h"

#include "core/AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace Longpath {
namespace WorldMapCatalog {

namespace {

// Ein Pixel. Absichtlich nicht mehr: 4096x2048 und 5400x2700 gehen
// glatt auf, und wer 2048x1000 hinlegt, soll es erfahren statt eine um
// 2,4 % gestauchte Karte zu bekommen, auf der jede Marke danebensitzt.
constexpr int kAspectTolerancePx = 1;

} // namespace

QString directory()
{
    // Derselbe Weg wie die Theme-Dateien. Das leere Profil ist der
    // Normalfall; AppSettings kennt die Verzweigung schon.
    return AppSettings::resolveConfigDir(QString{}) + QStringLiteral("/maps");
}

bool aspectIsUsable(const QSize& size, QString* reasonOut)
{
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
        if (reasonOut) {
            *reasonOut = QStringLiteral("Bild nicht lesbar");
        }
        return false;
    }
    const int want = size.height() * 2;
    if (std::abs(size.width() - want) > kAspectTolerancePx) {
        if (reasonOut) {
            // Die gefundenen Masse nennen, nicht nur „falsch": der
            // Betreiber soll sehen, ob er die Datei beschneiden kann
            // oder eine andere braucht.
            *reasonOut = QStringLiteral(
                "2:1 erwartet, %1x%2 gefunden (%3x%2 waere richtig)")
                .arg(size.width()).arg(size.height()).arg(want);
        }
        return false;
    }
    return true;
}

Entry describe(const QString& imagePath)
{
    Entry e;
    e.path = imagePath;

    const QFileInfo fi(imagePath);
    e.name = fi.completeBaseName();

    // Die Masse werden GELESEN, nicht dekodiert. QImageReader holt sie
    // aus dem Kopf der Datei; ein 58-MB-Bild zu entpacken, nur um die
    // Auswahl zu fuellen, waere je Eintrag eine halbe Sekunde.
    QImageReader reader(imagePath);
    e.size = reader.size();

    const QString sidecar = fi.absolutePath() + QLatin1Char('/')
                          + fi.completeBaseName() + QStringLiteral(".json");
    QFile f(sidecar);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        // Eine kaputte Beschreibung wirft das Bild NICHT aus der
        // Auswahl. Sie beschreibt es nur nicht mehr.
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject o = doc.object();
            const QString n = o.value(QStringLiteral("name")).toString();
            if (!n.isEmpty()) { e.name = n; }
            e.source      = o.value(QStringLiteral("source")).toString();
            e.attribution = o.value(QStringLiteral("attribution")).toString();
            e.attributionRequired =
                o.value(QStringLiteral("attributionRequired")).toBool(false);
        }
    }

    e.usable = aspectIsUsable(e.size, &e.reason);
    return e;
}

QVector<Entry> entries()
{
    QVector<Entry> out;

    QDir dir(directory());
    if (!dir.exists()) { return out; }

    // Was Qt auf DIESEM Rechner lesen kann, nicht eine ausgedachte
    // Liste: welche Formate uebersetzt sind, haengt von den Qt-Plugins
    // ab, und png/jpg fest zu verdrahten schliesst tif oder webp aus,
    // wo sie vorhanden sind.
    QStringList filters;
    const auto formats = QImageReader::supportedImageFormats();
    filters.reserve(formats.size());
    for (const QByteArray& f : formats) {
        filters << QStringLiteral("*.") + QString::fromLatin1(f);
    }

    const QFileInfoList files =
        dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
    out.reserve(files.size());
    for (const QFileInfo& fi : files) {
        out.append(describe(fi.absoluteFilePath()));
    }

    // Brauchbare zuerst, darin nach Namen. Ein abgelehntes Bild bleibt
    // sichtbar, steht aber nicht zwischen den waehlbaren.
    std::stable_sort(out.begin(), out.end(),
                     [](const Entry& a, const Entry& b) {
        if (a.usable != b.usable) { return a.usable; }
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return out;
}

} // namespace WorldMapCatalog
} // namespace Longpath
