#include "core/LogbuchRueckschau.h"

#include "core/AdifLog.h"
#include "core/LogbookDatei.h"
#include "models/LogEntry.h"

#include <QFile>

#include <algorithm>

namespace Longpath {
namespace LogbuchRueckschau {

namespace {

constexpr qint64 kErstesFenster = 64 * 1024;   // gemessen: ~78 Datensaetze
constexpr int    kFaktor        = 4;           // Fenster wachsen lassen

const QByteArray& kEor()
{
    static const QByteArray e = QByteArrayLiteral("<EOR>");
    return e;
}

// Jüngster zuerst. Datensätze ohne Zeit nach hinten, statt sie als
// "vor allem anderen" zu behandeln -- eine fehlende Zeit ist keine Zeit
// im Jahr 0.
void juengsterZuerst(QVector<LogEntry>& v)
{
    std::stable_sort(v.begin(), v.end(),
                     [](const LogEntry& a, const LogEntry& b) {
        if (a.timeOn.isValid() != b.timeOn.isValid()) {
            return a.timeOn.isValid();
        }
        return a.timeOn > b.timeOn;
    });
}

}  // namespace

QByteArray abDatensatzGrenze(const QByteArray& stueck)
{
    const QByteArray gross = stueck.toUpper();

    // Der Dateikopf zuerst: enthält das Stück den Anfang der Datei, ist
    // alles vor <EOH> Kopf und kein Kontakt.
    const qsizetype kopf = gross.indexOf(QByteArrayLiteral("<EOH>"));
    if (kopf >= 0) { return stueck.mid(kopf + 5); }

    const qsizetype ende = gross.indexOf(kEor());
    if (ende < 0) {
        // Keine Grenze im ganzen Stück: das war die Mitte eines einzigen
        // Datensatzes. Nichts davon ist verlässlich.
        return {};
    }
    return stueck.mid(ende + kEor().size());
}

QByteArray datensaetzeMit(const QByteArray& inhalt, const QByteArray& nadel)
{
    QByteArray aus;
    if (inhalt.isEmpty() || nadel.isEmpty()) { return aus; }

    // Eine Großschreibkopie, EINMAL. Die Stellen darin gelten
    // unverändert für das Original -- toUpper() ändert bei ASCII keine
    // Länge, und ADIF-Feldnamen wie Rufzeichen sind ASCII.
    const QByteArray gross = inhalt.toUpper();
    const QByteArray n     = nadel.toUpper();

    qsizetype hinterLetztem = 0;
    qsizetype pos           = 0;
    while ((pos = gross.indexOf(n, pos)) >= 0) {
        // Rückwärts zur vorigen Datensatzgrenze, vorwärts zur nächsten.
        qsizetype anfang = gross.lastIndexOf(kEor(), pos);
        anfang = (anfang < 0) ? 0 : anfang + kEor().size();
        qsizetype ende = gross.indexOf(kEor(), pos);
        ende = (ende < 0) ? inhalt.size() : ende + kEor().size();

        // Zwei Treffer im selben Datensatz: er steht schon drin.
        if (anfang < hinterLetztem) { anfang = hinterLetztem; }
        if (ende > anfang) {
            aus.append(inhalt.mid(anfang, ende - anfang));
            if (!aus.endsWith('\n')) { aus.append('\n'); }
            hinterLetztem = ende;
        }
        pos = ende;
    }
    return aus;
}

Befund fasseZusammen(const QVector<LogEntry>& treffer, const QString& rufzeichen,
                     const QString& band, const QString& mode)
{
    Befund b;
    const QString ruf = rufzeichen.trimmed().toUpper();
    if (ruf.isEmpty()) { return b; }

    for (const LogEntry& e : treffer) {
        // Das Vorsieb war großzügig (Bytesuche über die ganze Datei).
        // Hier entscheidet das geparste Feld, nicht die Bytestelle.
        if (e.call.trimmed().toUpper() != ruf) { continue; }
        ++b.anzahl;

        if (e.timeOn.isValid()
            && (!b.zuletzt.isValid() || e.timeOn > b.zuletzt)) {
            b.zuletzt     = e.timeOn;
            b.letztesBand = e.band;
            b.letzterMode = e.mode;
        }

        // Leeres Band oder leere Betriebsart lassen die Flaggen falsch.
        // "Ich weiß es nicht" darf nicht als "schon gearbeitet" ankommen --
        // das ist der eine Fehler, der einen Kontakt verhindert, den es
        // noch nicht gibt.
        if (!band.isEmpty()
            && e.band.compare(band, Qt::CaseInsensitive) == 0) {
            b.gleichesBand = true;
            if (!mode.isEmpty()
                && e.mode.compare(mode, Qt::CaseInsensitive) == 0) {
                b.gleicherMode = true;
            }
        }
    }
    return b;
}

QVector<LogEntry> letzte(int n, const QString& pfad)
{
    QVector<LogEntry> aus;
    if (n <= 0) { return aus; }

    const QString p = pfad.isEmpty() ? LogbookDatei::pfad() : pfad;
    QFile f(p);
    // Eine fehlende Datei ist kein Fehler, sondern ein leeres Logbuch --
    // genau das hat eine frische Installation.
    if (!f.open(QIODevice::ReadOnly)) { return aus; }
    const qint64 groesse = f.size();
    if (groesse <= 0) { return aus; }

    // Die Datei bleibt chronologisch, ältester zuerst (LogbookWindow
    // sortiert vor dem Schreiben ausdrücklich so). Das Dateiende hält
    // also die jüngsten Kontakte. Sortiert wird hier trotzdem, weil ein
    // angehängter Eintrag mit korrigierter Zeit sonst an falscher Stelle
    // stünde.
    qint64 fenster = kErstesFenster;
    for (;;) {
        const bool ganz = fenster >= groesse;
        if (!f.seek(ganz ? 0 : groesse - fenster)) { return aus; }
        QByteArray stueck = f.read(ganz ? groesse : fenster);
        if (!ganz) { stueck = abDatensatzGrenze(stueck); }
        aus = AdifLog::parse(stueck);
        if (aus.size() >= n || ganz) { break; }
        fenster *= kFaktor;
    }

    juengsterZuerst(aus);
    if (aus.size() > n) { aus.resize(n); }
    return aus;
}

Befund rueckschau(const QString& rufzeichen, const QString& band,
                  const QString& mode, const QString& pfad)
{
    const QString ruf = rufzeichen.trimmed().toUpper();
    if (ruf.isEmpty()) { return {}; }

    const QString p = pfad.isEmpty() ? LogbookDatei::pfad() : pfad;
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) { return {}; }

    const QByteArray teil = datensaetzeMit(f.readAll(), ruf.toUtf8());
    return fasseZusammen(AdifLog::parse(teil), ruf, band, mode);
}

}  // namespace LogbuchRueckschau
}  // namespace Longpath
