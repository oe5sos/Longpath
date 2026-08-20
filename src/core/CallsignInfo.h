#pragma once

// =================================================================
// src/core/CallsignInfo.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/aethersdr/AetherSDR (GPLv3; see
//       LICENSE and About dialog for the live contributor list)
//
//   This file is a port of AetherSDR `src/core/CallsignInfo.h` and the
//   Callsigns:: helpers from `src/core/CallsignUtils.h` [@3a1f59e].
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR. AetherSDR's JSON cache round-trip and its
//                 cty.dat prefix-fallback fields are omitted: NereusSDR
//                 reads cty.dat through CtyDatParser instead, and there
//                 is no lookup cache here yet.
// =================================================================

#include <QDateTime>
#include <QMetaType>
#include <QRegularExpression>
#include <QString>

namespace Longpath {

// One station's contact data as returned by a callsign-database
// lookup. Provider-neutral so a future HamQTH/hamdb backend can fill
// the same fields.
struct CallsignInfo {
    QString call;          // canonical uppercase callsign
    QString firstName;     // QRZ <fname>
    QString lastName;      // QRZ <name> (last name)
    QString nameFmt;       // QRZ <name_fmt> — provider-formatted name
    QString city;          // QRZ <addr2>
    QString state;         // QRZ <state>
    QString country;       // QRZ <country>
    QString county;        // QRZ <county>
    QString grid;          // Maidenhead locator, QRZ <grid>
    QString licenseClass;  // QRZ <class>
    QString email;         // QRZ <email>
    QString url;           // QRZ <url>
    QString imageUrl;      // QRZ <image>
    double  latitude{0.0};
    double  longitude{0.0};
    bool    hasLatLon{false};
    bool    lotw{false};    // accepts QSL via Logbook of The World
    bool    eqsl{false};    // accepts QSL via eQSL
    bool    mailQsl{false}; // returns paper QSL, QRZ <mqsl>
    qint64  fetchedUtc{0};  // epoch seconds when fetched

    bool isValid() const { return !call.isEmpty(); }

    // "Ann Meier" — name_fmt when the provider gives one, else first +
    // last, else the bare callsign.
    QString displayName() const
    {
        if (!nameFmt.trimmed().isEmpty()) { return nameFmt.trimmed(); }
        const QString first = firstName.trimmed();
        const QString last  = lastName.trimmed();
        if (!first.isEmpty() && !last.isEmpty()) {
            return first + QLatin1Char(' ') + last;
        }
        if (!first.isEmpty()) { return first; }
        if (!last.isEmpty())  { return last; }
        return call;
    }
};

namespace Callsigns {

// Amateur callsign shape: optional DX prefix ("VP2E/"), ITU prefix,
// digit, suffix, optional portable designator ("/P", "/QRP").
// Deliberately permissive — this gates a lookup, it does not police
// licensing. Requiring the base call to end in a letter rejects most
// CW garble ("5NN", "73", "599").
inline const QRegularExpression& regex()
{
    static const QRegularExpression re(
        QStringLiteral("(?:[A-Z0-9]{1,4}/)?"                  // DX prefix
                       "(?:[A-Z]{1,2}|[A-Z][0-9]|[0-9][A-Z])" // ITU prefix
                       "[0-9][A-Z0-9]{0,3}[A-Z]"              // digit + suffix
                       "(?:/[A-Z0-9]{1,4})?"));               // portable
    return re;
}

inline bool isLikelyCallsign(const QString& text)
{
    const QString s = text.trimmed().toUpper();
    const QRegularExpressionMatch m = regex().match(s);
    return m.hasMatch() && m.capturedStart() == 0
        && m.capturedLength() == s.size();
}

// Canonical lookup form: uppercase, trimmed.
inline QString normalized(const QString& call)
{
    return call.trimmed().toUpper();
}

} // namespace Callsigns
} // namespace Longpath

// Travels through QrzClient::lookupSucceeded, so the meta-object
// system has to know it. Matches the convention the rest of the
// project follows for struct signal payloads (see FreeDVStation.h).
Q_DECLARE_METATYPE(Longpath::CallsignInfo)
