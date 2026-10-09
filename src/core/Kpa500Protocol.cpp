// no-port-check: Longpath-eigene Datei, kein Port. Siehe den Kopf von
// core/Kpa500Protocol.h; die `// From Elecraft ...`-Verweise nennen
// Fundstellen in Elecrafts oeffentlicher Programmierreferenz.
// =================================================================
// src/core/Kpa500Protocol.cpp  (Longpath-eigen)
// =================================================================

#include "core/Kpa500Protocol.h"

#include <QStringList>

#include <utility>

namespace Longpath {
namespace Kpa500 {

namespace {

// Die Antwort des Boot-Laders auf 'I'. From Elecraft Rev A2,
// §BootLoader Command Reference, 'I' Identify: „the amplifier will
// respond with „KPA500"."
const char* kBootIdentifyReply = "KPA500";

// Zieht die Ziffernblöcke aus einem Datenfeld.
//
// WARUM NICHT AN FESTEN STELLEN GESCHNITTEN: das Dokument schreibt
// „^VIvvv iii;" und „^WSppp sss;" -- mit einem Zwischenraum. Ob der auf
// dem Draht wirklich ein Leerzeichen ist oder nur im Satz des PDF so
// aussieht, laesst sich am Dokument nicht entscheiden, und hier haengt
// kein Geraet, an dem man nachsehen koennte. Darum wird nach
// ZIFFERNBLOECKEN gesucht statt an Position 0..2 und 4..6 geschnitten:
// das liest beides richtig, mit Zwischenraum und ohne. Steht ein
// Zeichen drin, das weder Ziffer noch Trenner ist, trennt es eben --
// und ein Block, der nicht die erwartete Laenge hat, faellt oben durch
// die Laengenpruefung.
QStringList digitGroups(const QString& text)
{
    QStringList gruppen;
    QString lauf;
    for (QChar c : text) {
        if (c.isDigit()) {
            lauf.append(c);
        } else if (!lauf.isEmpty()) {
            gruppen << lauf;
            lauf.clear();
        }
    }
    if (!lauf.isEmpty()) {
        gruppen << lauf;
    }
    return gruppen;
}

// Zwei dreistellige Felder lesen -- mit Trenner dazwischen ODER ohne.
//
// Der erste Anlauf hat nur nach Ziffernbloecken gesucht und damit genau
// den Fall NICHT gekonnt, den er koennen wollte: ohne Trenner ist
// "475320" EIN Block aus sechs Ziffern, und die Laengenpruefung auf drei
// warf ihn weg. Der Pruefstand hat das gefangen
// (`derZwischenraumDarfSoOderSoSein`), nicht das Nachdenken.
//
// Darum ausdruecklich beide Formen: zwei Bloecke von je drei Ziffern,
// oder ein Block von sechs, der in der Mitte geteilt wird. Alles andere
// ist kein brauchbares Feld.
std::optional<std::pair<int, int>> twoThreeDigitFields(const QString& text)
{
    const QStringList g = digitGroups(text);
    if (g.size() == 2 && g.at(0).size() == 3 && g.at(1).size() == 3) {
        return std::pair<int, int>{g.at(0).toInt(), g.at(1).toInt()};
    }
    if (g.size() == 1 && g.at(0).size() == 6) {
        return std::pair<int, int>{g.at(0).left(3).toInt(),
                                   g.at(0).mid(3, 3).toInt()};
    }
    return std::nullopt;
}

}  // namespace

// ── Lesen ────────────────────────────────────────────────────────────

void ReplyParser::feed(const QByteArray& bytes)
{
    m_buf.append(bytes);

    while (true) {
        // Die Boot-Antwort kommt ohne Praefix und ohne Semikolon. Sie
        // wird VOR dem Semikolon-Suchlauf abgefangen, sonst bliebe sie
        // bis zur naechsten regulaeren Antwort im Puffer liegen -- und
        // genau sie kommt, wenn noch keine regulaere folgt.
        const int boot = m_buf.indexOf(kBootIdentifyReply);
        if (boot >= 0) {
            Reply r;
            r.isBootIdentify = true;
            r.verb = QString::fromLatin1(kBootIdentifyReply);
            if (m_onReply) { m_onReply(r); }
            m_buf.remove(0, boot + static_cast<int>(qstrlen(kBootIdentifyReply)));
            continue;
        }

        const int ende = m_buf.indexOf(kTerminator);
        if (ende < 0) {
            // Noch nichts Vollstaendiges. Den Puffer NICHT wachsen
            // lassen, bis der Speicher voll ist -- ein Geraet am falschen
            // Anschluss schickt moeglicherweise nie ein Semikolon.
            //
            // Zwei Faelle, und der zweite hat im ersten Anlauf gefehlt:
            //
            //   * Steht ein '^' drin, ist alles davor Rauschen und kann
            //     weg.
            //   * Steht KEINES drin, kann nichts im Puffer je eine
            //     Antwort anfangen -- bis auf die Boot-Antwort
            //     "KPA500", die ohne Praefix kommt und ueber zwei
            //     Fuetterungen geteilt sein kann. Darum bleiben fuenf
            //     Bytes stehen (ein Zeichen weniger als "KPA500"), der
            //     Rest fliegt.
            //
            // Der erste Anlauf hatte nur den ersten Fall und die
            // Bedingung `> 0`, also blieb reines Rauschen ohne jedes '^'
            // fuer immer liegen: 20000 gefuetterte Bytes, 20000 im
            // Puffer. Gefunden erst, als der Pruefstand die GROESSE
            // gemessen hat statt nur, ob die naechste Antwort noch
            // ankommt (sie kam -- deshalb war es unsichtbar).
            const int letztesPraefix = m_buf.lastIndexOf(kPrefix);
            if (letztesPraefix > 0) {
                m_buf.remove(0, letztesPraefix);
            } else if (letztesPraefix < 0) {
                constexpr int kBootSchwanz = 5;
                if (m_buf.size() > kBootSchwanz) {
                    m_buf.remove(0, m_buf.size() - kBootSchwanz);
                }
            }
            return;
        }

        QByteArray stueck = m_buf.left(ende);   // ohne das Semikolon
        m_buf.remove(0, ende + 1);

        const int praefix = stueck.lastIndexOf(kPrefix);
        if (praefix < 0) {
            // Kein '^' davor. Ein NACKTES Semikolon ist die Antwort auf
            // die Probe und damit eine Aussage (§Command Format); alles
            // andere ist Rauschen.
            if (stueck.trimmed().isEmpty()) {
                Reply r;
                r.isBareSemicolon = true;
                if (m_onReply) { m_onReply(r); }
            }
            continue;
        }
        stueck.remove(0, praefix + 1);          // ohne das '^'

        const QString text = QString::fromLatin1(stueck);
        // Die Befehlsbuchstaben sind zwei oder drei -- und drei nur bei
        // RVM, BRP, BRX, DMO. Statt eine Liste zu pflegen: alles von
        // vorn, solange es Buchstaben sind.
        //
        // DAS TRAEGT, WEIL KEINE ANTWORT BUCHSTABEN IN DEN DATEN HAT:
        // durchgezaehlt ueber alle RSP-Formate in Elecraft Rev A2 sind
        // die Daten ausschliesslich Ziffern und bei ^RVM ein Punkt.
        // Buchstaben in den Daten gibt es nur in einer Richtung -- im
        // SET `^FLC;` --, und SETs kommen nie herein. Waere das anders,
        // waere diese Regel falsch und es braeuchte die Liste.
        // (Im Pruefstand hat genau diese Verwechslung zugeschlagen: der
        // Kunstverstaerker las `^FLC;` als Verb "FLC" und das Loeschen
        // des Fehlers lief ins Leere.)
        int i = 0;
        while (i < text.size() && text.at(i).isLetter()) {
            ++i;
        }
        if (i < 2) {
            continue;   // kein brauchbares Verb
        }
        Reply r;
        r.verb = text.left(i).toUpper();
        r.data = text.mid(i);
        if (m_onReply) { m_onReply(r); }
    }
}

// ── Schreiben ────────────────────────────────────────────────────────

QByteArray buildGet(const QString& verb)
{
    return QStringLiteral("%1%2%3")
        .arg(QChar(kPrefix), verb.toUpper(), QChar(kTerminator))
        .toLatin1();
}

QByteArray buildSet(const QString& verb, const QString& data)
{
    return QStringLiteral("%1%2%3%4")
        .arg(QChar(kPrefix), verb.toUpper(), data, QChar(kTerminator))
        .toLatin1();
}

QByteArray buildPing()
{
    return QByteArray(1, kTerminator);
}

QByteArray buildBootIdentify()
{
    return QByteArrayLiteral("I");
}

QByteArray buildBootPowerOn()
{
    return QByteArrayLiteral("P");
}

QByteArray buildOperateSet(bool operate)
{
    // From Elecraft Rev A2, §^OS: „n = 0 for Standby, or n = 1 for
    // Operate mode."
    return buildSet(QString::fromLatin1(Verb::kOperate),
                    operate ? QStringLiteral("1") : QStringLiteral("0"));
}

QByteArray buildBandSet(int bandIndex)
{
    // From Elecraft Rev A2, §^BN: „^BNnn;" mit zwei Stellen, 00..10.
    // „All other values are ignored." -- ausserhalb des Bereichs wird
    // hier gar nichts gebaut, damit nicht etwas hinausgeht, das das
    // Geraet stillschweigend verwirft.
    if (bandIndex < 0 || bandIndex > 10) {
        return QByteArray();
    }
    return buildSet(QString::fromLatin1(Verb::kBand),
                    QStringLiteral("%1").arg(bandIndex, 2, 10, QLatin1Char('0')));
}

QByteArray buildFaultClear()
{
    // From Elecraft Rev A2, §^FL: „SET format: ^FLC; clears the current
    // fault." -- ein Buchstabe C, keine Zahl.
    return buildSet(QString::fromLatin1(Verb::kFault), QStringLiteral("C"));
}

QByteArray buildPowerOff()
{
    // From Elecraft Rev A2, §^ON: „SET format: ^ON0; turns the KPA500
    // off."
    return buildSet(QString::fromLatin1(Verb::kPowerState), QStringLiteral("0"));
}

// ── Entschluesseln ───────────────────────────────────────────────────

std::optional<PowerSwr> parsePowerSwr(const QString& data)
{
    const auto felder = twoThreeDigitFields(data);
    if (!felder) {
        return std::nullopt;
    }
    PowerSwr p;
    p.watts = felder->first;
    // Gedachtes Komma hinter der zweiten Stelle.
    p.swr = felder->second / 10.0f;
    return p;
}

std::optional<VoltsAmps> parseVoltsAmps(const QString& data)
{
    const auto felder = twoThreeDigitFields(data);
    if (!felder) {
        return std::nullopt;
    }
    VoltsAmps v;
    v.volts = felder->first / 10.0f;
    v.amps  = felder->second / 10.0f;
    return v;
}

std::optional<int> parseTemperature(const QString& data)
{
    const QStringList g = digitGroups(data);
    if (g.size() != 1 || g.at(0).size() != 3) {
        return std::nullopt;
    }
    const int t = g.at(0).toInt();
    // From Elecraft Rev A2, §^TM: „range of 0 - 150 degrees C."
    if (t < 0 || t > 150) {
        return std::nullopt;
    }
    return t;
}

QString bandName(int index)
{
    // From Elecraft Rev A2, §^BN -- woertlich die Liste des Dokuments:
    //   00 = 160m, 01 = 80m, 02 = 60m, 03 = 40m, 04 = 30m, 05 = 20m,
    //   06 = 17m, 07 = 15m, 08 = 12m, 09 = 10m, 10 = 6m
    static const QStringList tabelle = {
        QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
        QStringLiteral("40m"),  QStringLiteral("30m"), QStringLiteral("20m"),
        QStringLiteral("17m"),  QStringLiteral("15m"), QStringLiteral("12m"),
        QStringLiteral("10m"),  QStringLiteral("6m"),
    };
    if (index < 0 || index >= tabelle.size()) {
        return QStringLiteral("?m");
    }
    return tabelle.at(index);
}

std::optional<int> parseBand(const QString& data)
{
    const QStringList g = digitGroups(data);
    if (g.size() != 1 || g.at(0).size() != 2) {
        return std::nullopt;
    }
    const int b = g.at(0).toInt();
    if (b < 0 || b > 10) {
        return std::nullopt;
    }
    return b;
}

std::optional<bool> parseOperate(const QString& data)
{
    const QString t = data.trimmed();
    if (t == QStringLiteral("0")) { return false; }
    if (t == QStringLiteral("1")) { return true; }
    return std::nullopt;
}

std::optional<bool> parsePowerState(const QString& data)
{
    const QString t = data.trimmed();
    // From Elecraft Rev A2, §^ON: „RSP format: ^ONn; where n = 1." Eine
    // 0 kommt nur als Nachhall eines gerade gesendeten ^ON0; („if a ON;
    // quickly follows a ON0; you _may_ see a ON0; response") -- auch das
    // ist eine gueltige Antwort und heisst "aus".
    if (t == QStringLiteral("1")) { return true; }
    if (t == QStringLiteral("0")) { return false; }
    return std::nullopt;
}

std::optional<Fault> parseFault(const QString& data)
{
    const QStringList g = digitGroups(data);
    if (g.size() != 1 || g.at(0).size() != 2) {
        return std::nullopt;
    }
    Fault f;
    f.code = g.at(0).toInt();
    return f;
}

}  // namespace Kpa500
}  // namespace Longpath
