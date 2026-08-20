#pragma once

// =================================================================
// src/gui/applets/AppletVisibilityController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. No Thetis equivalent — Thetis exposes
// container-level show/hide via setup checkboxes, not per-applet
// toggles. AetherSDR has no equivalent.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Backs the Containers >
//                 Applets menu and the right-side panel ☰ menu.
// =================================================================

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

namespace Longpath {

class AppletVisibilityController : public QObject {
    Q_OBJECT
public:
    explicit AppletVisibilityController(QObject* parent = nullptr);

    // Register an applet's id, display name, and default visibility.
    // If an AppSettings key for this id already exists, the persisted
    // value wins over defaultVisible. Idempotent on the same id; later
    // calls overwrite the display name but preserve current state.
    void registerApplet(const QString& id,
                        const QString& displayName,
                        bool defaultVisible);

    // ── Kategorie und Schlagwoerter ──────────────────────────────────
    //
    // Vorlage des Betreibers, 2026-08-15: der „Add Panel"-Dialog bei
    // Zeus hat Kategorien in einer Spalte links (SPECTRUM, VFO, METERS,
    // DSP, LOG, TOOLS, …) und unter jedem Namen eine Schlagwortzeile:
    //
    //     TX Stage Meters
    //     tx · power · swr · alc · meters
    //
    // Die Schlagwoerter sind keine Zierde. Sie sind das, was die Suche
    // brauchbar macht: wer „swr" tippt, findet dieses Panel, ohne
    // seinen Namen zu kennen. Ein Suchfeld, das nur Titel durchsucht,
    // findet genau das, was man schon gefunden haette.
    //
    // Getrennt von registerApplet() und nicht als weitere Parameter:
    // es gibt elf Anmeldungen, und ein Aufruf mit fuenf Argumenten
    // waere an jeder Stelle schwerer zu lesen als zwei Zeilen. Wer die
    // Angabe weglaesst, landet in „Sonstiges" — sichtbar, aber
    // erkennbar unsortiert.
    void describeApplet(const QString& id,
                        const QString& category,
                        const QStringList& keywords);

    QString category(const QString& id) const;
    QStringList keywords(const QString& id) const;
    /// Alle vergebenen Kategorien, in der Reihenfolge ihres ersten
    /// Auftretens. Die Spalte links im Dialog liest das.
    QStringList categories() const;

    /// Passt das Widget auf den Suchbegriff? Name UND Schlagwoerter,
    /// ohne Ruecksicht auf Gross- und Kleinschreibung. Leerer Begriff
    /// passt auf alles.
    bool matches(const QString& id, const QString& needle) const;

    /// Was in „Sonstiges" landet, wenn niemand eine Kategorie angibt.
    static QString uncategorised();

    bool isVisible(const QString& id) const;          // user preference
    bool isAvailable(const QString& id) const;        // capability gate
    bool isEffectivelyVisible(const QString& id) const; // visible && available
    QStringList registeredIds() const;       // insertion order preserved
    QString displayName(const QString& id) const;

public slots:
    // User preference — persisted to AppSettings.
    void setVisible(const QString& id, bool visible);

    // Capability gate — NOT persisted. Lets the controller hide an
    // applet (and grey its menu entry) when an external feature flag
    // says the applet shouldn't be reachable. Example: when the 4O3A
    // master toggle is off, the Amplifier and Tuner applets become
    // unavailable. The user's persisted visibility preference is
    // preserved across availability changes, so re-enabling 4O3A pops
    // the applet back if the user wanted it visible.
    void setAvailable(const QString& id, bool available);

signals:
    // User preference changed (e.g., user clicked the menu).
    void visibilityChanged(const QString& id, bool visible);

    // Capability gate changed (e.g., 4O3A master toggle flipped).
    void availabilityChanged(const QString& id, bool available);

    // Convenience signal: fires when EITHER axis changes if it caused
    // the effective visibility to flip. Consumers that just want to
    // show/hide the wrapper should connect here.
    void effectiveVisibilityChanged(const QString& id, bool effective);

private:
    static QString settingsKey(const QString& id);  // "AppletRxVisible" etc.

    struct Entry {
        QString displayName;
        bool visible{true};       // user pref (persisted)
        bool available{true};     // capability gate (runtime)
        QString category;         // leer -> uncategorised()
        QStringList keywords;
    };
    QHash<QString, Entry> m_entries;   // id -> entry
    QStringList m_order;               // registration order
};

} // namespace Longpath
