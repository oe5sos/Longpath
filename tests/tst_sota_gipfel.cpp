// no-port-check: Longpath-original test, no Thetis logic.
//
// ── SOTA-Gipfel: Nachschlagen, Cache, Strecken (2026-11-11) ─────────
//
//   1. Referenzen normalisieren, URL bauen, die echte Antwort der API
//      fuer OE/OO-106 lesen (am 2026-11-11 aufgezeichnet).
//   2. Hoechstens einmal je Gipfel: zweimal fragen = eine Anfrage; der
//      Cache traegt in einen neuen Lauf, ohne Anfrage.
//   3. "Not found" (HTTP 400) wird gemerkt, auch ueber den Lauf hinaus;
//      ein Netzfehler nicht -- aber in diesem Lauf auch nicht wiederholt.
//   4. Die Strecke eines QSOs vom Gipfel: SN7L aus JO70SS vom
//      Gmundnerberg, nicht aus der Mitte von JN67VV.
// Kein Netz: der Transport ist eine Attrappe.

#include <QtTest/QtTest>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTimer>

#include "core/BeamHeading.h"
#include "core/Maidenhead.h"
#include "core/SotaGipfel.h"

using namespace Longpath;

namespace {

QByteArray aufzeichnung()
{
    QFile f(qEnvironmentVariable("LONGPATH_SOURCE_DIR")
            + QStringLiteral("/tests/data/hoehenprofil/sota-OE_OO-106.json"));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Antwortet wie api2.sota.org.uk: OE/OO-106 bekannt, sonst 400.
struct Attrappe {
    QVector<QUrl> gefragt;
    bool netzAus{false};
    HttpAbruf abruf()
    {
        return [this](const QUrl& url, std::function<void(const HttpAntwort&)> fertig) {
            gefragt << url;
            HttpAntwort a;
            if (netzAus) {
                a.netzFehler = QStringLiteral("Host unreachable");
            } else if (url.path().endsWith(QStringLiteral("/OE/OO-106"))) {
                a.status = 200;
                a.body = aufzeichnung();
            } else {
                a.status = 400;
                a.body = "Not found";
            }
            QTimer::singleShot(0, [fertig, a]() { fertig(a); });
        };
    }
};

LogEntry sn7l()
{
    LogEntry e;
    e.call = QStringLiteral("SN7L");
    e.band = QStringLiteral("2m");
    e.mode = QStringLiteral("SSB");
    e.freqMHz = 144.182;
    e.gridSquare = QStringLiteral("JO70SS");
    e.myGridSquare = QStringLiteral("JN67VV");
    e.mySotaRef = QStringLiteral("oe/oo-106");
    e.timeOn = QDateTime(QDate(2026, 7, 4), QTime(19, 26, 25), QTimeZone::UTC);
    return e;
}

} // namespace

class TstSotaGipfel : public QObject {
    Q_OBJECT
private slots:

    void referenzenUndAntwort()
    {
        QCOMPARE(SotaGipfelVerzeichnis::normalisiere(QStringLiteral(" oe/oo-106 ")),
                 QStringLiteral("OE/OO-106"));
        QCOMPARE(SotaGipfelVerzeichnis::normalisiere(QStringLiteral("W7A/AW-001")),
                 QStringLiteral("W7A/AW-001"));
        QCOMPARE(SotaGipfelVerzeichnis::normalisiere(QStringLiteral("VK3/VE-123")),
                 QStringLiteral("VK3/VE-123"));
        QVERIFY(SotaGipfelVerzeichnis::normalisiere(QStringLiteral("OE-123")).isEmpty());
        QVERIFY(SotaGipfelVerzeichnis::normalisiere(QStringLiteral("K-1234")).isEmpty());
        QVERIFY(SotaGipfelVerzeichnis::normalisiere(QString()).isEmpty());

        QCOMPARE(SotaGipfelVerzeichnis::anfrageUrl(QStringLiteral("OE/OO-106")).toString(),
                 QStringLiteral("https://api2.sota.org.uk/api/summits/OE/OO-106"));
        QVERIFY(!SotaGipfelVerzeichnis::anfrageUrl(QStringLiteral("JN67VV")).isValid());

        SotaGipfel g;
        QVERIFY(SotaGipfelVerzeichnis::leseAntwort(aufzeichnung(), g));
        QCOMPARE(g.ref, QStringLiteral("OE/OO-106"));
        QCOMPARE(g.name, QStringLiteral("Gmundnerberg"));
        QCOMPARE(g.altM, 884);
        QCOMPARE(g.lat, 47.9058);
        QCOMPARE(g.lon, 13.7353);
        QCOMPARE(g.locator, QStringLiteral("JN67UV"));          // "JN67uv" in der Antwort
        // Ohne "locator" in der Antwort: aus der Lage gerechnet.
        SotaGipfel ohne;
        QVERIFY(SotaGipfelVerzeichnis::leseAntwort(
            "{\"summitCode\":\"OE/OO-106\",\"latitude\":47.9058,\"longitude\":13.7353}", ohne));
        QCOMPARE(ohne.locator, QStringLiteral("JN67UV"));
        QVERIFY(!SotaGipfelVerzeichnis::leseAntwort("Not found", g));
        QVERIFY(!SotaGipfelVerzeichnis::leseAntwort("{\"summitCode\":\"OE/OO-106\"}", g));
    }

    void einmalJeGipfelUndCache()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("sota-summits.json"));
        Attrappe netz;
        {
            SotaGipfelVerzeichnis v(pfad);
            v.setAbruf(netz.abruf());
            v.setTakt(0);
            QSignalSpy bekannt(&v, &SotaGipfelVerzeichnis::gipfelBekannt);
            v.anfragen(QStringLiteral("OE/OO-106"));
            v.anfragen(QStringLiteral("oe/oo-106"));    // dieselbe, nicht noch einmal
            QVERIFY(v.wartet(QStringLiteral("OE/OO-106")));
            QVERIFY(bekannt.wait(2000));
            QVERIFY(!v.wartet(QStringLiteral("OE/OO-106")));
            v.anfragen(QStringLiteral("OE/OO-106"));    // steht jetzt im Cache
            QTest::qWait(50);
            QCOMPARE(netz.gefragt.size(), 1);
            QCOMPARE(v.abfragenGestellt(), 1);
            SotaGipfel g;
            QVERIFY(v.finde(QStringLiteral("OE/OO-106"), g));
            QCOMPARE(g.altM, 884);
        }
        QVERIFY(QFile::exists(pfad));
        {
            // Neuer Lauf: aus der Datei, keine Anfrage.
            SotaGipfelVerzeichnis v(pfad);
            v.setAbruf(netz.abruf());
            SotaGipfel g;
            QVERIFY(v.finde(QStringLiteral("OE/OO-106"), g));
            QCOMPARE(g.name, QStringLiteral("Gmundnerberg"));
            QCOMPARE(g.locator, QStringLiteral("JN67UV"));      // aus dem Cache
            v.anfragen(QStringLiteral("OE/OO-106"));
            QTest::qWait(50);
            QCOMPARE(netz.gefragt.size(), 1);
        }
    }

    void unbekanntBleibtUnbekannt()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("sota-summits.json"));
        Attrappe netz;
        {
            SotaGipfelVerzeichnis v(pfad);
            v.setAbruf(netz.abruf());
            v.setTakt(0);
            QSignalSpy fehler(&v, &SotaGipfelVerzeichnis::gipfelFehler);
            v.anfragen(QStringLiteral("OE/OO-999"));
            QVERIFY(fehler.wait(2000));
            QVERIFY(v.istUnbekannt(QStringLiteral("OE/OO-999")));
        }
        {
            SotaGipfelVerzeichnis v(pfad);
            v.setAbruf(netz.abruf());
            QVERIFY(v.istUnbekannt(QStringLiteral("oe/oo-999")));
            v.anfragen(QStringLiteral("OE/OO-999"));
            QTest::qWait(50);
            QCOMPARE(netz.gefragt.size(), 1);
        }
    }

    void netzfehlerNichtGemerktAberNichtWiederholt()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("sota-summits.json"));
        Attrappe netz;
        netz.netzAus = true;
        {
            SotaGipfelVerzeichnis v(pfad);
            v.setAbruf(netz.abruf());
            v.setTakt(0);
            QSignalSpy fehler(&v, &SotaGipfelVerzeichnis::gipfelFehler);
            v.anfragen(QStringLiteral("OE/OO-106"));
            QVERIFY(fehler.wait(2000));
            QVERIFY(!v.istUnbekannt(QStringLiteral("OE/OO-106")));
            v.anfragen(QStringLiteral("OE/OO-106"));
            QTest::qWait(50);
            QCOMPARE(netz.gefragt.size(), 1);
        }
        netz.netzAus = false;
        {
            // Naechster Start: neuer Versuch, der gelingt.
            SotaGipfelVerzeichnis v(pfad);
            v.setAbruf(netz.abruf());
            QSignalSpy bekannt(&v, &SotaGipfelVerzeichnis::gipfelBekannt);
            v.anfragen(QStringLiteral("OE/OO-106"));
            QVERIFY(bekannt.wait(2000));
            QCOMPARE(netz.gefragt.size(), 2);
        }
    }

    void warteschlangeNacheinander()
    {
        QTemporaryDir dir;
        Attrappe netz;
        SotaGipfelVerzeichnis v(dir.filePath(QStringLiteral("s.json")));
        v.setAbruf(netz.abruf());
        v.setTakt(0);
        QVector<LogEntry> log{sn7l(), sn7l(), sn7l()};
        log[1].mySotaRef = QStringLiteral("OE/OO-107");
        log[2].sotaRef = QStringLiteral("OE/OO-108");
        QSignalSpy fehler(&v, &SotaGipfelVerzeichnis::gipfelFehler);
        v.anfragenFuer(log);
        QCOMPARE(netz.gefragt.size(), 1);           // eine unterwegs, zwei warten
        QTRY_COMPARE_WITH_TIMEOUT(fehler.size(), 2, 3000);
        QCOMPARE(netz.gefragt.size(), 3);
    }

    void beiSotaGiltDerGipfelLocator()
    {
        // Martin 2026-11-10: „bei sota muss immer der locator der
        // sotareferenz angenommen werden".
        QTemporaryDir dir;
        Attrappe netz;
        SotaGipfelVerzeichnis v(dir.filePath(QStringLiteral("s.json")));
        v.setAbruf(netz.abruf());
        v.setTakt(0);
        LogEntry e = sn7l();                         // MY_GRIDSQUARE JN67VV (Tal)
        QCOMPARE(v.wendeGipfelLocatorAn(e), 0);      // Gipfel noch unbekannt
        QCOMPARE(e.myGridSquare, QStringLiteral("JN67VV"));

        QSignalSpy bekannt(&v, &SotaGipfelVerzeichnis::gipfelBekannt);
        v.anfragen(e.mySotaRef);
        QVERIFY(bekannt.wait(2000));
        QCOMPARE(v.gipfelLocator(QStringLiteral("oe/oo-106")), QStringLiteral("JN67UV"));
        QCOMPARE(v.wendeGipfelLocatorAn(e), 1);
        QCOMPARE(e.myGridSquare, QStringLiteral("JN67UV"));
        QCOMPARE(e.gridSquare, QStringLiteral("JO70SS"));   // kein SOTA_REF: bleibt
        QCOMPARE(v.wendeGipfelLocatorAn(e), 0);             // schon richtig

        // Ein genauerer Locator im selben Quadrat bleibt stehen.
        LogEntry genau = sn7l();
        genau.myGridSquare = QStringLiteral("jn67uv41");
        QCOMPARE(v.wendeGipfelLocatorAn(genau), 0);

        // Gipfel zu Gipfel: auch GRIDSQUARE der Gegenseite.
        LogEntry s2s = sn7l();
        s2s.sotaRef = QStringLiteral("OE/OO-106");
        QCOMPARE(v.wendeGipfelLocatorAn(s2s), 2);
        QCOMPARE(s2s.gridSquare, QStringLiteral("JN67UV"));

        // Ohne Gipfel: nichts.
        LogEntry heim = sn7l();
        heim.mySotaRef.clear();
        QCOMPARE(v.wendeGipfelLocatorAn(heim), 0);
        QCOMPARE(heim.myGridSquare, QStringLiteral("JN67VV"));
    }

    void streckeVomGipfel()
    {
        QTemporaryDir dir;
        Attrappe netz;
        SotaGipfelVerzeichnis v(dir.filePath(QStringLiteral("s.json")));
        v.setAbruf(netz.abruf());
        v.setTakt(0);

        QVector<LogEntry> log{sn7l()};
        double tlat = 0.0, tlon = 0.0, hlat = 0.0, hlon = 0.0;
        calculateLatLonFromGridSquare(QStringLiteral("JO70SS"), tlat, tlon);
        calculateLatLonFromGridSquare(QStringLiteral("JN67VV"), hlat, hlon);
        log[0].distanceKm = calculateDistanceKm(QStringLiteral("JN67VV"), QStringLiteral("JO70SS"));

        // Gipfel noch unbekannt: nichts geaendert, der Locator gilt.
        QCOMPARE(v.korrigiereStrecken(log), 0);
        SotaGipfelVerzeichnis::Ende a = v.eigenesEnde(log.at(0));
        QVERIFY(a.gueltig);
        QVERIFY(!a.gipfel);
        QCOMPARE(a.locator, QStringLiteral("JN67VV"));

        QSignalSpy bekannt(&v, &SotaGipfelVerzeichnis::gipfelBekannt);
        v.anfragenFuer(log);
        QVERIFY(bekannt.wait(2000));
        QCOMPARE(v.korrigiereStrecken(log), 1);
        const BeamHeading::GreatCircle soll =
            BeamHeading::greatCircle(47.9058, 13.7353, tlat, tlon);
        QVERIFY(std::abs(log.at(0).distanceKm - soll.km) < 1e-6);
        QVERIFY(std::abs(log.at(0).bearingDeg - soll.bearingDeg) < 1e-6);
        QVERIFY(std::abs(log.at(0).distanceKm - 344.39) < 0.05);   // wie im Entwurf

        a = v.eigenesEnde(log.at(0));
        QVERIFY(a.gipfel);
        QCOMPARE(a.g.altM, 884);
        const SotaGipfelVerzeichnis::Ende b = v.gegenEnde(log.at(0));
        QVERIFY(b.gueltig && !b.gipfel);
        QCOMPARE(b.locator, QStringLiteral("JO70SS"));

        // Gipfel zu Gipfel: die Gegenseite vom Gipfel, auch ohne Locator.
        LogEntry s2s = sn7l();
        s2s.gridSquare.clear();
        s2s.sotaRef = QStringLiteral("OE/OO-106");
        QVERIFY(v.gegenEnde(s2s).gipfel);
        // Ohne eigenen Gipfel und ohne Locator: der Ersatz.
        LogEntry ohne = sn7l();
        ohne.mySotaRef.clear();
        ohne.myGridSquare.clear();
        QVERIFY(!v.eigenesEnde(ohne).gueltig);
        QCOMPARE(v.eigenesEnde(ohne, QStringLiteral("JN67UV")).locator, QStringLiteral("JN67UV"));
    }
};

QTEST_GUILESS_MAIN(TstSotaGipfel)
#include "tst_sota_gipfel.moc"
