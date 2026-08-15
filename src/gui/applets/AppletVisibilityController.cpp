// =================================================================
// src/gui/applets/AppletVisibilityController.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See header for attribution.
//
// =================================================================

#include "AppletVisibilityController.h"
#include "core/AppSettings.h"

namespace NereusSDR {

AppletVisibilityController::AppletVisibilityController(QObject* parent)
    : QObject(parent)
{
}

QString AppletVisibilityController::settingsKey(const QString& id)
{
    return QStringLiteral("Applet") + id + QStringLiteral("Visible");
}

void AppletVisibilityController::registerApplet(const QString& id,
                                                 const QString& displayName,
                                                 bool defaultVisible)
{
    if (id.isEmpty()) { return; }

    if (!m_entries.contains(id)) {
        m_order.append(id);
    }

    Entry& e = m_entries[id];
    e.displayName = displayName;

    const QString stored = AppSettings::instance()
        .value(settingsKey(id), QString{}).toString();
    if (stored == QStringLiteral("True")) {
        e.visible = true;
    } else if (stored == QStringLiteral("False")) {
        e.visible = false;
    } else {
        e.visible = defaultVisible;
    }
}

bool AppletVisibilityController::isVisible(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->visible : false;
}

bool AppletVisibilityController::isAvailable(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->available : false;
}

bool AppletVisibilityController::isEffectivelyVisible(const QString& id) const
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return false; }
    return it->visible && it->available;
}

QStringList AppletVisibilityController::registeredIds() const
{
    return m_order;
}

QString AppletVisibilityController::displayName(const QString& id) const
{
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->displayName : QString{};
}

void AppletVisibilityController::setVisible(const QString& id, bool visible)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return; }
    if (it->visible == visible) { return; }

    const bool wasEffective = it->visible && it->available;
    it->visible = visible;
    const bool nowEffective = it->visible && it->available;

    AppSettings::instance().setValue(
        settingsKey(id),
        visible ? QStringLiteral("True") : QStringLiteral("False"));
    emit visibilityChanged(id, visible);
    if (wasEffective != nowEffective) {
        emit effectiveVisibilityChanged(id, nowEffective);
    }
}

void AppletVisibilityController::setAvailable(const QString& id, bool available)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return; }
    if (it->available == available) { return; }

    const bool wasEffective = it->visible && it->available;
    it->available = available;
    const bool nowEffective = it->visible && it->available;

    emit availabilityChanged(id, available);
    if (wasEffective != nowEffective) {
        emit effectiveVisibilityChanged(id, nowEffective);
    }
}

} // namespace NereusSDR

// ── Kategorie und Schlagwoerter ──────────────────────────────────────
//
// Siehe den Header: die Schlagwoerter sind das, was das Suchfeld im
// „Widget hinzufuegen"-Dialog brauchbar macht.

namespace NereusSDR {

QString AppletVisibilityController::uncategorised()
{
    return QStringLiteral("Sonstiges");
}

void AppletVisibilityController::describeApplet(const QString& id,
                                                const QString& category,
                                                const QStringList& keywords)
{
    auto it = m_entries.find(id);
    if (it == m_entries.end()) { return; }
    it->category = category.trimmed();
    // Kleingeschrieben und ohne Leerzeichen an den Raendern abgelegt.
    // Die Suche vergleicht dann ohne jedes Mal umzuwandeln, und ein
    // versehentliches „SWR " neben „swr" ergibt nicht zwei Begriffe.
    it->keywords.clear();
    for (const QString& k : keywords) {
        const QString clean = k.trimmed().toLower();
        if (!clean.isEmpty() && !it->keywords.contains(clean)) {
            it->keywords << clean;
        }
    }
}

QString AppletVisibilityController::category(const QString& id) const
{
    const auto it = m_entries.constFind(id);
    if (it == m_entries.constEnd() || it->category.isEmpty()) {
        return uncategorised();
    }
    return it->category;
}

QStringList AppletVisibilityController::keywords(const QString& id) const
{
    const auto it = m_entries.constFind(id);
    return it == m_entries.constEnd() ? QStringList{} : it->keywords;
}

QStringList AppletVisibilityController::categories() const
{
    // In Anmeldereihenfolge, nicht alphabetisch: die Reihenfolge der
    // Spalte links ist eine Gestaltungsentscheidung (SPECTRUM zuerst,
    // SWITCHES zuletzt), und Alphabet wuerde sie zerstoeren.
    QStringList out;
    for (const QString& id : m_order) {
        const QString c = category(id);
        if (!out.contains(c)) { out << c; }
    }
    return out;
}

bool AppletVisibilityController::matches(const QString& id,
                                         const QString& needle) const
{
    const QString n = needle.trimmed().toLower();
    if (n.isEmpty()) { return true; }

    const auto it = m_entries.constFind(id);
    if (it == m_entries.constEnd()) { return false; }

    if (it->displayName.toLower().contains(n)) { return true; }
    if (id.toLower().contains(n)) { return true; }
    for (const QString& k : it->keywords) {
        if (k.contains(n)) { return true; }
    }
    return false;
}

} // namespace NereusSDR
