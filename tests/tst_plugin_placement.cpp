// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_plugin_placement.cpp  (Longpath)
// =================================================================
// Wohin gehoert dieses Plugin -- und faellt es auch dorthin?
//
// „wichtig ist, dass man weiss, wo man diese einbauen soll. das muss
// deutlich markiert sein, vielleicht in gruppen oder so. diese sollte
// man dann in der timeline oben reinziehen koennen. aber relativ
// leicht auch wieder loeschen."
//
// Zwei Dinge, die auseinanderfallen koennen, ohne dass es auffaellt:
// die Zuordnung (eine Tabelle, die jemand spaeter erweitert) und das
// Ziehen (eine Reihe, die nach jedem Umbau wieder anders rechnet).
//
// Zur Zuordnung eine Warnung im Klartext: sie ist GERATEN. Eine Audio
// Unit nennt ihre Gattung nicht. Was dieser Prueftand festhaelt, ist
// nicht „die Tabelle ist richtig", sondern „die Tabelle tut, was sie
// behauptet" -- und vor allem: was sie NICHT erkennt, landet sichtbar
// unter „noch nicht eingeordnet" und nicht in der erstbesten Gruppe.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/PluginPlacement.h"
#include "core/strip/StripChain.h"

using namespace Longpath;
using PluginPlacement::Group;

class TstPluginPlacement : public QObject
{
    Q_OBJECT
private slots:

    void theKnownSuspectsLandInTheRightGroup()
    {
        // Die Namen aus der Liste, die dem Betreiber vorgelegt wurde.
        QCOMPARE(PluginPlacement::guess("TDR Nova", "Tokyo Dawn"), Group::Eq);
        QCOMPARE(PluginPlacement::guess("DC1A", "Klanghelm"), Group::Compress);
        QCOMPARE(PluginPlacement::guess("LALA", "Analog Obsession"),
                 Group::Compress);
        QCOMPARE(PluginPlacement::guess("FETISH", "Analog Obsession"),
                 Group::Compress);
        QCOMPARE(PluginPlacement::guess("MERICA", "Analog Obsession"),
                 Group::Eq);
        QCOMPARE(PluginPlacement::guess("T-De-Esser 2", "Techivation"),
                 Group::DeEss);
        QCOMPARE(PluginPlacement::guess("Denoiser Classic", "Bertom"),
                 Group::Clean);
        // Apples eigene Stimmfreistellung -- am laufenden Fenster
        // gesehen, dass sie unter „noch nicht eingeordnet" landete.
        QCOMPARE(PluginPlacement::guess("AUSoundIsolation", "Apple"),
                 Group::Clean);
        QCOMPARE(PluginPlacement::guess("ClipOnly2", "Airwindows"),
                 Group::Final);
        QCOMPARE(PluginPlacement::guess("SPAN", "Voxengo"), Group::Measure);
        QCOMPARE(PluginPlacement::guess("IVGI", "Klanghelm"), Group::Colour);

        // Und die, die auf jedem Mac schon liegen.
        QCOMPARE(PluginPlacement::guess("AUNBandEQ", "Apple"), Group::Eq);
        QCOMPARE(PluginPlacement::guess("AUDynamicsProcessor", "Apple"),
                 Group::Compress);
        QCOMPARE(PluginPlacement::guess("AUPeakLimiter", "Apple"),
                 Group::Final);
        QCOMPARE(PluginPlacement::guess("AUBandpass", "Apple"), Group::Eq);
    }

    // Das Wichtigere: was die Tabelle NICHT kennt, behauptet nichts.
    void whatItCannotTellStaysUnsorted()
    {
        QCOMPARE(PluginPlacement::guess("Zorblax 9000", "Niemand"),
                 Group::Unknown);
        QCOMPARE(PluginPlacement::suggestedPosition(Group::Unknown), -1);
        QVERIFY(!PluginPlacement::groupName(Group::Unknown).isEmpty());
        QVERIFY(PluginPlacement::groupHint(Group::Unknown)
                    .contains(QStringLiteral("could not tell")));
    }

    // Die vorgeschlagenen Stellen muessen in der Kettenreihenfolge
    // stehen. Eine Gruppe „Zischlaute", die VOR den Entzerrer zeigt,
    // waere ein Ratschlag, der das Gegenteil dessen sagt, was im
    // Hinweis danebensteht.
    void theSuggestionsRunInChainOrder()
    {
        const int clean = PluginPlacement::suggestedPosition(Group::Clean);
        const int eq    = PluginPlacement::suggestedPosition(Group::Eq);
        const int dees  = PluginPlacement::suggestedPosition(Group::DeEss);
        const int comp  = PluginPlacement::suggestedPosition(Group::Compress);
        const int col   = PluginPlacement::suggestedPosition(Group::Colour);
        const int last  = PluginPlacement::suggestedPosition(Group::Final);

        QVERIFY(clean < eq);
        QVERIFY(eq    < dees);
        QVERIFY(dees  < comp);
        QVERIFY(comp  < col);
        QVERIFY(col   < last);
        QCOMPARE(clean, 0);
        QCOMPARE(last, StripChain::kStageCount);
    }

    // Jede Gruppe muss eine Ueberschrift und einen Satz haben. Eine
    // leere Gruppenueberschrift ist eine Kachel ohne Aussage, und die
    // Oberflaeche zeigt sie trotzdem an.
    void everyGroupSaysSomething()
    {
        for (int i = 0; i < PluginPlacement::kGroupCount; ++i) {
            const auto g = static_cast<Group>(i);
            QVERIFY2(!PluginPlacement::groupName(g).isEmpty(),
                     qPrintable(QStringLiteral("Gruppe %1 ohne Namen").arg(i)));
            QVERIFY2(PluginPlacement::groupHint(g).size() > 20,
                     qPrintable(QStringLiteral("Gruppe %1 ohne Satz").arg(i)));
        }
    }
};

QTEST_APPLESS_MAIN(TstPluginPlacement)
#include "tst_plugin_placement.moc"
