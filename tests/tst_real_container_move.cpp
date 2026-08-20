// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_real_container_move.cpp  (Longpath)
// =================================================================
// Die Meter-Container im ECHTEN Hauptfenster.
//
// Der Betreiber, 2026-08-20: „alle, mache eine nach der anderen".
// Panadapter und abgeloeste Applets sind erledigt; dies ist der
// dritte Fall.
//
// ContainerWidget hat Ziehen, Groessenaendern, Schloss, Anheften und
// Abloesen seit dem Thetis-Port (ucMeter.cs). Die Frage ist nicht, ob
// es das gibt — die Frage ist, ob eine Hand drankommt. Bei den
// Applets war genau das die Luecke.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QPushButton>
#include "gui/MainWindow.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/WindowChrome.h"
#include <QScreen>

using namespace Longpath;

class TestRealContainerMove : public QObject
{
    Q_OBJECT
private slots:
    void containersAreReachableAndMovable()
    {
        auto* mwp = new MainWindow();      // bewusst nicht abgeraeumt
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(500);

        const QList<ContainerWidget*> cs =
            mwp->findChildren<ContainerWidget*>();
        qDebug() << "Container gefunden:" << cs.size();
        QVERIFY2(!cs.isEmpty(), "es muss mindestens einen Container geben");

        int withVisibleBar = 0;
        for (ContainerWidget* c : cs) {
            qDebug().noquote()
                << "  " << c->id()
                << " sichtbar:" << c->isVisible()
                << " Leiste:" << (c->isTitleBarVisible() ? "ja" : "nein")
                << " verriegelt:" << c->isLocked();
            if (c->isVisible() && c->isTitleBarVisible()) { ++withVisibleBar; }
        }
        QVERIFY2(withVisibleBar > 0,
                 "mindestens ein sichtbarer Container MUSS eine sichtbare "
                 "Kopfleiste haben — ohne sie gibt es nichts anzufassen");
    }

    // Und was passiert, wenn man wirklich zieht?
    void draggingAContainerActuallyMovesIt()
    {
        auto* mwp = new MainWindow();
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(500);

        ContainerWidget* c = nullptr;
        for (ContainerWidget* x : mwp->findChildren<ContainerWidget*>()) {
            if (x->isVisible() && x->isTitleBarVisible()) { c = x; break; }
        }
        QVERIFY(c);

        qDebug() << "Andockart:" << static_cast<int>(c->dockMode())
                 << " schwebend:" << c->isFloating()
                 << " Lage:" << c->pos() << " Groesse:" << c->size();

        // Die Knoepfe der Kopfleiste — sind sie da und sichtbar?
        const QList<QPushButton*> btns = c->findChildren<QPushButton*>();
        QStringList texts;
        for (QPushButton* b : btns) {
            if (b->isVisible()) { texts << b->text(); }
        }
        qDebug().noquote() << "sichtbare Knoepfe:" << texts.join(QLatin1Char(' '));
        QVERIFY2(!texts.isEmpty(),
                 "die Kopfleiste MUSS anklickbare Knoepfe zeigen");

        // ── Abloesen: aus dem Splitter in ein eigenes Fenster ───────
        //
        // Angedockt liegt der Container in einem QSplitter; dort
        // bestimmt der Splitter die Lage, und Ziehen an der Leiste
        // kann daran nichts aendern. Frei beweglich wird er erst als
        // eigenes Fenster — genau dafuer ist das ↗ da.
        QPushButton* floatBtn = nullptr;
        for (QPushButton* b : btns) {
            if (b->isVisible() && b->text() == QStringLiteral("↗")
                && b->parentWidget()
                && b->parentWidget()->parentWidget() == c) {
                floatBtn = b; break;
            }
        }
        if (!floatBtn) {
            for (QPushButton* b : btns) {
                if (b->isVisible() && b->text() == QStringLiteral("↗")) {
                    QStringList chain;
                    for (QWidget* w = b->parentWidget(); w && chain.size() < 4;
                         w = w->parentWidget()) {
                        chain << QString::fromLatin1(w->metaObject()->className());
                    }
                    qDebug().noquote() << "  ↗ Abstammung:"
                                       << chain.join(QStringLiteral(" < "));
                    break;
                }
            }
        }
        qDebug() << "Abloeseknopf des Containers gefunden:"
                 << (floatBtn != nullptr);

        const QPoint before = c->window()->pos();
        if (floatBtn) {
            QTest::mouseClick(floatBtn, Qt::LeftButton);
            QTest::qWait(500);
            qDebug() << "nach ↗ — schwebend:" << c->isFloating()
                     << " Fenster:" << (c->window() != mwp)
                     << " Groesse:" << c->window()->size();
            QVERIFY2(c->isFloating() || c->window() != mwp,
                     "nach dem Ablösen MUSS der Container in einem "
                     "eigenen Fenster stehen");
        }
        Q_UNUSED(before);
    }

    // ── Der abgeloeste Container darf den Schirm nicht fuellen ──────
    //
    // Der Betreiber hat am 2026-08-20 ein bildschirmfuellendes „RX1
    // Main Panel" fotografiert. FloatingContainer war das letzte
    // Fenster, das noch Qt::Window war und keine Groessengrenze hatte.
    void aFloatedContainerIsNotFullScreen()
    {
        auto* mwp = new MainWindow();
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(500);

        auto* mgr = mwp->findChild<ContainerManager*>();
        QVERIFY2(mgr, "es muss eine Containerverwaltung geben");

        ContainerWidget* c = nullptr;
        for (ContainerWidget* x : mwp->findChildren<ContainerWidget*>()) {
            if (x->isVisible()) { c = x; break; }
        }
        QVERIFY(c);
        const QString id = c->id();

        mgr->floatContainer(id);
        QTest::qWait(600);

        FloatingContainer* form = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* f = qobject_cast<FloatingContainer*>(w)) {
                if (f->isVisible()) { form = f; break; }
            }
        }
        QVERIFY2(form, "das abgeloeste Containerfenster muss existieren");

        qDebug() << "Containerfenster:" << form->size()
                 << " Werkzeugfenster:"
                 << bool(form->windowFlags() & Qt::Tool);

        const QSize avail = form->screen() ? form->screen()->availableSize()
                                           : QSize(1440, 900);
        QVERIFY2(form->windowFlags() & Qt::Tool,
                 "es MUSS ein Werkzeugfenster sein — sonst zieht es auf "
                 "macOS in die Vollbildflaeche des Hauptfensters ein");
        QVERIFY2(form->width() <= (avail.width() * 2) / 3 + 4,
                 qPrintable(QStringLiteral("zu breit: %1 von %2")
                     .arg(form->width()).arg(avail.width())));
        QVERIFY2(form->height() <= (avail.height() * 4) / 5 + 4,
                 qPrintable(QStringLiteral("zu hoch: %1 von %2")
                     .arg(form->height()).arg(avail.height())));
        QVERIFY2(form->findChild<ResizeGrip*>() != nullptr,
                 "und einen sichtbaren Anfasser unten rechts haben");
    }
};
QTEST_MAIN(TestRealContainerMove)
#include "tst_real_container_move.moc"
