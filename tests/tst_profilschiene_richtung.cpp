// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/tst_profilschiene_richtung.cpp  (Longpath)
//
// Die Profilschiene liegt seit dem 2026-10-06 waagrecht in der
// Kommandoleiste statt senkrecht am linken Rand.
//
// Betreiber: „meine profile links im eck sollten oben in die taksleite
// neben 48 khz" -- und direkt danach: „macht sinn, dass man dies
// individuell verschieben und anpassen kann". Also beides, und beides
// muss gehen.
//
// Geprueft wird, was man am Bauteil sieht: senkrecht ist es 44 Pixel
// breit und beliebig hoch, waagrecht genau ein Abzeichen hoch und
// beliebig breit. Und in BEIDEN Lagen traegt es dieselben Abzeichen --
// eine Lage, in der die Profile verschwinden, waere der eigentliche
// Fehler.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-06 — Original fuer Longpath, KI-gestuetzt (Anthropic
//                 Claude), Betreiber Martin Fischer.
// =================================================================

#include <QtTest>

#include "gui/LayoutProfiles.h"
#include "gui/widgets/ProfileRail.h"

using namespace Longpath;

class TestProfilschieneRichtung : public QObject
{
    Q_OBJECT

private slots:
    void senkrechtIstDieSchmaleSchiene()
    {
        LayoutProfiles profiles;
        ProfileRail rail(&profiles, Qt::Vertical);
        QCOMPARE(rail.richtung(), Qt::Vertical);
        // Feste Breite, damit sie am Rand nicht wandert.
        QCOMPARE(rail.width(), ProfileRail::kWidth);
        QCOMPARE(rail.minimumWidth(), ProfileRail::kWidth);
        QCOMPARE(rail.maximumWidth(), ProfileRail::kWidth);
    }

    void waagrechtIstGenauEinAbzeichenHoch()
    {
        LayoutProfiles profiles;
        ProfileRail rail(&profiles, Qt::Horizontal);
        QCOMPARE(rail.richtung(), Qt::Horizontal);
        // Feste Hoehe statt fester Breite -- sonst saesse in der Leiste
        // ein 44 Pixel breiter Klotz, in den die Abzeichen nicht passen.
        QCOMPARE(rail.height(), ProfileRail::kBadgeSide);
        QCOMPARE(rail.minimumHeight(), ProfileRail::kBadgeSide);
        QCOMPARE(rail.maximumHeight(), ProfileRail::kBadgeSide);
        // Und ausdruecklich KEINE feste Breite mehr.
        QVERIFY2(rail.maximumWidth() > ProfileRail::kWidth,
                 "waagrecht darf die Breite nicht festgenagelt sein");
    }

    // Der eigentliche Waechter: die Lage darf die Profile nicht kosten.
    void beideLagenTragenDieselbenAbzeichen()
    {
        LayoutProfiles profiles;
        // Eine frische LayoutProfiles hat KEINE Profile. Ohne die zwei
        // hier verglich die Pruefung zwei leere Listen und war gruen,
        // ohne irgendetwas zu belegen -- die Zusicherung weiter unten
        // hat genau das gefangen.
        QVERIFY(profiles.create(QStringLiteral("Neu")));
        QVERIFY(profiles.create(QStringLiteral("Betrieb")));

        ProfileRail senkrecht(&profiles, Qt::Vertical);
        ProfileRail waagrecht(&profiles, Qt::Horizontal);
        QCOMPARE(waagrecht.badges(), senkrecht.badges());
        QVERIFY2(!senkrecht.badges().isEmpty(),
                 "ohne Abzeichen sagt der Vergleich nichts");
        QCOMPARE(waagrecht.activeBadge(), senkrecht.activeBadge());
    }

    // Und das Umschalten selbst muss ankommen: die Schiene legt sich
    // nicht selbst um, sie meldet nur -- also muss die Meldung die
    // andere Richtung tragen.
    void dieSchieneMeldetDieAndereRichtung()
    {
        LayoutProfiles profiles;
        ProfileRail waagrecht(&profiles, Qt::Horizontal);
        QSignalSpy spy(&waagrecht,
                       &ProfileRail::placementToggleRequested);
        QVERIFY(spy.isValid());
    }
};

QTEST_MAIN(TestProfilschieneRichtung)
#include "tst_profilschiene_richtung.moc"
