// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_applet_fenstermasse.cpp  (Longpath)
// =================================================================
//
// Die zuletzt gezogene Groesse eines abgeloesten Applets.
//
// Betreiber am 2026-10-30: „ich wuerde gerne, wenn ich ft8 klicke,
// dass fenster groesser haben. besser waere vielleicht die groesse
// der letzen veraenderung zu speichern?"
//
// Dieser Lauf kam bei der Durchsicht desselben Abends dazu: die
// Einheit war gebaut und verdrahtet, aber als einzige der sechs
// neuen ohne Prueflauf -- und ihre wichtigste Regel ist genau die,
// die still bricht.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/applets/AppletFensterMasse.h"

using namespace Longpath;
namespace AFM = Longpath::AppletFensterMasse;

class TstAppletFensterMasse : public QObject
{
    Q_OBJECT

private slots:
    void gemerktesMaszKommtZurueck()
    {
        AFM::vergiss(QStringLiteral("Prueffall"));
        QVERIFY(!AFM::gemerkt(QStringLiteral("Prueffall")).isValid());

        AFM::merke(QStringLiteral("Prueffall"), QSize(480, 320));
        QCOMPARE(AFM::gemerkt(QStringLiteral("Prueffall")), QSize(480, 320));

        AFM::vergiss(QStringLiteral("Prueffall"));
        QVERIFY(!AFM::gemerkt(QStringLiteral("Prueffall")).isValid());
    }

    // ── Die Regel, die still bricht ───────────────────────────────
    //
    // Beim Andocken faellt das Fenster kurz auf fast nichts zusammen,
    // und `resizeEvent` feuert dabei. Ohne die Untergrenze merkte
    // sich das Fenster genau diesen Augenblick -- und ginge beim
    // naechsten Abloesen als Briefmarke auf.
    //
    // Das waere nicht aufgefallen: die Groesse WURDE ja gemerkt, nur
    // die falsche.
    void eineBriefmarkeWirdNichtGemerkt()
    {
        const QString id = QStringLiteral("Prueffall2");
        AFM::merke(id, QSize(480, 320));

        AFM::merke(id, QSize(12, 8));
        QVERIFY2(AFM::gemerkt(id) == QSize(480, 320),
                 "der Zusammenbruch beim Andocken wurde gemerkt");

        // Genau an der Grenze wird noch gemerkt.
        AFM::merke(id, QSize(AFM::kMinBreite, AFM::kMinHoehe));
        QCOMPARE(AFM::gemerkt(id), QSize(AFM::kMinBreite, AFM::kMinHoehe));

        // Eine Stelle darunter nicht mehr -- und zwar in JEDER
        // Richtung einzeln.
        AFM::merke(id, QSize(AFM::kMinBreite - 1, 400));
        QCOMPARE(AFM::gemerkt(id), QSize(AFM::kMinBreite, AFM::kMinHoehe));
        AFM::merke(id, QSize(400, AFM::kMinHoehe - 1));
        QCOMPARE(AFM::gemerkt(id), QSize(AFM::kMinBreite, AFM::kMinHoehe));

        AFM::vergiss(id);
    }

    // Ein ungueltiges Masz oder eine leere Kennung aendert nichts --
    // sonst traege ein Aufruf aus einem halb aufgebauten Fenster die
    // gemerkte Groesse fort.
    void unsinnAendertNichts()
    {
        const QString id = QStringLiteral("Prueffall3");
        AFM::merke(id, QSize(500, 400));
        AFM::merke(id, QSize());
        AFM::merke(id, QSize(-10, -10));
        QCOMPARE(AFM::gemerkt(id), QSize(500, 400));

        AFM::merke(QString(), QSize(600, 500));
        QVERIFY(!AFM::gemerkt(QString()).isValid());
        AFM::vergiss(id);
    }

    // Zwei Applets, zwei Groessen. Ohne den Namen im Schluessel
    // haetten alle dieselbe.
    void jedesAppletHatSeineEigene()
    {
        AFM::merke(QStringLiteral("A"), QSize(300, 200));
        AFM::merke(QStringLiteral("B"), QSize(700, 500));
        QCOMPARE(AFM::gemerkt(QStringLiteral("A")), QSize(300, 200));
        QCOMPARE(AFM::gemerkt(QStringLiteral("B")), QSize(700, 500));
        AFM::vergiss(QStringLiteral("A"));
        QCOMPARE(AFM::gemerkt(QStringLiteral("B")), QSize(700, 500));
        AFM::vergiss(QStringLiteral("B"));
    }
};

QTEST_MAIN(TstAppletFensterMasse)
#include "tst_applet_fenstermasse.moc"
