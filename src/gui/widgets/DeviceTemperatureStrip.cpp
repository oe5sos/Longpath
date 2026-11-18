// no-port-check: Longpath-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/widgets/DeviceTemperatureStrip.h"

// PaTempUnitNotifier wohnt in PaTempUnit.h, neben der Aufzaehlung.
#include "core/PaTempUnit.h"
#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>

#include <cmath>

namespace Longpath {

namespace {

// Abstand zwischen zwei Eintraegen. Kleiner als der Gruppenabstand des
// Hausstils (22–26 px): die Eintraege SIND eine Gruppe.
constexpr int kEintragAbstand = 10;
// Abstand der ganzen Leiste zu dem, was links von ihr steht. Sitzt im
// Widget und nicht im Kopf, damit er mit der Leiste verschwindet.
//
// Sechs und nicht zwoelf, und das ist gemessen: der Kopf braucht mit
// seinen neun Menues 1389 Punkte, das Notebook des Betreibers hat 1470.
// Mit zwoelf fehlten einem einzelnen Eintrag („FLEX 29°C") vier Punkte,
// und „Help" rutschte hinter das „»". Mit sechs passt er.
constexpr int kRandLinks = 6;
// Hoehe eines Eintrags. Fest, sonst zieht das Layout ihn auf die volle
// Hoehe des Kopfes -- und der Rahmen einer Meldung waere dann so hoch
// wie die Leiste selbst (am ersten Blatt gesehen).
constexpr int kEintragHoehe = 20;

// ── Wann die angezeigte Zahl der Messung folgt ───────────────────────
//
// Gezeigt werden ganze Grad. Mit blossem Runden springt die Zahl hin
// und her, sobald die Messung um eine halbe Stufe pendelt (die Flex
// meldet in Vierundsechzigsteln) — eine zappelnde Ziffer neben dem
// Namenszug ist genau das, was der Kopf nicht haben soll (HAUSSTIL:
// „Nichts springt"). Darum bleibt die Zahl stehen, bis die Messung
// weiter als diese Spanne von ihr entfernt ist. Die Anzeige liegt damit
// hoechstens 0,75 Grad neben der Messung statt 0,5.
constexpr double kHaltespanne = 0.75;

QString kapsel(const char* rahmen)
{
    // Radius 6, nie 3 (HAUSSTIL „Maße"). Ohne Meldung derselbe Rahmen
    // in durchsichtig: die Breite des Eintrags aendert sich dann nicht,
    // wenn das Geraet warnt, und nichts daneben rueckt.
    return QStringLiteral(
        "QLabel { font-size: 11px; border: 1px solid %1;"
        " border-radius: 6px; padding: 0px 4px; }")
        .arg(QLatin1String(rahmen));
}

} // namespace

DeviceTemperatureStrip::DeviceTemperatureStrip(QWidget* parent)
    : QWidget(parent)
{
    m_hbox = new QHBoxLayout(this);
    m_hbox->setContentsMargins(kRandLinks, 0, 0, 0);
    m_hbox->setSpacing(kEintragAbstand);

    // °C / °F: derselbe Schalter wie in der Fussleiste. Ohne das stuende
    // oben Celsius und unten Fahrenheit.
    connect(&PaTempUnitNotifier::instance(), &PaTempUnitNotifier::unitChanged,
            this, [this](PaTempUnit) {
        for (Slot& s : m_slots) {
            if (s.hasValue) { render(s); }
        }
    });

    // So breit wie ihre sichtbaren Eintraege, nicht breiter und nicht
    // schmaler: im Kopf soll das Layout sie weder strecken noch
    // quetschen. Wie viele Eintraege sichtbar sind, bestimmt die Grenze.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    // Leer geboren: ohne Geraet sieht der Kopf aus wie vorher.
    setVisible(false);
}

void DeviceTemperatureStrip::addSlot(const QString& id, const QString& label,
                                     const QString& description)
{
    if (find(id)) { return; }
    Slot s;
    s.id          = id;
    s.label       = label;
    s.description = description;
    s.widget      = new QLabel(this);
    s.widget->setObjectName(QStringLiteral("deviceTemp_%1").arg(id));
    s.widget->setTextFormat(Qt::RichText);
    s.widget->setStyleSheet(kapsel("transparent"));
    s.widget->setFixedHeight(kEintragHoehe);
    s.widget->setVisible(false);
    m_hbox->addWidget(s.widget, 0, Qt::AlignVCenter);
    m_slots.append(s);
}

void DeviceTemperatureStrip::setSlotLabel(const QString& id,
                                          const QString& label,
                                          const QString& description)
{
    Slot* s = find(id);
    if (!s) { return; }
    if (s->label == label && s->description == description) { return; }
    s->label       = label;
    s->description = description;
    if (s->hasValue) { render(*s); }
}

void DeviceTemperatureStrip::setReading(const QString& id, double degrees,
                                        Unit unit)
{
    Slot* s = find(id);
    if (!s || !std::isfinite(degrees)) { return; }

    const bool neu = !s->hasValue || s->unit != unit;
    s->degrees = degrees;
    s->unit    = unit;
    if (neu) {
        s->hasValue = true;
        s->shown    = static_cast<int>(std::lround(degrees));
        render(*s);
        return;
    }
    // Die Endstufen melden zehnmal je Sekunde. Solange die Zahl stehen
    // bleibt, wird hier nichts angefasst — kein Text, kein Stil, kein
    // Neuzeichnen.
    if (std::abs(degrees - static_cast<double>(s->shown)) <= kHaltespanne) {
        return;
    }
    s->shown = static_cast<int>(std::lround(degrees));
    render(*s);
}

void DeviceTemperatureStrip::setAlert(const QString& id, Alert alert,
                                      const QString& text)
{
    Slot* s = find(id);
    if (!s) { return; }
    if (s->alert == alert && s->alertText == text) { return; }
    s->alert     = alert;
    s->alertText = text;
    if (s->hasValue) { render(*s); }
}

void DeviceTemperatureStrip::clearReading(const QString& id)
{
    Slot* s = find(id);
    if (!s || !s->hasValue) { return; }
    s->hasValue = false;
    // Die Meldung geht mit dem Wert: ein Geraet, das weg ist, warnt
    // nicht mehr, und beim Wiederkommen soll nicht die alte dastehen.
    s->alert = Alert::None;
    s->alertText.clear();
    applyFit();
    emit contentsChanged();
}

bool DeviceTemperatureStrip::hasReading(const QString& id) const
{
    const Slot* s = find(id);
    return s && s->hasValue;
}

QString DeviceTemperatureStrip::text(const QString& id) const
{
    const Slot* s = find(id);
    if (!s || !s->hasValue) { return QString(); }
    return s->label + QLatin1Char(' ') + valueText(*s);
}

QStringList DeviceTemperatureStrip::texts() const
{
    QStringList out;
    for (const Slot& s : m_slots) {
        if (s.hasValue) { out << s.label + QLatin1Char(' ') + valueText(s); }
    }
    return out;
}

DeviceTemperatureStrip::Alert
DeviceTemperatureStrip::alert(const QString& id) const
{
    const Slot* s = find(id);
    return s ? s->alert : Alert::None;
}

int DeviceTemperatureStrip::count() const
{
    int n = 0;
    for (const Slot& s : m_slots) {
        if (s.hasValue) { ++n; }
    }
    return n;
}

DeviceTemperatureStrip::Slot* DeviceTemperatureStrip::find(const QString& id)
{
    for (Slot& s : m_slots) {
        if (s.id == id) { return &s; }
    }
    return nullptr;
}

const DeviceTemperatureStrip::Slot*
DeviceTemperatureStrip::find(const QString& id) const
{
    for (const Slot& s : m_slots) {
        if (s.id == id) { return &s; }
    }
    return nullptr;
}

QString DeviceTemperatureStrip::valueText(const Slot& s) const
{
    if (s.unit == Unit::AsReported) {
        return QStringLiteral("%1°").arg(s.shown);
    }
    if (PaTempUnitNotifier::currentUnit() == PaTempUnit::Fahrenheit) {
        // Aus der stehenden Celsius-Zahl gerechnet, nicht aus der
        // Messung: sonst zappelt in Fahrenheit, was in Celsius ruht.
        const double f = PaTempUnitNotifier::celsiusToFahrenheit(
            static_cast<double>(s.shown));
        return QStringLiteral("%1°F").arg(static_cast<int>(std::lround(f)));
    }
    return QStringLiteral("%1°C").arg(s.shown);
}

void DeviceTemperatureStrip::render(Slot& s)
{
    // Kuerzel in Skalengrau, Zahl warm und in fester Breite (HAUSSTIL
    // Regel 4: „Zahlen sind Monospace, Wörter nicht").
    s.widget->setText(QStringLiteral(
        "<span style=\"color:%1;\">%2</span>&nbsp;"
        "<span style=\"color:%3; font-family:%4;\">%5</span>")
        .arg(QLatin1String(Style::kTextScale),
             s.label.toHtmlEscaped(),
             QLatin1String(Style::kAmberText),
             QStringLiteral(LP_MONO_QSS),
             valueText(s)));

    // Die Meldung des Geraets als Rahmen, nicht als Farbwechsel der
    // Zahl: die Zahl bleibt eine Messung, der Rahmen ist der Zustand
    // (HAUSSTIL Regel 5, „umrandete Kapsel"). Kein kraeftiges Rot.
    switch (s.alert) {
    case Alert::None:
        s.widget->setStyleSheet(kapsel("transparent"));
        break;
    case Alert::Warning:
        s.widget->setStyleSheet(kapsel(Style::kAmberWarn));
        break;
    case Alert::Alarm:
        s.widget->setStyleSheet(kapsel(Style::kRedBorder));
        break;
    }

    QString tip = s.description;
    if (s.alert != Alert::None && !s.alertText.isEmpty()) {
        tip += QStringLiteral("\n") + s.alertText;
    }
    s.widget->setToolTip(tip);
    applyFit();
    emit contentsChanged();
}

void DeviceTemperatureStrip::setWidthLimit(int pixels)
{
    if (m_widthLimit == pixels) { return; }
    m_widthLimit = pixels;
    // Kein contentsChanged: die Grenze kommt vom Kopf, und der wuerde
    // sich sonst selbst rufen.
    applyFit();
}

int DeviceTemperatureStrip::firstEntryWidth() const
{
    for (const Slot& s : m_slots) {
        if (s.hasValue) { return kRandLinks + s.widget->sizeHint().width(); }
    }
    return 0;
}

int DeviceTemperatureStrip::visibleCount() const
{
    int n = 0;
    for (const Slot& s : m_slots) {
        if (s.hasValue && !s.widget->isHidden()) { ++n; }
    }
    return n;
}

void DeviceTemperatureStrip::applyFit()
{
    int belegt = kRandLinks;
    int n = 0;
    bool voll = false;
    for (Slot& s : m_slots) {
        if (!s.hasValue) {
            s.widget->setVisible(false);
            continue;
        }
        const int b = s.widget->sizeHint().width()
                      + (n > 0 ? kEintragAbstand : 0);
        // Ist einer zu breit, fallen auch alle rechts von ihm weg:
        // die Reihenfolge ist fest, eine Luecke in der Mitte waere
        // eine andere Reihenfolge.
        // Der erste bleibt, auch wenn die Grenze enger ist: eine
        // Anzeige, die bei vollem Kopf ganz verschwindet, ist auf
        // einem Notebook keine.
        if (!voll && n > 0 && m_widthLimit >= 0
            && belegt + b > m_widthLimit) {
            voll = true;
        }
        s.widget->setVisible(!voll);
        if (!voll) {
            belegt += b;
            ++n;
        }
    }
    setVisible(n > 0);
}

} // namespace Longpath
