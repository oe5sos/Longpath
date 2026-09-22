#pragma once
// =================================================================
// src/core/RxProfileManager.h  (Longpath)
// =================================================================
//
// Longpath-original file. Receive profiles: the receive-side DSP
// settings of a slice (AGC, noise reduction, noise blankers, ANF,
// squelch, APF, binaural) saved under a name and applied to a slice
// again. The counterpart of MicProfileManager for the receive side,
// but without its per-MAC scope: none of these settings depend on the
// radio, so a profile made on one radio serves on the next.
//
// What a profile does NOT carry: frequency, mode, filter, step,
// gains, antennas, RIT/XIT, lock/mute -- those are the frequency
// memories' business (MemoryRecord) or the band stack's.
//
// The set of settings is the list of SliceModel Q_PROPERTY names in
// profileProperties(). Capture reads them through QObject::property(),
// apply writes them through QMetaProperty::write() -- no per-setting
// code, and a new receive setting joins a profile by adding its
// property name to the list.
//
// AppSettings layout (global, not per station or MAC):
//   RxProfile/_names            = "Contest,Ragchew,..."   (manifest)
//   RxProfile/active            = "Contest"               (last applied/saved)
//   RxProfile/<name>/<property> = value as text (bool "True"/"False",
//                                  enums as their integer)
//
// The manifest is the source of truth for the list, as in
// MicProfileManager: AppSettings cannot enumerate keys by prefix, and
// a comma is stripped from names so the manifest stays unambiguous.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Longpath {

class AppSettings;
class SliceModel;

class RxProfileManager : public QObject {
    Q_OBJECT
public:
    explicit RxProfileManager(AppSettings& settings, QObject* parent = nullptr);

    // The SliceModel properties a profile carries, in a stable order.
    static const QStringList& profileProperties();

    QStringList profileNames() const;
    QString activeProfileName() const;
    bool hasProfile(const QString& name) const;

    // Capture `slice` under `name` (new or overwritten). A name is
    // trimmed, commas are dropped; an empty name is refused. Makes
    // `name` the active profile.
    bool saveProfile(const QString& name, const SliceModel* slice);

    // Write the profile's settings into `slice`. Unknown names and
    // null slices are refused; a value the profile lacks (a setting
    // that joined later) leaves the slice's current value alone.
    // Makes `name` the active profile.
    bool applyProfile(const QString& name, SliceModel* slice);

    bool deleteProfile(const QString& name);
    bool renameProfile(const QString& oldName, const QString& newName);

    // The stored values of a profile, by property name (tests, export).
    QHash<QString, QString> profileValues(const QString& name) const;

    // Text <-> QVariant for one property of `slice` (public for tests).
    static QString valueToText(const QVariant& v);
    static QVariant textToValue(const QString& text, int metaTypeId);

signals:
    void profileListChanged();
    void activeProfileChanged(const QString& name);

private:
    static QString cleanName(const QString& name);
    QStringList readManifest() const;
    void writeManifest(const QStringList& names);
    void setActive(const QString& name);
    QString keyFor(const QString& name, const QString& property) const;

    AppSettings& m_settings;
};

} // namespace Longpath
