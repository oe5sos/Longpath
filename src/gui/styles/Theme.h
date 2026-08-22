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
#include <QVector>

namespace Longpath::Style {

class Theme {
public:
    static Theme& instance();

    /// Read a theme file. Returns false and fills `error` on a broken
    /// file — and changes nothing, so a typo in the JSON leaves the
    /// running program with the palette it had rather than with none.
    bool loadFile(const QString& path, QString* error = nullptr);

    /// Liegt dieses Verzeichnis im Programmpaket? Mitgelieferte
    /// Paletten sind Angebote, keine Vorgabe: available() listet sie,
    /// loadUserTheme() ueberspringt sie. Siehe die Notiz dort.
    static bool isShippedThemeDir(const QString& dir);

    /// The places a personal theme may live, most specific first.
    /// Public so the Setup page can show them: „lege deine Datei hier
    /// ab" ist eine bessere Antwort als „irgendwo".
    static QStringList searchPaths();

    /// Load the first theme found in searchPaths(). No file is the
    /// normal case and not an error — dann gilt die Nereus-Palette.
    bool loadUserTheme();

    // ── Auswaehlbare Paletten ────────────────────────────────────────
    //
    // Bis 2026-08-20 nahm loadUserTheme() die ERSTE JSON-Datei in
    // alphabetischer Reihenfolge. Mit einer Datei war das eine Wahl,
    // mit dreien ist es Zufall — „tageslicht.json" gewaenne gegen
    // „werkbank.json", weil t vor w kommt, und der Betreiber haette
    // nie danach gefragt.
    //
    // Der Betreiber wollte zwei helle Paletten „welche ich bei viel
    // licht nutzen kann" und sie „bei themes wechseln" koennen. Dafuer
    // braucht es drei Dinge: die Liste, eine Wahl, und dass die Wahl
    // den Neustart ueberlebt.

    /// Eine gefundene Palette: Anzeigename und Datei.
    struct Entry { QString name; QString path; };

    /// Alle Paletten aus searchPaths(), nach Anzeigename sortiert.
    /// Doppelte Namen gewinnt der spezifischere Pfad — die eigene
    /// Datei neben den Einstellungen schlaegt die mitgelieferte.
    static QVector<Entry> available();

    /// Palette nach Anzeigenamen setzen und die Wahl merken. Ein
    /// leerer Name heisst „eingebaute Palette" (clear()).
    /// Gibt false zurueck, wenn der Name nicht gefunden wurde ODER die
    /// Datei kaputt ist — in beiden Faellen bleibt die laufende
    /// Palette stehen, statt das Programm farblos zu machen.
    bool activate(const QString& displayName, QString* error = nullptr);

    /// Die gemerkte Wahl anwenden. Ohne gemerkte Wahl faellt es auf
    /// loadUserTheme() zurueck, damit bestehende Installationen ihre
    /// Datei behalten.
    bool applyStoredChoice();

    /// Name der eingebauten Palette in der Auswahlliste.
    static QString builtInName();

    /// Back to the built-in palette.
    void clear();

    QString name() const { return m_name; }
    QString loadedFrom() const { return m_path; }
    // Auch ein Thema, das NUR die Form aendert, ist aktiv.
    //
    // Bis zum 2026-08-22 zaehlten hier nur Farben. Die Fassungen
    // „Flach" und „Tief" tragen absichtlich KEINE Farben — sie sollen
    // die Palette in Ruhe lassen und nur Ecken, Luft und Tiefe
    // umstellen. Ohne m_byForm haette das Programm sie als „kein Thema
    // aktiv" gemeldet, obwohl sichtbar etwas anders aussieht.
    bool isActive() const
    {
        return !m_byRole.isEmpty() || !m_byHex.isEmpty()
               || !m_byForm.isEmpty();
    }

    /// What this theme wants for a role, or an empty string when it has
    /// no opinion.
    QString forRole(const QString& role) const;

    // ── Form, nicht Farbe ────────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-21, zu Zeus: „das design hat teilweise
    // mehr stil ... die uebergaenge im hintergrund bei den widget
    // wirken sehr gut." Und danach, auf die Frage, wie das waehlbar
    // werden soll: „b" — also die Themendatei um Form-Regler
    // erweitern, statt nur weitere Farbschemata anzubieten.
    //
    // Der Grund steht in der Frage selbst: was ihm an Zeus gefaellt,
    // sind KEINE Farben. Es sind Verlaufstiefe, Polsterung,
    // Eckenradius — wie plastisch eine Flaeche wirkt. Ein Thema, das
    // nur Farben tauschen kann, kann genau das nicht.
    //
    // Vier Regler, mehr nicht. Jeder mit Vorgabe und Grenzen; was
    // ausserhalb liegt, wird still auf die Vorgabe zurueckgesetzt —
    // eine Themendatei ist Zubehoer und darf das Programm nicht
    // entstellen koennen.
    //
    //   radius   Eckenradius in Punkten          0..14   (7)
    //   luft-v   Polsterung oben/unten           0..12   (4)
    //   luft-h   Polsterung seitlich             0..24   (10)
    //   relief   aufgesetzt: Aufhellung oben     0..40   (16)
    //   mulde    versenkt: Tiefe                 0..40   (10)
    int forForm(const QString& name, int fallback) const;

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
    QHash<QString, int>     m_byForm;
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

} // namespace Longpath::Style
