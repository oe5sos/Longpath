// =================================================================
// src/gui/styles/Theme.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See Theme.h for why this exists.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/styles/Theme.h"

#include "gui/styles/ThemeQss.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QWidget>

#include <atomic>

namespace NereusSDR::Style {
namespace {

const QRegularExpression& hexRe()
{
    static const QRegularExpression re(
        QStringLiteral("^#[0-9a-fA-F]{6}$"));
    return re;
}

std::atomic<quint64> g_applied{0};

/// Set while we are writing a widget's stylesheet, so the StyleChange
/// our own write produces does not come back round.
///
/// A property rather than a member: the filter is one object watching
/// every widget, and re-entrancy is per widget, not global.
constexpr const char* kBusy = "nereusThemeBusy";

} // namespace

Theme& Theme::instance()
{
    static Theme t;
    return t;
}

QStringList Theme::searchPaths()
{
    QStringList out;

    // 1. NEREUS_THEME_DIR schlägt alles. Ein Test braucht einen Ort, den
    //    er kontrolliert, und wer zwei Themes vergleichen will, braucht
    //    einen Schalter, der keine Datei verschiebt.
    const QByteArray env = qgetenv("NEREUS_THEME_DIR");
    if (!env.isEmpty()) {
        out << QString::fromLocal8Bit(env);
    }

    // 2. Neben den Einstellungen. Dort gehören die eigenen Dateien hin,
    //    und dort überschreibt sie nie jemand.
    for (const QString& dir : QStandardPaths::standardLocations(
             QStandardPaths::AppConfigLocation)) {
        out << dir + QStringLiteral("/themes");
    }

    // 3. Im Projektordner. ./run.sh startet aus dem Repo-Wurzelverzeichnis,
    //    und beim Entwickeln ist „Datei danebenlegen und neu starten" der
    //    kürzeste Weg — kein Umweg über die Bibliothek. Steht in
    //    .gitignore, gehört also nicht in den Quellbaum.
    out << QDir::currentPath() + QStringLiteral("/themes");

    // 4. Neben der Binärdatei, für ein Theme, das mit einem Build kommt.
    out << QCoreApplication::applicationDirPath() + QStringLiteral("/themes");

    return out;
}

bool Theme::loadUserTheme()
{
    for (const QString& dir : searchPaths()) {
        QDir d(dir);
        if (!d.exists()) { continue; }
        const QStringList files =
            d.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString& f : files) {
            if (loadFile(d.filePath(f))) { return true; }
        }
    }
    return false;
}

bool Theme::loadFile(const QString& path, QString* error)
{
    auto fail = [&](const QString& why) {
        if (error) { *error = why; }
        return false;
    };

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("%1: %2").arg(path, f.errorString()));
    }
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        return fail(QStringLiteral("%1, Zeichen %2: %3")
                        .arg(QFileInfo(path).fileName())
                        .arg(pe.offset)
                        .arg(pe.errorString()));
    }
    if (!doc.isObject()) {
        return fail(QStringLiteral("%1: die Datei enthält kein Objekt")
                        .arg(QFileInfo(path).fileName()));
    }

    // ── Erst prüfen, dann übernehmen ─────────────────────────────────
    //
    // In lokale Tabellen einlesen und die alten erst am Ende ersetzen.
    // Ein Tippfehler in der Mitte der Datei darf nicht die halbe
    // Palette umstellen und die andere Hälfte stehen lassen — dann
    // sucht man den Fehler im Programm statt in der Datei.
    QHash<QString, QString> byRole, byHex;
    const QJsonObject root = doc.object();
    const QJsonObject colors = root.value(QStringLiteral("colors")).toObject();

    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
        const QString key = it.key().trimmed();
        const QString val = it.value().toString().trimmed();

        // JSON kennt keine Kommentare, und eine Palette ohne Notizen
        // ist nach drei Wochen nicht mehr zu lesen. Ein Schlüssel mit
        // Unterstrich davor ist eine Notiz und wird übergangen — sonst
        // hätte die erste Zwischenüberschrift die ganze Datei
        // durchfallen lassen, mit einer Fehlermeldung über einen
        // Farbwert, der gar keiner sein wollte.
        if (key.startsWith(QLatin1Char('_'))) { continue; }

        if (!hexRe().match(val).hasMatch()) {
            return fail(QStringLiteral(
                "%1: \"%2\" ist kein Farbwert der Form #rrggbb")
                    .arg(it.key(), it.value().toString()));
        }
        if (key.startsWith(QLatin1Char('#'))) {
            if (!hexRe().match(key).hasMatch()) {
                return fail(QStringLiteral(
                    "%1 ist weder eine Rolle noch ein Farbwert").arg(key));
            }
            byHex.insert(key.toLower(), val.toLower());
        } else {
            byRole.insert(key.toLower(), val.toLower());
        }
    }

    m_byRole = byRole;
    m_byHex  = byHex;
    m_name   = root.value(QStringLiteral("name")).toString(
                   QFileInfo(path).completeBaseName());
    m_path   = path;
    ++m_generation;
    return true;
}

void Theme::clear()
{
    if (!isActive() && m_name.isEmpty()) { return; }
    m_byRole.clear();
    m_byHex.clear();
    m_name.clear();
    m_path.clear();
    ++m_generation;
}

QString Theme::forRole(const QString& role) const
{
    return m_byRole.value(role.toLower());
}

QString Theme::forHex(const QString& legacyHex) const
{
    return m_byHex.value(legacyHex.toLower());
}

// ── Der Filter ───────────────────────────────────────────────────────

quint64 ThemeFilter::appliedCount()
{
    return g_applied.load(std::memory_order_relaxed);
}

bool ThemeFilter::eventFilter(QObject* watched, QEvent* event)
{
    const QEvent::Type t = event->type();
    // Polish trifft jedes Widget einmal, kurz vor dem ersten Zeichnen —
    // das ist der Normalfall, weil dieses Programm seine Stylesheets im
    // Konstruktor setzt. StyleChange fängt die, die später umschalten.
    if (t != QEvent::Polish && t != QEvent::StyleChange) {
        return false;
    }

    auto* w = qobject_cast<QWidget*>(watched);
    if (!w || w->property(kBusy).toBool()) {
        return false;
    }

    const QString qss = w->styleSheet();
    if (qss.isEmpty()) { return false; }

    const QString out = themed(qss);
    if (out == qss) { return false; }

    // Der eigentliche Schutz gegen die Endlosschleife ist nicht dieses
    // Flag, sondern dass themed() idempotent ist: der zweite Durchlauf
    // über ein bereits übersetztes Stylesheet liefert dasselbe zurück,
    // und die Prüfung oben bricht ab. Das Flag spart nur den Umweg.
    w->setProperty(kBusy, true);
    w->setStyleSheet(out);
    w->setProperty(kBusy, false);
    g_applied.fetch_add(1, std::memory_order_relaxed);

    // Nie verbrauchen. Der Filter färbt um, er nimmt niemandem ein
    // Ereignis weg.
    return false;
}

} // namespace NereusSDR::Style
