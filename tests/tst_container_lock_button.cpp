// =================================================================
// tests/tst_container_lock_button.cpp  (NereusSDR)
// =================================================================
//
// Das Schloss in der Titelleiste.
//
// Es GAB das Feststellen schon: m_locked sperrt Ziehen und
// Groessenaendern, und ein Rechtsklick-Menue schaltet es um. Was
// fehlte, war der sichtbare Zustand. Ein festgestelltes Fenster sah
// aus wie ein loses, und man ruettelt daran, bis man merkt, warum
// nichts geht.
//
// Geprueft wird deshalb nicht die Farbe (die haengt am Thema und waere
// ein Test, der bei jeder Palettenaenderung rot wird), sondern:
//
//   1. der Knopf ist da und schaltet um,
//   2. er meldet die Aenderung weiter,
//   3. das Zeichen wechselt zwischen offen und zu — der Zustand traegt
//      also auch ohne Farbe,
//   4. und ein zweites Setzen desselben Werts meldet NICHTS. Ohne diese
//      Schranke laeuft jede Wiederherstellung aus den Einstellungen in
//      eine Schleife aus Signalen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "gui/containers/ContainerWidget.h"

using namespace Longpath;

namespace {

// Der Schlossknopf traegt als einziger einen Hinweistext, der mit
// „Lock" oder „Locked" beginnt — die Suche ueber den Hinweis haelt,
// auch wenn sich die Reihenfolge der Knoepfe aendert.
QPushButton* lockButtonOf(ContainerWidget& c)
{
    for (QPushButton* b : c.findChildren<QPushButton*>()) {
        if (b->toolTip().startsWith(QStringLiteral("Lock"))) { return b; }
    }
    return nullptr;
}

} // namespace

class TestContainerLockButton : public QObject
{
    Q_OBJECT

private slots:

    void theLockButtonExists()
    {
        ContainerWidget c;
        QVERIFY2(lockButtonOf(c) != nullptr,
                 "ohne Knopf bleibt das Feststellen im Rechtsklick-Menue "
                 "vergraben");
    }

    void clickingItLocks()
    {
        ContainerWidget c;
        QPushButton* lock = lockButtonOf(c);
        QVERIFY(lock);
        QVERIFY(!c.isLocked());

        lock->click();
        QVERIFY2(c.isLocked(), "ein Klick stellt fest");
        lock->click();
        QVERIFY2(!c.isLocked(), "der naechste loest wieder");
    }

    void itSaysWhichWayItIs()
    {
        ContainerWidget c;
        QPushButton* lock = lockButtonOf(c);
        QVERIFY(lock);

        const QString open = lock->text();
        c.setLocked(true);
        const QString shut = lock->text();

        QVERIFY2(open != shut,
                 "offen und zu muessen sich am Zeichen unterscheiden, "
                 "nicht nur an der Farbe");
        QVERIFY2(lock->toolTip().contains(QStringLiteral("Locked")),
                 "und der Hinweistext sagt es in Worten");
    }

    void theChangeIsAnnounced()
    {
        ContainerWidget c;
        QSignalSpy spy(&c, &ContainerWidget::lockedChanged);

        c.setLocked(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    }

    // Die Schranke. Beim Wiederherstellen einer Anordnung wird jeder
    // Wert gesetzt, auch der, der schon stimmt.
    void settingTheSameValueSaysNothing()
    {
        ContainerWidget c;
        c.setLocked(true);

        QSignalSpy spy(&c, &ContainerWidget::lockedChanged);
        c.setLocked(true);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestContainerLockButton)
#include "tst_container_lock_button.moc"
