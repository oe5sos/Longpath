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
//     Menue haengt am Fenster.
// Das Rotor/Log-Feld hat dasselbe Muster, bleibt aber vorerst: sein
// Stylesheet „background: kAppBg" (ohne Selektor) faerbt heute auch seine
// Menues; am Fenster bekaemen sie das App-Grau #1a1a1e statt #08080a --
// eine sichtbare Aenderung, die Martin entscheidet.
//
// Modification history (Longpath):
//   2026-09-27 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================
#include <QtTest>
#include <QApplication>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>

#include "gui/LayoutProfiles.h"
#include "gui/widgets/ProfileRail.h"

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
    void theProfileRailMenuHangsOnTheWindow()
    {
        QTest::failOnWarning(notTopLevel());
        LayoutProfiles profiles;
        profiles.setHooks([]() { return QVariantMap{}; }, [](const QVariantMap&) {});
        QVERIFY(profiles.create(QStringLiteral("Neu")));

        QWidget top;
        top.resize(400, 300);
        auto* rail = new ProfileRail(&profiles, &top);
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
};

QTEST_MAIN(TstMenusOfNativeFields)
#include "tst_menus_of_native_fields.moc"
