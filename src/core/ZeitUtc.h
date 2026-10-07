#pragma once

// =================================================================
// src/core/ZeitUtc.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Eine UTC-Zeit bauen, ohne 43 Warnungen zu erzeugen.
//
// Qt 6.7 hat die Ueberladungen mit `Qt::TimeSpec` fuer veraltet
// erklaert; mit Qt 7 verschwinden sie. Der Baum benutzt sie an 22
// Stellen und erzeugt damit 43 der 156 Warnungen, die beim Linux-Lauf
// anfallen -- genug, um alles andere zu ertraenken.
//
// Warum dann nicht einfach `QTimeZone::UTC` einsetzen? Weil daneben
// stand, warum es so ist:
//
//     Qt::UTC (not QTimeZone::UTC, which is Qt 6.7+) so this keeps
//     compiling on the ubuntu-24.04-arm release runner's system
//     Qt 6.4.2.
//
// Die Begruendung ist fuer die CI nicht mehr gueltig -- CI und
// Veroeffentlichung holen seit laengerem Qt 6.8 ueber aqt, kein Lauf
// benutzt mehr das Qt der Distribution. Fuer das Uebersetzen von Hand
// gilt sie weiter: CONTRIBUTING und CLAUDE.md nennen `qt6-base-dev`
// als Weg, und auf Ubuntu 24.04 ist das Qt 6.4.2.
//
// Also beides: neues Qt bekommt den neuen Weg (und schweigt), altes Qt
// den alten. An EINER Stelle, nicht an zweiundzwanzig -- und beim
// Umstieg auf Qt 7 ist genau diese Datei zu aendern.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-07 — Created in C++20/Qt6 for Longpath, AI-assisted via
//                 Anthropic Claude (Claude Code), operator Martin
//                 Fischer.
// =================================================================

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#  include <QTimeZone>
#endif

namespace Longpath::Zeit {

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#  define LONGPATH_UTC QTimeZone::UTC
#else
#  define LONGPATH_UTC Qt::UTC
#endif

/// Datum und Uhrzeit als UTC.
inline QDateTime utc(const QDate& d, const QTime& t)
{
    return QDateTime(d, t, LONGPATH_UTC);
}

/// Sekunden seit 1970 als UTC.
inline QDateTime ausSekunden(qint64 sekunden)
{
    return QDateTime::fromSecsSinceEpoch(sekunden, LONGPATH_UTC);
}

/// Millisekunden seit 1970 als UTC.
inline QDateTime ausMillisekunden(qint64 millisekunden)
{
    return QDateTime::fromMSecsSinceEpoch(millisekunden, LONGPATH_UTC);
}

}  // namespace Longpath::Zeit
