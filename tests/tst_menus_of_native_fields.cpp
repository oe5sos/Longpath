// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_menus_of_native_fields.cpp  (Longpath)
// =================================================================
// Ein Rechtsklick-Menue haengt am Fenster, nicht am nativen Feld.
//
// Befund 2026-09-27 (Nachtschicht, Martins Protokoll): bei jedem
// Rechtsklick in der Profilleiste „QWidgetWindow(... name=
// "Longpath::ProfileRailClassWindow") must be a top level window.", einmal
// dasselbe fuers Rotor/Log-Feld. Beide haben ein eigenes natives Fenster
// (neben dem nativen Panadapter); ein QMenu mit so einem Elternteil bekommt
// von Qt keinen Fensterbezug -- QWindow::setTransientParent() lehnt ab. Auf
// macOS kann das Menue dann hinter schwebenden Werkzeugfenstern landen.
//
// Geprueft, mit bewusst nativ gemachtem Feld (wie im Programm):
//   - Profilleiste: Rechtsklick auf ein Abzeichen, keine Warnung, das
//     Menue haengt am Fenster;
//   - Rotor/Log-Feld: Rechtsklick ins Feld, ebenso. Seine Menues werden
//     dadurch heller -- das App-Menuegrau #1a1a1e statt des #08080a, das
//     ihnen das Feld-Stylesheet „background: kAppBg" (ohne Selektor)
//     vererbte. Martin, 2026-09-29, gefragt: „ja".
//
// Modification history (Longpath):
//   2026-09-27 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-09-29 — Rotor/Log-Feld dazu. Martin Fischer, KI-gestuetzt
//                 ueber Anthropic Claude.
// =================================================================
#include <QtTest>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>

#include "gui/LayoutProfiles.h"
#include "gui/widgets/ProfileRail.h"
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

namespace {

// Schliesst das Menue, sobald es offen ist, und merkt sich sein Elternteil.
void closeMenuSoon(QWidget** parentSeen)
{
    QTimer::singleShot(150, qApp, [parentSeen]() {
        if (auto* m = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            *parentSeen = m->parentWidget();
            m->close();
        }
    });
}

QRegularExpression notTopLevel()
{
    return QRegularExpression(QStringLiteral("must be a top level window"));
}

} // namespace

class TstMenusOfNativeFields : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Das Rotor/Log-Feld liest RotorLogbookPanel::logbookPath(); eine
        // gesetzte Sandbox-Variable lenkte das auf einen echten Ordner.
        qunsetenv("LONGPATH_CONFIG_DIR");
    }

    void theProfileRailMenuHangsOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        LayoutProfiles profiles;
        profiles.setHooks([]() { return QVariantMap{}; }, [](const QVariantMap&) {});
        QVERIFY(profiles.create(QStringLiteral("Neu")));

        QWidget top;
        top.resize(400, 300);
        // Ausdruecklich senkrecht: diese Pruefung geht um das native
        // Fenster der Schiene am linken Rand (2026-09-27), und das gibt
        // es nur in dieser Lage.
        auto* rail = new ProfileRail(&profiles, Qt::Vertical, &top);
        rail->setAttribute(Qt::WA_NativeWindow);   // wie neben dem Panadapter
        rail->setGeometry(0, 0, 60, 300);
        top.show();
        QVERIFY(QTest::qWaitForWindowExposed(&top));
        QVERIFY(rail->windowHandle());

        QPushButton* badge = nullptr;
        for (QPushButton* b : rail->findChildren<QPushButton*>()) {
            if (b->contextMenuPolicy() == Qt::CustomContextMenu) { badge = b; break; }
        }
        QVERIFY(badge);

        QWidget* parentSeen = nullptr;
        closeMenuSoon(&parentSeen);
        emit badge->customContextMenuRequested(QPoint(5, 5));
        QCOMPARE(parentSeen, rail->window());
    }

    void theLogbookPanelMenuHangsOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        QWidget top;
        top.resize(500, 600);
        auto* panel = new RotorLogbookPanel(nullptr, nullptr, nullptr, &top);
        panel->setAttribute(Qt::WA_NativeWindow);
        panel->setGeometry(0, 0, 500, 600);
        top.show();
        QVERIFY(QTest::qWaitForWindowExposed(&top));
        QVERIFY(panel->windowHandle());

        QWidget* parentSeen = nullptr;
        closeMenuSoon(&parentSeen);
        QContextMenuEvent ev(QContextMenuEvent::Mouse, QPoint(40, 40),
                             panel->mapToGlobal(QPoint(40, 40)));
        QApplication::sendEvent(panel, &ev);
        QCOMPARE(parentSeen, panel->window());
    }
};

QTEST_MAIN(TstMenusOfNativeFields)
#include "tst_menus_of_native_fields.moc"
