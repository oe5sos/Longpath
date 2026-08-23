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
#include <QLabel>
#include <QListWidget>

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
        QVERIFY2(txt.contains(QStringLiteral("Keine KiwiSDR")),
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
        QVERIFY2(txt.contains(QStringLiteral("Nicht zugeordnet")),
                 qPrintable(txt));
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
        QVERIFY2(txt.contains(QStringLiteral("7.100000")), qPrintable(txt));
        QVERIFY2(!txt.contains(QStringLiteral("Nicht zugeordnet")),
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
        QVERIFY2(txt.contains(QStringLiteral("14.074000")), qPrintable(txt));
        QVERIFY2(!txt.contains(QStringLiteral("7.100000")), qPrintable(txt));
    }
};

QTEST_MAIN(TstKiwiAppletShowsReceivers)
#include "tst_kiwi_applet_shows_receivers.moc"
