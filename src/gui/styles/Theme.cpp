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
#include "core/AppSettings.h"
#include <algorithm>
#include <QSet>

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

namespace Longpath::Style {
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

// Die Orte, an denen eine MITGELIEFERTE Palette liegt. Sie zaehlen
// fuer available(), nicht fuer loadUserTheme().
bool Theme::isShippedThemeDir(const QString& dir)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    if (appDir.isEmpty()) { return false; }
    const QString a = QDir(appDir + QStringLiteral("/themes")).absolutePath();
    const QString b = QDir(appDir + QStringLiteral("/../Resources/themes"))
                          .absolutePath();
    const QString d = QDir(dir).absolutePath();
    return d == a || d == b;
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

    // 5. Auf macOS in Contents/Resources — DORT liefern wir aus.
    //
    // Nicht neben die Binärdatei: Contents/MacOS ist für ausführbaren
    // Code, und codesign meldet jede andere Datei dort als
    // „In subcomponent". Eine Signatur, die eine Datendatei im
    // Programmverzeichnis mitsiegelt, ist beim nächsten Aktualisieren
    // dieser Datei ungültig. Am 2026-08-20 beim Neusignieren der
    // Schreibtischkopie aufgefallen.
    //
    // Pfad 4 bleibt: er trägt Linux und Windows, und ein von Hand
    // danebengelegtes Theme soll weiter greifen.
#ifdef Q_OS_MAC
    out << QCoreApplication::applicationDirPath()
               + QStringLiteral("/../Resources/themes");
#endif

    return out;
}

bool Theme::loadUserTheme()
{
    // ── Nur EIGENE Dateien, nicht die mitgelieferten ─────────────────
    //
    // „User" ist woertlich gemeint: eine Datei, die der Betreiber
    // hingelegt hat. Mitgelieferte Paletten gehoeren NICHT dazu — sie
    // sind Angebote, keine Vorgabe.
    //
    // Am 2026-08-20 ist genau das schiefgegangen: mit kreide.json im
    // Programmpaket nahm loadUserTheme() sie beim Start automatisch,
    // ohne dass jemand sie gewaehlt hatte. Der Betreiber sah ein halb
    // helles Programm und in seinen Einstellungen stand nicht einmal
    // ein ActiveTheme — er HATTE nichts umgestellt.
    //
    // available() listet die mitgelieferten weiterhin: gewaehlt werden
    // sollen sie, gefunden werden nicht.
    for (const QString& dir : searchPaths()) {
        if (isShippedThemeDir(dir)) { continue; }
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

// ── Auswaehlbare Paletten ───────────────────────────────────────────
//
// Siehe Theme.h fuer den Grund. Kurz: mit einer Datei war „nimm die
// erste" eine Wahl, mit dreien ist es Zufall.

QString Theme::builtInName()
{
    return QStringLiteral("Nacht (eingebaut)");
}

QVector<Theme::Entry> Theme::available()
{
    QVector<Entry> out;
    QSet<QString> seenName;

    // searchPaths() liefert vom Spezifischen zum Allgemeinen. Wer
    // zuerst kommt, behaelt den Namen: eine eigene „Tageslicht"-Datei
    // neben den Einstellungen schlaegt die mitgelieferte.
    for (const QString& dir : searchPaths()) {
        QDir d(dir);
        if (!d.exists()) { continue; }
        const QStringList files =
            d.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString& f : files) {
            const QString path = d.filePath(f);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (!doc.isObject()) { continue; }
            // Ohne Namen der Dateiname ohne Endung: eine Palette ohne
            // Namen ist immer noch besser als keine in der Liste.
            QString name = doc.object().value(QStringLiteral("name")).toString();
            if (name.isEmpty()) { name = QFileInfo(f).completeBaseName(); }
            if (seenName.contains(name)) { continue; }
            seenName.insert(name);
            out.append({name, path});
        }
    }
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return out;
}

bool Theme::activate(const QString& displayName, QString* error)
{
    auto& st = AppSettings::instance();

    if (displayName.isEmpty() || displayName == builtInName()) {
        clear();
        st.setValue("ActiveTheme", QString());
        return true;
    }
    for (const Entry& e : available()) {
        if (e.name != displayName) { continue; }
        // Erst laden, dann merken. Andersherum stuende nach einem
        // Neustart eine kaputte Datei im Merker und das Programm
        // faende jedes Mal dieselbe Enttaeuschung.
        if (!loadFile(e.path, error)) { return false; }
        st.setValue("ActiveTheme", displayName);
        return true;
    }
    if (error) {
        *error = QStringLiteral("Palette %1 nicht gefunden").arg(displayName);
    }
    return false;
}

bool Theme::applyStoredChoice()
{
    const QString choice =
        AppSettings::instance().value("ActiveTheme", QString()).toString();

    // Kein Merker: das Verhalten von vorher. Wer schon eine eigene
    // Datei liegen hat, soll sie nach dem Aktualisieren wiederfinden,
    // ohne sie neu auszuwaehlen.
    if (choice.isEmpty()) {
        if (AppSettings::instance().contains("ActiveTheme")) {
            clear();          // bewusst „eingebaut" gewaehlt
            return true;
        }
        return loadUserTheme();
    }
    QString err;
    if (activate(choice, &err)) { return true; }
    qWarning("Gemerkte Palette nicht anwendbar: %s", qUtf8Printable(err));
    return loadUserTheme();
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

    // ── Der Formteil ────────────────────────────────────────────────
    //
    // Absichtlich NACH den Farben und in eine eigene Tabelle: eine
    // Datei ohne „formen" ist vollstaendig gueltig und bekommt die
    // Vorgaben. So bleibt jede bestehende Palette unveraendert
    // brauchbar.
    QHash<QString, int> byForm;
    const QJsonObject formen = root.value(QStringLiteral("formen")).toObject();
    struct Limit { const char* key; int lo; int hi; };
    static const Limit kLimits[] = {
        {"radius", 0, 14}, {"luft-v", 0, 12}, {"luft-h", 0, 24},
        {"relief", 0, 40}, {"mulde",  0, 40},
    };
    for (auto it = formen.constBegin(); it != formen.constEnd(); ++it) {
        const QString key = it.key().trimmed().toLower();
        if (key.startsWith(QLatin1Char('_'))) { continue; }
        bool known = false;
        for (const Limit& l : kLimits) {
            if (key != QLatin1String(l.key)) { continue; }
            known = true;
            const int v = it.value().toInt(-1);
            // Ausserhalb der Grenzen wird STILL uebergangen, nicht
            // abgelehnt: eine Themendatei ist Zubehoer. Ein Vertipper
            // darf die Oberflaeche nicht entstellen, aber er darf auch
            // nicht die ganze Palette durchfallen lassen — dann suchte
            // man den Fehler im Programm.
            if (v >= l.lo && v <= l.hi) { byForm.insert(key, v); }
            break;
        }
        if (!known) { continue; }
    }

    m_byRole = byRole;
    m_byHex  = byHex;
    m_byForm = byForm;
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
    m_byForm.clear();
    m_name.clear();
    m_path.clear();
    ++m_generation;
}

QString Theme::forRole(const QString& role) const
{
    return m_byRole.value(role.toLower());
}

int Theme::forForm(const QString& name, int fallback) const
{
    return m_byForm.value(name.toLower(), fallback);
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

} // namespace Longpath::Style
