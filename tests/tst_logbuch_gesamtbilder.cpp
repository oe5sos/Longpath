// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Drei Gesamtbilder des Logbuchs (2026-11-10) ─────────────────────
//
// Betreiber waehlte am 09.11. aus fuenf Entwurfsblaettern (werkzeug/
// logbuch-2026-11-09/blaetter): Sonnenzyklus, Kontinent-Uhr, DXCC-Wand.
//   1. Sonnenzyklus: SWPC-JSON lesen, Monate lueckenlos, Glaettung
//      randrichtig, ohne Flux nur Balken und ein Satz dazu.
//   2. Flux-Archiv: hoechstens einmal am Tag, nach einem Versuch eine
//      Stunde Ruhe; Cache schreiben und wieder lesen; kaputte Antwort
//      laesst den alten Cache stehen.
//   3. Kontinent-Uhr: Zaehlung je Kontinent und Stunde, jeder Ring auf
//      seine beste Stunde bezogen; jede Zelle hat einen Tooltip; der
//      Zeiger-Takt laeuft nur, solange das Bild zu sehen ist.
//   4. DXCC-Wand: 340 Gebiete aus cty.dat (ohne WAE), WAE zaehlt fuer
//      sein Gebiet; keine Kachel ueberdeckt eine andere, bei keiner
//      Fenstergroesse; Tooltip mit Gebiet und Baendern.
//   5. Im Logbuch-Fenster: Stats -> ↗ -> Reiter, mit echten Maus-
//      ereignissen; jede der drei Seiten rechnet, was die Filter zeigen.
//   6. Renderfall (nur mit LONGPATH_LOGBOOK_FIXTURE + LONGPATH_GRAB_DIR):
//      die drei Seiten (und die Richtungen) aus einer Kopie des echten
//      Logs als PNG.

#include <QtTest/QtTest>
#include <QAccessible>
#include <QAccessibleActionInterface>
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>

#include <cmath>

#include "FensterFrist.h"
#include "core/AdifLog.h"
#include "core/CtyDatParser.h"
#include "core/LogbuchGesamtbilder.h"
#include "core/SolarFluxArchiv.h"
#include "gui/LogbookWindow.h"
#include "gui/widgets/LogbuchGesamtbilderWidgets.h"
#include "models/LogEntry.h"

using namespace Longpath;
using namespace Longpath::LogbuchBilder;

namespace {

LogEntry qso(const QString& call, const QString& band, const QDate& tag, int stunde,
             bool bestaetigt = false)
{
    LogEntry e;
    e.call = call;
    e.band = band;
    e.mode = QStringLiteral("SSB");
    e.myGridSquare = QStringLiteral("JN67VV");
    e.timeOn = QDateTime(tag, QTime(stunde, 12), QTimeZone::UTC);
    if (bestaetigt) { e.extras.push_back({QStringLiteral("QSL_RCVD"), QStringLiteral("Y")}); }
    return e;
}

QVector<LogEntry> beispielLog()
{
    const QDate apr(2024, 4, 10), jun(2024, 6, 3);
    return {
        qso(QStringLiteral("DL1ABC"), QStringLiteral("20m"), apr, 9),
        qso(QStringLiteral("DL2XYZ"), QStringLiteral("40m"), apr, 9),
        qso(QStringLiteral("W1AW"),   QStringLiteral("20m"), apr, 19),
        qso(QStringLiteral("K1ABC"),  QStringLiteral("15m"), jun, 19),
        qso(QStringLiteral("JA1XYZ"), QStringLiteral("15m"), jun, 7),
        qso(QStringLiteral("IT9ABC"), QStringLiteral("40m"), jun, 18),   // Sizilien -> Italien
        qso(QStringLiteral("I2ABC"),  QStringLiteral("2m"),  jun, 18, true),
        qso(QStringLiteral("1234"),   QStringLiteral("20m"), jun, 12),   // kein Gebiet
    };
}

// Ein Ausschnitt in der Form von observed-solar-cycle-indices.json.
QByteArray swpcJson()
{
    return QByteArrayLiteral(
        "[{\"time-tag\": \"1749-01\", \"ssn\": 96.7, \"f10.7\": -1.0},"
        " {\"time-tag\": \"2024-04\", \"ssn\": 136.5, \"f10.7\": 160.5},"
        " {\"time-tag\": \"2024-05\", \"ssn\": 172.0, \"f10.7\": 190.0},"
        " {\"time-tag\": \"2024-06\", \"ssn\": 160.0, \"f10.7\": 180.0},"
        " {\"time-tag\": \"2024-07\", \"ssn\": 196.0, \"f10.7\": 210.0}]");
}

// Ein Klick, wie ihn der Bediener macht: Ereignisse an das Widget.
// QTest::mouseClick verpufft im Pruefstandslauf (kein echter Schirm).
void klick(QWidget* w)
{
    const QPointF m = QRectF(w->rect()).center();
    QMouseEvent druck(QEvent::MouseButtonPress, m, w->mapToGlobal(m), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, m, w->mapToGlobal(m), Qt::LeftButton,
                    Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &los);
}

} // namespace

class TstLogbuchGesamtbilder : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        const QString pfad = qEnvironmentVariable("LONGPATH_SOURCE_DIR") + QStringLiteral("/cty.dat");
        QVERIFY2(m_cty.loadFromFile(pfad), qPrintable(pfad));
    }
    void init()
    {
        // Das Fenster liest den Cache am Standardort; jeder Fall beginnt ohne.
        QFile::remove(SolarFluxArchiv::standardPfad());
    }

    // ── 1  Sonnenzyklus ─────────────────────────────────────────────
    void swpcLesen()
    {
        const FluxJeMonat f = fluxAusSwpc(swpcJson());
        QCOMPARE(f.size(), 4);                       // 1749 hat keinen Wert
        QCOMPARE(f.value(QStringLiteral("2024-05")), 190.0);
        QVERIFY(!f.contains(QStringLiteral("1749-01")));
        QVERIFY(fluxAusSwpc("kaputt").isEmpty());
        QVERIFY(fluxAusSwpc("{\"a\": 1}").isEmpty());
    }

    void glaettungRandrichtig()
    {
        const double nan = std::nan("");
        QVector<double> g = gleitendesMittel({10, 20, 30, 40, 50}, 2);
        QCOMPARE(g, (QVector<double>{20, 25, 30, 35, 40}));   // am Rand: was da ist
        g = gleitendesMittel({10, nan, 30, nan}, 2);
        QCOMPARE(g[0], 20.0);
        QVERIFY(std::isnan(g[1]));                    // kein Rohwert -> keine Linie
        QCOMPARE(g[2], 20.0);
        QVERIFY(std::isnan(g[3]));                    // die Linie endet, wo die Daten enden
    }

    void sonnenzyklusRechnen()
    {
        const Sonnenzyklus s = sonnenzyklus(beispielLog(), fluxAusSwpc(swpcJson()), QDate(2024, 8, 15));
        QCOMPARE(s.monate, (QStringList{QStringLiteral("2024-04"), QStringLiteral("2024-05"),
                                        QStringLiteral("2024-06"), QStringLiteral("2024-07"),
                                        QStringLiteral("2024-08")}));
        QCOMPARE(s.qsos, (QVector<int>{3, 0, 5, 0, 0}));
        QCOMPARE(s.gesamt, 8);
        QCOMPARE(s.besterMonat, 2);
        QCOMPARE(s.fluxBis(), QStringLiteral("2024-07"));
        QVERIFY(std::isnan(s.flux[4]) && std::isnan(s.glatt[4]));
        QCOMPARE(s.glatt[0], (160.5 + 190.0 + 180.0) / 3.0);
        // Ohne Flux: nur die Balken.
        const Sonnenzyklus ohne = sonnenzyklus(beispielLog(), {}, QDate(2024, 8, 15));
        QVERIFY(!ohne.hatFlux());
        QCOMPARE(ohne.qsos.size(), 5);
        QVERIFY(sonnenzyklus({}, fluxAusSwpc(swpcJson()), QDate(2024, 8, 15)).monate.isEmpty());
    }

    void sonnenzyklusWidgetSagtWasFehlt()
    {
        SonnenzyklusWidget w;
        w.resize(1100, 600);
        w.setDaten(sonnenzyklus(beispielLog(), {}, QDate(2024, 8, 15)), {}, false);
        QVERIFY(w.hinweis().contains(QStringLiteral("nothing is cached")));
        w.setDaten(sonnenzyklus(beispielLog(), {}, QDate(2024, 8, 15)), {}, true);
        QVERIFY(w.hinweis().startsWith(QStringLiteral("Fetching")));
        const QDateTime alt = QDateTime::currentDateTimeUtc().addDays(-5);
        w.setDaten(sonnenzyklus(beispielLog(), fluxAusSwpc(swpcJson()), QDate(2024, 8, 15)), alt, false);
        QVERIFY(w.hinweis().isEmpty());
        QVERIFY2(w.fusszeile().contains(QStringLiteral("through 2024-07")), qPrintable(w.fusszeile()));
        QVERIFY2(w.fusszeile().contains(QStringLiteral("offline, cached")), qPrintable(w.fusszeile()));
        QVERIFY(w.fusszeile().contains(QStringLiteral("best month 2024-06 with 5 QSOs")));
        QVERIFY(!w.grab().isNull());
    }

    // ── 2  Flux-Archiv ──────────────────────────────────────────────
    void fluxHoechstensTaeglich()
    {
        const QDateTime jetzt(QDate(2026, 11, 10), QTime(20, 0), QTimeZone::UTC);
        QVERIFY(SolarFluxArchiv::brauchtAbruf({}, {}, jetzt));                         // kein Cache
        QVERIFY(!SolarFluxArchiv::brauchtAbruf(jetzt.addSecs(-23 * 3600), {}, jetzt));  // frisch
        QVERIFY(SolarFluxArchiv::brauchtAbruf(jetzt.addSecs(-25 * 3600), {}, jetzt));   // alt
        // Gerade versucht (ohne Netz): eine Stunde Ruhe, auch ohne Cache.
        QVERIFY(!SolarFluxArchiv::brauchtAbruf({}, jetzt.addSecs(-1800), jetzt));
        QVERIFY(SolarFluxArchiv::brauchtAbruf({}, jetzt.addSecs(-3700), jetzt));
    }

    void fluxCacheSchreibenUndLesen()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("sub/swpc.json"));
        const QDateTime jetzt = QDateTime::currentDateTimeUtc();
        {
            SolarFluxArchiv a(pfad);
            QVERIFY(!a.ladeCache());
            QVERIFY(a.uebernimm(swpcJson(), jetzt));
            QVERIFY(QFile::exists(pfad));
            QVERIFY(!a.uebernimm("<html>Service Unavailable</html>", jetzt));   // bleibt
            QCOMPARE(a.werte().size(), 4);
        }
        SolarFluxArchiv b(pfad);
        QVERIFY(b.ladeCache());
        QCOMPARE(b.werte().value(QStringLiteral("2024-07")), 210.0);
        QVERIFY(b.stand().isValid());
        QVERIFY(!SolarFluxArchiv::brauchtAbruf(b.stand(), {}, jetzt.addSecs(60)));
    }

    // ── 3  Kontinent-Uhr ────────────────────────────────────────────
    void kontinentUhrZaehlt()
    {
        const KontinentUhr u = kontinentUhr(beispielLog(), &m_cty);
        QCOMPARE(KontinentUhr::kontinente().first(), QStringLiteral("EU"));   // innen
        QCOMPARE(KontinentUhr::kontinente().last(), QStringLiteral("OC"));    // aussen
        QCOMPARE(u.gesamt, 7);
        QCOMPARE(u.ohneZuordnung, 1);
        QCOMPARE(u.summe(QStringLiteral("EU")), 4);
        QCOMPARE(u.anzahl.value(QStringLiteral("EU"))[9], 2);
        QCOMPARE(u.anzahl.value(QStringLiteral("NA"))[19], 2);
        QCOMPARE(u.besteStunde(QStringLiteral("EU")), 9);
        QCOMPARE(u.anteil(QStringLiteral("EU"), 18), 1.0);      // 2 von 2: auch hell
        QCOMPARE(u.anteil(QStringLiteral("AS"), 7), 1.0);       // eigener Massstab je Ring
        QCOMPARE(u.besteStunde(QStringLiteral("AF")), -1);
    }

    void kontinentUhrJedeZelleHatEinenTooltip()
    {
        KontinentUhrWidget w;
        w.resize(760, 820);
        w.setDaten(kontinentUhr(beispielLog(), &m_cty));
        w.setJetztFuerTest(QDateTime(QDate(2026, 11, 10), QTime(18, 25), QTimeZone::UTC));
        QSet<int> gefunden;
        for (int x = 0; x < w.width(); x += 3) {
            for (int y = 0; y < w.height(); y += 3) {
                int ring = -1, stunde = -1;
                if (w.zelleBei(QPointF(x, y), &ring, &stunde)) { gefunden.insert(ring * 24 + stunde); }
            }
        }
        QCOMPARE(gefunden.size(), 6 * 24);
        QVERIFY2(w.zellenText(1, 19).startsWith(QStringLiteral("North America · 19–20 UTC: 2 QSOs")),
                 qPrintable(w.zellenText(1, 19)));
        QVERIFY(!w.grab().isNull());
    }

    void kontinentUhrTaktNurSichtbar()
    {
        KontinentUhrWidget w;
        w.resize(600, 660);
        QVERIFY(!w.takt()->isActive());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QVERIFY(w.takt()->isActive());
        QVERIFY(w.takt()->remainingTime() > 0 && w.takt()->remainingTime() <= 60050);
        w.hide();
        QVERIFY(!w.takt()->isActive());
    }

    // ── 4  DXCC-Wand ────────────────────────────────────────────────
    void dxccWandRechnet()
    {
        const DxccWand w = dxccWand(beispielLog(), &m_cty);
        QCOMPARE(w.gesamt(), 340);                     // 346 Zeilen minus sechs WAE
        for (const DxccKachel& k : w.kacheln) { QVERIFY(!k.praefix.startsWith(QLatin1Char('*'))); }
        QCOMPARE(w.kacheln.first().kontinent, QStringLiteral("EU"));
        QCOMPARE(w.kacheln.last().kontinent, QStringLiteral("OC"));
        QCOMPARE(w.gearbeitet, 4);                     // DL, K, JA, I
        QCOMPARE(w.ohneZuordnung, 1);
        QCOMPARE(w.bestaetigt, 1);
        const auto it = std::find_if(w.kacheln.cbegin(), w.kacheln.cend(),
                                     [](const DxccKachel& k) { return k.praefix == QLatin1String("I"); });
        QVERIFY(it != w.kacheln.cend());
        QCOMPARE(it->baender, (QSet<QString>{QStringLiteral("40m"), QStringLiteral("2m")}));   // IT9 zaehlt mit
        QVERIFY(it->bestaetigt);
        int alle = 0, gearb = 0;
        w.zaehle(QStringLiteral("EU"), &alle, &gearb);
        QCOMPARE(gearb, 2);
        QVERIFY(alle > 60);
        QCOMPARE(dxccFuerWae(QStringLiteral("*4U1V")), QStringLiteral("OE"));
    }

    void dxccWandKachelnUeberdeckenSichNicht_data()
    {
        QTest::addColumn<QSize>("groesse");
        QTest::newRow("dialog") << QSize(1220, 700);
        QTest::newRow("schmal") << QSize(880, 480);
        QTest::newRow("breit") << QSize(1900, 760);
        QTest::newRow("hoch") << QSize(700, 1000);
    }
    void dxccWandKachelnUeberdeckenSichNicht()
    {
        QFETCH(QSize, groesse);
        DxccWandWidget w;
        w.resize(groesse);
        w.setDaten(dxccWand(beispielLog(), &m_cty));
        const QVector<QRectF> r = w.kachelRechtecke();
        QCOMPARE(r.size(), 340);
        const QRectF innen = QRectF(w.rect()).adjusted(0, 60, 0, -30);   // unter dem Titel, ueber dem Fuss
        for (int i = 0; i < r.size(); ++i) {
            QVERIFY2(!r[i].isEmpty(), qPrintable(QString::number(i)));
            QVERIFY2(innen.contains(r[i]), qPrintable(QStringLiteral("Kachel %1 ragt hinaus").arg(i)));
            for (int j = i + 1; j < r.size(); ++j) {
                QVERIFY2(!r[i].intersects(r[j]), qPrintable(QStringLiteral("%1 / %2").arg(i).arg(j)));
            }
            QCOMPARE(w.kachelBei(r[i].center()), i);
        }
        QVERIFY(r.first().width() >= 14.0);           // auch schmal noch eine Kachel
    }

    void dxccWandTooltip()
    {
        DxccWandWidget w;
        w.resize(1220, 700);
        w.setDaten(dxccWand(beispielLog(), &m_cty));
        QCOMPARE(w.titel(), QStringLiteral("DXCC wall — 4 of 340 worked"));
        int italien = -1;
        for (int i = 0; i < w.daten().kacheln.size(); ++i) {
            if (w.daten().kacheln[i].praefix == QLatin1String("I")) { italien = i; }
        }
        QVERIFY(italien >= 0);
        const QString t = w.kachelText(italien);
        QVERIFY2(t.contains(QStringLiteral("Italy (I) · Europe")), qPrintable(t));
        QVERIFY2(t.contains(QStringLiteral("2 QSOs · 40m · 2m · confirmed")), qPrintable(t));
        QVERIFY(w.fusszeile().contains(QStringLiteral("1 confirmed")));
        DxccWandWidget ohne;
        ohne.setDaten(dxccWand({qso(QStringLiteral("DL1ABC"), QStringLiteral("20m"), QDate(2026, 1, 1), 9)}, &m_cty));
        QVERIFY(ohne.fusszeile().contains(QStringLiteral("worked, not confirmed")));
    }

    // ── 5  Im Logbuch-Fenster, ueber die Bedienung ──────────────────
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

        // Stats -> Reihe -> ↗ -> Dialog
        QVERIFY(w.statsToggleForTest());
        QVERIFY(w.statsToggleForTest()->isVisible() && w.statsToggleForTest()->isEnabled());
        // Seit Entwurf C ist „Statistics" eine eigene Seite: von „Log" aus
        // mit einem Klick dorthin.
        w.setModusForTest(0);
        QVERIFY(!w.statsSectionForTest()->isVisible());
        klick(w.statsToggleForTest());
        QTRY_VERIFY_WITH_TIMEOUT(w.statsSectionForTest()->isVisible(), 2000);
        auto* raus = w.findChild<QPushButton*>(QStringLiteral("statsPopOut"));
        QVERIFY(raus && raus->isVisible() && raus->isEnabled());
        klick(raus);
        QTRY_VERIFY_WITH_TIMEOUT(w.statistikDialogForTest() && w.statistikDialogForTest()->isVisible(), 2000);
        const QList<QPushButton*> reiter = w.statistikReiterForTest();
        QCOMPARE(reiter.size(), 6);
        QCOMPARE(reiter[3]->text(), QStringLiteral("Solar cycle"));
        QCOMPARE(reiter[4]->text(), QStringLiteral("Continent clock"));
        QCOMPARE(reiter[5]->text(), QStringLiteral("DXCC wall"));

        // Sonnenzyklus: ohne Cache und ohne Netz (Pruefstand) nur Balken.
        const QSize vorher = w.statistikDialogForTest()->size();
        klick(reiter[3]);
        QVERIFY(w.sonnenzyklusForTest()->isVisible());
        QCOMPARE(w.sonnenzyklusForTest()->daten().gesamt, 8);
        QVERIFY(!w.sonnenzyklusForTest()->daten().hatFlux());
        QVERIFY(!w.sonnenzyklusForTest()->hinweis().isEmpty());
        QVERIFY(!w.solarFluxForTest()->laeuft());              // Pruefstaende holen nie
        // Das Fenster waechst, damit die Bilder Platz haben -- und
        // schrumpft nie, auch nicht auf einem kleinen Schirm.
        QCOMPARE(w.statistikDialogForTest()->size(),
                 vorher.expandedTo(LogbookWindow::gesamtbildGroesse(w.statistikDialogForTest())));
        // Neue Werte kommen an (wie nach einem Abruf): die Seite zieht nach.
        QVERIFY(w.solarFluxForTest()->uebernimm(swpcJson(), QDateTime::currentDateTimeUtc()));
        emit w.solarFluxForTest()->aktualisiert();
        QVERIFY(w.sonnenzyklusForTest()->daten().hatFlux());
        QVERIFY(w.sonnenzyklusForTest()->hinweis().isEmpty());

        klick(reiter[4]);
        QVERIFY(w.kontinentUhrForTest()->isVisible());
        QCOMPARE(w.kontinentUhrForTest()->daten().gesamt, 7);
        QVERIFY(w.kontinentUhrForTest()->takt()->isActive());

        klick(reiter[5]);
        QVERIFY(w.dxccWandForTest()->isVisible());
        QVERIFY(!w.kontinentUhrForTest()->takt()->isActive());   // Uhr nicht zu sehen: Takt steht
        QCOMPARE(w.dxccWandForTest()->daten().gearbeitet, 4);
        QCOMPARE(w.dxccWandForTest()->kachelRechtecke().size(), 340);

        // Was die Filter zeigen: die Suche nach JA1XYZ laesst ein Gebiet.
        w.searchForTest()->setText(QStringLiteral("JA1XYZ"));
        QTRY_COMPARE_WITH_TIMEOUT(w.dxccWandForTest()->daten().gearbeitet, 1, 3000);
        w.searchForTest()->clear();
        QFile::remove(SolarFluxArchiv::standardPfad());
    }

    // ── 5b Reiter per Bedienhilfe (Live-Befund 2026-11-11) ──────────
    // Die Bedienhilfe (AXPress, VoiceOver, Automatisierung) drueckt einen
    // ankreuzbaren Knopf ueber toggle() -- das sendet toggled, aber kein
    // clicked. Am laufenden Programm rastete der Reiter ein, die Seite
    // blieb auf "Overview". Jede angebotene Handlung muss umschalten.
    void reiterPerBedienhilfe()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, beispielLog()));
        LogbookWindow w(pfad);
        w.setCtyDat(&m_cty);
        w.resize(1400, 900);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        w.zeigeStatistikSeiteForTest(0);
        QDialog* d = w.statistikDialogForTest();
        QVERIFY(d && d->isVisible());
        const QList<QPushButton*> reiter = w.statistikReiterForTest();
        QCOMPARE(reiter.size(), 6);
        // Die Seite, die ein Reiter zeigen muss: der Stapel steht auf ihr,
        // und bei den drei Gesamtbildern ist das Bild selbst zu sehen.
        auto* stapel = d->findChild<QStackedWidget*>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(stapel);
        QCOMPARE(stapel->count(), 6);
        const auto zeigt = [&](int seite) {
            if (stapel->currentIndex() != seite) { return false; }
            switch (seite) {
            case 3: return w.sonnenzyklusForTest()->isVisible();
            case 4: return w.kontinentUhrForTest()->isVisible();
            case 5: return w.dxccWandForTest()->isVisible();
            default: return stapel->currentWidget()->isVisible();
            }
        };
        int gedrueckt = 0;
        for (int seite : {3, 4, 5, 0, 5, 2, 3}) {
            QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(reiter[seite]);
            QVERIFY(iface && iface->actionInterface());
            const QStringList handlungen = iface->actionInterface()->actionNames();
            QVERIFY2(!handlungen.isEmpty(), "keine Handlung angeboten");
            for (const QString& h : handlungen) {
                if (h == QAccessibleActionInterface::setFocusAction()) { continue; }
                // Vorher woanders hin, damit die Handlung etwas zu tun hat.
                w.zeigeStatistikSeiteForTest(seite == 1 ? 2 : 1);
                QVERIFY(!zeigt(seite));
                iface->actionInterface()->doAction(h);
                const QByteArray warum = QStringLiteral("%1 auf \"%2\" schaltet nicht um")
                                             .arg(h, reiter[seite]->text()).toUtf8();
                QTRY_VERIFY2_WITH_TIMEOUT(zeigt(seite), warum.constData(), 3000);
                QVERIFY(reiter[seite]->isChecked());
                ++gedrueckt;
            }
        }
        QVERIFY(gedrueckt >= 7);
        QFile::remove(SolarFluxArchiv::standardPfad());
    }

    // ── 6  Renderfall aus dem echten Log ────────────────────────────
    void ausDemEchtenLog()
    {
        const QString fix = qEnvironmentVariable("LONGPATH_LOGBOOK_FIXTURE");
        const QString aus = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (fix.isEmpty() || aus.isEmpty()) {
            QSKIP("Renderfall: LONGPATH_LOGBOOK_FIXTURE und LONGPATH_GRAB_DIR setzen");
        }
        QTemporaryDir tmp;
        const QString kopie = tmp.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(QFile::copy(fix, kopie));
        LogbookWindow w(kopie);
        w.setCtyDat(&m_cty);
        w.resize(1400, 900);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        w.zeigeStatistikSeiteForTest(0);
        // Flux aus einer Datei (die Antwort von SWPC), nicht aus dem Netz.
        QFile sfi(qEnvironmentVariable("LONGPATH_SFI_JSON"));
        if (sfi.open(QIODevice::ReadOnly)) {
            QVERIFY(w.solarFluxForTest()->uebernimm(sfi.readAll(), QDateTime::currentDateTimeUtc()));
        }
        QDialog* d = w.statistikDialogForTest();
        QVERIFY(d);
        d->resize(1240, 820);
        const QStringList namen = {QStringLiteral("1-richtungen"), QStringLiteral("2-sonnenzyklus"),
                                   QStringLiteral("3-kontinent-uhr"), QStringLiteral("4-dxcc-wand")};
        for (int s = 2; s <= 5; ++s) {
            w.zeigeStatistikSeiteForTest(s);
            QTest::qWait(150);
            QImage bild(d->size() * 2, QImage::Format_ARGB32_Premultiplied);
            bild.setDevicePixelRatio(2.0);
            bild.fill(Qt::black);
            d->render(&bild);
            QVERIFY(bild.save(aus + QStringLiteral("/") + namen[s - 2] + QStringLiteral(".png")));
        }
        qInfo().noquote() << "Wand:" << w.dxccWandForTest()->titel()
                          << "| Uhr:" << w.kontinentUhrForTest()->daten().gesamt
                          << "ohne" << w.kontinentUhrForTest()->daten().ohneZuordnung
                          << "| Sonne:" << w.sonnenzyklusForTest()->fusszeile();
        QFile::remove(SolarFluxArchiv::standardPfad());
    }

private:
    CtyDatParser m_cty;
};

QTEST_MAIN(TstLogbuchGesamtbilder)
#include "tst_logbuch_gesamtbilder.moc"
