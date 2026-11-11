// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Die drei Logbuch-Bilder (2026-11-08) ────────────────────────────
//
// Betreiber: „Alle drei" (Entwurfsblaetter werkzeug/logbuch-2026-11-08).
//   1. Bedarf: Zellen, Urteil, die Betriebsart des Funkgeraets (CWU = CW).
//   2. Band x Stunde: Zaehlung und Filter.
//   3. Rose: Peilung, Sektor, UKW nur mit Locator, KW mit Landesmitte.
//   4. Im Logbuch-Fenster: Tippen zeigt die Matrix, Esc nimmt sie weg;
//      die Statistik hat ihre zwei neuen Seiten und rechnet sie.

#include <QtTest/QtTest>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>

#include "FensterFrist.h"
#include "core/AdifLog.h"
#include "core/CtyDatParser.h"
#include "core/LogbuchBilder.h"
#include "core/TransverterSetup.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/LogbookWindow.h"
#include "gui/widgets/LogbuchBilderWidgets.h"
#include "models/LogEntry.h"

using namespace Longpath;
using LogbuchBilder::Bedarf;

namespace {
LogEntry qso(const QString& call, const QString& band, const QString& mode,
             const QString& grid, int stunde, bool bestaetigt = false)
{
    LogEntry e;
    e.call = call;
    e.band = band;
    e.mode = mode;
    e.gridSquare = grid;
    e.myGridSquare = QStringLiteral("JN67VV");
    e.timeOn = QDateTime(QDate(2026, 10, 1), QTime(stunde, 12), QTimeZone::UTC);
    if (bestaetigt) { e.extras.push_back({QStringLiteral("QSL_RCVD"), QStringLiteral("Y")}); }
    return e;
}
QVector<LogEntry> beispielLog()
{
    return {
        qso(QStringLiteral("DL1ABC"), QStringLiteral("20m"), QStringLiteral("SSB"), QStringLiteral("JO62"), 9, true),
        qso(QStringLiteral("DL2XYZ"), QStringLiteral("40m"), QStringLiteral("FT8"), QStringLiteral("JN58"), 18),
        qso(QStringLiteral("DL1ABC"), QStringLiteral("40m"), QStringLiteral("FT8"), QStringLiteral("JO62"), 19),
        qso(QStringLiteral("W1AW"),   QStringLiteral("20m"), QStringLiteral("SSB"), QStringLiteral("FN31"), 19),
        qso(QStringLiteral("K1ABC"),  QStringLiteral("15m"), QStringLiteral("SSB"), QString(), 19),  // KW ohne Locator
        qso(QStringLiteral("OE5ABC"), QStringLiteral("2m"),  QStringLiteral("FM"),  QString(), 10),  // UKW ohne Locator
        qso(QStringLiteral("OK1ABC"), QStringLiteral("2m"),  QStringLiteral("SSB"), QStringLiteral("JO70"), 7),
    };
}
} // namespace

class TstLogbuchBilder : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        const QString pfad = qEnvironmentVariable("LONGPATH_SOURCE_DIR") + QStringLiteral("/cty.dat");
        QVERIFY2(m_cty.loadFromFile(pfad), qPrintable(pfad));
    }

    void derBedarfUrteiltRichtig()
    {
        const QVector<LogEntry> log = beispielLog();
        const QString feld = QStringLiteral("JN67VV");
        // Auf 20 m SSB bestaetigt.
        Bedarf b = LogbuchBilder::bedarf(log, QStringLiteral("DL1ABC"), QStringLiteral("20m"),
                                         QStringLiteral("USB"), &m_cty, feld);
        QCOMPARE(b.landName, QStringLiteral("Fed. Rep. of Germany"));
        QCOMPARE(b.qsosLand, 3);
        QCOMPARE(b.qsosCall, 2);
        QCOMPARE(b.urteil, Bedarf::Urteil::Bestaetigt);
        QVERIFY(b.bestaetigt.contains(QStringLiteral("20m|PHONE")));
        QVERIFY(b.gearbeitet.contains(QStringLiteral("40m|DATA")));
        QVERIFY(b.diesesCall.contains(QStringLiteral("40m|DATA")));
        QVERIFY(b.lageBekannt && b.km > 300.0);
        // 15 m: neues Band.
        b = LogbuchBilder::bedarf(log, QStringLiteral("DL1ABC"), QStringLiteral("15m"),
                                  QStringLiteral("USB"), &m_cty, feld);
        QCOMPARE(b.urteil, Bedarf::Urteil::NeuesBand);
        // 20 m in CW -- in der Schreibweise des Funkgeraets "CWU".
        b = LogbuchBilder::bedarf(log, QStringLiteral("DL9QQ"), QStringLiteral("20m"),
                                  QStringLiteral("CWU"), &m_cty, feld);
        QCOMPARE(b.jetzt, QStringLiteral("20m|CW"));
        QCOMPARE(b.urteil, Bedarf::Urteil::NeueBetriebsart);
        QCOMPARE(b.qsosCall, 0);
        // 40 m FT8 gearbeitet, aber nicht bestaetigt.
        b = LogbuchBilder::bedarf(log, QStringLiteral("DL2XYZ"), QStringLiteral("40m"),
                                  QStringLiteral("DIGU"), &m_cty, feld);
        QCOMPARE(b.urteil, Bedarf::Urteil::Unbestaetigt);
        // Ein Land, das nicht im Log steht.
        b = LogbuchBilder::bedarf(log, QStringLiteral("JA1ABC"), QStringLiteral("20m"),
                                  QStringLiteral("USB"), &m_cty, feld);
        QCOMPARE(b.urteil, Bedarf::Urteil::NeuesLand);
        QCOMPARE(b.urteilText(), QStringLiteral("NEW ONE"));
    }

    void bandUndStundeZaehlen()
    {
        const LogbuchBilder::BandStunde s = LogbuchBilder::bandStunde(beispielLog());
        QCOMPARE(s.gesamt, 7);
        QCOMPARE(s.zeilen, (QStringList{QStringLiteral("40m"), QStringLiteral("20m"),
                                        QStringLiteral("15m"), QStringLiteral("2m")}));
        QCOMPARE(s.anzahl.value(QStringLiteral("40m"))[19], 1);
        QCOMPARE(s.anzahl.value(QStringLiteral("40m"))[18], 1);
        QCOMPARE(s.besteStunde(QStringLiteral("20m")), 9);   // gleich viele: die fruehere
        const LogbuchBilder::BandStunde nurUkw = LogbuchBilder::bandStunde(
            beispielLog(), [](const LogEntry& e) { return LogbuchBilder::istUkw(e.band); });
        QCOMPARE(nurUkw.gesamt, 2);
    }

    void dieRoseNimmtUkwNurMitLocator()
    {
        const LogbuchBilder::Rose r = LogbuchBilder::rose(beispielLog(), &m_cty, QStringLiteral("JN67VV"));
        QCOMPARE(r.ohneOrt, 1);          // OE5ABC auf 2 m ohne Locator
        QCOMPARE(r.ausLand, 1);          // K1ABC auf 15 m: an die Landesmitte (USA)
        QCOMPARE(r.punkte.size(), 6);
        // W1AW (FN31) liegt von JN67 aus bei gut 290 Grad.
        bool westen = false;
        for (const auto& q : r.punkte) { if (q.km > 6000.0 && q.grad > 280.0 && q.grad < 310.0) { westen = true; } }
        QVERIFY(westen);
        int a = -1, l = 0;
        r.groessteLuecke(&a, &l);
        QVERIFY(l > 10);                  // der Sueden ist leer
    }

    void imLogbuchFenster()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, beispielLog()));
        LogbookWindow w(pfad);
        w.setCtyDat(&m_cty);
        w.resize(1400, 900);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        // Tippen zeigt die Matrix (ohne Funkgeraet ohne Rahmen).
        QVERIFY(w.bedarfForTest());
        QVERIFY(!w.bedarfForTest()->isVisible());
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("dl1abc"));
        QTRY_VERIFY_WITH_TIMEOUT(w.bedarfForTest()->isVisible(), 2000);
        const Bedarf& b = w.bedarfForTest()->bedarf();
        QCOMPARE(b.call, QStringLiteral("DL1ABC"));
        QCOMPARE(b.qsosCall, 2);
        QVERIFY(b.jetzt.isEmpty());
        // Esc nimmt sie weg.
        QTest::keyClick(w.entryCallForTest(), Qt::Key_Escape);
        QVERIFY(!w.bedarfForTest()->isVisible());

        // Die Statistik: Band x Stunde und Richtungen rechnen.
        w.zeigeStatistikSeiteForTest(1);
        QVERIFY(w.bandStundeForTest());
        QCOMPARE(w.bandStundeForTest()->daten().gesamt, 7);
        w.zeigeStatistikSeiteForTest(2);
        QVERIFY(w.roseForTest());
        QCOMPARE(w.roseForTest()->rose().punkte.size(), 6);
    }

    // Am Transverter zeigt die Eingabezeile die Antenne: 28,300 heisst
    // 144,300 und "2m" -- vorher stand dort die ZF und "10m".
    void amTransverterZeigtDieZeileDieAntenne()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, beispielLog()));
        TransverterSetup t;
        t.enabled = true; t.ifLowHz = 28000000; t.ifHighHz = 32000000; t.offsetHz = 116000000;
        RadioModel radio;                          // laedt den Transverter aus der Datei ...
        TransverterSetup::setCurrent(t);           // ... darum erst danach setzen
        const int idx = radio.addSlice();
        radio.setActiveSlice(idx);
        radio.activeSlice()->setFrequency(28300000.0);
        LogbookWindow w(pfad);
        w.setRadio(&radio);
        QCOMPARE(w.entryFreqForTest()->text(), QStringLiteral("144.300.000"));
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("ok1abc"));
        w.aktualisiereBedarfForTest();
        TransverterSetup::setCurrent({});
    }

private:
    CtyDatParser m_cty;
};

QTEST_MAIN(TstLogbuchBilder)
#include "tst_logbuch_bilder.moc"
