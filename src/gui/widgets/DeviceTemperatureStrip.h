// no-port-check: Longpath-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QLabel;

namespace Longpath {

/// Die Temperaturen der angeschlossenen Geraete, im Kopf neben dem
/// Namenszug.
///
/// Betreiber, 2026-11-18: „temperaturanzeige von den geräten wäre auch
/// im bereich des logo longpath oben interessant".
///
/// Je Geraet ein kurzer Eintrag — Kuerzel grau, Zahl warm (HAUSSTIL:
/// „Warm = gemessen"). Drei Dinge, die diese Klasse NICHT tut, und
/// jedes mit Absicht:
///
///   1. Sie fragt nichts ab. Sie bekommt Werte gereicht, die ohnehin
///      als Signal vorliegen (gui/DeviceTemperatureFeed).
///   2. Sie zeigt nichts, was nicht gemeldet wurde. Ein Platz ohne Wert
///      ist unsichtbar; sind alle leer, ist die ganze Leiste unsichtbar
///      und der Kopf sieht aus wie vorher. Keine Null, kein Strich —
///      ein Strich behauptete ein Geraet, das eine Temperatur haette.
///   3. Sie kennt keine Schwellen. Eine Warnstufe gibt es nur, wenn das
///      GERAET sie meldet (setAlert), nie aus einer Zahl, die sich hier
///      jemand ausgedacht hat.
class DeviceTemperatureStrip : public QWidget {
    Q_OBJECT

public:
    /// In welcher Einheit die Zahl ankam.
    enum class Unit {
        /// Grad Celsius. Wird nach der Vorliebe des Betreibers gezeigt
        /// (PaTempUnitNotifier, derselbe Schalter wie in der Fussleiste).
        Celsius,
        /// Das Geraet sagt nicht, ob es Celsius oder Fahrenheit meldet
        /// (SPE: so, wie sein eigenes Display eingestellt ist). Dann
        /// steht die Zahl mit blossem Gradzeichen da und wird NICHT
        /// umgerechnet.
        AsReported
    };

    /// Was das Geraet selbst ueber seine Temperatur sagt.
    enum class Alert { None, Warning, Alarm };

    explicit DeviceTemperatureStrip(QWidget* parent = nullptr);

    /// Einen Platz anlegen. Die Reihenfolge der Aufrufe ist die
    /// Reihenfolge im Kopf — fest, damit ein Eintrag nicht wandert, je
    /// nachdem, welches Geraet zuerst geantwortet hat.
    void addSlot(const QString& id, const QString& label,
                 const QString& description);

    /// Kuerzel und Erklaerung eines Platzes aendern (das Funkgeraet
    /// heisst je nach Geraet anders).
    void setSlotLabel(const QString& id, const QString& label,
                      const QString& description);

    /// Ein Messwert ist eingetroffen. Unbekannte Plaetze werden
    /// uebergangen; nicht-endliche Werte ebenso.
    void setReading(const QString& id, double degrees,
                    Unit unit = Unit::Celsius);

    /// Das Geraet meldet selbst eine Grenze (oder nimmt sie zurueck).
    /// `text` ist die Meldung des Geraets und landet im Tooltip.
    void setAlert(const QString& id, Alert alert,
                  const QString& text = QString());

    /// Der Wert gilt nicht mehr (Geraet getrennt, ausgeschaltet).
    void clearReading(const QString& id);

    /// Wie breit die Leiste hoechstens werden darf. Was rechts nicht
    /// mehr hineinpasst, bleibt weg — ganze Eintraege, nie ein
    /// angeschnittener. Der ERSTE gemeldete Eintrag bleibt immer; der
    /// Kopf gibt ihm den Platz (siehe TitleBar). Negativ heisst: keine
    /// Grenze.
    void setWidthLimit(int pixels);
    /// Die Breite, die der erste gemeldete Eintrag braucht; 0 ohne
    /// Messwert.
    int  firstEntryWidth() const;
    /// Wie viele Eintraege gerade wirklich zu sehen sind (count() zaehlt
    /// die gemeldeten).
    int  visibleCount() const;

    // ── Fuer Pruefstaende ────────────────────────────────────────────
    bool        hasReading(const QString& id) const;
    /// Der Eintrag als schlichter Text, „FLEX 29°C". Leer ohne Wert.
    QString     text(const QString& id) const;
    /// Alle sichtbaren Eintraege, von links nach rechts.
    QStringList texts() const;
    Alert       alert(const QString& id) const;
    int         count() const;

signals:
    /// Inhalt oder Breite hat sich geaendert. Der Kopf rechnet dann
    /// neu, wie viel Platz er der Leiste geben kann.
    void contentsChanged();

private:
    struct Slot {
        QString id;
        QString label;
        QString description;
        QLabel* widget{nullptr};
        bool    hasValue{false};
        double  degrees{0.0};
        /// Die Zahl, die gerade dasteht. Eigenes Feld, weil sie der
        /// Messung nicht sofort folgt — siehe setReading().
        int     shown{0};
        Unit    unit{Unit::Celsius};
        Alert   alert{Alert::None};
        QString alertText;
    };

    Slot*       find(const QString& id);
    const Slot* find(const QString& id) const;
    QString     valueText(const Slot& s) const;
    void        render(Slot& s);
    /// Entscheidet, welche Eintraege zu sehen sind: die mit Wert, von
    /// links, solange sie in die Grenze passen — der erste immer.
    void        applyFit();

    QHBoxLayout*  m_hbox{nullptr};
    QVector<Slot> m_slots;
    int           m_widthLimit{-1};
};

} // namespace Longpath
