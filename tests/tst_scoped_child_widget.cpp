// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_scoped_child_widget.cpp  (Longpath)
// =================================================================
//
// Am 2026-08-30 stuerzte Longpath mit "pointer being freed was not
// allocated" ab: der Betreiber hatte ein Kontextmenue offen (ein
// STAPEL-lokales QMenu mitten in seinem eigenen exec()) und schloss
// dabei das Fenster. Die Aufraeumschleife im closeEvent sammelte das
// Menue als Top-Level-Fenster ein und rief deleteLater() darauf —
// "delete this" auf eine Stapeladresse.
//
// Damals wurde die Schleife entschaerft. Die Ursache — ein Qt-Kind auf
// dem Stapel, das seine eigene Ereignisschleife dreht — blieb. Dieser
// Prüfstand haelt fest, was ScopedChildWidget dagegen leistet:
// normales Aufraeumen, und Aufraeumen NACHDEM das Elternteil das Kind
// schon mitgenommen hat. Mit einem rohen Zeiger statt des QPointer
// faellt der zweite Fall in ein use-after-free.
// =================================================================

#include <QtTest/QtTest>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QWidget>

#include "gui/ScopedChildWidget.h"

using namespace Longpath;

class TstScopedChildWidget : public QObject { Q_OBJECT
private slots:

    void normalScopeExitDestroysTheChild()
    {
        QWidget parent;
        QPointer<QMenu> watch;
        {
            ScopedChildWidget<QMenu> owner(&parent);
            watch = owner.get();
            QVERIFY(watch != nullptr);
            QVERIFY(bool(owner));
            QCOMPARE(watch->parentWidget(), &parent);
        }
        QVERIFY2(watch.isNull(), "Das Kind hat den Rahmen ueberlebt");
    }

    void theParentMayTakeTheChildFirst()
    {
        QPointer<QMenu> watch;
        {
            auto* parent = new QWidget();
            ScopedChildWidget<QMenu> owner(parent);
            watch = owner.get();
            QVERIFY(watch != nullptr);

            // Genau der Fall von 2026-08-30: das Elternteil stirbt,
            // waehrend der Halter noch im Rahmen steht.
            delete parent;

            QVERIFY2(watch.isNull(), "Qt hat das Kind nicht mit dem Elternteil abgeraeumt");
            QVERIFY2(!owner, "Der Halter haelt noch etwas, das es nicht mehr gibt");
            // Und jetzt der Rahmenausgang: er darf NICHT ein zweites Mal
            // freigeben. Mit einem rohen Zeiger endet der Lauf hier.
        }
        QVERIFY(watch.isNull());
    }

    // Der Halter ist zugleich die Auskunft "hat mein Elternteil das
    // ueberlebt?" — genau die Frage, die jede Stelle nach exec() stellt,
    // bevor sie wieder an `this` fasst.
    void theOwnerAnswersWhetherTheParentSurvived()
    {
        auto* parent = new QWidget();
        ScopedChildWidget<QMenu> owner(parent);
        QVERIFY2(bool(owner), "Frisch gebaut muss der Halter etwas halten");
        delete parent;
        QVERIFY2(!owner, "Nach dem Tod des Elternteils muss der Halter leer melden");
    }

    // Der weiterleitende Konstruktor (2026-10-28): ein Dialog, der
    // sein Elternteil erst als letztes Argument nimmt, muss genauso
    // durch den Halter gehen koennen wie ein QMenu.
    void theForwardingConstructorBuildsAMultiArgChild()
    {
        QWidget parent;
        ScopedChildWidget<QMessageBox> owner(
            QMessageBox::Warning, QStringLiteral("Titel"),
            QStringLiteral("Text"),
            QMessageBox::Ok | QMessageBox::Cancel, &parent);
        QVERIFY(bool(owner));
        QCOMPARE(owner.get()->parentWidget(), &parent);
        QCOMPARE(owner.get()->icon(), QMessageBox::Warning);
        QCOMPARE(owner.get()->text(), QStringLiteral("Text"));
        QVERIFY(owner.get()->standardButtons() & QMessageBox::Cancel);
    }

    // Und er schuetzt genauso: stirbt das Elternteil, meldet der
    // Halter leer und gibt beim Rahmenausgang nichts mehr frei.
    void theForwardingConstructorIsGuardedToo()
    {
        QPointer<QMessageBox> watch;
        {
            auto* parent = new QWidget();
            ScopedChildWidget<QMessageBox> owner(
                QMessageBox::Information, QStringLiteral("T"),
                QStringLiteral("X"), QMessageBox::Ok, parent);
            watch = owner.get();
            QVERIFY(watch != nullptr);
            delete parent;
            QVERIFY2(watch.isNull(), "Qt hat den Dialog nicht mitgenommen");
            QVERIFY2(!owner, "Der Halter haelt noch etwas, das es nicht mehr gibt");
        }
        QVERIFY(watch.isNull());
    }

    void theChildStaysUsableWhileTheParentLives()
    {
        QWidget parent;
        ScopedChildWidget<QMenu> owner(&parent);
        QMenu& menu = *owner.get();
        QAction* a = menu.addAction(QStringLiteral("Test"));
        QVERIFY(a != nullptr);
        QCOMPARE(menu.actions().size(), 1);
        QCOMPARE(menu.actions().first(), a);
    }
};

QTEST_MAIN(TstScopedChildWidget)
#include "tst_scoped_child_widget.moc"
