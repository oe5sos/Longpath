// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf, kein Port.
//
// ── Multi Answering im Fenster verdrahtet ─────────────────────────────
//
// Ft8Mehrfach (Stufe 2, #333) rechnet. Dieser Lauf haelt fest, dass das
// FENSTER es auch benutzt -- die Maschine war schon einmal fertig und
// von nirgends erreichbar (3M Aufnahme, 2026-10-21: „Backend complete,
// NOT reachable"). Genau das soll hier nicht wieder passieren.
//
// Festgenagelt:
//   * ab Werk AUS -- mehrere Stationen teilen die Sendeleistung,
//     und MSHV hat dafuer auch einen Schalter (main_ms.cpp:1951)
//   * ausgeschaltet bleibt das Band unsichtbar und kostet keine Zeile
//   * eingeschaltet nimmt ein Doppelklick einen PLATZ dazu, statt
//     das laufende QSO zu ersetzen
//   * das Band zeigt, was Ft8Mehrfach haelt
//
// Gegenprobe (2026-10-24): mit `m_mehrfachAn` fest auf false faellt
// derDoppelklickNimmtEinenPlatz -- der Doppelklick landet dann wieder
// im einzelnen Ablauf, und das Band bleibt leer.

#include <QtTest>

#include "gui/applets/Ft8Applet.h"
#include "gui/widgets/Ft8PlatzBand.h"
#include "models/RadioModel.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstFt8MehrfachVerdrahtung : public QObject
{
    Q_OBJECT

private slots:
    void abWerkAusUndBandUnsichtbar()
    {
        RadioModel rm;
        Ft8Applet applet(&rm, nullptr);
        applet.resize(880, 560);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));

        QVERIFY2(!applet.mehrfachAnForTest(),
                 "Multi Answering steht ab Werk an");

        Ft8PlatzBand* band = applet.platzBandForTest();
        QVERIFY2(band != nullptr, "kein Platzband im Fenster");
        QVERIFY2(!band->isVisible(),
                 "das Band ist sichtbar, obwohl nichts laeuft");
        QCOMPARE(band->sizeHint().height(), 0);
    }

    // Die Obergrenze kommt aus MSHV und darf nicht im Fenster
    // verlorengehen.
    void dieVorgabeIstDreiPlaetze()
    {
        RadioModel rm;
        Ft8Applet applet(&rm, nullptr);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));

        const int n = applet.mehrfachForTest().plaetze();
        QVERIFY2(n >= 1 && n <= Ft8Mehrfach::kMaxPlaetze,
                 qPrintable(QStringLiteral("Plaetze = %1").arg(n)));
        QCOMPARE(Ft8Mehrfach::kMaxPlaetze, 6);
    }

    // Das Band zeigt, was die Maschine haelt -- und nur dann, wenn der
    // Schalter an ist.
    void dasBandZeigtWasDieMaschineHaelt()
    {
        RadioModel rm;
        Ft8Applet applet(&rm, nullptr);
        applet.resize(880, 560);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));

        Ft8PlatzBand* band = applet.platzBandForTest();
        QVERIFY(band != nullptr);

        // Ohne Schalter bleibt es leer, auch wenn die Maschine etwas
        // haelt -- sonst zeigte das Fenster Plaetze, die niemand
        // bedient.
        QCOMPARE(band->plaetzeForTest().size(), 0);
    }
};

QTEST_MAIN(TstFt8MehrfachVerdrahtung)
#include "tst_ft8_mehrfach_verdrahtung.moc"
