// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_logbuch_karte_pfeil.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Pfeil „↗" NEBEN dem 3D-Knopf in der eingebetteten Logbuch-Karte
// (Live-Befund 2026-11-13: „neben dem 3d ist ein pfeil, der funktioniert
// auch nicht").
//
// Warum ein eigener Lauf, obwohl tst_logbook_durchgang das Herausloesen
// schon kennt: dort steht
//
//     emit map->popOutRequested();
//
// Der Prueflauf sendet das Signal SELBST. Ob der Knopf es ueberhaupt
// aussendet, hat damit nie jemand gemessen -- genau die Luecke, durch die
// ein Knopf faellt, der unsichtbar, abgeschaltet, verdeckt oder gar nicht
// verdrahtet ist. Dieselbe Klasse wie der 3D-Knopf nebenan (2026-11-10)
// und wie „Eigene Bewegung ist kein Ziehen": ein Prueflauf, der die
// Bedienung umgeht, belegt die Bedienung nicht.
//
// Hier klickt die Maus auf den echten Knopf im echten Logbuchfenster.

#include <QtTest>
#include <QLayout>

#include "core/AppSettings.h"
#include "gui/LogbookWindow.h"
#include "gui/QsoMapWindow.h"
#include "gui/WindowPlacement.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include "FensterFrist.h"

using namespace Longpath;

namespace {

QString writeAdif(const QDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("log.adi"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:5>K1ABC <QSO_DATE:8>20260902 <TIME_ON:6>143710 <BAND:3>20m <MODE:3>SSB "
            "<GRIDSQUARE:4>FN42 <MY_GRIDSQUARE:6>JN67UT <EOR>\n"
            "<CALL:6>DL1XYZ <QSO_DATE:8>20261013 <TIME_ON:6>081500 <BAND:2>2m <MODE:3>FT8 "
            "<GRIDSQUARE:6>JO62QM <MY_GRIDSQUARE:6>JN67UT <EOR>\n");
    f.close();
    return path;
}

// Der Pfeil traegt keinen Namen, nur sein Zeichen -- gesucht wird er
// darum in der Leiste der eingebetteten Karte.
QPushButton* pfeilIn(QsoMapWindow* karte)
{
    for (QPushButton* b : karte->findChildren<QPushButton*>()) {
        if (b->text() == QString::fromUtf8("↗")) { return b; }
    }
    return nullptr;
}

// Jedes eigene Kartenfenster ausser der eingebetteten Spalte.
QsoMapWindow* eigenesFenster(QWidget* unter, QsoMapWindow* ausser)
{
    for (QsoMapWindow* m : unter->findChildren<QsoMapWindow*>()) {
        if (m != ausser && m->isWindow() && m->isVisible()) { return m; }
    }
    return nullptr;
}

} // namespace

class TstLogbuchKartePfeil : public QObject { Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookMode"),
                                        QStringLiteral("Log"));
    }

    // ── Der Pfeil ist da, sichtbar, bedienbar und frei ───────────────
    //
    // Vier Fragen, die ein „tut nichts" erklaeren koennen, bevor man im
    // Verhalten sucht: gibt es ihn, sieht man ihn, kann man ihn
    // druecken, und liegt nichts darueber.
    void pfeil_ist_erreichbar()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY2(karte, "keine eingebettete Karte im Logbuch");
        QTRY_VERIFY(karte->isVisible());

        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY2(pfeil, "kein Pfeil-Knopf in der Kartenleiste");
        QTRY_VERIFY2(pfeil->isVisible(),
                     "der Pfeil ist unsichtbar — setEmbedded() blendet ihn ein");
        QVERIFY2(pfeil->isEnabled(), "der Pfeil ist abgeschaltet");

        // Und nichts liegt darueber: ein verdeckter Knopf nimmt den Klick
        // nicht an, sieht aber bedienbar aus.
        QWidget* getroffen = w.childAt(pfeil->mapTo(&w, pfeil->rect().center()));
        QVERIFY2(getroffen == pfeil, "ueber dem Pfeil liegt etwas");
    }

    // ── Ein echter Mausklick oeffnet das eigene Kartenfenster ────────
    void mausklick_oeffnet_das_eigene_fenster()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte);
        QTRY_VERIFY(karte->isVisible());
        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY(pfeil);
        QTRY_VERIFY(pfeil->isVisible());

        QVERIFY2(!eigenesFenster(&w, karte),
                 "ein eigenes Kartenfenster stand schon offen");

        // Die Maus, nicht das Signal.
        QTest::mouseClick(pfeil, Qt::LeftButton, Qt::NoModifier,
                          pfeil->rect().center());
        QCoreApplication::processEvents();

        QsoMapWindow* gross = nullptr;
        QTRY_VERIFY2((gross = eigenesFenster(&w, karte)) != nullptr,
                     "der Klick auf den Pfeil oeffnete kein eigenes Fenster");
        QVERIFY(QTest::qWaitForWindowExposed(gross, kFensterFrist));

        // Und die eingebettete Spalte bleibt, wo sie war -- der Pfeil
        // loest heraus, er verschiebt nicht.
        QVERIFY2(karte->isVisible(), "die Kartenspalte verschwand beim Herausloesen");

        gross->close();
        QCoreApplication::processEvents();
    }

    // ── Es verschwindet nicht unter dem Logbuch ──────────────────────
    //
    // Der eigentliche Live-Befund. Das Fenster ging auf und arbeitete,
    // lag aber deckungsgleich hinter dem Logbuch (gemessen: Karte
    // 1470x923 an (0,33), Logbuch 1468x790 an (1,109)) -- und damit
    // sah der Betreiber weiter das Logbuch.
    //
    // Hier wird die gespeicherte Lage absichtlich auf die des Logbuchs
    // gesetzt. Danach muss das Fenster woanders liegen.
    //
    // Die Masse sind nicht beliebig, und der erste Anlauf mass nichts:
    // mit einem bildschirmfuellenden Logbuch (798x757 auf dem 800x800
    // des Offscreen-Plugins) passt `restoreGeometry` die Karte von sich
    // aus in den Schirm ein, die Deckungsgleichheit kommt gar nicht
    // zustande, und der Lauf blieb auch mit stillgelegter Behebung
    // gruen. Gemerkt an der Gegenprobe, nicht am Lauf. Darum klein:
    // beide Fenster passen nebeneinander in den Schirm, Qt rueckt
    // nichts zurecht, und was gemessen wird, ist Longpaths Ausweichen.
    void es_versteckt_sich_nicht_hinter_dem_logbuch()
    {
        QTemporaryDir dir;
        // Das Logbuch GROSS, die Karte klein darin -- nur so wird sie
        // wirklich ganz verdeckt. Andersherum (der erste Anlauf) ist die
        // Karte wegen ihrer Mindesthoehe hoeher als das Logbuch, ragt
        // unten heraus und ist nie mehr als zu zwei Dritteln verdeckt;
        // der Lauf blieb dann auch mit stillgelegter Behebung gruen.
        LogbookWindow w(writeAdif(QDir(dir.path())));
        // Die linke Haelfte, volle Hoehe. Beides hat einen Grund:
        //   - volle Hoehe, damit die Karte mit ihrer Mindesthoehe (rund
        //     540) ganz hineinpasst und wirklich verdeckt wird,
        //   - nur die Haelfte der Breite, damit rechts daneben ein Platz
        //     frei bleibt, der breit genug fuer die Karte ist.
        // Auf dem kleinen Offscreen-Schirm (800x800) heisst das 400x800
        // gegen eine Karte von 317x536 -- beides geht sich aus, und der
        // Lauf misst auch in der CI statt sich zu ueberspringen.
        QScreen* schirm0 = QGuiApplication::primaryScreen();
        QVERIFY(schirm0);
        const QRect flaeche = schirm0->availableGeometry();
        w.setGeometry(QRect(flaeche.topLeft(),
                            QSize(flaeche.width() / 2, flaeche.height())));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        // Die Karte oeffnet INNERHALB des Logbuchs. An die obere linke
        // Ecke, nicht in die Mitte: die Karte hat eine Mindesthoehe von
        // rund 540, und zentriert ragte sie unten heraus -- die Verdeckung
        // blieb dann bei 0,63 und der Lauf blieb auch ohne Behebung
        // gruen. Auch das kam aus der Gegenprobe, nicht aus dem Lauf.
        QsoMapWindow drin;
        drin.setMinimumSize(0, 0);
        const QRect lb = w.frameGeometry();
        drin.setGeometry(QRect(lb.topLeft() + QPoint(20, 20), QSize(240, 180)));
        AppSettings::instance().setValue(QStringLiteral("QsoMapGeometry"),
                                        drin.saveGeometry());

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte);
        QTRY_VERIFY(karte->isVisible());
        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY(pfeil);
        QTRY_VERIFY(pfeil->isVisible());

        QTest::mouseClick(pfeil, Qt::LeftButton, Qt::NoModifier,
                          pfeil->rect().center());
        QCoreApplication::processEvents();

        QsoMapWindow* gross = nullptr;
        QTRY_VERIFY((gross = eigenesFenster(&w, karte)) != nullptr);
        QVERIFY(QTest::qWaitForWindowExposed(gross, kFensterFrist));

        // Gemessen wird das ERGEBNIS, nicht der Weg: die gespeicherte
        // Lage war deckungsgleich, am Ende darf sie es nicht sein.
        //
        // Nicht die Lage „vorher" gegen „nachher" -- der erste Anlauf tat
        // das und uebersah, dass `qWaitForWindowExposed` Ereignisse
        // verarbeitet und das Ausweichen damit laengst gelaufen ist, wenn
        // die erste Messung kommt. Unter cocoa lag die Karte beim ersten
        // Blick schon bei (576,48), und der Lauf schloss daraus, der Fall
        // sei gar nicht hergestellt.
        //
        // Dass dieser Lauf wirklich etwas misst, steht nicht in ihm,
        // sondern in der Gegenprobe: mit stillgelegter Behebung
        // (Schwelle auf 1.01) faellt er.
        //
        // Ist die Karte so gross wie der Schirm, kann sie nirgends
        // ausweichen -- dann tritt das Logbuch zurueck, und eine
        // Fensterreihenfolge ist unter dem Offscreen-Plugin nicht
        // messbar („This plugin does not support raise()"). Darum nur
        // dort pruefen, wo es einen Platz gibt.
        QScreen* schirm = gross->screen() ? gross->screen()
                                          : QGuiApplication::primaryScreen();
        QVERIFY(schirm);
        const bool gibtPlatz = !Longpath::freierPlatz(
            gross->frameGeometry().size(), schirm->availableGeometry(),
            { w.frameGeometry() }).isEmpty();
        if (!gibtPlatz) {
            qInfo().noquote() << "Logbuch" << w.frameGeometry()
                              << "Karte" << gross->frameGeometry();
            gross->close();
            QCoreApplication::processEvents();
            AppSettings::instance().setValue(QStringLiteral("QsoMapGeometry"), QByteArray{});
            QSKIP("Neben dem Logbuch ist kein Platz, der fuer die Karte reicht "
                  "— auf dem 800 Punkte breiten Offscreen-Schirm laesst die "
                  "Mindestbreite des Logbuchs (592) keinen. Dann tritt das "
                  "Logbuch zurueck, und eine Fensterreihenfolge ist offscreen "
                  "nicht messbar. Mit -platform cocoa laeuft der Fall.");
        }

        QTRY_VERIFY_WITH_TIMEOUT(
            Longpath::coveredFraction(gross->frameGeometry(),
                                      { w.frameGeometry() }) < 0.75,
            5000);
        qInfo().noquote() << "Logbuch" << w.frameGeometry()
                          << "Karte" << gross->frameGeometry()
                          << "verdeckt"
                          << Longpath::coveredFraction(gross->frameGeometry(),
                                                       { w.frameGeometry() });
        gross->close();
        QCoreApplication::processEvents();
        AppSettings::instance().setValue(QStringLiteral("QsoMapGeometry"), QByteArray{});
    }

    // ── Eine Lage, die der Betreiber selbst gewaehlt hat, bleibt ──────
    //
    // Die Gegenprobe zum Fall darueber: steht die Karte frei, wird sie
    // NICHT verschoben. Sonst waere aus der Behebung eine zweite
    // Ueberraschung geworden.
    void freie_lage_bleibt_unangetastet()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        // Klein und links oben, damit daneben ueberhaupt Platz bleibt:
        // der Offscreen-Schirm misst 800x800, und das Logbuch fuellt ihn
        // sonst ganz aus (gemessen 798x757). Die Mindestgroesse gilt der
        // Bedienbarkeit, nicht dieser Messung -- hier geht es nur um
        // Lagen, und die Karte liegt in beiden Faellen woanders.
        w.setMinimumSize(0, 0);
        w.resize(360, 280);
        w.move(0, 0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        // Eine Lage neben dem Logbuch, INNERHALB des Prueschirms:
        // restoreGeometry holt ein Fenster, das darueber hinausragt, von
        // sich aus wieder herein -- der Prueflauf haette dann das
        // Zurechtruecken von Qt gemessen statt das Ausweichen von
        // Longpath. Gesucht wird mit derselben Funktion, die auch das
        // Programm benutzt.
        QScreen* schirm = QGuiApplication::primaryScreen();
        QVERIFY(schirm);
        const QRect daneben_rect = Longpath::freierPlatz(
            QSize(200, 100), schirm->availableGeometry(), { w.frameGeometry() });
        QVERIFY2(!daneben_rect.isEmpty(),
                 qPrintable(QStringLiteral("kein freier Platz fuer 200x100 neben dem "
                                           "Logbuch %1 auf dem Schirm %2")
                                .arg(QDebug::toString(w.frameGeometry()),
                                     QDebug::toString(schirm->availableGeometry()))));
        QsoMapWindow daneben;
        daneben.setGeometry(daneben_rect);
        AppSettings::instance().setValue(QStringLiteral("QsoMapGeometry"),
                                        daneben.saveGeometry());

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte);
        QTRY_VERIFY(karte->isVisible());
        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY(pfeil);
        QTRY_VERIFY(pfeil->isVisible());

        QTest::mouseClick(pfeil, Qt::LeftButton, Qt::NoModifier,
                          pfeil->rect().center());
        QCoreApplication::processEvents();
        QsoMapWindow* gross = nullptr;
        QTRY_VERIFY((gross = eigenesFenster(&w, karte)) != nullptr);
        QVERIFY(QTest::qWaitForWindowExposed(gross, kFensterFrist));

        // Gemessen wird, was die Behebung verspricht: eine Lage, die das
        // Logbuch nicht verdeckt, wird nicht angefasst. Wohin Qt das
        // Fenster beim Wiederherstellen gelegt hat, ist dabei egal --
        // entscheidend ist, dass es danach dort LIEGEN BLEIBT.
        const QPoint vorDemUmlauf = gross->geometry().topLeft();
        QVERIFY2(Longpath::coveredFraction(gross->frameGeometry(),
                                           { w.frameGeometry() }) < 0.75,
                 "die Probelage liegt doch unter dem Logbuch");
        QTest::qWait(300);   // dem Ausweich-Umlauf Zeit geben, falls er kaeme
        qInfo().noquote() << "Logbuch" << w.frameGeometry()
                          << "Karte vorher" << vorDemUmlauf
                          << "nachher" << gross->geometry().topLeft();
        QCOMPARE(gross->geometry().topLeft(), vorDemUmlauf);
        gross->close();
        QCoreApplication::processEvents();
        AppSettings::instance().setValue(QStringLiteral("QsoMapGeometry"), QByteArray{});
    }

    // ── Zweimal hintereinander ───────────────────────────────────────
    //
    // Das Fenster wird beim ersten Mal angelegt und danach
    // wiederverwendet; ein zweiter Druck nach dem Schliessen muss es
    // wieder zeigen.
    void zweiter_druck_zeigt_es_wieder()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte);
        QTRY_VERIFY(karte->isVisible());
        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY(pfeil);
        QTRY_VERIFY(pfeil->isVisible());

        for (int runde = 1; runde <= 2; ++runde) {
            QTest::mouseClick(pfeil, Qt::LeftButton, Qt::NoModifier,
                              pfeil->rect().center());
            QCoreApplication::processEvents();
            QsoMapWindow* gross = nullptr;
            QTRY_VERIFY2((gross = eigenesFenster(&w, karte)) != nullptr,
                         qPrintable(QStringLiteral("Runde %1: kein eigenes Fenster")
                                        .arg(runde)));
            QVERIFY(QTest::qWaitForWindowExposed(gross, kFensterFrist));
            gross->close();
            QCoreApplication::processEvents();
            QTRY_VERIFY(!eigenesFenster(&w, karte));
        }
    }
};

QTEST_MAIN(TstLogbuchKartePfeil)
#include "tst_logbuch_karte_pfeil.moc"
