// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_profilschiene_bindung.cpp  (Longpath)
// =================================================================
//
// Der Weg vom Rechtsklick bis in die Einstellungsdatei.
//
// Anlass: der Betreiber meldete am 2026-10-30 „verklinkung
// funktioniert nicht", nachdem „Use for FT8" gebaut war. Es gab
// Prueflaeufe fuer `bindDigi` und fuer `profileFor` -- und keinen
// fuer das Stueck dazwischen. Drei Dinge konnten es sein, und
// keines liess sich ausschliessen: der Menuepunkt erscheint nicht,
// er loest nichts aus, oder er wird nicht gespeichert.
//
// `showMenuFor` ist dafuer nicht zu gebrauchen: `QMenu::exec()`
// dreht auf macOS eine Cocoa-Schleife, in der weder Taktgeber noch
// gepostete Ereignisse ankommen (siehe
// docs/architecture/2026-10-15-pruefstaende-und-der-schirm.md).
// Darum sind Aufbau und Auswertung getrennt, und dieser Lauf fasst
// genau dazwischen an.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"
#include "gui/widgets/ProfileRail.h"

#include <QMenu>

using namespace Longpath;

class TstProfilschieneBindung : public QObject
{
    Q_OBJECT

private slots:
    // ── Der gemeldete Fall ────────────────────────────────────────
    void einHakenAufUseForFt8BindetDasProfil()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("Neu"));
        lp.create(QStringLiteral("FT8"));
        QVERIFY(!lp.isBoundToDigi(QStringLiteral("FT8"),
                                  DigiFrequenzen::Digimodus::Ft8));

        ProfileRail schiene(&lp, Qt::Horizontal);

        QVERIFY2(schiene.waehleImMenueForTest(QStringLiteral("FT8"),
                                              QStringLiteral("Use for FT8")),
                 "den Menuepunkt gibt es nicht oder er loest nichts aus");

        QVERIFY2(lp.isBoundToDigi(QStringLiteral("FT8"),
                                  DigiFrequenzen::Digimodus::Ft8),
                 "der Haken kam nicht in der Profilverwaltung an");

        // Und die Bindung wirkt: der FT8-Klick findet das Profil.
        QCOMPARE(lp.profileFor(Band::Band40m, DSPMode::DIGU,
                               DigiFrequenzen::Digimodus::Ft8),
                 QStringLiteral("FT8"));
    }

    // Noch einmal klicken loest sie wieder -- ein Haken, der nur
    // hingeht und nicht zurueck, ist eine Falle.
    void einZweiterKlickLoestDieBindung()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("FT8"));
        ProfileRail schiene(&lp, Qt::Horizontal);

        QVERIFY(schiene.waehleImMenueForTest(QStringLiteral("FT8"),
                                             QStringLiteral("Use for FT8")));
        QVERIFY(lp.isBoundToDigi(QStringLiteral("FT8"),
                                 DigiFrequenzen::Digimodus::Ft8));

        QVERIFY(schiene.waehleImMenueForTest(QStringLiteral("FT8"),
                                             QStringLiteral("Use for FT8")));
        QVERIFY2(!lp.isBoundToDigi(QStringLiteral("FT8"),
                                   DigiFrequenzen::Digimodus::Ft8),
                 "der zweite Klick muss sie wieder loesen");
    }

    // Der Haken steht beim naechsten Oeffnen richtig da -- sonst
    // weiss niemand, ob die Bindung sitzt.
    void derHakenZeigtDenGemerktenZustand()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("FT4-Fenster"));
        lp.bindDigi(QStringLiteral("FT4-Fenster"),
                    DigiFrequenzen::Digimodus::Ft4, true);

        ProfileRail schiene(&lp, Qt::Horizontal);
        QMenu menu;
        const auto teile = schiene.baueMenueForTest(
            QStringLiteral("FT4-Fenster"), menu);
        QVERIFY(teile.ft4);
        QVERIFY2(teile.ft4->isChecked(),
                 "der Haken bei FT4 muss gesetzt dastehen");
        QVERIFY(teile.ft8);
        QVERIFY(!teile.ft8->isChecked());
    }

    // FT8 und FT4 sind zwei Haken, nicht einer.
    void ft8UndFt4SindGetrennt()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("P"));
        ProfileRail schiene(&lp, Qt::Horizontal);

        QVERIFY(schiene.waehleImMenueForTest(QStringLiteral("P"),
                                             QStringLiteral("Use for FT8")));
        QVERIFY(lp.isBoundToDigi(QStringLiteral("P"),
                                 DigiFrequenzen::Digimodus::Ft8));
        QVERIFY(!lp.isBoundToDigi(QStringLiteral("P"),
                                  DigiFrequenzen::Digimodus::Ft4));
    }
};

QTEST_MAIN(TstProfilschieneBindung)
#include "tst_profilschiene_bindung.moc"
