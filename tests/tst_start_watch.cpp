// Der Startwaechter.
//
// Er soll genau eine Klemme aufmachen: eine kaputte gespeicherte
// Anordnung kann Longpath unstartbar machen, und dann kommt man an die
// Einstellungen nicht mehr heran, um sie zu reparieren -- dafuer
// braeuchte man ja das Programm.
//
// Das Verfahren steht und faellt mit zwei Eigenschaften, und beide
// haben hier einen Fall:
//
//   * Die Marke muss einen Absturz UEBERLEBEN. Sie liegt darum in einer
//     eigenen Datei, die beim Start geschrieben und beim Gelingen
//     geloescht wird -- nicht in den Einstellungen, die erst beim
//     Beenden geschrieben werden und die ein Absturz nie erreicht.
//   * Ein EINZELNER Absturz darf nichts ausloesen. Der kann alles
//     Moegliche sein: Stromausfall, ein Absturz beim Beenden, ein
//     hartes Abschiessen kurz nach dem Start. Erst die Wiederholung
//     deutet auf etwas Gespeichertes.

#include <QDir>
#include <QFile>
#include <QtTest>

#include "core/StartWatch.h"
#include "gui/MainWindow.h"

using namespace Longpath;

class TstStartWatch : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanupTestCase();
    void ohneMarkeIstAllesInOrdnung();
    void einGelungenerStartLoeschtDieMarke();
    void einSteckengebliebenerStartZaehltHoch();
    void ersteinzelnerAbsturzLoestNichtsAus();
    void zweiInFolgeLoesenDenSicherenStartAus();
    void einGelungenerStartDazwischenSetztZurueck();
    void eineUnleserlicheMarkeGiltAlsEinAbsturz();
    void dieMarkeLiegtNichtInDenEinstellungen();
    void derHinweisSagtWasPassiertIst();
    void dasHauptfensterFaehrtWirklichSicherHoch();
};

void TstStartWatch::init()
{
    StartWatch::reset();
}

void TstStartWatch::cleanupTestCase()
{
    StartWatch::reset();
}

void TstStartWatch::ohneMarkeIstAllesInOrdnung()
{
    QCOMPARE(StartWatch::failedStarts(), 0);
    QVERIFY(!StartWatch::shouldStartSafely());
}

void TstStartWatch::einGelungenerStartLoeschtDieMarke()
{
    QCOMPARE(StartWatch::beginStart(), 0);
    QVERIFY2(QFile::exists(StartWatch::markerPath()),
             "waehrend des Starts muss die Marke liegen");

    StartWatch::markRunning();
    QVERIFY2(!QFile::exists(StartWatch::markerPath()),
             "nach einem gelungenen Start muss die Marke weg sein");
    QCOMPARE(StartWatch::failedStarts(), 0);
}

void TstStartWatch::einSteckengebliebenerStartZaehltHoch()
{
    // Start eins: laeuft an, kommt nicht durch (kein markRunning).
    QCOMPARE(StartWatch::beginStart(), 0);
    QCOMPARE(StartWatch::failedStarts(), 0);   // der laufende zaehlt nicht mit

    // Start zwei sieht die Marke von Start eins.
    QCOMPARE(StartWatch::beginStart(), 1);
    QCOMPARE(StartWatch::failedStarts(), 1);
}

void TstStartWatch::ersteinzelnerAbsturzLoestNichtsAus()
{
    StartWatch::beginStart();                  // kommt nicht durch
    const int vorige = StartWatch::beginStart();
    QCOMPARE(vorige, 1);
    QVERIFY2(!StartWatch::shouldStartSafely(),
             "ein einzelner Absturz darf den sicheren Start nicht ausloesen -- "
             "der kann alles Moegliche sein");
}

void TstStartWatch::zweiInFolgeLoesenDenSicherenStartAus()
{
    StartWatch::beginStart();                  // 1: kommt nicht durch
    StartWatch::beginStart();                  // 2: kommt nicht durch
    const int vorige = StartWatch::beginStart();
    QCOMPARE(vorige, 2);
    QCOMPARE(StartWatch::failedStarts(), 2);
    QVERIFY2(StartWatch::shouldStartSafely(),
             "nach zwei steckengebliebenen Starts muss sicher hochgefahren werden");
}

void TstStartWatch::einGelungenerStartDazwischenSetztZurueck()
{
    StartWatch::beginStart();
    StartWatch::beginStart();                  // jetzt steht der Zaehler auf 1
    QCOMPARE(StartWatch::failedStarts(), 1);

    StartWatch::markRunning();                 // dieser kam durch
    QCOMPARE(StartWatch::failedStarts(), 0);

    // Und der naechste faengt wieder bei null an -- ein alter Absturz
    // darf nicht mit einem neuen zusammenzaehlen.
    StartWatch::beginStart();
    QVERIFY2(!StartWatch::shouldStartSafely(),
             "ein gelungener Start muss den Zaehler wirklich zuruecksetzen");
}

void TstStartWatch::eineUnleserlicheMarkeGiltAlsEinAbsturz()
{
    // Ein Absturz MITTEN im Schreiben der Marke. Die Datei ist da, aber
    // der Inhalt ist Unsinn -- das darf weder zum Durchzaehlen fuehren
    // noch den Waechter verwirren.
    QFile f(StartWatch::markerPath());
    QVERIFY(QDir().mkpath(QFileInfo(f).absolutePath()));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("kaputt");
    f.close();

    // Die Marke ist da, also lief ein Start an und kam nicht durch.
    QCOMPARE(StartWatch::failedStarts(), 0);   // 1 in der Marke - 1 laufender
    const int vorige = StartWatch::beginStart();
    QCOMPARE(vorige, 1);
}

void TstStartWatch::dieMarkeLiegtNichtInDenEinstellungen()
{
    // Der Kern des Verfahrens: die Marke muss einen Absturz ueberleben.
    // Die Einstellungen werden beim BEENDEN geschrieben -- ein Absturz
    // erreicht das nie, dort stuende also nie etwas.
    StartWatch::beginStart();
    const QString marke = StartWatch::markerPath();
    QVERIFY2(QFile::exists(marke), "die Marke muss als eigene Datei liegen");
    QVERIFY2(!marke.endsWith(QStringLiteral(".xml")),
             qPrintable(QStringLiteral("die Marke zeigt auf die Einstellungsdatei: %1")
                            .arg(marke)));

    // Und sie muss WIRKLICH auf der Platte stehen, nicht nur im
    // Speicher: ein Absturz schreibt nichts mehr nach.
    QFile f(marke);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray inhalt = f.readAll().trimmed();
    f.close();
    bool ok = false;
    const int n = inhalt.toInt(&ok);
    QVERIFY2(ok, qPrintable(QStringLiteral("die Marke enthaelt keine Zahl: %1")
                                .arg(QString::fromUtf8(inhalt))));
    QCOMPARE(n, 1);
}

void TstStartWatch::derHinweisSagtWasPassiertIst()
{
    const QString text = StartWatch::safeStartNotice(2);
    QVERIFY2(text.contains(QStringLiteral("2")),
             "der Hinweis nennt nicht, wie oft es schiefging");
    // Die beiden Dinge, die der Benutzer wissen MUSS: seine
    // Einstellungen sind heil, und was stattdessen anders ist.
    QVERIFY2(text.contains(QStringLiteral("Einstellungen")),
             qPrintable(QStringLiteral("der Hinweis beruhigt nicht ueber die "
                                       "Einstellungen: %1").arg(text)));
    QVERIFY2(text.contains(QStringLiteral("Anordnung")),
             qPrintable(QStringLiteral("der Hinweis sagt nicht, was anders ist: %1")
                            .arg(text)));
}

void TstStartWatch::dasHauptfensterFaehrtWirklichSicherHoch()
{
    // Die Regeln oben koennten alle stimmen und das Hauptfenster
    // trotzdem stur die gespeicherte Anordnung laden -- dann waere der
    // ganze Waechter wirkungslos. Dieser Fall baut darum das ECHTE
    // Fenster auf, einmal mit und einmal ohne vorangegangene
    // steckengebliebene Starts.
    //
    // Der Hinweisdialog bleibt dabei weg (LONGPATH_NO_DIALOGS): ein
    // modaler Dialog haelt den Lauf an, bis jemand klickt, und es
    // klickt niemand.
    qputenv("LONGPATH_NO_DIALOGS", "1");

    {
        // Erst der normale Fall: keine Marke, also normal hochfahren.
        StartWatch::reset();
        auto* mw = new MainWindow();
        mw->resize(1024, 700);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        QTest::qWait(150);
        QVERIFY2(!mw->startedSafely(),
                 "ohne steckengebliebene Starts darf nicht sicher hochgefahren werden");
        QCOMPARE(mw->failedStartCount(), 0);
        // Bewusst nicht abgeraeumt, wie in tst_real_mainwindow_detach:
        // MainWindow startet Arbeitsfaeden, deren geordnetes Ende an
        // einer laufenden Ereignisschleife haengt.
        mw->hide();
    }

    {
        // Jetzt zwei steckengebliebene Starts vortaeuschen. Dafuer
        // braucht es DREI Aufrufe: zwei, die nicht durchkamen, und den
        // jetzigen -- `beginStart()` zaehlt den laufenden Start mit,
        // `failedStarts()` rechnet ihn wieder heraus.
        StartWatch::reset();
        StartWatch::beginStart();            // 1, kommt nicht durch
        StartWatch::beginStart();            // 2, kommt nicht durch
        StartWatch::beginStart();            // 3, der jetzige
        QCOMPARE(StartWatch::failedStarts(), 2);
        QVERIFY2(StartWatch::shouldStartSafely(),
                 "die Vorbedingung des Falls stimmt nicht");

        auto* mw = new MainWindow();
        mw->resize(1024, 700);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        QTest::qWait(150);
        QVERIFY2(mw->startedSafely(),
                 "nach zwei steckengebliebenen Starts muss das Hauptfenster die "
                 "gespeicherte Anordnung ueberspringen");
        QCOMPARE(mw->failedStartCount(), 2);
        mw->hide();
    }

    qunsetenv("LONGPATH_NO_DIALOGS");
    StartWatch::reset();
}

QTEST_MAIN(TstStartWatch)
#include "tst_start_watch.moc"
