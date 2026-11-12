// no-port-check: Longpath-original, no Thetis logic.
//
// src/core/SotaNachtrag.cpp  (Longpath) -- siehe SotaNachtrag.h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "core/SotaNachtrag.h"

#include "core/SotaGipfel.h"

#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cstdlib>

namespace Longpath::SotaNachtrag {

namespace {

// ── CSV nach RFC 4180, Trenner ',' oder ';' ──────────────────────────
QVector<QStringList> zerlege(const QString& text, QChar trenner)
{
    QVector<QStringList> zeilen;
    QStringList zeile;
    QString feld;
    bool inAnf = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (inAnf) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                    feld += c;
                    ++i;
                } else {
                    inAnf = false;
                }
            } else {
                feld += c;
            }
        } else if (c == QLatin1Char('"')) {
            inAnf = true;
        } else if (c == trenner) {
            zeile << feld.trimmed();
            feld.clear();
        } else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            if (c == QLatin1Char('\r') && i + 1 < text.size()
                && text.at(i + 1) == QLatin1Char('\n')) {
                ++i;
            }
            zeile << feld.trimmed();
            feld.clear();
            if (!(zeile.size() == 1 && zeile.first().isEmpty())) { zeilen << zeile; }
            zeile.clear();
        } else {
            feld += c;
        }
    }
    if (!feld.isEmpty() || !zeile.isEmpty()) {
        zeile << feld.trimmed();
        zeilen << zeile;
    }
    return zeilen;
}

enum class Reihenfolge { TagMonat, MonatTag };

const QRegularExpression& slashDatum()
{
    static const QRegularExpression r(
        QStringLiteral("^(\\d{1,2})[/.](\\d{1,2})[/.](\\d{2}|\\d{4})$"));
    return r;
}

QDate leseDatum(const QString& s, Reihenfolge rf)
{
    const QString t = s.trimmed();
    static const QRegularExpression iso(QStringLiteral("^(\\d{4})-(\\d{2})-(\\d{2})"));
    static const QRegularExpression kompakt(QStringLiteral("^(\\d{4})(\\d{2})(\\d{2})$"));
    QRegularExpressionMatch m = iso.match(t);
    if (!m.hasMatch()) { m = kompakt.match(t); }
    if (m.hasMatch()) {
        return QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
    }
    m = slashDatum().match(t);
    if (m.hasMatch()) {
        int a = m.captured(1).toInt(), b = m.captured(2).toInt();
        int j = m.captured(3).toInt();
        if (m.captured(3).size() == 2) { j += 2000; }
        if (rf == Reihenfolge::MonatTag) { std::swap(a, b); }
        return QDate(j, b, a);
    }
    return QDate();
}

QTime leseZeit(const QString& s)
{
    static const QRegularExpression r(
        QStringLiteral("^(\\d{1,2}):?(\\d{2})(?::?(\\d{2}))?$"));
    const QRegularExpressionMatch m = r.match(s.trimmed());
    if (!m.hasMatch()) { return QTime(); }
    return QTime(m.captured(1).toInt(), m.captured(2).toInt(),
                 m.captured(3).isEmpty() ? 0 : m.captured(3).toInt());
}

QString kopfName(const QString& s)
{
    QString k = s.trimmed().toLower();
    k.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    return k;
}

bool istRufzeichen(const QString& s)
{
    // Auch Contestrufzeichen ohne Buchstaben am Ende (OL73); dafuer muss
    // irgendwo ein Buchstabe stehen, und ein Band ("144MHz") ist keines.
    static const QRegularExpression r(QStringLiteral(
        "^([A-Z0-9]{1,4}/)?[A-Z0-9]{1,3}[0-9][A-Z0-9]{0,4}(/[A-Z0-9]{1,4})?$"));
    static const QRegularExpression band(QStringLiteral(
        "^[0-9]+(\\.[0-9]+)?(M|CM|MM|KHZ|MHZ|GHZ)$"));
    static const QRegularExpression buchstabe(QStringLiteral("[A-Z]"));
    const QString t = s.trimmed().toUpper();
    return r.match(t).hasMatch() && !band.match(t).hasMatch()
           && buchstabe.match(t).hasMatch();
}

int abstandMin(const QTime& a, const QTime& b)
{
    return std::abs(a.secsTo(b)) / 60;
}

} // namespace

QString basisRufzeichen(const QString& call)
{
    const QStringList teile = call.trimmed().toUpper().split(QLatin1Char('/'),
                                                              Qt::SkipEmptyParts);
    QString best;
    for (const QString& t : teile) {
        bool ziffer = false;
        for (const QChar c : t) {
            if (c.isDigit()) { ziffer = true; break; }
        }
        if (ziffer && t.size() > best.size()) { best = t; }
    }
    return best.isEmpty() ? call.trimmed().toUpper() : best;
}

Datei leseCsv(const QByteArray& csv, QString* fehler)
{
    Datei d;
    QString text = QString::fromUtf8(csv);
    if (text.startsWith(QChar(0xFEFF))) { text.remove(0, 1); }
    const QChar trenner = text.count(QLatin1Char(';')) > text.count(QLatin1Char(','))
                              ? QLatin1Char(';') : QLatin1Char(',');
    const QVector<QStringList> roh = zerlege(text, trenner);
    if (roh.isEmpty()) {
        if (fehler) { *fehler = QStringLiteral("The file is empty."); }
        return d;
    }

    // Tag/Monat oder Monat/Tag -- fuer die ganze Datei einmal.
    Reihenfolge rf = Reihenfolge::TagMonat;
    for (const QStringList& z : roh) {
        for (const QString& f : z) {
            const QRegularExpressionMatch m = slashDatum().match(f.trimmed());
            if (m.hasMatch() && m.captured(2).toInt() > 12) { rf = Reihenfolge::MonatTag; }
        }
    }

    // ── Die Spalten finden ───────────────────────────────────────────
    int iDatum = -1, iGipfel = -1, iZeit = -1, iCall = -1, iGegenGipfel = -1;
    int start = 0;
    const QStringList& erste = roh.first();
    const bool v2 = !erste.isEmpty()
                    && (erste.first().compare(QStringLiteral("V2"), Qt::CaseInsensitive) == 0
                        || erste.first().compare(QStringLiteral("V1"), Qt::CaseInsensitive) == 0);
    bool kopf = false;
    if (v2) {
        d.format = QStringLiteral("SOTA V2");
        iGipfel = 2; iDatum = 3; iZeit = 4; iCall = 7; iGegenGipfel = 8;
    } else {
        int eigenerRuf = -1;
        for (int i = 0; i < erste.size(); ++i) {
            const QString k = kopfName(erste.at(i));
            const bool gegen = k.contains(QLatin1String("other")) || k.contains(QLatin1String("his"))
                               || k.contains(QLatin1String("s2s")) || k.contains(QLatin1String("worked"))
                               || k.contains(QLatin1String("chased")) || k.contains(QLatin1String("2"));
            if (iDatum < 0 && k.contains(QLatin1String("date"))) { iDatum = i; }
            else if (k.contains(QLatin1String("summit")) && !k.contains(QLatin1String("name"))
                     && !k.contains(QLatin1String("point"))) {
                if (gegen) { if (iGegenGipfel < 0) { iGegenGipfel = i; } }
                else if (iGipfel < 0) { iGipfel = i; }
            } else if (iZeit < 0 && (k == QLatin1String("time") || k == QLatin1String("timeutc")
                                     || k == QLatin1String("utc"))) {
                iZeit = i;
            } else if (k.contains(QLatin1String("call")) || k.endsWith(QLatin1String("station"))) {
                const bool eigen = k.contains(QLatin1String("my")) || k.contains(QLatin1String("own"))
                                   || k.contains(QLatin1String("activator"));
                if (eigen) { eigenerRuf = i; }
                else if (iCall < 0 || gegen) { iCall = i; }
            }
        }
        Q_UNUSED(eigenerRuf);
        kopf = iDatum >= 0 && iGipfel >= 0;
        if (kopf) {
            d.format = QStringLiteral("header");
            start = 1;
        } else {
            d.format = QStringLiteral("loose");
            iDatum = iGipfel = iZeit = iCall = iGegenGipfel = -1;
        }
    }

    for (int r = start; r < roh.size(); ++r) {
        const QStringList& z = roh.at(r);
        Zeile zeile;
        if (iDatum >= 0) {
            if (iDatum < z.size()) { zeile.datum = leseDatum(z.at(iDatum), rf); }
            if (iGipfel < z.size()) {
                zeile.gipfel = SotaGipfelVerzeichnis::normalisiere(z.at(iGipfel));
            }
            if (iZeit >= 0 && iZeit < z.size()) { zeile.zeit = leseZeit(z.at(iZeit)); }
            if (iCall >= 0 && iCall < z.size() && istRufzeichen(z.at(iCall))) {
                zeile.gegenCall = z.at(iCall).trimmed().toUpper();
            }
            if (iGegenGipfel >= 0 && iGegenGipfel < z.size()) {
                zeile.gegenGipfel = SotaGipfelVerzeichnis::normalisiere(z.at(iGegenGipfel));
            }
        } else {
            for (const QString& f : z) {
                if (zeile.gipfel.isEmpty()) {
                    const QString g = SotaGipfelVerzeichnis::normalisiere(f);
                    if (!g.isEmpty()) { zeile.gipfel = g; continue; }
                }
                if (!zeile.datum.isValid()) { zeile.datum = leseDatum(f, rf); }
            }
        }
        if (!zeile.datum.isValid() || zeile.gipfel.isEmpty()) {
            ++d.uebersprungen;
            continue;
        }
        d.zeilen << zeile;
    }

    // Eine Rufzeichenspalte, in der ueberall dasselbe steht, ist das
    // eigene Rufzeichen (Aktivierungsliste), nicht die Gegenstation.
    QSet<QString> rufe;
    int mitRuf = 0;
    for (const Zeile& z : d.zeilen) {
        if (!z.gegenCall.isEmpty()) { rufe.insert(z.gegenCall); ++mitRuf; }
    }
    if (rufe.size() == 1 && mitRuf > 3 && !v2) {
        for (Zeile& z : d.zeilen) { z.gegenCall.clear(); }
        rufe.clear();
    }
    d.mitQsos = !rufe.isEmpty();

    if (d.zeilen.isEmpty() && fehler) {
        *fehler = QStringLiteral(
            "No line with both a summit reference (like OE/OO-106) and a date was found.");
    }
    return d;
}

Ergebnis vorschlaege(const QVector<LogEntry>& log, const Datei& datei)
{
    Ergebnis erg;

    // Tag -> Gipfel -> Zeilen
    QMap<QDate, QMap<QString, QVector<const Zeile*>>> tage;
    for (const Zeile& z : datei.zeilen) { tage[z.datum][z.gipfel].push_back(&z); }

    QSet<QDate> tageMitQso;
    QSet<QDate> mehrdeutigGemeldet;
    for (int i = 0; i < log.size(); ++i) {
        const LogEntry& e = log.at(i);
        if (!e.timeOn.isValid()) { continue; }
        const QDateTime utc = e.timeOn.toUTC();
        const QDate tag = utc.date();
        const auto t = tage.constFind(tag);
        if (t == tage.constEnd()) { continue; }
        tageMitQso.insert(tag);
        if (!e.mySotaRef.trimmed().isEmpty()) {
            ++erg.schonGesetzt;
            continue;
        }

        // Die Zeilen dieses Rufzeichens an diesem Tag, je Gipfel.
        const QString basis = basisRufzeichen(e.call);
        const Zeile* treffer = nullptr;
        int besterAbstand = 1 << 30;
        for (auto g = t->constBegin(); g != t->constEnd(); ++g) {
            for (const Zeile* z : g.value()) {
                if (z->gegenCall.isEmpty() || basisRufzeichen(z->gegenCall) != basis) {
                    continue;
                }
                const int ab = z->zeit.isValid() ? abstandMin(z->zeit, utc.time()) : 0;
                if (!treffer || ab < besterAbstand) {
                    treffer = z;
                    besterAbstand = ab;
                }
            }
        }

        Vorschlag v;
        v.index = i;
        if (treffer) {
            v.gipfel = treffer->gipfel;
            v.gegenGipfel = e.sotaRef.trimmed().isEmpty() ? treffer->gegenGipfel : QString();
            v.imSotaLog = true;
            v.vorgewaehlt = true;
            v.hinweis = QStringLiteral("in your SOTA log");
            if (!v.gegenGipfel.isEmpty()) {
                v.hinweis += QStringLiteral(", summit to summit");
            }
        } else if (t->size() == 1) {
            v.gipfel = t->constBegin().key();
            // Nennt die Datei Gegenstationen, und diese ist nicht dabei,
            // war das QSO vermutlich nicht vom Gipfel.
            v.vorgewaehlt = !datei.mitQsos;
            v.hinweis = datei.mitQsos
                ? QStringLiteral("not in your SOTA log for this day — check")
                : QStringLiteral("activation on this day");
        } else {
            if (!mehrdeutigGemeldet.contains(tag)) {
                mehrdeutigGemeldet.insert(tag);
                erg.mehrdeutig << QStringLiteral("%1: %2")
                                      .arg(tag.toString(Qt::ISODate),
                                           QStringList(t->keys()).join(QStringLiteral(", ")));
            }
            continue;
        }
        erg.vorschlaege << v;
    }
    for (auto t = tage.constBegin(); t != tage.constEnd(); ++t) {
        if (!tageMitQso.contains(t.key())) { ++erg.tageOhneQso; }
    }
    return erg;
}

int anwenden(QVector<LogEntry>& log, const QVector<Vorschlag>& gewaehlt)
{
    int n = 0;
    for (const Vorschlag& v : gewaehlt) {
        if (v.index < 0 || v.index >= log.size() || v.gipfel.isEmpty()) { continue; }
        LogEntry& e = log[v.index];
        bool geaendert = false;
        if (e.mySotaRef.trimmed().isEmpty()) {
            e.mySotaRef = v.gipfel;
            geaendert = true;
        }
        if (e.sotaRef.trimmed().isEmpty() && !v.gegenGipfel.isEmpty()) {
            e.sotaRef = v.gegenGipfel;
            geaendert = true;
        }
        if (geaendert) { ++n; }
    }
    return n;
}

} // namespace Longpath::SotaNachtrag
