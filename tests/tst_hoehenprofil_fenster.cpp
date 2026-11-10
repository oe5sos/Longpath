// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Hoehenprofil und SOTA-Nachtrag in der Bedienung (2026-11-11) ────
//
//   1. "Profile" in der Stationskarte: nur fuer 2 m / 70 cm / 23 cm,
//      gesperrt ohne Lage der Gegenstation, weg fuer Stationen, die
//      nicht im Log stehen.
//   2. Mit MY_SOTA_REF: der Gipfel wird nachgeschlagen, die Peilung der
//      Karte kommt vom Gipfel; ein echter Klick auf "Profile" oeffnet das
//      Fenster, das mit aufgezeichneten Hoehen (Cache) "No line of sight"
//      zeigt, Antenne am Gipfel 5 m, Gegenstation 10 m; eine geaenderte
//      Antenne rechnet neu.
//   3. Unbekannter Gipfel -> Locator mit Hinweis; kein Netz -> Satz statt
//      Bild.
//   4. Logbuch: Knopf "SOTA…" da; Anwenden sichert die Datei zuerst und
//      schreibt MY_SOTA_REF; Entfernung danach vom Gipfel.
//   5. Nur mit LONGPATH_GRAB_DIR (+ LONGPATH_LOGBOOK_FIXTURE, optional
//      LONGPATH_HOEHEN_NETZ=1): die drei Wege des Entwurfsblatts aus dem
//      echten Log als PNG. Sonst uebersprungen.
// Netz nur in 5 und nur auf Wunsch; sonst Attrappen und Cache.

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTimer>

#include "FensterFrist.h"
#include "core/AdifLog.h"
#include "core/AppSettings.h"
#include "core/BeamHeading.h"
#include "core/HoehenDaten.h"
#include "core/Hoehenprofil.h"
#include "core/Maidenhead.h"
#include "core/SotaGipfel.h"
#include "core/SotaNachtrag.h"
#include "gui/HoehenprofilFenster.h"
#include "gui/LogbookWindow.h"
#include "gui/SotaNachtragDialog.h"
#include "gui/widgets/HoehenprofilWidget.h"
#include "gui/widgets/QsoDetailPane.h"

using namespace Longpath;

namespace {

QString daten(const QString& name)
{
    return qEnvironmentVariable("LONGPATH_SOURCE_DIR")
           + QStringLiteral("/tests/data/hoehenprofil/") + name;
}

QByteArray lies(const QString& pfad)
{
    QFile f(pfad);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// SOTA-API als Attrappe: OE/OO-106 aus der Aufzeichnung, sonst 400.
HttpAbruf sotaAttrappe(int* zaehler = nullptr)
{
    return [zaehler](const QUrl& url, std::function<void(const HttpAntwort&)> fertig) {
        if (zaehler) { ++*zaehler; }
        HttpAntwort a;
        if (url.path().endsWith(QStringLiteral("/OE/OO-106"))) {
            a.status = 200;
            a.body = lies(daten(QStringLiteral("sota-OE_OO-106.json")));
        } else {
            a.status = 400;
            a.body = "Not found";
        }
        QTimer::singleShot(0, [fertig, a]() { fertig(a); });
    };
}

// Ein echter Transport ohne Pruefmodus-Tor -- nur fuer Teil 5 auf Wunsch.
HttpAbruf echtesNetz(QObject* besitzer)
{
    auto* nam = new QNetworkAccessManager(besitzer);
    return [nam](const QUrl& url, std::function<void(const HttpAntwort&)> fertig) {
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Longpath"));
        QNetworkReply* r = nam->get(req);
        QObject::connect(r, &QNetworkReply::finished, r, [r, fertig]() {
            r->deleteLater();
            HttpAntwort a;
            a.status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            a.body = r->readAll();
            if (a.status == 0) { a.netzFehler = r->errorString(); }
            fertig(a);
        });
    };
}

QVector<double> aufgezeichnet(const QString& call)
{
    QVector<double> h;
    for (const QJsonValue& v :
         QJsonDocument::fromJson(lies(daten(QStringLiteral("hoehe-%1-gipfel.json").arg(call)))).array()) {
        h << v.toDouble();
    }
    return h;
}

// Die 100 aufgezeichneten Punkte auf die Punktzahl des Fensters strecken.
QVector<double> gestreckt(const QVector<double>& h, int n)
{
    QVector<double> aus;
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i) * (h.size() - 1) / (n - 1);
        const int j = std::min(static_cast<int>(x), static_cast<int>(h.size()) - 2);
        aus << h.at(j) + (x - j) * (h.at(j + 1) - h.at(j));
    }
    return aus;
}

// Den Cache des Fensters fuer den Weg fuellen, den es fuer `e` holen wird.
void cacheFuer(const LogEntry& e, const QVector<double>& hundert)
{
    SotaGipfelVerzeichnis& v = SotaGipfelVerzeichnis::instanz();
    const SotaGipfelVerzeichnis::Ende a = v.eigenesEnde(e);
    const SotaGipfelVerzeichnis::Ende b = v.gegenEnde(e);
    const double km = Hoehenprofil::grosskreisKm(a.lat, a.lon, b.lat, b.lon);
    const HoehenAbruf::Weg w{a.lat, a.lon, b.lat, b.lon, Hoehenprofil::punktzahlFuer(km)};
    HoehenAbruf abruf;
    QVERIFY(abruf.inCacheLegen(w, gestreckt(hundert, w.punkte)));
}

LogEntry zweiMeter(const QString& call, const QString& grid, int tag, int monat, int h, int m)
{
    LogEntry e;
    e.call = call;
    e.band = QStringLiteral("2m");
    e.mode = QStringLiteral("SSB");
    e.freqMHz = 144.180;
    e.gridSquare = grid;
    e.myGridSquare = QStringLiteral("JN67VV");
    e.timeOn = QDateTime(QDate(2026, monat, tag), QTime(h, m), QTimeZone::UTC);
    return e;
}

void klick(QWidget* w)
{
    const QPointF p(w->width() / 2.0, w->height() / 2.0);
    const QPointF g = w->mapToGlobal(p);
    QMouseEvent druck(QEvent::MouseButtonPress, p, g, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, p, g, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &los);
}

} // namespace

class TstHoehenprofilFenster : public QObject {
    Q_OBJECT
private slots:

    void initTestCase()
    {
        SotaGipfelVerzeichnis::instanz().setAbruf(sotaAttrappe());
        SotaGipfelVerzeichnis::instanz().setTakt(0);
    }

    void profilKnopfNurFuerUkw()
    {
        QsoDetailPane pane;
        pane.resize(320, 700);
        QPushButton* b = pane.profilButtonForTest();
        QVERIFY(b);

        LogEntry kw = zweiMeter(QStringLiteral("IZ7HNO"), QStringLiteral("JN80OL"), 4, 7, 9, 56);
        kw.band = QStringLiteral("20m");
        kw.freqMHz = 14.2;
        pane.setEntry(kw);
        QVERIFY(b->isHidden());

        pane.setEntry(zweiMeter(QStringLiteral("OK1AME"), QStringLiteral("JN69RI"), 4, 7, 19, 9));
        QVERIFY(!b->isHidden());
        QVERIFY(b->isEnabled());

        LogEntry ohne = zweiMeter(QStringLiteral("OE5PEN/P"), QString(), 21, 6, 9, 39);
        pane.setEntry(ohne);
        QVERIFY(!b->isHidden());
        QVERIFY(!b->isEnabled());
        QVERIFY(b->toolTip().contains(QStringLiteral("GRIDSQUARE")));

        LogEntry siebzig = zweiMeter(QStringLiteral("OE1CJG"), QStringLiteral("JN88EE"), 1, 1, 1, 1);
        siebzig.band = QStringLiteral("70cm");
        siebzig.freqMHz = 438.15;
        pane.setEntry(siebzig);
        QVERIFY(!b->isHidden());

        pane.showCallsign(QStringLiteral("OK1AME"));
        QVERIFY(b->isHidden());
    }

    void profilVomGipfel()
    {
        LogEntry e = zweiMeter(QStringLiteral("SN7L"), QStringLiteral("JO70SS"), 4, 7, 19, 26);
        e.mySotaRef = QStringLiteral("OE/OO-106");

        QsoDetailPane pane;
        pane.resize(320, 700);
        pane.show();
        QSignalSpy peilung(&pane, &QsoDetailPane::beamChanged);
        QSignalSpy bekannt(&SotaGipfelVerzeichnis::instanz(),
                           &SotaGipfelVerzeichnis::gipfelBekannt);
        pane.setEntry(e);
        SotaGipfel g;
        if (!SotaGipfelVerzeichnis::instanz().finde(e.mySotaRef, g)) {
            QVERIFY(bekannt.wait(kFensterFrist));
        }
        // Die Peilung der Karte kommt jetzt vom Gipfel, nicht aus JN67VV.
        double tlat = 0.0, tlon = 0.0;
        calculateLatLonFromGridSquare(QStringLiteral("JO70SS"), tlat, tlon);
        const BeamHeading::GreatCircle soll = BeamHeading::greatCircle(47.9058, 13.7353, tlat, tlon);
        QVERIFY(!peilung.isEmpty());
        QVERIFY2(std::abs(peilung.last().at(0).toDouble() - soll.bearingDeg) < 1e-6,
                 qPrintable(QString::number(peilung.last().at(0).toDouble(), 'f', 4)));

        cacheFuer(e, aufgezeichnet(QStringLiteral("SN7L")));
        klick(pane.profilButtonForTest());
        HoehenprofilFenster* f = pane.profilFensterForTest();
        QVERIFY2(f, "Profile button did not open a window");
        QVERIFY(f->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(f->profil()->hatProfil(), kFensterFrist);

        QCOMPARE(f->eigeneAntenne()->value(), 5.0);    // portabel am Gipfel
        QCOMPARE(f->gegenAntenne()->value(), 10.0);
        QCOMPARE(f->weg().punkte, 300);
        const Hoehenprofil::Ergebnis erg = f->profil()->ergebnis();   // Kopie
        QCOMPARE(erg.urteil, Hoehenprofil::Urteil::KeineSicht);
        QVERIFY(erg.hatHorizont);
        // Am Gipfel die Hoehe der SOTA-Datenbank, nicht des Modells (874).
        QCOMPARE(erg.hoeheA, 884.0 + 5.0);
        QVERIFY(std::abs(erg.gesamtKm - 344.39) < 0.05);
        QCOMPARE(f->statusText(), QStringLiteral("No line of sight"));

        // Eine andere Antenne rechnet neu, ohne neu zu holen.
        f->gegenAntenne()->setValue(30.0);
        QVERIFY(std::abs(f->profil()->ergebnis().hoeheB - erg.hoeheB - 20.0) < 1e-9);

        // Zweiter Druck auf einem anderen QSO: dasselbe Fenster.
        LogEntry pch = zweiMeter(QStringLiteral("OE5PCH"), QStringLiteral("JN68XF"), 21, 6, 11, 19);
        pch.mySotaRef = QStringLiteral("OE/OO-106");
        pch.mode = QStringLiteral("FM");
        cacheFuer(pch, aufgezeichnet(QStringLiteral("OE5PCH")));
        pane.setEntry(pch);
        klick(pane.profilButtonForTest());
        QCOMPARE(pane.profilFensterForTest(), f);
        QTRY_VERIFY_WITH_TIMEOUT(f->profil()->hatProfil()
                                 && std::abs(f->profil()->ergebnis().gesamtKm - 39.59) < 0.05,
                                 kFensterFrist);
        QVERIFY(f->windowTitle().contains(QStringLiteral("OE5PCH")));
        f->close();
        // Die geaenderte Antenne ist gemerkt -- fuer die folgenden Faelle
        // die Vorgabe wiederherstellen.
        AppSettings::instance().setValue(QStringLiteral("ProfileAntennaOther"),
                                         QStringLiteral("10"));
        QCOMPARE(HoehenprofilFenster::vorgabeGegen(), 10.0);
    }

    void gipfelUnbekanntNimmtLocator()
    {
        LogEntry e = zweiMeter(QStringLiteral("OM2Y"), QStringLiteral("JN88RS"), 4, 7, 19, 8);
        e.mySotaRef = QStringLiteral("OE/OO-999");
        HoehenprofilFenster f;
        QSignalSpy fertig(&f, &HoehenprofilFenster::profilFertig);
        // Nach der Absage gilt JN67VV; dafuer die Hoehen bereitlegen.
        LogEntry ohne = e;
        ohne.mySotaRef.clear();
        cacheFuer(ohne, aufgezeichnet(QStringLiteral("OM2Y")));
        f.setEntry(e);
        QVERIFY(fertig.wait(kFensterFrist));
        QVERIFY(SotaGipfelVerzeichnis::instanz().istUnbekannt(QStringLiteral("OE/OO-999")));
        QCOMPARE(f.eigeneAntenne()->value(), 10.0);    // kein Gipfel: Heimantenne
        QVERIFY(std::abs(f.weg().lat1 - 47.8958333) < 1e-5);  // Mitte JN67VV
    }

    void keinNetzKeinBild()
    {
        // Ein Weg ohne Cache: der Pruefmodus-Transport meldet "offline".
        LogEntry e = zweiMeter(QStringLiteral("OK3FW"), QStringLiteral("JN98AS"), 4, 7, 19, 10);
        HoehenprofilFenster f;
        QSignalSpy fehler(&f.abruf(), &HoehenAbruf::fehler);
        f.setEntry(e);
        QVERIFY(fehler.wait(kFensterFrist));
        QVERIFY(!f.profil()->hatProfil());
        QVERIFY(f.statusText().contains(QStringLiteral("offline")));

        LogEntry kw = e;
        kw.band = QStringLiteral("20m");
        kw.freqMHz = 14.2;
        f.setEntry(kw);
        QVERIFY(!f.hatWeg());
    }

    void logbuchNachtragSichertUndSchreibt()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVector<LogEntry> log{
            zweiMeter(QStringLiteral("OM2Y"), QStringLiteral("JN88RS"), 4, 7, 19, 8),
            zweiMeter(QStringLiteral("SN7L"), QStringLiteral("JO70SS"), 4, 7, 19, 26),
        };
        LogEntry kw = zweiMeter(QStringLiteral("IZ7HNO"), QStringLiteral("JN80OL"), 4, 7, 9, 56);
        kw.band = QStringLiteral("20m");
        log << kw;
        QVERIFY(AdifLog::write(pfad, log));
        const QByteArray vorher = lies(pfad);

        LogbookWindow w(pfad);
        bool knopf = false;
        for (QPushButton* b : w.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("SOTA…")) { knopf = b->isEnabled(); }
        }
        QVERIFY2(knopf, "no enabled SOTA… button in the logbook");

        const QByteArray csv =
            "V2,OE5SOS/P,OE/OO-106,04/07/2026,1908,144MHz,SSB,OM2Y,,\n"
            "V2,OE5SOS/P,OE/OO-106,04/07/2026,1926,144MHz,SSB,SN7L,,\n";
        const SotaNachtrag::Ergebnis e =
            SotaNachtrag::vorschlaege(w.entriesForTest(), SotaNachtrag::leseCsv(csv));
        QCOMPARE(e.vorschlaege.size(), 3);
        QVector<SotaNachtrag::Vorschlag> gewaehlt;
        for (const SotaNachtrag::Vorschlag& v : e.vorschlaege) {
            if (v.vorgewaehlt) { gewaehlt << v; }
        }
        QCOMPARE(gewaehlt.size(), 2);              // IZ7HNO steht nicht im SOTA-Log

        QString sicherung;
        QVERIFY(w.sotaVorschlaegeAnwenden(gewaehlt, &sicherung));
        QVERIFY(sicherung.startsWith(pfad + QStringLiteral(".before-sota-")));
        QCOMPARE(lies(sicherung), vorher);         // die Sicherung ist das alte Log

        const QVector<LogEntry> neu = AdifLog::read(pfad);
        int mitGipfel = 0;
        for (const LogEntry& x : neu) {
            if (x.mySotaRef == QLatin1String("OE/OO-106")) { ++mitGipfel; }
            if (x.call == QLatin1String("IZ7HNO")) { QVERIFY(x.mySotaRef.isEmpty()); }
        }
        QCOMPARE(mitGipfel, 2);

        // Nach dem Neueinlesen: SN7L vom Gipfel gemessen (344,39 km statt
        // 343,89) -- sobald der Gipfel nachgeschlagen ist (laeuft der Fall
        // allein, geschieht das erst jetzt).
        auto sn7lKm = [&w]() {
            for (const LogEntry& x : w.entriesForTest()) {
                if (x.call == QLatin1String("SN7L")) { return x.distanceKm; }
            }
            return 0.0;
        };
        QTRY_VERIFY2_WITH_TIMEOUT(std::abs(sn7lKm() - 344.39) < 0.05,
                                  qPrintable(QString::number(sn7lKm(), 'f', 2)), kFensterFrist);
        // Nichts gewaehlt: nichts geschrieben, keine Sicherung.
        QVERIFY(!w.sotaVorschlaegeAnwenden({}));
    }

    void nachtragDialogZeigtUndWaehlt()
    {
        QVector<LogEntry> log{
            zweiMeter(QStringLiteral("SN7L"), QStringLiteral("JO70SS"), 4, 7, 19, 26),
            zweiMeter(QStringLiteral("OM2Y"), QStringLiteral("JN88RS"), 4, 7, 19, 8),
        };
        LogEntry kw = zweiMeter(QStringLiteral("IZ7HNO"), QStringLiteral("JN80OL"), 4, 7, 9, 56);
        kw.band = QStringLiteral("20m");
        log << kw;
        SotaNachtragDialog dlg(log);
        QVERIFY(!dlg.anwendenForTest()->isEnabled());      // noch keine Datei
        QVERIFY(dlg.ladeCsvDaten(
            "V2,OE5SOS/P,OE/OO-106,04/07/2026,1908,144MHz,SSB,OM2Y,,\n"
            "V2,OE5SOS/P,OE/OO-106,04/07/2026,1926,144MHz,SSB,SN7L,,\n"
            "V2,OE5SOS/P,OE/OO-120,21/06/2026,1000,144MHz,FM,OE5ABC,,\n",
            QStringLiteral("test.csv")));
        QTableWidget* t = dlg.tabelleForTest();
        QCOMPARE(t->rowCount(), 3);
        QCOMPARE(dlg.gewaehlt().size(), 2);
        QCOMPARE(dlg.anwendenForTest()->text(), QStringLiteral("Apply to 2 contacts"));
        QVERIFY(dlg.zusammenfassung().contains(QStringLiteral("1 activation day without")));
        // Die nicht bestaetigte Zeile (IZ7HNO) anhaken: drei.
        for (int r = 0; r < t->rowCount(); ++r) {
            if (t->item(r, 2)->text() == QLatin1String("IZ7HNO")) {
                QCOMPARE(t->item(r, 0)->checkState(), Qt::Unchecked);
                t->item(r, 0)->setCheckState(Qt::Checked);
            }
        }
        QCOMPARE(dlg.gewaehlt().size(), 3);
        QCOMPARE(dlg.anwendenForTest()->text(), QStringLiteral("Apply to 3 contacts"));
        // Alles ab: Apply gesperrt.
        for (int r = 0; r < t->rowCount(); ++r) { t->item(r, 0)->setCheckState(Qt::Unchecked); }
        QVERIFY(!dlg.anwendenForTest()->isEnabled());
        // Unbrauchbare Datei: der Grund steht im Dialog.
        QVERIFY(!dlg.ladeCsvDaten("hallo\n", QStringLiteral("leer.csv")));
        QCOMPARE(t->rowCount(), 0);

        const QString ziel = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!ziel.isEmpty()) {
            QVERIFY(dlg.ladeCsvDaten(
                "V2,OE5SOS/P,OE/OO-106,04/07/2026,1908,144MHz,SSB,OM2Y,,\n"
                "V2,OE5SOS/P,OE/OO-106,04/07/2026,1926,144MHz,SSB,SN7L,,\n",
                QStringLiteral("test.csv")));
            dlg.show();
            QApplication::processEvents();
            QVERIFY(dlg.grab().save(ziel + QStringLiteral("/sota-nachtrag-dialog.png")));
        }
    }

    void bilderAusDemEchtenLog()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        const QString logPfad = qEnvironmentVariable("LONGPATH_LOGBOOK_FIXTURE");
        if (ziel.isEmpty() || logPfad.isEmpty()) {
            QSKIP("LONGPATH_GRAB_DIR and LONGPATH_LOGBOOK_FIXTURE not set");
        }
        const bool netz = qEnvironmentVariable("LONGPATH_HOEHEN_NETZ") == QLatin1String("1");
        // Die geholten Hoehen bleiben neben den Bildern liegen, damit man
        // die Rechnung nachpruefen kann.
        const QString cacheDir = ziel + QStringLiteral("/hoehen");
        if (netz) {
            // Echter Gipfel aus der SOTA-Datenbank (einmal).
            SotaGipfelVerzeichnis::instanz().setAbruf(echtesNetz(this));
        }
        const QVector<LogEntry> log = AdifLog::read(logPfad);
        QDir().mkpath(ziel);
        QVector<QImage> bilder;
        for (const QString& call : {QStringLiteral("SN7L"), QStringLiteral("OM2Y"),
                                    QStringLiteral("OE5PCH")}) {
            LogEntry e;
            for (const LogEntry& x : log) {
                if (x.call.trimmed() == call && x.band == QLatin1String("2m")) { e = x; }
            }
            QVERIFY2(e.isValid(), qPrintable(call + QStringLiteral(" not in the log")));
            // Was der Nachtrag eintragen wird -- nur hier im Speicher.
            e.mySotaRef = QStringLiteral("OE/OO-106");

            // Die Vorgaben des Auftrags, nicht was ein frueherer Fall gemerkt hat.
            AppSettings::instance().setValue(QStringLiteral("ProfileAntennaSummit"), QStringLiteral("5"));
            AppSettings::instance().setValue(QStringLiteral("ProfileAntennaOther"), QStringLiteral("10"));
            HoehenprofilFenster f;
            f.resize(1600, 520);
            if (netz) {
                f.abruf().setCacheOrdner(cacheDir);
                f.abruf().setAbruf(echtesNetz(&f));
            } else {
                cacheFuer(e, aufgezeichnet(call));
            }
            QSignalSpy fertig(&f, &HoehenprofilFenster::profilFertig);
            SotaGipfel g;
            if (!SotaGipfelVerzeichnis::instanz().finde(e.mySotaRef, g)) {
                QSignalSpy bekannt(&SotaGipfelVerzeichnis::instanz(),
                                   &SotaGipfelVerzeichnis::gipfelBekannt);
                f.setEntry(e);
                QVERIFY(bekannt.wait(30000));
            } else {
                f.setEntry(e);
            }
            QVERIFY2(fertig.wait(60000), qPrintable(f.statusText()));
            f.show();
            QApplication::processEvents();
            const QImage bild = f.grab().toImage();
            QVERIFY(bild.save(ziel + QStringLiteral("/hoehenprofil-%1.png").arg(call)));
            bilder << bild;
            qInfo().noquote() << call << f.profil()->urteilZeile()
                              << "| Volumen" << f.profil()->ergebnis().schnittUeberGrundM
                              << "m | Winkel" << f.profil()->ergebnis().streuwinkelGrad
                              << "| Punkte" << f.weg().punkte
                              << "| Tangenten" << f.profil()->ergebnis().tangenteA
                              << f.profil()->ergebnis().tangenteB
                              << "| Antennen" << f.eigeneAntenne()->value()
                              << f.gegenAntenne()->value();
            QCOMPARE(f.eigeneAntenne()->value(), 5.0);
            QCOMPARE(f.gegenAntenne()->value(), 10.0);
        }
        // Alle drei untereinander, wie das Entwurfsblatt.
        int h = 0;
        for (const QImage& b : bilder) { h += b.height(); }
        QImage blatt(bilder.first().width(), h, QImage::Format_RGB32);
        QPainter p(&blatt);
        int y = 0;
        for (const QImage& b : bilder) {
            p.drawImage(0, y, b);
            y += b.height();
        }
        p.end();
        QVERIFY(blatt.save(ziel + QStringLiteral("/hoehenprofil-gmundnerberg.png")));
    }
};

QTEST_MAIN(TstHoehenprofilFenster)
#include "tst_hoehenprofil_fenster.moc"
