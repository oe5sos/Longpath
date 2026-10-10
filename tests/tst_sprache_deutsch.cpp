// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_sprache_deutsch.cpp  (Longpath)
// =================================================================
// „generell sollte für mich primär alles in deutsch sein, eine
//  englische version sollte es natürlich auch geben."
//
// Zwei Zusagen, die leicht zu brechen sind und die niemand beim
// Ansehen bemerkt:
//
//   1. Deutsch ist die Vorgabe -- nicht Englisch mit einem Schalter.
//   2. Auf Englisch gestellt kommt WIRKLICH das Englische zurück und
//      nicht ein halb übersetztes Gemisch.
//
// Dazu der Durchgang durch den Widget-Baum: er muss Beschriftungen,
// Knöpfe, Reiter, Auswahlfelder und Kurzhilfen erwischen -- und die
// hinterlegten Daten der Auswahlfelder in Ruhe lassen, denn an
// denen hängen die Einstellungen.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QTabWidget>
#include <QWidget>

#include "core/Sprache.h"

using namespace Longpath;

class TstSpracheDeutsch : public QObject {
    Q_OBJECT

private slots:
    void germanIsTheDefault()
    {
        Sprache::setzeSprache(Sprache::Lang::Deutsch);
        QCOMPARE(Sprache::T(QStringLiteral("Hear myself")),
                 QStringLiteral("Mich hören"));
        QVERIFY2(Sprache::eintraege() > 100,
                 qPrintable(QStringLiteral("nur %1 Einträge")
                                .arg(Sprache::eintraege())));
    }

    void englishGivesBackTheEnglish()
    {
        Sprache::setzeSprache(Sprache::Lang::English);
        QCOMPARE(Sprache::T(QStringLiteral("Hear myself")),
                 QStringLiteral("Hear myself"));
        Sprache::setzeSprache(Sprache::Lang::Deutsch);
    }

    // Was nicht in der Tabelle steht, bleibt stehen -- sichtbar
    // englisch statt leer. Ein fehlender Eintrag darf nie einen Text
    // verschlucken.
    void whatIsMissingStaysVisible()
    {
        Sprache::setzeSprache(Sprache::Lang::Deutsch);
        const QString fremd = QStringLiteral("Something nobody translated");
        QVERIFY(!Sprache::hatUebersetzung(fremd));
        QCOMPARE(Sprache::T(fremd), fremd);
    }

    void theTreeWalkReachesEveryKind()
    {
        Sprache::setzeSprache(Sprache::Lang::Deutsch);

        QWidget root;
        auto* label = new QLabel(QStringLiteral("Monitor level."), &root);
        auto* box   = new QCheckBox(QStringLiteral("Gate on"), &root);
        auto* combo = new QComboBox(&root);
        combo->addItem(QStringLiteral("Safe"), QStringLiteral("safe-key"));
        auto* tabs  = new QTabWidget(&root);
        tabs->addTab(new QWidget, QStringLiteral("Plugins"));
        tabs->addTab(new QWidget, QStringLiteral("Voice check"));
        label->setToolTip(QStringLiteral("Monitor level."));

        Sprache::uebersetzeBaum(&root);

        QCOMPARE(label->text(), QStringLiteral("Lautstärke des Mithörers."));
        QCOMPARE(label->toolTip(), QStringLiteral("Lautstärke des Mithörers."));
        QCOMPARE(box->text(), QStringLiteral("Gate an"));
        QCOMPARE(combo->itemText(0), QStringLiteral("Sicher"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("Stimmprüfung"));

        // Und das Entscheidende: die hinterlegten Daten sind
        // unberührt. Würde der Durchgang sie mitübersetzen, fänden
        // die Einstellungen ihren Eintrag nicht mehr wieder.
        QCOMPARE(combo->itemData(0).toString(), QStringLiteral("safe-key"));
    }

    // Zweimal durchgehen darf nichts kaputtmachen -- der Baum wird
    // nach jedem Umbau der Reiter erneut übersetzt.
    void twiceIsHarmless()
    {
        Sprache::setzeSprache(Sprache::Lang::Deutsch);
        QWidget root;
        auto* label = new QLabel(QStringLiteral("Close"), &root);
        Sprache::uebersetzeBaum(&root);
        const QString einmal = label->text();
        Sprache::uebersetzeBaum(&root);
        QCOMPARE(label->text(), einmal);
        QCOMPARE(einmal, QStringLiteral("Schließen"));
    }
};

QTEST_MAIN(TstSpracheDeutsch)
#include "tst_sprache_deutsch.moc"
