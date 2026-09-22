// =================================================================
// src/core/RxProfileManager.cpp  (Longpath)
// =================================================================
//
// Longpath-original file. See RxProfileManager.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/RxProfileManager.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "models/SliceModel.h"

#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>

namespace Longpath {

namespace {

const QLatin1String kPrefix("RxProfile/");
const QLatin1String kManifestKey("RxProfile/_names");
const QLatin1String kActiveKey("RxProfile/active");

} // namespace

RxProfileManager::RxProfileManager(AppSettings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
}

const QStringList& RxProfileManager::profileProperties()
{
    // SliceModel Q_PROPERTY names. Grouped as the RX applet's popups
    // group them; the order is the order in which apply() writes, which
    // matters only where a mode switch resets its parameters (activeNr
    // before the NR parameters, nbMode before the NB ones).
    static const QStringList kProps = {
        // AGC
        QStringLiteral("agcMode"), QStringLiteral("agcThreshold"), QStringLiteral("agcHang"),
        QStringLiteral("agcSlope"), QStringLiteral("agcAttack"), QStringLiteral("agcDecay"),
        QStringLiteral("agcHangThreshold"), QStringLiteral("agcMaxGain"),
        QStringLiteral("agcFixedGain"), QStringLiteral("autoAgcEnabled"),
        QStringLiteral("autoAgcOffset"),
        // Noise reduction: the slot, then every slot's parameters
        QStringLiteral("activeNr"),
        QStringLiteral("nr1Taps"), QStringLiteral("nr1Delay"), QStringLiteral("nr1Gain"),
        QStringLiteral("nr1Leakage"), QStringLiteral("nr1Position"),
        QStringLiteral("nr2GainMethod"), QStringLiteral("nr2NpeMethod"),
        QStringLiteral("nr2TrainT1"), QStringLiteral("nr2TrainT2"), QStringLiteral("nr2AeFilter"),
        QStringLiteral("nr2Position"), QStringLiteral("nr2Post2Run"),
        QStringLiteral("nr2Post2Level"), QStringLiteral("nr2Post2Factor"),
        QStringLiteral("nr2Post2Rate"), QStringLiteral("nr2Post2Taper"),
        QStringLiteral("nr3Position"), QStringLiteral("nr3UseDefaultGain"),
        QStringLiteral("nr4Position"), QStringLiteral("nr4Reduction"),
        QStringLiteral("nr4Smoothing"), QStringLiteral("nr4Whitening"),
        QStringLiteral("nr4Rescale"), QStringLiteral("nr4PostThresh"), QStringLiteral("nr4Algo"),
        QStringLiteral("dfnrAttenLimit"), QStringLiteral("dfnrPostFilterBeta"),
        QStringLiteral("bnrStrength"),
        QStringLiteral("mnrStrength"), QStringLiteral("mnrOversub"), QStringLiteral("mnrFloor"),
        QStringLiteral("mnrAlpha"), QStringLiteral("mnrBias"), QStringLiteral("mnrGsmooth"),
        // Automatic notch
        QStringLiteral("anfEnabled"), QStringLiteral("anfTaps"), QStringLiteral("anfDelay"),
        QStringLiteral("anfGain"), QStringLiteral("anfLeakage"), QStringLiteral("anfPosition"),
        // Noise blankers
        QStringLiteral("nbMode"), QStringLiteral("nb1Threshold"), QStringLiteral("nb1TransitionMs"),
        QStringLiteral("nb1LeadMs"), QStringLiteral("nb1LagMs"), QStringLiteral("nb2Mode"),
        QStringLiteral("snbEnabled"), QStringLiteral("snbK1"), QStringLiteral("snbK2"),
        QStringLiteral("snbOutputBandwidthHz"),
        // Squelch, APF, binaural
        QStringLiteral("ssqlEnabled"), QStringLiteral("ssqlThresh"),
        QStringLiteral("amsqEnabled"), QStringLiteral("amsqThresh"),
        QStringLiteral("fmsqEnabled"), QStringLiteral("fmsqThresh"),
        QStringLiteral("apfEnabled"), QStringLiteral("apfTuneHz"),
        QStringLiteral("binauralEnabled"),
    };
    return kProps;
}

QString RxProfileManager::cleanName(const QString& name)
{
    QString n = name.trimmed();
    n.remove(QLatin1Char(','));
    return n;
}

QString RxProfileManager::keyFor(const QString& name, const QString& property) const
{
    return kPrefix + name + QLatin1Char('/') + property;
}

QStringList RxProfileManager::readManifest() const
{
    const QString raw = m_settings.value(kManifestKey).toString();
    if (raw.isEmpty()) {
        return {};
    }
    QStringList names = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    names.removeDuplicates();
    return names;
}

void RxProfileManager::writeManifest(const QStringList& names)
{
    if (names.isEmpty()) {
        m_settings.remove(kManifestKey);
    } else {
        m_settings.setValue(kManifestKey, names.join(QLatin1Char(',')));
    }
    emit profileListChanged();
}

void RxProfileManager::setActive(const QString& name)
{
    if (activeProfileName() == name) {
        return;
    }
    if (name.isEmpty()) {
        m_settings.remove(kActiveKey);
    } else {
        m_settings.setValue(kActiveKey, name);
    }
    emit activeProfileChanged(name);
}

QStringList RxProfileManager::profileNames() const
{
    return readManifest();
}

QString RxProfileManager::activeProfileName() const
{
    return m_settings.value(kActiveKey).toString();
}

bool RxProfileManager::hasProfile(const QString& name) const
{
    return readManifest().contains(cleanName(name));
}

QString RxProfileManager::valueToText(const QVariant& v)
{
    const QMetaType t = v.metaType();
    if (t.id() == QMetaType::Bool) {
        return v.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    }
    if (t.flags() & QMetaType::IsEnumeration) {
        // The enums here are `enum class X : int`; the integer is the
        // stored form (the way SliceModel persists them itself).
        int value = 0;
        if (t.sizeOf() == sizeof(int)) {
            value = *static_cast<const int*>(v.constData());
        } else {
            value = v.toInt();
        }
        return QString::number(value);
    }
    if (t.id() == QMetaType::Double || t.id() == QMetaType::Float) {
        return QString::number(v.toDouble(), 'g', 12);
    }
    return v.toString();
}

QVariant RxProfileManager::textToValue(const QString& text, int metaTypeId)
{
    const QMetaType t(metaTypeId);
    if (t.id() == QMetaType::Bool) {
        return text == QLatin1String("True") || text == QLatin1String("true")
               || text == QLatin1String("1");
    }
    if (t.flags() & QMetaType::IsEnumeration) {
        bool ok = false;
        int value = text.toInt(&ok);
        if (!ok) { return {}; }
        if (t.sizeOf() == sizeof(int)) {
            return QVariant(t, &value);
        }
        return {};
    }
    if (t.id() == QMetaType::Double || t.id() == QMetaType::Float) {
        bool ok = false;
        const double d = text.toDouble(&ok);
        return ok ? QVariant(d) : QVariant();
    }
    if (t.id() == QMetaType::Int) {
        bool ok = false;
        const int i = text.toInt(&ok);
        return ok ? QVariant(i) : QVariant();
    }
    if (t.id() == QMetaType::QString) {
        return text;
    }
    QVariant v(text);
    return v.convert(t) ? v : QVariant();
}

bool RxProfileManager::saveProfile(const QString& name, const SliceModel* slice)
{
    const QString n = cleanName(name);
    if (n.isEmpty() || !slice) {
        return false;
    }
    const QMetaObject* meta = slice->metaObject();
    int stored = 0;
    for (const QString& prop : profileProperties()) {
        const int idx = meta->indexOfProperty(prop.toLatin1().constData());
        if (idx < 0) {
            qCWarning(lcRxProfiles) << "no such SliceModel property:" << prop;
            continue;
        }
        const QVariant v = meta->property(idx).read(slice);
        m_settings.setValue(keyFor(n, prop), valueToText(v));
        ++stored;
    }
    QStringList names = readManifest();
    if (!names.contains(n)) {
        names.append(n);
        writeManifest(names);
    }
    setActive(n);
    qCInfo(lcRxProfiles) << "saved" << n << "(" << stored << "settings )";
    return true;
}

bool RxProfileManager::applyProfile(const QString& name, SliceModel* slice)
{
    const QString n = cleanName(name);
    if (!slice || !readManifest().contains(n)) {
        return false;
    }
    const QMetaObject* meta = slice->metaObject();
    int applied = 0;
    for (const QString& prop : profileProperties()) {
        const QString key = keyFor(n, prop);
        if (!m_settings.contains(key)) {
            continue;       // joined the list after this profile was saved
        }
        const int idx = meta->indexOfProperty(prop.toLatin1().constData());
        if (idx < 0) {
            continue;
        }
        const QMetaProperty mp = meta->property(idx);
        const QVariant v = textToValue(m_settings.value(key).toString(), mp.metaType().id());
        if (!v.isValid()) {
            qCWarning(lcRxProfiles) << "unreadable value for" << prop << "in" << n;
            continue;
        }
        if (mp.write(slice, v)) {
            ++applied;
        } else {
            qCWarning(lcRxProfiles) << "could not write" << prop << "in" << n;
        }
    }
    setActive(n);
    qCInfo(lcRxProfiles) << "applied" << n << "(" << applied << "settings )";
    return true;
}

bool RxProfileManager::deleteProfile(const QString& name)
{
    const QString n = cleanName(name);
    QStringList names = readManifest();
    if (!names.contains(n)) {
        return false;
    }
    for (const QString& prop : profileProperties()) {
        m_settings.remove(keyFor(n, prop));
    }
    names.removeAll(n);
    writeManifest(names);
    if (activeProfileName() == n) {
        setActive(QString());
    }
    return true;
}

bool RxProfileManager::renameProfile(const QString& oldName, const QString& newName)
{
    const QString o = cleanName(oldName);
    const QString n = cleanName(newName);
    QStringList names = readManifest();
    if (n.isEmpty() || o == n || !names.contains(o) || names.contains(n)) {
        return false;
    }
    for (const QString& prop : profileProperties()) {
        const QString from = keyFor(o, prop);
        if (m_settings.contains(from)) {
            m_settings.setValue(keyFor(n, prop), m_settings.value(from));
            m_settings.remove(from);
        }
    }
    names.replace(names.indexOf(o), n);
    writeManifest(names);
    if (activeProfileName() == o) {
        setActive(n);
    }
    return true;
}

QHash<QString, QString> RxProfileManager::profileValues(const QString& name) const
{
    QHash<QString, QString> out;
    const QString n = cleanName(name);
    for (const QString& prop : profileProperties()) {
        const QString key = keyFor(n, prop);
        if (m_settings.contains(key)) {
            out.insert(prop, m_settings.value(key).toString());
        }
    }
    return out;
}

} // namespace Longpath
