// =================================================================
// src/gui/LayoutProfiles.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See LayoutProfiles.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/LayoutProfiles.h"

#include "core/AppSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace Longpath {

LayoutProfiles::LayoutProfiles(QObject* parent) : QObject(parent) {}

QString LayoutProfiles::settingsKey()
{
    return QStringLiteral("LayoutProfiles");
}

void LayoutProfiles::setHooks(Capture capture, Apply apply)
{
    m_capture = std::move(capture);
    m_apply = std::move(apply);
}

LayoutProfiles::Profile* LayoutProfiles::find(const QString& name)
{
    for (Profile& p : m_profiles) {
        if (p.name == name) { return &p; }
    }
    return nullptr;
}

const LayoutProfiles::Profile* LayoutProfiles::find(const QString& name) const
{
    for (const Profile& p : m_profiles) {
        if (p.name == name) { return &p; }
    }
    return nullptr;
}

bool LayoutProfiles::exists(const QString& name) const
{
    return find(name) != nullptr;
}

// ── Verwalten ────────────────────────────────────────────────────────

bool LayoutProfiles::create(const QString& name)
{
    return createInternal(name, std::nullopt);
}

bool LayoutProfiles::createWith(const QString& name, const QVariantMap& state)
{
    return createInternal(name, state);
}

bool LayoutProfiles::createInternal(const QString& name,
                                    const std::optional<QVariantMap>& given)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || exists(n)) { return false; }

    // ── „Speichern unter" ────────────────────────────────────────────
    //
    // Erst das laufende Fenster ins bisher aktive Profil sichern, dann
    // anlegen, dann hinueberwechseln. Alle drei Schritte sind noetig,
    // und der mittlere allein waere ein Fehler:
    //
    // Ein Profil, das angelegt wird, ohne dass man hinwechselt, laesst
    // das alte weiter behaupten, das Fenster gehoere ihm. Beim naechsten
    // Umschalten sichert captureIntoCurrent() dann den Aufbau des NEUEN
    // Profils ins ALTE — und dessen Anordnung ist weg. Das ist kein
    // Randfall: es passiert beim zweiten Profil, das jemand anlegt.
    captureIntoCurrent();

    Profile p;
    p.name = n;
    p.state = given ? *given
                    : (m_capture ? m_capture() : QVariantMap{});
    m_profiles.append(p);
    m_order << n;

    m_current = n;
    // Nur bei vorgegebenem Zustand herstellen. Bei einer Aufnahme des
    // jetzigen wäre es ein Herstellen dessen, was ohnehin schon da ist
    // — überflüssige Arbeit, die im Fenster als Zucken sichtbar würde.
    if (given && m_apply) { m_apply(*given); }
    emit currentChanged(m_current);
    emit profilesChanged();
    return true;
}

bool LayoutProfiles::duplicate(const QString& from, const QString& to)
{
    const QString t = to.trimmed();
    const Profile* src = find(from);
    if (!src || t.isEmpty() || exists(t)) { return false; }

    Profile p = *src;
    p.name = t;
    // Die Bindungen NICHT mitkopieren. Zwei Profile, die beide auf
    // „40 m in CW" hoeren, waeren ein Konflikt, den niemand angelegt
    // hat — und profileFor() muesste raten. Der Aufbau wird kopiert,
    // wofuer er gilt, entscheidet man neu.
    p.bands.clear();
    p.modes.clear();
    m_profiles.append(p);
    m_order << t;
    emit profilesChanged();
    return true;
}

bool LayoutProfiles::rename(const QString& from, const QString& to)
{
    const QString t = to.trimmed();
    if (t.isEmpty() || from == t || exists(t)) { return false; }
    Profile* p = find(from);
    if (!p) { return false; }

    p->name = t;
    m_order.replace(m_order.indexOf(from), t);
    if (m_current == from) {
        m_current = t;
        emit currentChanged(m_current);
    }
    emit profilesChanged();
    return true;
}

bool LayoutProfiles::remove(const QString& name)
{
    if (!exists(name)) { return false; }

    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).name == name) { m_profiles.removeAt(i); break; }
    }
    m_order.removeAll(name);

    if (m_current == name) {
        // Auf das erste verbleibende wechseln und dessen Aufbau
        // herstellen. Ein leeres m_current nach dem Loeschen waere ein
        // Fenster, das keinem Profil mehr gehoert — und die naechste
        // Umgestaltung landete nirgends.
        m_current = m_order.isEmpty() ? QString{} : m_order.first();
        if (!m_current.isEmpty() && m_apply) {
            if (const Profile* p = find(m_current)) { m_apply(p->state); }
        }
        emit currentChanged(m_current);
    }
    emit profilesChanged();
    return true;
}

bool LayoutProfiles::activate(const QString& name)
{
    if (!exists(name)) { return false; }
    if (name == m_current) { return true; }

    // ── Erst sichern, dann wechseln ──────────────────────────────────
    //
    // Ohne das geht jede Umgestaltung verloren, sobald jemand ein
    // anderes Profil anklickt — und zwar unwiederbringlich, weil es
    // kein Rueckgaengig gibt. Wer eine halbe Stunde sein CW-Fenster
    // gebaut hat und dann auf „SSB" klickt, hat sie weggeklickt.
    captureIntoCurrent();

    m_current = name;
    if (m_apply) {
        if (const Profile* p = find(name)) { m_apply(p->state); }
    }
    emit currentChanged(m_current);
    return true;
}

void LayoutProfiles::captureIntoCurrent()
{
    if (m_current.isEmpty() || !m_capture) { return; }
    if (Profile* p = find(m_current)) { p->state = m_capture(); }
}

void LayoutProfiles::applyCurrent()
{
    if (m_current.isEmpty() || !m_apply) { return; }
    if (const Profile* p = find(m_current)) {
        m_apply(p->state);
        // Review-Fund 2026-08-28: activate() emits currentChanged after
        // applying; this sibling had silently dropped that. Harmless
        // today (load() already emits it before this runs at startup),
        // but restoring it keeps the contract the same for whichever
        // caller reaches this next -- no captureIntoCurrent() call here
        // on purpose, unlike activate(): this re-applies the ALREADY
        // current profile onto itself, there is no other profile being
        // switched away from to capture into.
        emit currentChanged(m_current);
    }
}

QVariantMap LayoutProfiles::snapshot(const QString& name) const
{
    const Profile* p = find(name);
    return p ? p->state : QVariantMap{};
}

QByteArray LayoutProfiles::exportToJson(const QString& name) const
{
    const Profile* p = find(name);
    if (!p) { return {}; }

    // Dieselben vier Felder wie ein Array-Eintrag in save() -- absichtlich
    // dieselbe Form, damit eine spaetere Wiedereinspielung (createWith()
    // mit dem "state"-Feld) ohne eigenes zweites Format auskommt.
    QJsonObject o;
    o.insert(QStringLiteral("name"), p->name);
    o.insert(QStringLiteral("state"), QJsonObject::fromVariantMap(p->state));
    QJsonArray bands;
    for (int v : p->bands) { bands.append(v); }
    QJsonArray modes;
    for (int v : p->modes) { modes.append(v); }
    o.insert(QStringLiteral("bands"), bands);
    o.insert(QStringLiteral("modes"), modes);

    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

// ── Bindung ──────────────────────────────────────────────────────────

void LayoutProfiles::bindBand(const QString& name, Band b, bool on)
{
    if (Profile* p = find(name)) {
        if (on) { p->bands.insert(static_cast<int>(b)); }
        else    { p->bands.remove(static_cast<int>(b)); }
        emit profilesChanged();
    }
}

void LayoutProfiles::bindMode(const QString& name, DSPMode m, bool on)
{
    if (Profile* p = find(name)) {
        if (on) { p->modes.insert(static_cast<int>(m)); }
        else    { p->modes.remove(static_cast<int>(m)); }
        emit profilesChanged();
    }
}

bool LayoutProfiles::isBoundToBand(const QString& name, Band b) const
{
    const Profile* p = find(name);
    return p && p->bands.contains(static_cast<int>(b));
}

bool LayoutProfiles::isBoundToMode(const QString& name, DSPMode m) const
{
    const Profile* p = find(name);
    return p && p->modes.contains(static_cast<int>(m));
}

QString LayoutProfiles::profileFor(Band b, DSPMode m) const
{
    // In Anlegereihenfolge, damit das Ergebnis vorhersagbar ist. Eine
    // Bestenauswahl („das mit den meisten Treffern") waere klueger und
    // schlechter: bei einem Fenster, in dem man sendet, ist eine Regel,
    // die man im Kopf nachvollziehen kann, mehr wert als eine, die
    // meistens das Richtige trifft.
    for (const QString& n : m_order) {
        const Profile* p = find(n);
        if (!p) { continue; }
        // Ein Profil ohne jede Bindung passt nie automatisch — sonst
        // risse das erste angelegte jeden Bandwechsel an sich.
        if (p->bands.isEmpty() && p->modes.isEmpty()) { continue; }
        // Eine leere Seite heisst „egal", nicht „nie".
        const bool bandOk = p->bands.isEmpty()
                         || p->bands.contains(static_cast<int>(b));
        const bool modeOk = p->modes.isEmpty()
                         || p->modes.contains(static_cast<int>(m));
        if (bandOk && modeOk) { return p->name; }
    }
    return {};
}

// ── Platte ───────────────────────────────────────────────────────────

void LayoutProfiles::save() const
{
    QJsonArray arr;
    for (const QString& n : m_order) {
        const Profile* p = find(n);
        if (!p) { continue; }
        QJsonObject o;
        o.insert(QStringLiteral("name"), p->name);
        o.insert(QStringLiteral("state"),
                 QJsonObject::fromVariantMap(p->state));
        QJsonArray bands;
        for (int v : p->bands) { bands.append(v); }
        QJsonArray modes;
        for (int v : p->modes) { modes.append(v); }
        o.insert(QStringLiteral("bands"), bands);
        o.insert(QStringLiteral("modes"), modes);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("profiles"), arr);
    root.insert(QStringLiteral("current"), m_current);

    AppSettings::instance().setValue(
        settingsKey(),
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    // setValue() only updates the in-memory map -- AppSettings needs an
    // explicit save() to flush to disk (same two-step pattern every other
    // AppSettings writer in this codebase follows, e.g.
    // SpectrumWidget::setBackgroundFillColor()). Without it, a profile
    // create/rename/duplicate/remove survived only until something ELSE
    // happened to trigger a full settings flush (a VFO change, etc.) --
    // quit shortly after deleting profiles, and the deletion never made
    // it to disk, so the next launch reloaded the stale list (Betreiber,
    // 2026-08-27: "bis auf eines alle geloescht, dann app geschlossen,
    // dann app geoeffnet und wieder alle da").
    //
    // The flush itself is COALESCED (Review-Fund 2026-08-28): save() is
    // called from ~13 places in MainWindow.cpp, several on ordinary
    // interactive gestures (e.g. AppletFloatingWindow::geometrySettled,
    // 400 ms after a drag/resize ends) -- an unconditional
    // AppSettings::save() there did a full settings-tree XML
    // re-serialize + backup-rotate + fsync on every such gesture,
    // stalling the GUI thread on every drag release. setValue() above is
    // synchronous and cheap (in-memory only), so the data is already
    // safe from this call's point of view; only the disk COMMIT is
    // deferred, the same bool-guard + QTimer::singleShot idiom
    // RadioModel::scheduleSettingsSave() already uses for the same
    // reason. MainWindow::closeEvent's own unconditional
    // AppSettings::instance().save() at quit is the backstop that
    // guarantees this reaches disk even if the 500 ms debounce hasn't
    // fired yet -- so the original bug this save() call fixed (a
    // deletion lost on quit) stays fixed; only a crash within that
    // 500 ms window could still lose it, versus the unbounded window
    // before this method had any explicit save() at all.
    if (!m_diskFlushScheduled) {
        m_diskFlushScheduled = true;
        QTimer::singleShot(500, this, [this]() {
            m_diskFlushScheduled = false;
            AppSettings::instance().save();
        });
    }
}

void LayoutProfiles::load()
{
    const QString raw = AppSettings::instance()
                            .value(settingsKey(), QString{}).toString();
    if (raw.isEmpty()) { return; }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        // Nichts anfassen. Eine beschaedigte Datei darf nicht dazu
        // fuehren, dass beim naechsten Speichern die heile Fassung
        // ueberschrieben wird — und genau das passierte, wenn man hier
        // auf „keine Profile" zuruecksetzt.
        return;
    }

    QList<Profile> loaded;
    QStringList order;
    const QJsonObject root = doc.object();
    for (const QJsonValue& v : root.value(QStringLiteral("profiles")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty() || order.contains(name)) { continue; }
        Profile p;
        p.name = name;
        p.state = o.value(QStringLiteral("state")).toObject().toVariantMap();
        for (const QJsonValue& b : o.value(QStringLiteral("bands")).toArray()) {
            p.bands.insert(b.toInt());
        }
        for (const QJsonValue& m : o.value(QStringLiteral("modes")).toArray()) {
            p.modes.insert(m.toInt());
        }
        loaded.append(p);
        order << name;
    }

    m_profiles = loaded;
    m_order = order;
    const QString cur = root.value(QStringLiteral("current")).toString();
    // Ein gespeichertes „aktiv", das es nicht mehr gibt, wird zum
    // ersten vorhandenen. Sonst zeigt die Schiene auf ein Profil, das
    // niemand anklicken kann.
    m_current = order.contains(cur)
                    ? cur
                    : (order.isEmpty() ? QString{} : order.first());
    emit profilesChanged();
    emit currentChanged(m_current);
}

} // namespace Longpath
