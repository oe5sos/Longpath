// =================================================================
// src/gui/styles/ThemeQss.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See ThemeQss.h for why this exists and Theme.h
// for the layer it belongs to.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-15 — Rollen ergänzt; die Tabelle fragt jetzt Theme, damit
//                 die Palette aus einer Datei außerhalb des Repos
//                 kommen kann.
// =================================================================

#include "gui/styles/ThemeQss.h"

#include "gui/StyleConstants.h"
#include "gui/styles/Theme.h"

#include <QHash>

namespace Longpath::Style {
namespace {

// ── Die Tabelle ──────────────────────────────────────────────────────
//
// Links die Rolle, in der Mitte was der Nereus-Quelltext hinschreibt,
// rechts was Nereus heute daraus malt. Eine Theme-Datei kann jede Rolle
// überschreiben; wo sie schweigt, gilt die rechte Spalte.
//
// Eine Konstante ohne Zeile ist eine Farbe, die kein Theme je erreicht.
// tst_theme_qss läuft über StyleConstants und schlägt fehl, wenn eine
// fehlt — das ist die einzige Absicherung dagegen.
const QVector<ThemeEntry>& table()
{
    static const QVector<ThemeEntry> t = {
        // Flächen
        { "app-bg",            "#0f0f1a", Style::kAppBg },
        { "panel",             "#0a0a18", Style::kPanelBg },
        { "statusbar-bg",      "#0a0a14", Style::kStatusBarBg },
        { "disabled-bg",       "#1a1a2a", Style::kDisabledBg },

        // Text
        { "text",              "#c8d8e8", Style::kTextPrimary },
        { "text-secondary",    "#8090a0", Style::kTextSecondary },
        { "text-tertiary",     "#708090", Style::kTextTertiary },
        { "text-scale",        "#607080", Style::kTextScale },
        { "text-inactive",     "#405060", Style::kTextInactive },
        { "label-mid",         "#8899aa", Style::kLabelMid },
        { "title-text",        "#8aa8c0", Style::kTitleText },
        { "disabled-text",     "#556070", Style::kDisabledText },

        // Rahmen und Flächen zweiter Ordnung
        { "button",            "#1a2a3a", Style::kButtonBg },
        // ── #203040 war vier Konstanten auf einmal ──────────────────
        //
        // kButtonHover, kGroove, kBorderSubtle und kStatusBarBorder
        // trugen alle denselben Wert. Beim Entblauen sind sie
        // auseinandergegangen, und eine Zeile kann nur ein Ziel haben.
        //
        // Sie zeigt auf den Rahmen. Einen Rahmen sieht man immer, einen
        // Hover-Zustand nur, solange die Maus stillsteht; wenn eine der
        // Bedeutungen falsch getroffen wird, soll es die seltene sein.
        { "border-subtle",     "#203040", Style::kBorderSubtle },
        { "button-alt-hover",  "#204060", Style::kButtonAltHover },
        { "border",            "#205070", Style::kBorder },
        // ── Der GRUND des Eingabefelds fehlte hier ──────────────────
        //
        // Bis zum 2026-08-21 fuehrte die Tabelle nur den Rahmen
        // (inset-border), nicht die Fuellung. kInsetBg (#08080a) kam
        // hier null Mal vor — jedes Eingabefeld und jedes Drehfeld
        // folgte also KEINEM Thema und blieb im hellen Thema
        // „Kreide" fast schwarz. Dieselbe Krankheit wie beim
        // Panadapter (siehe panadapter-bg weiter unten), nur an einer
        // Stelle, an der sie niemandem aufgefallen ist, weil die
        // Felder klein sind.
        //
        // Gefunden von tst_theme_form_knobs, nicht von mir: die
        // Pruefung verlangte, dass ein Thema die Feldfarbe aendern
        // kann, und das ging nicht.
        { "inset",             "#0a0a18", Style::kInsetBg },   // eigener Wert seit 2026-08-21
        { "inset-border",      "#1e2e3e", Style::kInsetBorder },
        { "status-sep",        "#304050", Style::kStatusSep },
        { "disabled-border",   "#2a3040", Style::kDisabledBorder },

        // Titelleiste
        { "titlebar-top",      "#3a4a5a", Style::kTitleGradTop },
        { "titlebar-mid",      "#2a3a4a", Style::kTitleGradMid },
        { "titlebar-bottom",   "#1a2a38", Style::kTitleGradBot },
        { "titlebar-border",   "#0a1a28", Style::kTitleBorder },

        // Bedeutung — jede Rolle genau ein Job
        { "accent",            "#00b4d8", Style::kAccent },
        { "ok-bg",             "#006040", Style::kGreenBg },
        { "ok",                "#00ff88", Style::kGreenText },
        { "ok-border",         "#00a060", Style::kGreenBorder },
        { "sel-bg",            "#0070c0", Style::kBlueBg },
        { "sel-border",        "#0090e0", Style::kBlueBorder },
        { "sel-hover",         "#0088d8", Style::kBlueHover },
        { "measured-bg",       "#604000", Style::kAmberBg },
        // Die Grundflaeche des Panadapters. Eigene Rolle, weil ein
        // Thema sie unabhaengig vom Panel setzen koennen muss: manche
        // helle Palette will ein helles Spektrum, manche behaelt das
        // dunkle wegen des Kontrasts der Kurve.
        { "panadapter-bg",     "#0a0a12", Style::kPanadapterBg },
        { "measured",          "#ffb800", Style::kAmberText },
        // Zweiter Vorgaenger derselben Rolle.
        //
        // Der Quelltext schreibt an 67 Stellen #c2924f hin — das war
        // der Wert von kAmberText, bis die Textleiter am 2026-08-20
        // auf #d8a55f angehoben wurde. In der Tabelle stand als
        // „legacy" aber nur der noch aeltere Wert #ffb800, und damit
        // blieben alle 67 Stellen im alten, dunkleren Ton stehen,
        // waehrend der Rest heller wurde.
        //
        // Eine Rolle darf mehrere Vorgaenger haben: `seen` schluesselt
        // auf den legacy-Wert, nicht auf die Rolle.
        { "measured",          "#c2924f", Style::kAmberText },
        { "measured-border",   "#906000", Style::kAmberBorder },
        { "warn",              "#ddbb00", Style::kAmberWarn },
        { "danger-bg",         "#cc2222", Style::kRedBg },

        // Abzeichen — eigene Abstufung, siehe StyleConstants.h.
        // Der mittlere Wert ist hier derselbe wie der Zielwert: diese
        // Rollen sind neu (2026-08-17) und haben keine Nereus-Vorgeschichte,
        // aus der etwas zu ersetzen waere.
        { "badge-info-bg",     "#161e27", Style::kBadgeInfoBg },
        { "badge-ok-bg",       "#212b27", Style::kBadgeOkBg },
        { "badge-off-bg",      "#18181a", Style::kBadgeOffBg },
        { "badge-warn-bg",     "#372c1d", Style::kBadgeWarnBg },
        { "badge-tx-bg",       "#3f2224", Style::kBadgeTxBg },
        { "tx",                "#c25a5c", Style::kTxRed },
        { "danger",            "#ff4444", Style::kRedBorder },

        // DSP-Umschalter
        { "dsp-on-bg",         "#1a6030", Style::kDspToggleBg },
        { "dsp-on-border",     "#20a040", Style::kDspToggleBorder },
        { "dsp-on",            "#80ff80", Style::kDspToggleText },

        // Filterüberlagerungen
        { "txfilter-border",   "#ff7833", Style::kTxFilterOverlayBorder },
        { "txfilter-label",    "#ffaa70", Style::kTxFilterOverlayLabel },

        // Overlay
        { "overlay-border",    "#304050", Style::kOverlayBorder },

        // Instrumente
        { "instrument-face",     "#c8c8c0", Style::kInstrumentFace },
        { "instrument-glow-hi",  "#5c5842", Style::kInstrumentGlowHi },
        { "instrument-glow-lo",  "#33322a", Style::kInstrumentGlowLo },
        { "instrument-limit",    "#a86b6d", Style::kInstrumentLimit },


        // ── Absichtlich ohne Zeile ───────────────────────────────────
        //
        // kBlueText und kRedText waren beide #ffffff — Text auf einem
        // gefüllten Knopf. Eine Zeile für Weiß kann nicht unterscheiden,
        // ob ein "color: #ffffff" auf einem Knopf steht oder mitten in
        // einem Absatz. Sie sind nur über den Namen erreichbar.
        //
        // kEqBand1/5/6 ebenso: die acht Entzerrertöne halten überlagerte
        // Kurven auseinander und antworten der Lesbarkeit gegeneinander,
        // nicht dem Chrome.
        //
        // Spektrum ebenso, und hier war es knapp. „trace" (#00e5ff)
        // hätte auch die Ziffern im VFO getroffen — dieselbe Farbe,
        // andere Aufgabe, und eine gedämpfte Frequenzanzeige will
        // niemand. „grid" wäre #ffffff gewesen, also jeder weiße
        // Knopftext im Programm. „grid-text" (#ffff00) hätte die
        // Thetis-Meter mitgenommen.
        //
        // Alle drei sind über ihren Namen erreichbar — Style::role(
        // "trace", …) im Malcode, "trace" in der Theme-Datei. Nur die
        // Ersetzung nach Hexwert bleibt aus, weil ein Hexwert nicht
        // sagt, wofür er dasteht.
    };
    return t;
}

/// Der fertige Ersetzungsplan: alte Farbe → was gemalt werden soll.
/// Wird neu gebaut, wenn sich das Theme ändert — ein Stylesheet-Durchlauf
/// trifft beim Start jedes Widget und darf nicht jedes Mal eine Datei
/// befragen.
struct Plan {
    QVector<QPair<QByteArray, QByteArray>> rows;   // legacy → target
    quint64 generation{0};
    bool moves{false};
};

const Plan& plan()
{
    static Plan p;
    const quint64 gen = Theme::instance().generation();
    if (p.generation == gen && !p.rows.isEmpty()) { return p; }

    Theme& th = Theme::instance();
    p.rows.clear();
    p.moves = false;

    QHash<QByteArray, bool> seen;
    for (const ThemeEntry& e : table()) {
        QString target = th.forRole(QString::fromLatin1(e.role));
        if (target.isEmpty()) {
            target = th.forHex(QString::fromLatin1(e.legacy));
        }
        if (target.isEmpty()) {
            target = QString::fromLatin1(e.current);
        }
        const QByteArray legacy = QByteArray(e.legacy).toLower();
        p.rows.append({legacy, target.toLower().toLatin1()});
        seen.insert(legacy, true);
        if (legacy != target.toLower().toLatin1()) { p.moves = true; }

        // ── Auch der HEUTIGE Wert ist ein Schluessel ─────────────────
        //
        // `legacy` ist der historische Wert der Rolle — beim
        // Entblauen 2026 stehengeblieben (#203040, #205070, #ffb800).
        // Der Quelltext schreibt aber laengst `Style::kBorderSubtle`
        // hin, und das ist heute #1f1f23. Diesen Wert kannte die
        // Tabelle nicht, also liess sie ihn stehen.
        //
        // Solange alles dunkel war, fiel das nicht auf: die Themendatei
        // aenderte Tonwerte um zwei Stufen, und was nicht mitkam, sah
        // trotzdem passend aus. Am 2026-08-20 kam die erste HELLE
        // Palette (Kreide), und im gerenderten Hauptfenster blieben
        // Panadapter, Applet-Spalte und Fussleiste dunkel — auf hellem
        // Grund drei schwarze Loecher.
        //
        // Eine Rolle darf mehrere Schluessel haben; `seen` schluesselt
        // auf den Wert, nicht auf die Rolle. Der historische bleibt
        // stehen, damit aeltere Stellen weiter greifen.
        const QByteArray current = QByteArray(e.current).toLower();
        if (!current.isEmpty() && !seen.contains(current)) {
            p.rows.append({current, target.toLower().toLatin1()});
            seen.insert(current, true);
            if (current != target.toLower().toLatin1()) { p.moves = true; }
        }
    }

    // Farben, für die die Datei eine Meinung hat, die aber keine Rolle
    // in der Tabelle haben. Es sind 162 solche im Programm — siehe
    // tools/colour_audit.py — und bis die benannt sind, ist ein
    // Hex-Schlüssel in der Theme-Datei der einzige Weg, sie anzufassen.
    const auto& extra = th.hexOverrides();
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        const QByteArray legacy = it.key().toLower().toLatin1();
        if (seen.contains(legacy)) { continue; }   // Rolle hat Vorrang
        const QByteArray target = it.value().toLower().toLatin1();
        p.rows.append({legacy, target});
        if (legacy != target) { p.moves = true; }
    }

    p.generation = gen;
    return p;
}

/// Sechs Hex-Ziffern ab i, Groß-/Kleinschreibung egal.
bool matchesAt(const QString& s, int i, const QByteArray& hex)
{
    if (i + 7 > s.size()) { return false; }
    if (s.at(i) != QLatin1Char('#')) { return false; }
    for (int k = 1; k < 7; ++k) {
        if (s.at(i + k).toLower() != QLatin1Char(hex.at(k))) { return false; }
    }
    // Eine siebte Hex-Ziffer heißt #rrggbbaa. Die ersten sechs davon zu
    // ersetzen ließe das Alpha-Paar an einer Farbe hängen, die niemand
    // gewählt hat.
    if (i + 7 < s.size()) {
        const QChar n = s.at(i + 7).toLower();
        if (n.isDigit()
            || (n >= QLatin1Char('a') && n <= QLatin1Char('f'))) {
            return false;
        }
    }
    return true;
}

} // namespace

const QVector<ThemeEntry>& themeTable() { return table(); }

QString role(const char* roleName, const char* fallback)
{
    const QString r = Theme::instance().forRole(QString::fromLatin1(roleName));
    return r.isEmpty() ? QString::fromLatin1(fallback) : r;
}

QString hexRole(const QString& nereusHex)
{
    if (nereusHex.isEmpty()) { return nereusHex; }
    const Plan& p = plan();
    if (!p.moves) { return nereusHex; }
    const QByteArray key = nereusHex.toLower().toLatin1();
    for (const auto& row : p.rows) {
        if (row.first == key) { return QString::fromLatin1(row.second); }
    }
    return nereusHex;
}

QString hexRole(const char* nereusHex)
{
    return hexRole(QString::fromLatin1(nereusHex));
}

int formInt(const char* name, int fallback)
{
    return Theme::instance().forForm(QString::fromLatin1(name), fallback);
}

QString themed(QString qss)
{
    if (qss.isEmpty()) { return qss; }
    const Plan& p = plan();
    if (!p.moves) { return qss; }

    // ── Ein Durchlauf, von links nach rechts ─────────────────────────
    //
    // Nicht QString::replace in einer Schleife über die Tabelle: ein
    // Wert, den Zeile 3 schreibt, könnte von Zeile 9 noch einmal
    // umgeschrieben werden, und das Ergebnis hinge an der Reihenfolge
    // der Zeilen. Höchstens eine Ersetzung je Position macht es
    // reihenfolgeunabhängig, und das ist es, was
    // themed(themed(s)) == themed(s) wahr macht.
    QString out;
    out.reserve(qss.size());

    for (int i = 0; i < qss.size(); ) {
        if (qss.at(i) == QLatin1Char('#')) {
            bool hit = false;
            for (const auto& row : p.rows) {
                if (matchesAt(qss, i, row.first)) {
                    out += QLatin1String(row.second.constData(),
                                         row.second.size());
                    i += 7;
                    hit = true;
                    break;
                }
            }
            if (hit) { continue; }
        }
        out += qss.at(i);
        ++i;
    }
    return out;
}

bool hasLegacyColour(const QString& qss)
{
    const Plan& p = plan();
    if (!p.moves) { return false; }
    for (int i = 0; i < qss.size(); ++i) {
        if (qss.at(i) != QLatin1Char('#')) { continue; }
        for (const auto& row : p.rows) {
            if (row.first == row.second) { continue; }
            if (matchesAt(qss, i, row.first)) { return true; }
        }
    }
    return false;
}

} // namespace Longpath::Style
