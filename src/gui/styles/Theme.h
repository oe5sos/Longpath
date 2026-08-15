#pragma once

// =================================================================
// src/gui/styles/Theme.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Technik Nereus, Design du ────────────────────────────────────────
//
// OE5SOS, 2026-08-15:
//
//   „Es werden immer Änderungen von Nereus kommen, die ich dann
//    downloade und die sich dann automatisch meiner Farben und meinem
//    Design anpassen sollen. Technik Nereus, Design ich."
//
// Das geht nicht, solange das Design IM Quelltext steht. StyleConstants.h
// ist genau die Datei, die ein Upstream-Commit auch anfasst — Konflikt
// bei jedem Update, für immer. Und ein Panel, das mit dem nächsten
// Download kommt, bringt Nereus-Farben mit und weiß nichts von einer
// fremden Palette.
//
// Also eine Schicht:
//
//   Theme-Datei    ~/Library/Application Support/NereusSDR/themes/*.json
//                  Rollen → Werte. Kein C++, nicht im Repo, überlebt
//                  jeden Download.
//
//   Theme + themed()  bildet ab, was Nereus malt, auf das, was die
//                  Datei will.
//
//   src/           1737 Literale in 134 Widgets. Dürfen bleiben.
//
// Der Quelltext muss nichts wissen. Er schreibt weiter #00b4d8 hin; die
// Datei sagt „wo Nereus #00b4d8 malt, male #c2924f".
//
// ── Rollen, nicht Hex ────────────────────────────────────────────────
//
// Die Datei nennt Rollen:
//
//   { "name": "OE5SOS", "colors": { "accent": "#c2924f" } }
//
// Ein Hex-Wert als Schlüssel geht auch — dann ist er der Nereus-Wert,
// der ersetzt werden soll. Das ist der Notausgang für eine Farbe, die
// noch keine Rolle hat, und es sind derzeit 162 davon.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace NereusSDR::Style {

class Theme {
public:
    static Theme& instance();

    /// Read a theme file. Returns false and fills `error` on a broken
    /// file — and changes nothing, so a typo in the JSON leaves the
    /// running program with the palette it had rather than with none.
    bool loadFile(const QString& path, QString* error = nullptr);

    /// The places a personal theme may live, most specific first.
    /// Public so the Setup page can show them: „lege deine Datei hier
    /// ab" ist eine bessere Antwort als „irgendwo".
    static QStringList searchPaths();

    /// Load the first theme found in searchPaths(). No file is the
    /// normal case and not an error — dann gilt die Nereus-Palette.
    bool loadUserTheme();

    /// Back to the built-in palette.
    void clear();

    QString name() const { return m_name; }
    QString loadedFrom() const { return m_path; }
    bool isActive() const { return !m_byRole.isEmpty() || !m_byHex.isEmpty(); }

    /// What this theme wants for a role, or an empty string when it has
    /// no opinion.
    QString forRole(const QString& role) const;

    /// What this theme wants in place of a Nereus colour, or empty.
    QString forHex(const QString& legacyHex) const;

    /// Alle Hex-Ersetzungen der Datei. Für die 162 Farben im Programm,
    /// die noch keine Rolle haben — bis die benannt sind, ist das der
    /// einzige Weg, sie zu erreichen.
    const QHash<QString, QString>& hexOverrides() const { return m_byHex; }

    /// Bumped on every change. themed() caches its substitution plan
    /// and rebuilds it when this moves — a stylesheet pass runs on
    /// every widget at startup and must not re-read a QHash each time.
    quint64 generation() const { return m_generation; }

private:
    Theme() = default;

    QHash<QString, QString> m_byRole;
    QHash<QString, QString> m_byHex;
    QString m_name;
    QString m_path;
    quint64 m_generation{1};
};

// ── Der eine Einhängepunkt ───────────────────────────────────────────
//
// Statt vierhundert setStyleSheet-Aufrufe einzuwickeln: ein Filter auf
// der Anwendung.
//
//   app.installEventFilter(new Style::ThemeFilter(&app));
//
// Qt schickt QEvent::Polish an JEDES Widget, kurz bevor es zum ersten
// Mal gezeichnet wird — auch an eines, das erst mit dem nächsten
// Download in den Baum kommt und von diesem Theme nie gehört hat. Der
// Filter liest dessen Stylesheet, schickt es durch themed() und setzt
// es zurück.
//
// Das ist die Antwort auf „soll sich automatisch anpassen".
//
// ── Wo er nicht hinreicht ────────────────────────────────────────────
//
// Malcode. S-Meter, Spektrum, Wasserfall und Charts malen mit QColor
// direkt, und dafür gibt es keinen Haken. Rund 600 Vorkommen, darunter
// die größten Farbflächen der App. Die müssen Style::… benutzen, Widget
// für Widget — siehe Phase 6 in docs/design/ROADMAP.md.
class ThemeFilter : public QObject {
public:
    explicit ThemeFilter(QObject* parent = nullptr) : QObject(parent) {}

    bool eventFilter(QObject* watched, QEvent* event) override;

    /// Wie viele Widgets der Filter bisher umgefärbt hat. Für den Test
    /// und für die Diagnoseseite; ein Filter, der eingehängt ist und
    /// nie zuschlägt, sieht sonst genauso aus wie einer, der wirkt.
    static quint64 appliedCount();
};

} // namespace NereusSDR::Style
