// =================================================================
// src/core/antenna/Touchstone.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See Touchstone.h for the parts of the format
// that bite.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/antenna/Touchstone.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace Longpath::Touchstone {
namespace {

double unitMultiplier(const QString& u, bool* ok)
{
    *ok = true;
    const QString s = u.toUpper();
    if (s == QLatin1String("HZ"))  { return 1.0; }
    if (s == QLatin1String("KHZ")) { return 1e3; }
    if (s == QLatin1String("MHZ")) { return 1e6; }
    if (s == QLatin1String("GHZ")) { return 1e9; }
    *ok = false;
    return 0.0;
}

// Strip a '!' comment. It may start anywhere in the line, which is the
// clause people forget — several instruments append "! point 37" to
// every data row.
QString stripComment(const QString& line)
{
    const int bang = line.indexOf(QLatin1Char('!'));
    return bang < 0 ? line : line.left(bang);
}

} // namespace

Sweep parseS1p(const QString& text, const QString& sourceName)
{
    Sweep out;
    out.source = sourceName;

    double  mult   = 0.0;      // set by the option line
    QString format;            // RI / MA / DB
    bool    haveOption = false;

    const QStringList lines = text.split(QRegularExpression(
        QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);

    int dataLines = 0;
    int badLines  = 0;

    for (const QString& raw : lines) {
        const QString line = stripComment(raw).trimmed();
        if (line.isEmpty()) { continue; }

        // ── The option line ──────────────────────────────────────────
        if (line.startsWith(QLatin1Char('#'))) {
            // Only the first one counts. A second '#' is a malformed
            // file, and honouring it would change the units halfway
            // through a sweep.
            if (haveOption) { continue; }
            haveOption = true;

            const QStringList f = line.mid(1).split(
                QRegularExpression(QStringLiteral("\\s+")),
                Qt::SkipEmptyParts);
            for (int i = 0; i < f.size(); ++i) {
                const QString t = f.at(i).toUpper();
                bool ok = false;
                const double m = unitMultiplier(t, &ok);
                if (ok) { mult = m; continue; }
                if (t == QLatin1String("RI") || t == QLatin1String("MA")
                    || t == QLatin1String("DB")) {
                    format = t;
                    continue;
                }
                if (t == QLatin1String("R") && i + 1 < f.size()) {
                    bool rok = false;
                    const double r = f.at(i + 1).toDouble(&rok);
                    if (rok && r > 0.0) { out.referenceOhms = r; }
                    ++i;
                    continue;
                }
                // S / Y / Z / G / H — only S makes sense for a sweep of
                // an antenna, and anything else is a file about
                // something other than what we are here for.
                if (t == QLatin1String("Y") || t == QLatin1String("Z")
                    || t == QLatin1String("G") || t == QLatin1String("H")) {
                    out.points.clear();
                    out.note = QStringLiteral(
                        "This file holds %1-parameters, not S-parameters.")
                            .arg(t);
                    return out;
                }
            }

            // The standard's defaults are GHZ and MA. Applying them
            // silently would read a 7.05 MHz row as 7.05 GHz, which is
            // wrong by a factor of a thousand and looks like a working
            // file. Better to say the header is incomplete.
            if (mult <= 0.0) {
                out.note = QStringLiteral(
                    "The '#' line does not say the frequency unit "
                    "(HZ / KHZ / MHZ / GHZ), so the numbers cannot be "
                    "read safely.");
                return out;
            }
            if (format.isEmpty()) {
                out.note = QStringLiteral(
                    "The '#' line does not say the number format "
                    "(RI / MA / DB).");
                return out;
            }
            continue;
        }

        if (!haveOption) {
            out.note = QStringLiteral(
                "No '#' option line before the data — this may not be a "
                "Touchstone file.");
            return out;
        }

        // ── A data row ───────────────────────────────────────────────
        const QStringList n = line.split(
            QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
        if (n.size() < 3) { ++badLines; continue; }

        bool f1 = false, f2 = false, f3 = false;
        const double freq = n.at(0).toDouble(&f1);
        const double a    = n.at(1).toDouble(&f2);
        const double b    = n.at(2).toDouble(&f3);
        if (!f1 || !f2 || !f3) { ++badLines; continue; }

        SweepPoint p;
        p.freqHz = freq * mult;
        if (format == QLatin1String("RI")) {
            p.gamma = {a, b};
        } else {
            // MA and DB differ only in how the magnitude is written.
            const double mag = (format == QLatin1String("DB"))
                                   ? std::pow(10.0, a / 20.0) : a;
            const double rad = b * M_PI / 180.0;
            p.gamma = {mag * std::cos(rad), mag * std::sin(rad)};
        }
        out.points.append(p);
        ++dataLines;
    }

    if (out.points.isEmpty()) {
        out.note = haveOption
            ? QStringLiteral("The file has a header but no readable data "
                             "rows.")
            : QStringLiteral("Nothing in this file looks like a Touchstone "
                             "sweep.");
        return out;
    }

    // A handful of unreadable rows in an otherwise good file is worth
    // reporting but not worth refusing — a truncated download still has
    // most of a sweep in it.
    if (badLines > 0) {
        out.note = QStringLiteral("%1 of %2 rows could not be read and were "
                                  "skipped.")
                       .arg(badLines).arg(badLines + dataLines);
    }

    // Some instruments sweep downwards. Everything downstream walks the
    // sweep in order and interpolates between neighbours, so put it
    // right here rather than in five places later.
    bool descending = false;
    for (int i = 1; i < out.points.size(); ++i) {
        if (out.points.at(i).freqHz < out.points.at(i - 1).freqHz) {
            descending = true;
            break;
        }
    }
    if (descending) {
        std::sort(out.points.begin(), out.points.end(),
                  [](const SweepPoint& x, const SweepPoint& y) {
                      return x.freqHz < y.freqHz;
                  });
    }

    return out;
}

Sweep readS1p(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Sweep s;
        s.source = path;
        s.note = QStringLiteral("Could not open %1").arg(path);
        return s;
    }
    QTextStream in(&f);
    return parseS1p(in.readAll(), QFileInfo(path).fileName());
}

} // namespace Longpath::Touchstone
