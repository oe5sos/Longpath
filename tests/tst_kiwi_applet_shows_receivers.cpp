// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das KiwiSDR-Applet zeigt, was ihm gegeben wurde — und zwar dort, wo
// es der Betreiber liest.
//
// Der Grund fuer diesen Zuschnitt steht in der Sitzung vom
// 2026-08-22/23 mehrfach: Tests waren gruen, weil sie die falsche
// Stelle gemessen haben. Ein Test, der nur prueft, ob setReceivers()
// ohne Absturz zurueckkehrt, sagt nichts. Also wird hier der TEXT der
// erzeugten Beschriftungen gelesen — das, was am Bildschirm steht.
//
// Portiert (Stufe 4) aus AetherSDRs KiwiSdrApplet; der Test ist
// eigener Zuschnitt, Aether hat fuer dieses Applet keinen.

#include <QtTest>
#include <QLocale>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>

#include "gui/applets/KiwiSdrApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

// Alle sichtbaren Beschriftungstexte eines Baums, in einer Zeichenkette.
QString allText(QWidget* w)
{
    QStringList out;
    for (QLabel* l : w->findChildren<QLabel*>()) {
        if (l->isVisibleTo(w) && !l->text().isEmpty()) { out << l->text(); }
    }
    return out.join(QStringLiteral(" | "));
}

} // namespace

class TstKiwiAppletShowsReceivers : public QObject
{
    Q_OBJECT

private slots:
    void leerZeigtDenHinweis()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));

        const QString txt = allText(&applet);
        qInfo().noquote() << "Leer:" << txt;
        QVERIFY2(txt.contains(KiwiSdrApplet::tr("No KiwiSDR receive antenna configured")),
                 qPrintable(txt));

        auto* list = applet.findChild<QListWidget*>();
        QVERIFY(list);
        QVERIFY2(!list->isVisible(),
                 "Ohne Empfaenger darf die Liste nicht dastehen");
    }

    void empfaengerStehenMitZustandDa()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));

        KiwiSdrReceiverStatus a;
        a.id     = QStringLiteral("kiwi-1");
        a.name   = QStringLiteral("Gmunden Nord");
        a.state  = KiwiSdrClient::State::Connected;
        a.detail = QStringLiteral("kiwi.example.at:8073");

        KiwiSdrReceiverStatus b;
        b.id    = QStringLiteral("kiwi-2");
        b.name  = QStringLiteral("Traunstein");
        b.state = KiwiSdrClient::State::Busy;

        applet.setReceivers({a, b});

        auto* list = applet.findChild<QListWidget*>();
        QVERIFY(list);
        QCOMPARE(list->count(), 2);
        QVERIFY(list->isVisible());

        const QString txt = allText(&applet);
        qInfo().noquote() << "Belegt:" << txt;
        QVERIFY2(txt.contains(QStringLiteral("Gmunden Nord")), qPrintable(txt));
        QVERIFY2(txt.contains(QStringLiteral("Traunstein")),   qPrintable(txt));
        QVERIFY2(txt.contains(QStringLiteral("Verbunden")),    qPrintable(txt));
        QVERIFY2(txt.contains(QStringLiteral("Belegt")),       qPrintable(txt));
        QVERIFY2(txt.contains(QStringLiteral("kiwi.example.at:8073")),
                 qPrintable(txt));
        // Ohne Scheibe muss das auch DASTEHEN, nicht bloss fehlen.
        QVERIFY2(txt.contains(KiwiSdrApplet::tr("Not assigned")),
                 qPrintable(txt));
    }

    // Martin 2026-11-25: "man sieht nur 3 kiwis" -- die Liste war auf
    // 176 px festgenagelt. Gemessen wird, wie viele Zeilen ganz im
    // Sichtfeld stehen, bevor und nachdem das Fenster hoeher gezogen wird.
    static int sichtbareZeilen(QListWidget* list)
    {
        int n = 0;
        const QRect view = list->viewport()->rect();
        for (int i = 0; i < list->count(); ++i) {
            if (view.contains(list->visualItemRect(list->item(i)))) { ++n; }
        }
        return n;
    }

    void dieListeWaechstMitDemFenster()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        QVector<KiwiSdrReceiverStatus> alle;
        for (int i = 0; i < 12; ++i) {
            KiwiSdrReceiverStatus r;
            r.id    = QStringLiteral("kiwi-%1").arg(i);
            r.name  = QStringLiteral("Kiwi %1").arg(i);
            r.state = KiwiSdrClient::State::Connected;
            alle.append(r);
        }
        applet.resize(320, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));
        applet.setReceivers(alle);
        auto* list = applet.findChild<QListWidget*>();
        QVERIFY(list);

        applet.resize(320, 400);
        QTest::qWait(50);
        const int klein = sichtbareZeilen(list);
        applet.resize(320, 1000);
        QTest::qWait(50);
        const int gross = sichtbareZeilen(list);
        qInfo() << "Zeilen sichtbar: klein" << klein << "gross" << gross;
        QVERIFY2(gross > klein, "Hoeheres Fenster muss mehr Zeilen zeigen");
        QVERIFY2(gross >= 8, qPrintable(QString::number(gross)));
        // Mehr Eintraege als Platz: es wird gescrollt, nicht abgeschnitten.
        QVERIFY(gross < 12 || list->verticalScrollBar()->maximum() == 0);
    }

    // Betreiber 2026-11-25: nach der Wahl eines Kiwi verschwand die
    // "Heard"-Pille -- der Name schob sie neben den Knoepfen aus dem Applet.
    void gewaehlterNameSchiebtKeinenKnopfHinaus()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.resize(300, 300);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));
        applet.setOwnSignalReceiverName(
            QStringLiteral("Ein sehr langer Name eines Kiwi mit viel Text drumherum"));
        QTest::qWait(50);
        auto* other = applet.findChild<QPushButton*>(QStringLiteral("kiwiOtherButton"));
        auto* heard = applet.findChild<QPushButton*>(QStringLiteral("kiwiHearingButton"));
        QVERIFY(other && heard);
        QVERIFY(heard->isVisible());
        QVERIFY(heard->width() >= 40);
        const QRect inApplet = QRect(QPoint(0, 0), applet.size());
        QVERIFY2(inApplet.contains(heard->mapTo(&applet, heard->rect().center())),
                 "Gehoert-Knopf liegt ausserhalb des Applets");
        QVERIFY(!(other->geometry().intersects(heard->geometry())));
    }

    // Anderer Kiwi / Gehoert sind Schaltflaechen: nach dem Klick darf nichts
    // eingerastet (dunkel) stehen bleiben.
    void aktionsKnoepfeRastenNichtEin()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));
        for (const char* name : {"kiwiOtherButton", "kiwiHearingButton"}) {
            auto* b = applet.findChild<QPushButton*>(QString::fromLatin1(name));
            QVERIFY(b);
            QVERIFY2(!b->isCheckable(), name);
            QTest::mouseClick(b, Qt::LeftButton);
            QVERIFY2(!b->isChecked(), name);
        }
        // Gegenprobe: Solo IST ein Schalter.
        auto* solo = applet.findChild<QPushButton*>(QStringLiteral("kiwiSoloButton"));
        QVERIFY(solo && solo->isCheckable());
    }

    // Bild 2026-11-25: in einer hohen Kachel klebten die Zeilen unten, mit einer
    // grossen Luecke unter den Knoepfen. Die Liste muss gleich unter dem
    // Namen beginnen, egal wie hoch das Fenster ist.
    void zeilenKlebenNichtAmUnterenRand()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.resize(320, 900);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));
        KiwiSdrReceiverStatus a;
        a.id = QStringLiteral("k1"); a.name = QStringLiteral("Eins");
        a.state = KiwiSdrClient::State::Connected;
        applet.setReceivers({a});
        QTest::qWait(80);
        auto* list = applet.findChild<QListWidget*>();
        auto* heard = applet.findChild<QPushButton*>(QStringLiteral("kiwiHearingButton"));
        QVERIFY(list && heard);
        const int gap = list->geometry().top() - heard->geometry().bottom();
        QVERIFY2(gap < 80, qPrintable(QStringLiteral("Luecke %1 px").arg(gap)));
    }

    // Bild 2026-11-25 (COMBINED): die "WF"-Pille rechts war abgeschnitten, weil
    // die Zeile so breit war wie ihr laengster Text. Sie muss in die Liste passen.
    void zeilenPassenInDieBreite()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.resize(260, 500);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));
        KiwiSdrReceiverStatus a;
        a.id = QStringLiteral("k1");
        a.name = QStringLiteral("Ein Kiwi mit einem recht langen Namen im Verzeichnis");
        a.state = KiwiSdrClient::State::Error;
        a.detail = QStringLiteral("SND requested uncompressed requested · W/F requested uncompressed requested und noch viel mehr Text dahinter");
        applet.setReceivers({a});
        QTest::qWait(80);
        auto* list = applet.findChild<QListWidget*>();
        QVERIFY(list);
        const int viewW = list->viewport()->width();
        QWidget* row = list->itemWidget(list->item(0));
        QVERIFY(row);
        QVERIFY2(row->width() <= viewW, qPrintable(QStringLiteral("Zeile %1 > Liste %2").arg(row->width()).arg(viewW)));
        for (QPushButton* b : row->findChildren<QPushButton*>()) {
            const QPoint right = b->mapTo(list->viewport(), QPoint(b->width() - 1, 0));
            QVERIFY2(right.x() < viewW, "Pille ragt ueber den rechten Rand");
        }
    }

    // Jede Kiwi-Zeile hat einen Ton-Schalter; er meldet den Wunsch.
    void jedeZeileHatEinenTonSchalter()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.resize(320, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));
        KiwiSdrReceiverStatus a;
        a.id = QStringLiteral("k1"); a.name = QStringLiteral("Eins");
        a.state = KiwiSdrClient::State::Disconnected;
        KiwiSdrReceiverStatus b;
        b.id = QStringLiteral("k2"); b.name = QStringLiteral("Zwei");
        b.state = KiwiSdrClient::State::Disconnected;
        applet.setReceivers({a, b});
        QTest::qWait(60);
        auto* list = applet.findChild<QListWidget*>();
        QSignalSpy spy(&applet, &KiwiSdrApplet::soundRequested);
        QWidget* row2 = list->itemWidget(list->item(1));
        auto* snd = row2->findChild<QPushButton*>(QStringLiteral("kiwiRowSound"));
        QVERIFY(snd);
        QVERIFY(!snd->isChecked());
        snd->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("k2"));
        QCOMPARE(spy.at(0).at(1).toBool(), true);
        // "Radio stumm" ist ein Schalter und meldet sich.
        auto* mute = applet.findChild<QPushButton*>(QStringLiteral("kiwiRadioMuteButton"));
        QVERIFY(mute && mute->isCheckable());
        QSignalSpy mspy(&applet, &KiwiSdrApplet::radioMuteToggled);
        mute->click();
        QCOMPARE(mspy.count(), 1);
    }

    void dieScheibeStehtDabeiUndFolgtIhr()
    {
        RadioModel model;
        KiwiSdrApplet applet(&model);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, 15000));

        SliceModel slice(0);
        slice.setFrequency(7'100'000.0);
        slice.setFilter(-2900, -100);

        KiwiSdrReceiverStatus r;
        r.id            = QStringLiteral("kiwi-1");
        r.name          = QStringLiteral("Gmunden Nord");
        r.state         = KiwiSdrClient::State::Camping;
        r.assignedSlice = &slice;
        applet.setReceivers({r});

        QString txt = allText(&applet);
        qInfo().noquote() << "Mit Scheibe:" << txt;
        // Das Dezimaltrennzeichen ist seit 2026-10-13 das des Systems
        // (src/gui/Zahlentext.h, anzeigeZahl()). Hier stand bis dahin
        // "7.100000" festgenagelt -- auf einem deutschen Rechner also
        // rot, und genau so ist dieser Fall aufgefallen: beim
        // Gesamtlauf, NACH dem Commit, der die Umstellung brachte.
        //
        // Erwartet wird darum, was QLocale::system() schreibt, nicht ein
        // bestimmtes Zeichen -- sonst faellt der Fall auf einem
        // englischen Rechner und in der CI.
        QLocale lok = QLocale::system();
        lok.setNumberOptions(QLocale::OmitGroupSeparator);
        const QString erwartet = lok.toString(7.1, 'f', 6);
        QVERIFY2(txt.contains(erwartet), qPrintable(txt + " | erwartet: " + erwartet));
        QVERIFY2(!txt.contains(KiwiSdrApplet::tr("Not assigned")),
                 qPrintable(txt));

        // ── Der eigentliche Punkt ───────────────────────────────────
        //
        // Aether haengt das Applet an die Signale der Scheibe. Wenn die
        // Verbindung fehlt, sieht man beim Aufbau alles richtig und
        // merkt monatelang nicht, dass sich danach nichts mehr ruehrt
        // — genau der Fehler, der in dieser Sitzung beim Panadapter
        // dreimal aufgetreten ist (die Kopfzeile des Kommentars stand
        // da, die Verbindung nicht).
        slice.setFrequency(14'074'000.0);
        txt = allText(&applet);
        qInfo().noquote() << "Nach dem Abstimmen:" << txt;
        // Wieder die Locale, nicht das Zeichen -- siehe oben.
        const QString erwartetNeu = lok.toString(14.074, 'f', 6);
        const QString erwartetAlt = lok.toString(7.1, 'f', 6);
        QVERIFY2(txt.contains(erwartetNeu),
                 qPrintable(txt + " | erwartet: " + erwartetNeu));
        QVERIFY2(!txt.contains(erwartetAlt),
                 qPrintable(txt + " | nicht erwartet: " + erwartetAlt));
    }
};

QTEST_MAIN(TstKiwiAppletShowsReceivers)
#include "tst_kiwi_applet_shows_receivers.moc"
