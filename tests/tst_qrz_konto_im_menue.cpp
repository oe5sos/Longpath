// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_qrz_konto_im_menue.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der QRZ-Zugang ist wieder im Tools-Menue (2026-11-10). Vom 2026-08-07
// (57d524b0f) an gab es keinen Weg mehr in MainWindow::
// openQrzCredentialsDialog(): der einzige Zugang war ein „QRZ…"-Knopf im
// alten Rotorfenster, der mit ihm verschwand -- waehrend Rotor-Logbuch,
// Stationskarte und Karte weiter „Add your QRZ account in Tools" sagten.
//
// Dieser Lauf baut das echte Hauptfenster, sucht den Eintrag IM Tools-
// Menue (nicht irgendwo), loest ihn aus und haelt fest, dass der Dialog
// mit Rufzeichen-, Passwort- und Logbuch-Schluessel-Feld erscheint. Dazu
// der Nachbar „Google 3D map...", der im selben Zug dazukam.

#include <QtTest>

#include "gui/MainWindow.h"
#include "gui/Google3dSetupDialog.h"
#include "FensterFrist.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>

using namespace Longpath;

namespace {

// Nicht ueber mw.menuBar(): die Leiste wandert in die eigene Titelleiste
// (TitleBar::setMenuBar), und ein spaeterer menuBar()-Aufruf baut dann
// eine neue, leere. Wie tst_contest_panels_im_containers_menu.
QMenu* toolsMenu(MainWindow& mw)
{
    for (QMenu* m : mw.findChildren<QMenu*>()) {
        if (m->title().remove(QLatin1Char('&')) == QLatin1String("Tools")) { return m; }
    }
    return nullptr;
}

QAction* actionIn(QMenu* menu, const QString& objectName)
{
    for (QAction* a : menu->actions()) {
        if (a->objectName() == objectName) { return a; }
    }
    return nullptr;
}

} // namespace

class TstQrzKontoImMenue : public QObject { Q_OBJECT
private slots:
    void qrz_account_is_in_the_tools_menu_and_opens_the_dialog()
    {
        MainWindow mw;
        mw.resize(1280, 800);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));

        QMenu* tools = toolsMenu(mw);
        QVERIFY2(tools, "kein Tools-Menue in der Menueleiste");
        QAction* qrz = actionIn(tools, QStringLiteral("actQrzAccount"));
        QVERIFY2(qrz, "kein QRZ-Eintrag im Tools-Menue");
        QCOMPARE(qrz->text(), QStringLiteral("&QRZ account..."));
        QVERIFY(qrz->isEnabled());

        // Den modalen Dialog von aussen erkennen und schliessen: exec()
        // dreht seine eigene Schleife, der Zeitgeber laeuft darin.
        bool seen = false;
        QStringList placeholders;
        QTimer watch;
        watch.setInterval(50);
        connect(&watch, &QTimer::timeout, &mw, [&]() {
            auto* dlg = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dlg || dlg->objectName() != QLatin1String("qrzAccountDialog")) { return; }
            seen = dlg->windowTitle() == QLatin1String("QRZ account");
            for (QLineEdit* e : dlg->findChildren<QLineEdit*>()) {
                placeholders << e->placeholderText();
            }
            watch.stop();
            dlg->reject();
        });
        watch.start();
        QTimer::singleShot(15000, &mw, [&]() {   // Notbremse, falls er nie kommt
            watch.stop();
            if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget())) { d->reject(); }
        });
        qrz->trigger();   // kehrt zurueck, sobald der Dialog zu ist
        watch.stop();

        QVERIFY2(seen, "der QRZ-Dialog ist nicht erschienen");
        QVERIFY2(placeholders.contains(QStringLiteral("your QRZ callsign")),
                 qPrintable(placeholders.join(QLatin1String(" | "))));
        QVERIFY(placeholders.contains(QStringLiteral("your QRZ password")));
        QVERIFY(placeholders.contains(QStringLiteral("for uploading QSOs")));
        // Nach dem Schliessen haengt nichts mehr am Hauptfenster.
        QTRY_VERIFY(mw.findChild<QDialog*>(QStringLiteral("qrzAccountDialog")) == nullptr);

        // Der Nachbar aus demselben Zug: der eigene Google-Schluessel.
        QAction* g3d = actionIn(tools, QStringLiteral("actGoogle3dMap"));
        QVERIFY2(g3d, "kein Google-3D-Eintrag im Tools-Menue");
        g3d->trigger();
        Google3dSetupDialog* setup = nullptr;
        QTRY_VERIFY((setup = mw.findChild<Google3dSetupDialog*>()) != nullptr);
        QVERIFY(setup->isVisible());
        setup->close();
    }
};

QTEST_MAIN(TstQrzKontoImMenue)
#include "tst_qrz_konto_im_menue.moc"
