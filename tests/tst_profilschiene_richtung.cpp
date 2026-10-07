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
#include "gui/widgets/CommandBar.h"
#include "gui/widgets/ProfileRail.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

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
        //
        // maximumWidth() > kWidth allein war zu schwach: das ist Qts
        // Vorgabe (QWIDGETSIZE_MAX) und gilt auch, wenn jemand
        // zusaetzlich die Hoehe festnagelt. Darum gegen die senkrechte
        // Lage stellen -- DORT ist die Breite fest, hier nicht.
        QVERIFY2(rail.maximumWidth() > ProfileRail::kWidth,
                 "waagrecht darf die Breite nicht festgenagelt sein");
        ProfileRail senkrecht(&profiles, Qt::Vertical);
        QVERIFY2(senkrecht.maximumWidth() != rail.maximumWidth(),
                 "senkrecht und waagrecht duerfen nicht dieselbe "
                 "Breitenfestlegung haben");
        QVERIFY2(senkrecht.maximumHeight() != rail.maximumHeight(),
                 "senkrecht und waagrecht duerfen nicht dieselbe "
                 "Hoehenfestlegung haben");
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

        // badges() liest die eigene Liste, nicht die ANORDNUNG -- bindet
        // jemand das Einhaengen an die Richtung, bleibt die Leiste leer
        // und dieser Vergleich trotzdem gruen (2026-10-07 gefunden).
        // Also auch nachsehen, dass die Abzeichen wirklich in der
        // Anordnung stecken.
        for (ProfileRail* r : {&senkrecht, &waagrecht}) {
            QLayout* anordnung = r->layout();
            QVERIFY(anordnung != nullptr);
            int knoepfe = 0;
            for (int i = 0; i < anordnung->count(); ++i) {
                if (anordnung->itemAt(i) && anordnung->itemAt(i)->widget()) {
                    ++knoepfe;
                }
            }
            // Zwei Abzeichen plus das gestrichelte Plus.
            QCOMPARE(knoepfe, senkrecht.badges().size() + 1);
        }
    }

    // ── Die Leiste: Platz und Aufraeumen (2026-10-07) ────────────────
    //
    // Beides am 2026-10-07 beim Gegenlesen gefunden, beides von mir
    // eingebaut:
    //
    //   1. addGroupWidget setzte bei m_row->count() - 1 ein, mit der
    //      Begruendung "die Dehnung ist das letzte Element". Nach
    //      addTrailing() ist sie das NICHT mehr -- dann landet die
    //      Gruppe hinter der Dehnung und sitzt ganz rechts.
    //   2. Es gab kein Gegenstueck: nahm man das Bauteil wieder weg,
    //      blieb die Versalzeile stehen, und beim naechsten Mal kam eine
    //      zweite dazu.
    void dieGruppeSitztVorDerDehnungUndRaeumtSichWiederWeg()
    {
        CommandBar leiste;
        // Genau wie im echten Fenster: das Plus haengt RECHTS, also
        // hinter der Dehnung. Ohne diese Zeile faellt der Fehler nicht
        // auf -- und genau deshalb ist er mir durchgerutscht.
        auto* plus = new QPushButton(QStringLiteral("+"), &leiste);
        leiste.addTrailing(plus);

        auto* bauteil = new QWidget(&leiste);
        auto zaehleVersalzeilen = [&leiste]() {
            int n = 0;
            for (QLabel* l : leiste.findChildren<QLabel*>()) {
                if (l->text().compare(QStringLiteral("PROFIL"),
                                      Qt::CaseInsensitive) == 0) { ++n; }
            }
            return n;
        };
        auto platzVon = [&leiste](QWidget* w) {
            auto* row = leiste.findChild<QHBoxLayout*>();
            if (!row) { return -1; }
            for (int i = 0; i < row->count(); ++i) {
                QLayoutItem* it = row->itemAt(i);
                if (it && it->layout() && it->layout()->indexOf(w) >= 0) { return i; }
            }
            return -1;
        };
        auto platzDerDehnung = [&leiste]() {
            auto* row = leiste.findChild<QHBoxLayout*>();
            if (!row) { return -1; }
            for (int i = 0; i < row->count(); ++i) {
                if (row->itemAt(i) && row->itemAt(i)->spacerItem()) { return i; }
            }
            return -1;
        };

        leiste.addGroupWidget(QStringLiteral("Profil"), bauteil);
        QCOMPARE(zaehleVersalzeilen(), 1);
        const int platz = platzVon(bauteil);
        const int dehnung = platzDerDehnung();
        QVERIFY2(platz >= 0 && dehnung >= 0, "Gruppe oder Dehnung nicht gefunden");
        QVERIFY2(platz < dehnung,
                 "die Gruppe muss VOR der Dehnung sitzen, sonst steht sie "
                 "am rechten Rand statt neben der Rate");

        // Dreimal hin und her: es darf genau eine Versalzeile bleiben.
        for (int runde = 0; runde < 3; ++runde) {
            leiste.removeGroupWidget(bauteil);
            QCOMPARE(zaehleVersalzeilen(), 0);
            leiste.addGroupWidget(QStringLiteral("Profil"), bauteil);
            QCOMPARE(zaehleVersalzeilen(), 1);
            QVERIFY(platzVon(bauteil) < platzDerDehnung());
        }
    }

    // Und das Umschalten selbst muss ankommen: die Schiene legt sich
    // nicht selbst um, sie meldet nur -- also muss die Meldung die
    // andere Richtung tragen.
    // Die Schiene muss die ANDERE Richtung melden, nicht die eigene.
    //
    // Hier stand bis zum 2026-10-07 nur QVERIFY(spy.isValid()) -- das
    // prueft, dass es das Signal GIBT, nicht was darin steht. Dreht man
    // in ProfileRail das Fragezeichen um und meldet die eigene Richtung,
    // bleibt die Schiene beim Umschalten liegen (MainWindow steigt bei
    // gleicher Richtung frueh aus) -- und die Pruefung blieb gruen.
    void dieSchieneMeldetDieAndereRichtung()
    {
        LayoutProfiles profiles;
        QVERIFY(profiles.create(QStringLiteral("Neu")));

        for (auto richtung : {Qt::Horizontal, Qt::Vertical}) {
            ProfileRail schiene(&profiles, richtung);
            QSignalSpy spy(&schiene, &ProfileRail::placementToggleRequested);
            QVERIFY(spy.isValid());

            schiene.meldeUmhaengenForTest();
            QCOMPARE(spy.count(), 1);
            const auto gemeldet =
                spy.at(0).at(0).value<Qt::Orientation>();
            QVERIFY2(gemeldet != richtung,
                     "die Schiene muss die andere Richtung melden, sonst "
                     "passiert beim Umschalten nichts");
        }
    }
};

QTEST_MAIN(TestProfilschieneRichtung)
#include "tst_profilschiene_richtung.moc"
