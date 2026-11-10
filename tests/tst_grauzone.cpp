// SPDX-License-Identifier: GPL-3.0-or-later
//
// Grauzone je QSO, SFI und Kp des Tages (2026-11-10).
//
// Betreiber: das Logbuch soll ein QSO erklaeren — „Grauzone und
// Ausbreitung je QSO, wenn es billig ist". Geprueft wird:
//
//   * die Sonnenhoehe gegen NOAA-Werte (unabhaengig gerechnet nach den
//     Formeln des NOAA-Solar-Calculator-Blatts, nicht aus SolarTimes),
//   * die Grenzen der Grauzone (−6° / +6°, beide eingeschlossen),
//   * woher die Orte kommen — Locator, Ersatzfeld, cty.dat-Landesmitte,
//     und dass die Landesmitte OST-positiv bleibt (ein zweites Minus
//     machte aus Tag in den USA Nacht in Asien),
//   * das Lesen der GFZ-Datei aus einem aufgezeichneten Ausschnitt (kein
//     Netz), mit den Spalten aus dem Dateikopf,
//   * Filter, Statistik und die Zeile in der Detailkarte.
//
// Der Messlauf am Ende laeuft nur mit LONGPATH_GRAUZONE_LOG (eine Kopie
// des echten Logs) und LONGPATH_GFZ_DATEI (die ganze GFZ-Datei).
// no-port-check: Longpath-original.

#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QTimeZone>

#include "core/AdifLog.h"
#include "core/CtyDatParser.h"
#include "core/GfzIndizes.h"
#include "core/Grauzone.h"
#include "core/LogFilter.h"
#include "core/LogbookStats.h"
#include "core/Maidenhead.h"
#include "gui/widgets/QsoDetailPane.h"

#include <cmath>

using namespace Longpath;
using Grauzone::Licht;

namespace {

QDateTime utc(int y, int mo, int d, int h, int mi)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi), QTimeZone::UTC);
}

LogEntry qso(const QString& call, const QDateTime& t,
             const QString& ihrFeld = QString(), const QString& meinFeld = QString(),
             const QString& band = QStringLiteral("40m"))
{
    LogEntry e;
    e.call = call;
    e.timeOn = t;
    e.gridSquare = ihrFeld;
    e.myGridSquare = meinFeld;
    e.band = band;
    e.mode = QStringLiteral("SSB");
    return e;
}

QString quelle(const QString& rel)
{
    return QStringLiteral(LONGPATH_SRC_DIR) + QLatin1Char('/') + rel;
}

QByteArray auszug()
{
    QFile f(quelle(QStringLiteral("tests/data/grauzone/gfz-auszug.txt")));
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    return f.readAll();
}

} // namespace

class TstGrauzone : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();

    void sonnenhoehe_gegen_noaa_data();
    void sonnenhoehe_gegen_noaa();
    void grenzen_der_grauzone();
    void beide_enden_aus_locatoren();
    void ersatzfeld_fuer_das_eigene_ende();
    void landesmitte_bleibt_ost_positiv();
    void ohne_zeit_kein_urteil();
    void kurztexte();

    void gfz_liest_den_ausschnitt();
    void gfz_kp_in_dritteln();
    void gfz_spalten_kommen_aus_dem_kopf();
    void gfz_fehlende_werte_bleiben_leer();
    void gfz_fremde_datei_ersetzt_nichts();
    void gfz_ohne_cache_keine_zahlen();

    void filter_grey_line();
    void statistik_zaehlt_grauzone();
    void detailkarte_zeigt_die_zeile();

    void messlauf_echtes_log();

private:
    CtyDatParser m_cty;
};

void TstGrauzone::initTestCase()
{
    QVERIFY2(m_cty.loadFromFile(quelle(QStringLiteral("cty.dat"))),
             qPrintable(quelle(QStringLiteral("cty.dat"))));
    QVERIFY(!auszug().isEmpty());
}

// ── Sonne ────────────────────────────────────────────────────────────

void TstGrauzone::sonnenhoehe_gegen_noaa_data()
{
    QTest::addColumn<QDateTime>("t");
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");
    QTest::addColumn<double>("noaa");

    // Geometrische Hoehe (ohne Brechung) nach dem NOAA-Blatt, am
    // 2026-11-10 in einem eigenen Skript gerechnet.
    QTest::newRow("Wien Mittag")    << utc(2026, 8, 7, 12, 0)   << 48.21  << 16.37   << 55.85;
    QTest::newRow("JN67 Dezember")  << utc(2026, 12, 21, 15, 30) << 47.9  << 13.8    << -3.01;
    QTest::newRow("New York Maerz") << utc(2026, 3, 20, 22, 0)  << 40.71  << -74.01  << 12.02;
    QTest::newRow("Sydney Juni")    << utc(2026, 6, 21, 7, 0)   << -33.87 << 151.21  << -1.97;
    QTest::newRow("Aequator")       << utc(2026, 3, 20, 12, 7)  << 0.0    << 0.0     << 89.88;
    QTest::newRow("Tokio Nacht")    << utc(2026, 1, 15, 15, 0)  << 35.68  << 139.69  << -75.22;
    QTest::newRow("JN67 Morgen")    << utc(2026, 11, 10, 6, 30) << 47.9   << 13.8    << 3.17;
}

void TstGrauzone::sonnenhoehe_gegen_noaa()
{
    QFETCH(QDateTime, t);
    QFETCH(double, lat);
    QFETCH(double, lon);
    QFETCH(double, noaa);
    const double h = Grauzone::sonnenhoehe(t, lat, lon);
    QVERIFY2(std::abs(h - noaa) < 0.5,
             qPrintable(QStringLiteral("%1 statt %2").arg(h).arg(noaa)));
}

void TstGrauzone::grenzen_der_grauzone()
{
    QCOMPARE(Grauzone::kGrenzeGrad, 6.0);
    QCOMPARE(Grauzone::licht(6.0),     Licht::Grauzone);
    QCOMPARE(Grauzone::licht(-6.0),    Licht::Grauzone);
    QCOMPARE(Grauzone::licht(0.0),     Licht::Grauzone);
    QCOMPARE(Grauzone::licht(6.001),   Licht::Tag);
    QCOMPARE(Grauzone::licht(-6.001),  Licht::Nacht);
    QCOMPARE(Grauzone::licht(45.0),    Licht::Tag);
    QCOMPARE(Grauzone::licht(-45.0),   Licht::Nacht);
    QCOMPARE(Grauzone::licht(std::nan("")), Licht::Unbekannt);
}

void TstGrauzone::beide_enden_aus_locatoren()
{
    // 21.12., 15:30 UTC: JN67 kurz nach Sonnenuntergang (−2,3°), FN31
    // am Vormittag (+22°).
    const LogEntry e = qso(QStringLiteral("W1AW"), utc(2026, 12, 21, 15, 30),
                           QStringLiteral("FN31"), QStringLiteral("JN67"));
    const Grauzone::QsoLicht l = Grauzone::fuer(e, QString(), &m_cty);
    QCOMPARE(l.hier.herkunft, Grauzone::Herkunft::Locator);
    QCOMPARE(l.dort.herkunft, Grauzone::Herkunft::Locator);
    QCOMPARE(l.hier.licht, Licht::Grauzone);
    QCOMPARE(l.dort.licht, Licht::Tag);
    QVERIFY(l.grauzonenQso());
    QCOMPARE(Grauzone::kurztext(l), QStringLiteral("Grey line at your end"));
}

void TstGrauzone::ersatzfeld_fuer_das_eigene_ende()
{
    const LogEntry ohne = qso(QStringLiteral("W1AW"), utc(2026, 12, 21, 15, 30),
                              QStringLiteral("FN31"));
    // Kein MY_GRIDSQUARE, kein Ersatz: mein Ende bleibt unbekannt.
    QVERIFY(!Grauzone::fuer(ohne, QString(), &m_cty).hier.bekannt());
    // Mit Ersatz: das Ersatzfeld.
    const Grauzone::QsoLicht l = Grauzone::fuer(ohne, QStringLiteral("JN67"), &m_cty);
    QCOMPARE(l.hier.woher, QStringLiteral("JN67"));
    QCOMPARE(l.hier.licht, Licht::Grauzone);
    // MY_GRIDSQUARE geht dem Ersatz vor.
    LogEntry mit = ohne;
    mit.myGridSquare = QStringLiteral("FN31");
    QCOMPARE(Grauzone::fuer(mit, QStringLiteral("JN67"), &m_cty).hier.woher,
             QStringLiteral("FN31"));
}

void TstGrauzone::landesmitte_bleibt_ost_positiv()
{
    // 10.11., 22:00 UTC, kein Locator: USA-Mitte (37,6 N / 91,9 W) hat
    // Nachmittag (+9,9°). Gespiegelt nach 91,9 OST waere es dort 04 Uhr
    // nachts (−29°) — genau der Fehler, den feldDerLandesmitte hatte.
    const LogEntry e = qso(QStringLiteral("W1AW"), utc(2026, 11, 10, 22, 0),
                           QString(), QStringLiteral("JN67"));
    const Grauzone::QsoLicht l = Grauzone::fuer(e, QString(), &m_cty);
    QCOMPARE(l.dort.herkunft, Grauzone::Herkunft::Landesmitte);
    QCOMPARE(l.dort.woher, QStringLiteral("United States"));
    QVERIFY2(l.dort.lon < -85.0 && l.dort.lon > -100.0, qPrintable(QString::number(l.dort.lon)));
    QCOMPARE(l.dort.licht, Licht::Tag);
    QCOMPARE(l.hier.licht, Licht::Nacht);
    QCOMPARE(Grauzone::kurztext(l), QStringLiteral("Night here, day there"));

    // Ohne cty.dat und ohne Locator: das andere Ende ist unbekannt, nicht
    // irgendwo.
    const Grauzone::QsoLicht ohne = Grauzone::fuer(e, QString(), nullptr);
    QVERIFY(!ohne.dort.bekannt());
    QCOMPARE(Grauzone::kurztext(ohne), QStringLiteral("Night at your end"));
}

void TstGrauzone::ohne_zeit_kein_urteil()
{
    const LogEntry e = qso(QStringLiteral("W1AW"), QDateTime(),
                           QStringLiteral("FN31"), QStringLiteral("JN67"));
    const Grauzone::QsoLicht l = Grauzone::fuer(e, QString(), &m_cty);
    QCOMPARE(l.hier.licht, Licht::Unbekannt);
    QCOMPARE(l.dort.licht, Licht::Unbekannt);
    QVERIFY(!l.grauzonenQso());
    QVERIFY(Grauzone::kurztext(l).isEmpty());
}

void TstGrauzone::kurztexte()
{
    auto mit = [](Licht h, Licht d) {
        Grauzone::QsoLicht l;
        l.hier.licht = h;
        l.dort.licht = d;
        return Grauzone::kurztext(l);
    };
    QCOMPARE(mit(Licht::Grauzone, Licht::Grauzone), QStringLiteral("Grey line at both ends"));
    QCOMPARE(mit(Licht::Tag, Licht::Grauzone),      QStringLiteral("Grey line at their end"));
    QCOMPARE(mit(Licht::Grauzone, Licht::Nacht),    QStringLiteral("Grey line at your end"));
    QCOMPARE(mit(Licht::Tag, Licht::Tag),           QStringLiteral("Daylight at both ends"));
    QCOMPARE(mit(Licht::Nacht, Licht::Nacht),       QStringLiteral("Dark at both ends"));
    QCOMPARE(mit(Licht::Tag, Licht::Nacht),         QStringLiteral("Day here, night there"));
    QCOMPARE(mit(Licht::Unbekannt, Licht::Tag),     QStringLiteral("Day at their end"));
    QVERIFY(mit(Licht::Unbekannt, Licht::Unbekannt).isEmpty());
}

// ── GFZ ──────────────────────────────────────────────────────────────

void TstGrauzone::gfz_liest_den_ausschnitt()
{
    GfzIndizes g;
    QCOMPARE(g.lies(auszug()), 14);
    QCOMPARE(g.ersterTag(), QDate(1947, 2, 14));
    QCOMPARE(g.letzterTag(), QDate(2026, 11, 9));

    // Halloween-Sturm 2003: Kp 9 von 06 bis 09 UT, F10.7obs 291,7.
    const GfzTag* t = g.tag(QDate(2003, 10, 29));
    QVERIFY(t);
    QCOMPARE(t->kp[2], 9.0);
    QCOMPARE(t->ap, 204);
    QCOMPARE(t->sn, 250);
    QCOMPARE(t->f107adj, 287.7);
    QCOMPARE(g.sfi(QDate(2003, 10, 29)), 291.7);
    QCOMPARE(g.kp(utc(2003, 10, 29, 7, 30)), 9.0);
    QCOMPARE(g.kurztext(utc(2003, 10, 29, 7, 30)), QStringLiteral("SFI 292 · Kp 9"));

    // Das letzte Fenster des Tages ist Kp8 (21–24 UT).
    QCOMPARE(g.kp(utc(2026, 11, 8, 23, 59)), 2.667);
    QCOMPARE(g.kurztext(utc(2026, 11, 8, 23, 59)), QStringLiteral("SFI 128 · Kp 3-"));
    // Nicht im Ausschnitt: nichts, auch kein „0".
    QVERIFY(g.kurztext(utc(2026, 11, 10, 12, 0)).isEmpty());
    QVERIFY(g.hinweis(utc(2003, 10, 29, 7, 30)).contains(QStringLiteral("06–09 UT")));
}

void TstGrauzone::gfz_kp_in_dritteln()
{
    QCOMPARE(GfzIndizes::kpText(0.0),   QStringLiteral("0"));
    QCOMPARE(GfzIndizes::kpText(0.333), QStringLiteral("0+"));
    QCOMPARE(GfzIndizes::kpText(0.667), QStringLiteral("1-"));
    QCOMPARE(GfzIndizes::kpText(2.333), QStringLiteral("2+"));
    QCOMPARE(GfzIndizes::kpText(3.0),   QStringLiteral("3"));
    QCOMPARE(GfzIndizes::kpText(8.667), QStringLiteral("9-"));
    QCOMPARE(GfzIndizes::kpText(9.0),   QStringLiteral("9"));
    QVERIFY(GfzIndizes::kpText(-1.0).isEmpty());
}

void TstGrauzone::gfz_spalten_kommen_aus_dem_kopf()
{
    // Dieselbe Zeile, aber F10.7obs und F10.7adj im Kopf UND in den Daten
    // vertauscht: gelesen wird nach Namen, nicht nach Position.
    const QByteArray text =
        "#YYY MM DD  days  days_m  Bsr dB    Kp1    Kp2    Kp3    Kp4    Kp5    Kp6    Kp7    Kp8"
        "  ap1  ap2  ap3  ap4  ap5  ap6  ap7  ap8    Ap  SN F10.7adj F10.7obs D\n"
        "2003 10 29 26234 26234.5 2323 27  4.667  4.000  9.000  8.000  7.667  7.667  8.667  8.667"
        "   39   27  400  207  179  179  300  300   204 250    287.7    291.7 2\n";
    GfzIndizes g;
    QCOMPARE(g.lies(text), 1);
    QCOMPARE(g.sfi(QDate(2003, 10, 29)), 291.7);
}

void TstGrauzone::gfz_fehlende_werte_bleiben_leer()
{
    GfzIndizes g;
    QVERIFY(g.lies(auszug()) > 0);
    // 15.02.1947: F10.7 −1.0 im Original — nur Kp.
    QVERIFY(std::isnan(g.sfi(QDate(1947, 2, 15))));
    QCOMPARE(g.kurztext(utc(1947, 2, 15, 1, 0)), QStringLiteral("Kp 1-"));

    // Ein Kp-Fenster mit −1.000 (so meldet das GFZ fehlende Werte): kein
    // Kp, aber der Fluss.
    QByteArray text = auszug();
    text += "2026 11 10 34647 34647.5 2635 16  1.000 -1.000 -1.000 -1.000 -1.000 -1.000 -1.000 -1.000"
            "    4   -1   -1   -1   -1   -1   -1   -1    -1  -1    130.0    127.5 0\n";
    QVERIFY(g.lies(text) > 0);
    QCOMPARE(g.kurztext(utc(2026, 11, 10, 1, 0)), QStringLiteral("SFI 130 · Kp 1"));
    QCOMPARE(g.kurztext(utc(2026, 11, 10, 12, 0)), QStringLiteral("SFI 130"));
}

void TstGrauzone::gfz_fremde_datei_ersetzt_nichts()
{
    GfzIndizes g;
    QVERIFY(g.lies(auszug()) > 0);
    // Die 301-Seite, die kp.gfz-potsdam.de wirklich liefert.
    QCOMPARE(g.lies("<html><head><title>301 Moved Permanently</title></head></html>"), 0);
    // Daten ohne Kopf: Spalten unbekannt, also nichts.
    QCOMPARE(g.lies("2003 10 29 26234 26234.5 2323 27  4.667  4.000  9.000\n"), 0);
    QCOMPARE(g.anzahl(), 14);
    QCOMPARE(g.sfi(QDate(2003, 10, 29)), 291.7);
}

void TstGrauzone::gfz_ohne_cache_keine_zahlen()
{
    QTemporaryDir dir;
    GfzQuelle& q = GfzQuelle::instanz();
    q.setCachePfad(dir.filePath(QStringLiteral("gibt-es-nicht.txt")));
    QVERIFY(q.daten().leer());
    QVERIFY(q.daten().kurztext(utc(2003, 10, 29, 7, 30)).isEmpty());
    // Kein Abruf aus dem Pruefstand: es entsteht auch keine Versuchsmarke.
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("gibt-es-nicht.txt.versuch"))));

    q.setCachePfad(quelle(QStringLiteral("tests/data/grauzone/gfz-auszug.txt")));
    QCOMPARE(q.daten().anzahl(), 14);
}

// ── Logbuch ─────────────────────────────────────────────────────────

void TstGrauzone::filter_grey_line()
{
    const LogEntry grau  = qso(QStringLiteral("W1AW"), utc(2026, 12, 21, 15, 30),
                               QStringLiteral("FN31"));           // JN67 −2,3°
    const LogEntry nacht = qso(QStringLiteral("W1AW"), utc(2026, 11, 10, 22, 0),
                               QString());                        // USA Tag, JN67 Nacht
    LogFilter f;
    QVERIFY(f.matches(grau) && f.matches(nacht));
    f.greyLineOnly = true;
    QVERIFY(f.isActive());
    // Ohne eigenes Feld kennt der Filter mein Ende nicht: FN31 hat Tag.
    QVERIFY(!f.matches(grau));
    f.greyLineHome = QStringLiteral("JN67");
    f.greyLineCty = &m_cty;
    QVERIFY(f.matches(grau));
    QVERIFY(!f.matches(nacht));
}

void TstGrauzone::statistik_zaehlt_grauzone()
{
    const QVector<LogEntry> log = {
        qso(QStringLiteral("W1AW"), utc(2026, 12, 21, 15, 30), QStringLiteral("FN31")),
        qso(QStringLiteral("W1AW"), utc(2026, 11, 10, 22, 0)),
        qso(QStringLiteral("DL1ABC"), QDateTime()),
    };
    const LogbookStats s = LogbookStats::compute(log, &m_cty, utc(2026, 12, 31, 0, 0),
                                                 QStringLiteral("JN67"));
    QCOMPARE(s.greyLine, 1);
    QCOMPARE(s.greyLineJudged, 2);    // das ohne Zeit zaehlt nicht mit
}

void TstGrauzone::detailkarte_zeigt_die_zeile()
{
    GfzQuelle::instanz().setCachePfad(quelle(QStringLiteral("tests/data/grauzone/gfz-auszug.txt")));

    QsoDetailPane pane;
    pane.setCty(&m_cty);
    pane.setMeinFeldErsatz(QStringLiteral("JN67"));
    auto* zeile = pane.findChild<QLabel*>(QStringLiteral("qsoLicht"));
    QVERIFY(zeile);

    // 05.11.2026, 06:30 UTC: JN67 +4,1° (Grauzone), FN31 −54° (Nacht);
    // GFZ: F10.7obs 123,7, Kp 06–09 UT = 1.000.
    pane.setEntry(qso(QStringLiteral("W1AW"), utc(2026, 11, 5, 6, 30), QStringLiteral("FN31")));
    QCOMPARE(zeile->text(), QStringLiteral("Grey line at your end · SFI 124 · Kp 1"));
    QVERIFY(!zeile->isHidden());
    QVERIFY(zeile->toolTip().contains(QStringLiteral("locator JN67")));
    QVERIFY(zeile->toolTip().contains(QStringLiteral("locator FN31")));

    // Ein Tag, den die Datei nicht hat: nur das Licht, keine erfundenen
    // Zahlen.
    pane.setEntry(qso(QStringLiteral("W1AW"), utc(2026, 11, 10, 22, 0)));
    QCOMPARE(zeile->text(), QStringLiteral("Night here, day there"));

    // Eine nachgeschlagene Station ohne QSO: keine Zeile.
    pane.showCallsign(QStringLiteral("W1AW"));
    QVERIFY(zeile->isHidden());
    pane.clearEntry();
    QVERIFY(zeile->isHidden());
}

// ── Messlauf am echten Log (nur mit Umgebungsvariablen) ─────────────

void TstGrauzone::messlauf_echtes_log()
{
    const QString logPfad = qEnvironmentVariable("LONGPATH_GRAUZONE_LOG");
    const QString gfzPfad = qEnvironmentVariable("LONGPATH_GFZ_DATEI");
    if (logPfad.isEmpty()) { QSKIP("LONGPATH_GRAUZONE_LOG nicht gesetzt"); }

    QString fehler;
    QVector<LogEntry> log = AdifLog::read(logPfad, &fehler);
    QVERIFY2(fehler.isEmpty(), qPrintable(fehler));
    std::stable_sort(log.begin(), log.end(), [](const LogEntry& a, const LogEntry& b) {
        return a.timeOn > b.timeOn;
    });
    QString meinFeld;                      // wie LogbookWindow::meinFeld()
    for (const LogEntry& e : log) {
        if (isValidGridSquare(e.myGridSquare.trimmed())) { meinFeld = e.myGridSquare.trimmed(); break; }
    }
    GfzIndizes gfz;
    if (!gfzPfad.isEmpty()) {
        QFile f(gfzPfad);
        if (f.open(QIODevice::ReadOnly)) { gfz.lies(f.readAll()); }
    }

    int beurteilt = 0, grau = 0, hier = 0, dort = 0, beide = 0;
    int ausLocator = 0, ausLand = 0, unbekannt = 0, mitWetter = 0;
    int dortGrauLocator = 0, dortGrauLand = 0;
    QMap<QString, QPair<int, int>> jeBand;     // Band -> (grau, beurteilt)
    QMap<int, int> jeStunde;
    for (const LogEntry& e : log) {
        const Grauzone::QsoLicht l = Grauzone::fuer(e, meinFeld, &m_cty);
        if (l.dort.herkunft == Grauzone::Herkunft::Locator) { ++ausLocator; }
        else if (l.dort.herkunft == Grauzone::Herkunft::Landesmitte) { ++ausLand; }
        else { ++unbekannt; }
        if (!gfz.kurztext(e.timeOn).isEmpty()) { ++mitWetter; }
        if (l.hier.licht == Licht::Unbekannt && l.dort.licht == Licht::Unbekannt) { continue; }
        ++beurteilt;
        auto& b = jeBand[e.band.trimmed().toLower()];
        ++b.second;
        if (!l.grauzonenQso()) { continue; }
        ++grau;
        ++b.first;
        ++jeStunde[e.timeOn.toUTC().time().hour()];
        const bool gh = l.hier.licht == Licht::Grauzone;
        const bool gd = l.dort.licht == Licht::Grauzone;
        if (gh && gd) { ++beide; } else if (gh) { ++hier; } else { ++dort; }
        if (gd) {
            ++(l.dort.herkunft == Grauzone::Herkunft::Locator ? dortGrauLocator : dortGrauLand);
        }
    }
    qInfo().noquote() << QStringLiteral("QSOs %1, mein Feld %2, beurteilt %3, Grauzone %4 (hier %5, dort %6, beide %7)")
                             .arg(log.size()).arg(meinFeld).arg(beurteilt).arg(grau).arg(hier).arg(dort).arg(beide);
    qInfo().noquote() << QStringLiteral("anderes Ende: Locator %1, Landesmitte %2, unbekannt %3; mit SFI/Kp %4")
                             .arg(ausLocator).arg(ausLand).arg(unbekannt).arg(mitWetter);
    qInfo().noquote() << QStringLiteral("Grauzone am anderen Ende: aus Locator %1, aus Landesmitte %2")
                             .arg(dortGrauLocator).arg(dortGrauLand);
    for (auto it = jeBand.cbegin(); it != jeBand.cend(); ++it) {
        qInfo().noquote() << QStringLiteral("  %1: %2 von %3").arg(it.key(), -5)
                                 .arg(it.value().first).arg(it.value().second);
    }
    QStringList stunden;
    for (auto it = jeStunde.cbegin(); it != jeStunde.cend(); ++it) {
        stunden << QStringLiteral("%1h:%2").arg(it.key(), 2, 10, QLatin1Char('0')).arg(it.value());
    }
    qInfo().noquote() << QStringLiteral("  Grauzone je UTC-Stunde: ") + stunden.join(QLatin1Char(' '));
}

QTEST_MAIN(TstGrauzone)
#include "tst_grauzone.moc"
