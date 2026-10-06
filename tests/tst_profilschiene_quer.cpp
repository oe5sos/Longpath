// Die Profilschiene quer — dieselbe Schiene, nur gedreht.
//
// Betreiber am 2026-10-06: „weiters würde ich gerne die profile von mir
// oben in die leiste einfügen um links mehr platz zu haben."
//
// Die Vorlage hat die Schiene am linken Rand über die volle Höhe. Auf
// einem breiten Schirm kostet das 44 Punkte über das ganze Fenster, für
// drei Abzeichen.
//
// WAAGRECHT ist bewusst keine zweite Umsetzung, sondern dieselbe
// gedreht: aus der Spalte wird eine Zeile, aus der festen Breite eine
// feste Höhe, aus dem Strich rechts einer unten. Zwei Umsetzungen
// nebeneinander wären zwei Stellen, an denen ein neues Abzeichen
// einzutragen wäre — und eine davon würde man vergessen.
//
// Genau das hält dieser Stand fest: beide Lagen zeigen DIESELBEN
// Abzeichen, und keine Lage verliert eines.

#include "gui/LayoutProfiles.h"
#include "gui/widgets/ProfileRail.h"

#include <QtTest>
#include <QPushButton>

using Longpath::LayoutProfiles;
using Longpath::ProfileRail;

class TstProfilschieneQuer : public QObject
{
    Q_OBJECT

private slots:
    void querIstFlachUndLaengsIstSchmal();
    void beideLagenZeigenDieselbenAbzeichen();
};

void TstProfilschieneQuer::querIstFlachUndLaengsIstSchmal()
{
    LayoutProfiles p;
    ProfileRail laengs(&p, ProfileRail::Ausrichtung::Senkrecht);
    ProfileRail quer(&p, ProfileRail::Ausrichtung::Waagrecht);

    // Laengs: feste Breite, Hoehe waechst mit dem Fenster.
    QVERIFY2(laengs.minimumWidth() == laengs.maximumWidth(),
             "Die senkrechte Schiene hat keine feste Breite mehr");
    // Quer: feste Hoehe, Breite waechst mit der Leiste. Das ist der
    // ganze Punkt -- eine quer liegende Schiene mit fester BREITE wuerde
    // die Leiste wieder beschneiden.
    QVERIFY2(quer.minimumHeight() == quer.maximumHeight(),
             "Die quere Schiene hat keine feste Hoehe");
    QCOMPARE(quer.maximumHeight(), laengs.maximumWidth());
}

void TstProfilschieneQuer::beideLagenZeigenDieselbenAbzeichen()
{
    LayoutProfiles p;
    ProfileRail laengs(&p, ProfileRail::Ausrichtung::Senkrecht);
    ProfileRail quer(&p, ProfileRail::Ausrichtung::Waagrecht);

    const int a = laengs.findChildren<QPushButton*>().size();
    const int b = quer.findChildren<QPushButton*>().size();
    QVERIFY2(a > 0, "Die Schiene zeigt gar keine Knoepfe");
    QCOMPARE(b, a);
}

QTEST_MAIN(TstProfilschieneQuer)
#include "tst_profilschiene_quer.moc"
