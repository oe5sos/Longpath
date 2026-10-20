// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall.
//
// Der Schutzrahmen (2026-10-14).
//
// Der Betreiber, mit zwei Bildschirmfotos: „ich muss immer auf
// Vollformat klicken, um alle Fenster und Icons zu sehen" -- und gleich
// darauf der Vorschlag, der die Loesung ist: „es sollte vielleicht einen
// grossen Rahmen geben, ueber den man nichts ziehen kann und keine
// Fenster anordnen kann."
//
// DER BEFUND, in seiner Einstellungsdatei nachgelesen: beide
// Layoutprofile tragen `"mainWindow":{"fullScreen":true}`, und die
// schwebenden Felder stehen mit ABSOLUTEN Bildschirmkoordinaten darin
// (BwFilter y=128, pan-0 y=304, Rotor x=1184 h=704). Im Vollbild beginnt
// das Hauptfenster oben; ohne Vollbild rutscht es um die Titelleiste
// nach unten -- die schwebenden Fenster bleiben stehen und liegen dann
// ueber der Knopfreihe. Die Ueberschriften (BAND, MODE, FILTER …)
// schauen heraus, die Knoepfe darunter sind verdeckt. Genau das zeigen
// die zwei Fotos.
//
// Zwei Dinge, die ich auf dem Weg dorthin NACHGEMESSEN und damit
// AUSGESCHLOSSEN habe -- sie standen als Verdacht im Raum:
//
//   * Die Kommandoleiste wird NICHT gequetscht. Gemessen an einem
//     echten Fenster von 1452 bis 800 px Hoehe: sie behaelt ihre 64 px
//     bei jeder Hoehe, auch mit Martins Profil.
//   * Es ist KEINE zu grosse Mindesthoehe. `minimumSizeHint` des
//     Fensters ist 1231 x 396 -- die Hoehe ist unkritisch.

#include <QtTest/QtTest>

#include "gui/MainWindow.h"

#include <QStatusBar>
#include "gui/WindowPlacement.h"

using namespace Longpath;

class TstSchutzrahmen : public QObject {
    Q_OBJECT

private:
    // Ein Schirm, wie Martins: 1728 x 1117 Punkte, oben die Menueleiste.
    static QRect schirm() { return QRect(0, 25, 1728, 1092); }
    // Die Kommandoleiste des Fensters, wenn es NICHT im Vollbild ist:
    // Titelleiste (28) + Menuezeile unter der Systemleiste.
    static QRect obenStreifen() { return QRect(0, 96, 1728, 64); }
    static QRect untenStreifen() { return QRect(0, 1071, 1728, 46); }
    static QList<QRect> rahmen() { return {obenStreifen(), untenStreifen()}; }

private slots:
    void einFensterAufDerKommandoleisteWirdHERUNTERGESCHOBEN()
    {
        // Genau Martins Fall: der Bandbreitenfilter stand im Vollbild bei
        // y=128 richtig unter der Leiste. Ohne Vollbild liegt die Leiste
        // bei 96..159 -- das Fenster deckt sie zur Haelfte zu.
        const QRect wunsch(8, 128, 664, 176);
        const QRect neu = ausSperrstreifenSchieben(wunsch, rahmen(), schirm());

        QVERIFY2(!neu.intersects(obenStreifen()),
                 qPrintable(QStringLiteral("liegt immer noch auf der Leiste: %1,%2 %3x%4")
                                .arg(neu.x()).arg(neu.y()).arg(neu.width()).arg(neu.height())));
        // Es rutscht nach UNTEN, direkt unter die Leiste -- nicht nach
        // oben hinter die Titelleiste, und nicht irgendwohin.
        QCOMPARE(neu.top(), obenStreifen().bottom() + 1);
        // Breite, Hoehe und die waagerechte Lage bleiben: die Groesse
        // gehoert dem Betreiber, und seitlich auszuweichen wuerde eine
        // Anordnung zerreissen, die er nebeneinander gestellt hat.
        QCOMPARE(neu.size(), wunsch.size());
        QCOMPARE(neu.left(), wunsch.left());
    }

    void einFensterAufDerStatuszeileWirdHINAUFGESCHOBEN()
    {
        const QRect wunsch(8, 1050, 400, 160);     // ragt in die untere Leiste
        const QRect neu = ausSperrstreifenSchieben(wunsch, rahmen(), schirm());
        QVERIFY(!neu.intersects(untenStreifen()));
        QCOMPARE(neu.bottom(), untenStreifen().top() - 1);
        QCOMPARE(neu.size(), wunsch.size());
    }

    void einFensterDAZWISCHENWirdNICHTAngefasst()
    {
        // Die wichtigste Eigenschaft: es darf nur eingreifen, wo es muss.
        // Ein Werkzeug, das jedes Fenster bei jeder Groessenaenderung
        // verschiebt, ist schlimmer als das Uebel.
        const QRect wunsch(8, 304, 1176, 352);
        QCOMPARE(ausSperrstreifenSchieben(wunsch, rahmen(), schirm()), wunsch);
    }

    void einFensterAufEINEMZWEITENSchirmBleibtWoEsIst()
    {
        // Die Sperrstreifen sind die des Hauptfensters. Ein Fenster, das
        // der Betreiber auf einen zweiten Schirm gezogen hat, beruehrt
        // sie gar nicht -- und wird darum nicht angefasst.
        const QRect zweiter(1728, 0, 1920, 1080);
        const QRect wunsch(1800, 100, 400, 300);   // in Hoehe der oberen Leiste
        QCOMPARE(ausSperrstreifenSchieben(wunsch, rahmen(), zweiter), wunsch);
    }

    void ohneSperrenGeschiehtNichts()
    {
        const QRect wunsch(8, 128, 664, 176);
        QCOMPARE(ausSperrstreifenSchieben(wunsch, {}, schirm()), wunsch);
    }

    void einFensterDasNIRGENDSMehrHinpasstBleibtUnveraendert()
    {
        // Lieber verdeckt als halb vom Schirm geschoben: ein Fenster,
        // dessen Titelleiste ausserhalb liegt, laesst sich nicht mehr
        // fassen. Hier ist das Fenster hoeher als der Platz zwischen den
        // beiden Leisten.
        const QRect wunsch(8, 100, 400, 1000);
        const QRect neu = ausSperrstreifenSchieben(wunsch, rahmen(), schirm());
        QCOMPARE(neu, wunsch);
    }

    void derKUERZERE_WegGewinnt()
    {
        // Ein Fenster, das nur mit seiner UNTERSTEN Kante in die obere
        // Leiste ragt, soll hinauf und nicht durch die ganze Leiste
        // hindurch nach unten.
        const QRect wunsch(8, 60, 400, 50);        // 60..109, Leiste ab 96
        const QRect neu = ausSperrstreifenSchieben(wunsch, rahmen(), schirm());
        QVERIFY(!neu.intersects(obenStreifen()));
        QCOMPARE(neu.bottom(), obenStreifen().top() - 1);
        QVERIFY2(neu.top() < wunsch.top(),
                 "nach unten geschoben, obwohl der Weg nach oben kuerzer war");
    }

    void mehrereStreifenNacheinander()
    {
        // Wer dem einen ausweicht, darf nicht im anderen landen. Mit
        // einem schmalen Schirm, auf dem die beiden Leisten nah
        // beieinander liegen.
        const QRect eng(0, 0, 800, 400);
        const QList<QRect> zwei{QRect(0, 100, 800, 60), QRect(0, 240, 800, 60)};
        const QRect wunsch(0, 120, 300, 100);      // mitten im oberen
        const QRect neu = ausSperrstreifenSchieben(wunsch, zwei, eng);
        QVERIFY2(!neu.intersects(zwei.at(0)) && !neu.intersects(zwei.at(1)),
                 qPrintable(QStringLiteral("liegt auf einem Streifen: %1,%2 %3x%4")
                                .arg(neu.x()).arg(neu.y())
                                .arg(neu.width()).arg(neu.height())));
        QVERIFY(eng.contains(neu));
    }

    // ── Am ECHTEN Fenster ────────────────────────────────────────────────

    void dasFensterKenntSeineBeidenBedienleisten()
    {
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));

        const QList<QRect> sperren = mw.schutzrahmen();
        QCOMPARE(sperren.size(), 2);          // Kommandoleiste + Statuszeile
        for (const QRect& sp : sperren) {
            QVERIFY2(sp.isValid(), "ein Streifen ohne Flaeche");
            QVERIFY2(sp.height() > 0, "ein Streifen ohne Hoehe");
            // Ueber die GANZE Fensterbreite: ein Fenster, das nur halb
            // darueberliegt, verdeckt die Leiste trotzdem.
            QCOMPARE(sp.width(), mw.width());
        }
        // Der obere liegt oben, der untere unten.
        QVERIFY(sperren.at(0).top() < sperren.at(1).top());
        // Und beide liegen IM Fenster.
        const QRect f = mw.frameGeometry();
        for (const QRect& sp : sperren) {
            QVERIFY2(f.intersects(sp), "ein Streifen liegt ausserhalb des Fensters");
        }
    }

    void eineVERSTECKTELeisteErgibtKEINENStreifen()
    {
        // Hier stand zuerst „vor dem Zeigen gibt es keinen Rahmen" --
        // und der Fall war ROT. Der Grund ist ein Befund fuer sich:
        // **MainWindow ist schon im Konstruktor sichtbar**, lange bevor
        // der Aufrufer `show()` ruft. `isVisible()` taugt hier also
        // nicht als Wache, und die Wache im Code ist entfernt.
        //
        // Was wirklich zaehlt, steht hier: eine Leiste, die der
        // Betreiber abgeschaltet hat, darf keinen Sperrstreifen
        // erzeugen -- sonst bliebe ein Band gesperrt, auf dem gar nichts
        // mehr liegt.
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        QCOMPARE(mw.schutzrahmen().size(), 2);

        mw.statusBar()->hide();
        QCoreApplication::processEvents();
        QCOMPARE(mw.schutzrahmen().size(), 1);

        mw.statusBar()->show();
        QCoreApplication::processEvents();
        QCOMPARE(mw.schutzrahmen().size(), 2);
    }

    void einSchwebendesFensterAufDerLeisteWirdHERUNTERGEHOLT()
    {
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));

        const QList<QRect> sperren = mw.schutzrahmen();
        QCOMPARE(sperren.size(), 2);
        const QRect oben = sperren.at(0);

        // Ein schwebendes Fenster MIT dem Hauptfenster als Elternteil --
        // genau so entstehen die abgeloesten Applets.
        auto* schweber = new QWidget(&mw, Qt::Window);
        schweber->setObjectName(QStringLiteral("Pruefschweber"));
        schweber->resize(400, 180);
        schweber->show();
        QVERIFY(QTest::qWaitForWindowExposed(schweber, 15000));
        // Mitten auf die Kommandoleiste setzen.
        //
        // QTRY statt processEvents()+QVERIFY (2026-10-18): genau hier
        // ist der Lauf in der CI umgefallen -- und zwar an der
        // VORBEDINGUNG („der Pruefaufbau liegt gar nicht auf der
        // Leiste"), nicht am Ergebnis. Lokal 19/19 in drei Durchgaengen,
        // in der CI einmal rot; der Unterschied steht in der Laufzeit
        // derselben Reihe: 12,86 s dort gegen 3,2 s hier, weil daneben
        // 487 Prueflaeufe die Maschine belegten.
        //
        // move() auf einem Fenster ist eine BITTE an das Fenstersystem,
        // keine Zuweisung. Ein einzelnes processEvents() reicht dafuer
        // auf einer unbelasteten Maschine und unter Last eben nicht --
        // dann liest frameGeometry() noch die alte Lage.
        schweber->move(oben.left() + 20, oben.top() + 10);
        QTRY_VERIFY_WITH_TIMEOUT(
            schweber->frameGeometry().intersects(oben), 5000);

        const int verschoben = mw.schwebendeAusDemSchutzrahmenHolen();
        QCOMPARE(verschoben, 1);
        // Dieselbe Begruendung wie oben -- das Verschieben selbst ist
        // auch nur eine Bitte.
        QTRY_VERIFY_WITH_TIMEOUT(
            !schweber->frameGeometry().intersects(oben), 5000);
        QVERIFY2(!schweber->frameGeometry().intersects(oben),
                 qPrintable(QStringLiteral("liegt immer noch auf der Leiste: %1,%2")
                                .arg(schweber->x()).arg(schweber->y())));
        // Die Groesse gehoert dem Betreiber.
        QCOMPARE(schweber->size(), QSize(400, 180));

        // Und ein zweiter Durchgang fasst nichts mehr an -- sonst
        // wanderte das Fenster bei jeder Groessenaenderung weiter.
        QCOMPARE(mw.schwebendeAusDemSchutzrahmenHolen(), 0);
    }

    void einFensterNEBENDerLeisteWirdNICHTAngefasst()
    {
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        const QList<QRect> sperren = mw.schutzrahmen();
        QCOMPARE(sperren.size(), 2);

        auto* schweber = new QWidget(&mw, Qt::Window);
        schweber->resize(300, 120);
        schweber->show();
        QVERIFY(QTest::qWaitForWindowExposed(schweber, 15000));
        // Zwischen die beiden Leisten.
        const int mitte = (sperren.at(0).bottom() + sperren.at(1).top()) / 2;
        schweber->move(sperren.at(0).left() + 40, mitte - 60);
        QCoreApplication::processEvents();
        const QRect vorher = schweber->frameGeometry();

        QCOMPARE(mw.schwebendeAusDemSchutzrahmenHolen(), 0);
        QCoreApplication::processEvents();
        QCOMPARE(schweber->frameGeometry(), vorher);
    }

    // ── Lagen relativ zum Fenster (2026-10-14) ───────────────────────────

    void dieLageWirdRELATIVZumFensterGemerkt()
    {
        // DIE eigentliche Heilung. Bis heute standen nur absolute
        // Bildschirmkoordinaten im Profil -- eine Anordnung aus dem
        // Vollbild passte in keiner anderen Fenstergroesse.
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        QCoreApplication::processEvents();

        const QPoint ecke = mw.frameGeometry().topLeft();
        const QRect lage(ecke.x() + 100, ecke.y() + 200, 400, 180);

        QVariantMap eintrag;
        mw.merkeLage(eintrag, lage);
        // Beides steht drin: absolut (fuer aeltere Fassungen und fuer
        // ein Fenster auf einem zweiten Schirm) UND relativ.
        QCOMPARE(eintrag.value(QStringLiteral("x")).toInt(), lage.x());
        QCOMPARE(eintrag.value(QStringLiteral("y")).toInt(), lage.y());
        QCOMPARE(eintrag.value(QStringLiteral("dx")).toInt(), 100);
        QCOMPARE(eintrag.value(QStringLiteral("dy")).toInt(), 200);

        // Zurueckgelesen am SELBEN Ort: dieselbe Lage.
        QCOMPARE(mw.lageAusProfil(eintrag), lage);
    }

    void dieAnordnungFOLGTDemFenster()
    {
        // Der Fall, um den es geht: gespeichert im Vollbild, gelesen
        // ohne. Das Hauptfenster steht dann woanders -- und das
        // schwebende Fenster soll MITGEHEN, nicht stehenbleiben.
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        QCoreApplication::processEvents();

        const QPoint vorher = mw.frameGeometry().topLeft();
        QVariantMap eintrag;
        mw.merkeLage(eintrag, QRect(vorher.x() + 8, vorher.y() + 300, 664, 176));

        // Das Fenster wandert (so wie beim Verlassen des Vollbilds: die
        // Titelleiste schiebt es nach unten).
        mw.move(vorher + QPoint(0, 44));
        QCoreApplication::processEvents();

        const QRect neu = mw.lageAusProfil(eintrag);
        QCOMPARE(neu.topLeft(), mw.frameGeometry().topLeft() + QPoint(8, 300));
        QCOMPARE(neu.size(), QSize(664, 176));
    }

    void einALTESProfilOHNEdxdyWirdWEITERGELESEN()
    {
        // Rueckwaertssicherheit: Martins Profil ist zwei Megabyte gross
        // und traegt heute nur x/y/w/h. Es darf nicht beim ersten Start
        // nach dieser Aenderung seine Anordnung verlieren.
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));

        QVariantMap alt;
        alt.insert(QStringLiteral("x"), 8);
        alt.insert(QStringLiteral("y"), 128);
        alt.insert(QStringLiteral("w"), 664);
        alt.insert(QStringLiteral("h"), 176);
        QCOMPARE(mw.lageAusProfil(alt), QRect(8, 128, 664, 176));
    }

    // ── Der sichtbare Hinweis (2026-10-14) ───────────────────────────────

    void beimZiehenWirdDerRahmenSICHTBAR()
    {
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        QCoreApplication::processEvents();
        // WARTEN, nicht sofort pruefen: der Melder liegt statisch (siehe
        // den Prueffall unten), sein Hinweis ueberlebt also das Ende des
        // vorigen Falls und geht erst nach seiner eigenen Frist wieder
        // weg. Hier war ein nacktes QVERIFY2 -- auf dieser Maschine
        // immer gruen, auf dem Linux-Runner am 2026-10-20 rot (#279,
        // Shard 3/4): dort war der Hinweis des vorigen Falls noch da.
        // Dieselbe Lehre wie bei der Fensterfrist: eine Frist, die hier
        // reicht, reicht dort nicht.
        QTRY_VERIFY_WITH_TIMEOUT(!mw.schutzrahmenHinweisSichtbarFuerTest(), 5000);

        // Ein schwebendes Fenster meldet eine Bewegung -- genau das tut
        // `meldeZug()` aus dem moveEvent der drei Fensterarten.
        auto* schweber = new QWidget(&mw, Qt::Window);
        schweber->resize(300, 120);
        schweber->show();
        QVERIFY(QTest::qWaitForWindowExposed(schweber, 15000));

        // Weit weg vom Rahmen: nichts passiert. Der Hinweis soll nicht
        // bei jeder Bewegung blinken, sondern wenn man IHM nahe kommt.
        const QList<QRect> sperren = mw.schutzrahmen();
        QCOMPARE(sperren.size(), 2);
        const int mitte = (sperren.at(0).bottom() + sperren.at(1).top()) / 2;
        schweber->move(sperren.at(0).left() + 40, mitte - 60);
        QCoreApplication::processEvents();
        meldeZug(schweber);
        QCoreApplication::processEvents();
        QVERIFY2(!mw.schutzrahmenHinweisSichtbarFuerTest(),
                 "der Hinweis blinkt, obwohl das Fenster weit weg ist");

        // Auf den Rahmen zu: jetzt soll er zu sehen sein.
        schweber->move(sperren.at(0).left() + 40, sperren.at(0).top() + 8);
        QCoreApplication::processEvents();
        meldeZug(schweber);
        QCoreApplication::processEvents();
        QVERIFY2(mw.schutzrahmenHinweisSichtbarFuerTest(),
                 "der Rahmen bleibt beim Ziehen unsichtbar");

        // Und er geht von selbst wieder weg.
        QTRY_VERIFY_WITH_TIMEOUT(!mw.schutzrahmenHinweisSichtbarFuerTest(), 3000);
    }

    void einFREMDESFensterLaesstDenRahmenInRuhe()
    {
        // Der Melder liegt statisch. Er darf nur auf die eigenen
        // Schwebefenster hoeren -- sonst zeigte ein beliebiges
        // Werkzeugfenster den Rahmen eines fremden Hauptfensters.
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        QCoreApplication::processEvents();

        auto* fremd = new QWidget(nullptr, Qt::Window);   // KEIN Elternteil
        fremd->resize(200, 100);
        fremd->show();
        QVERIFY(QTest::qWaitForWindowExposed(fremd, 15000));
        meldeZug(fremd);
        QCoreApplication::processEvents();
        QVERIFY(!mw.schutzrahmenHinweisSichtbarFuerTest());
        delete fremd;
    }

};

QTEST_MAIN(TstSchutzrahmen)
#include "tst_schutzrahmen.moc"
