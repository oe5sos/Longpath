// =================================================================
// src/core/QsoConfirmation.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See QsoConfirmation.h for why "non-empty means
// confirmed" is the wrong shortcut.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/QsoConfirmation.h"

#include <QStringList>

namespace Longpath::QsoConfirmation {

State parse(const QString& adifValue)
{
    const QString v = adifValue.trimmed().toUpper();
    if (v.isEmpty()) { return State::Unknown; }
    if (v == QLatin1String("Y") || v == QLatin1String("V")) {
        // V is "verified" — LoTW's word for the same thing.
        return State::Confirmed;
    }
    if (v == QLatin1String("N")) { return State::No; }
    if (v == QLatin1String("R")) { return State::Requested; }
    if (v == QLatin1String("I")) { return State::Ignored; }
    // Something no version of the standard defines. Unknown rather than
    // a guess: a wrong guess here becomes a wrong award count.
    return State::Unknown;
}

State field(const LogEntry& e, const QString& adifName)
{
    const QString want = adifName.toUpper();
    for (const auto& kv : e.extras) {
        if (kv.first.compare(want, Qt::CaseInsensitive) == 0) {
            return parse(kv.second);
        }
    }
    return State::Unknown;
}

bool isConfirmed(const LogEntry& e)
{
    return qslCard(e) == State::Confirmed
        || lotw(e)    == State::Confirmed
        || eqsl(e)    == State::Confirmed;
}

QString badge(const LogEntry& e)
{
    // Order by how much weight an award programme gives them: LoTW
    // first because it is the one most of them accept without a card.
    QString out;
    if (lotw(e)    == State::Confirmed) { out += QLatin1Char('L'); }
    if (qslCard(e) == State::Confirmed) { out += QLatin1Char('C'); }
    if (eqsl(e)    == State::Confirmed) { out += QLatin1Char('e'); }
    return out;
}

QString describe(const LogEntry& e)
{
    auto word = [](State s) -> QString {
        switch (s) {
        case State::Confirmed: return QStringLiteral("confirmed");
        case State::No:        return QStringLiteral("not confirmed");
        case State::Requested: return QStringLiteral("requested");
        case State::Ignored:   return QStringLiteral("not applicable");
        case State::Unknown:   break;
        }
        return {};
    };

    QStringList parts;
    struct Row { const char* name; State s; };
    for (const Row& r : {Row{"LoTW", lotw(e)},
                         Row{"card", qslCard(e)},
                         Row{"eQSL", eqsl(e)}}) {
        const QString w = word(r.s);
        if (!w.isEmpty()) {
            parts << QStringLiteral("%1: %2")
                         .arg(QString::fromLatin1(r.name), w);
        }
    }
    if (parts.isEmpty()) {
        // Nothing said, which is not the same as "no". Worth spelling
        // out, because a blank cell otherwise reads as a refusal.
        return QStringLiteral("No confirmation recorded either way.");
    }
    return parts.join(QStringLiteral(" · "));
}

} // namespace Longpath::QsoConfirmation
