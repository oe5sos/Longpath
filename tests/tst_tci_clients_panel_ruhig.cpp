// =================================================================
// tests/tst_tci_clients_panel_ruhig.cpp  (Longpath)
// =================================================================
//
// Das TCI-Clients-Panel baute jede Sekunde alle Zeilen neu, weil sich die
// Verbindungsdauer aendert -- fuer einen Moment war es kuerzer, und die
// Spalte darunter sprang. Jetzt behaelt eine Zeile ihre Widgets, solange
// dasselbe Programm da ist; nur der Text wird nachgezogen.
//
// Ein Programm ist hier ein QWebSocket auf der Schleife zu einem TciServer
// ohne Funkgeraet. Nachgebaut nach Upstream NereusSDR d96e834b3 (#765,
// J.J. Boyd KG4VCF). Martin Fischer (OE5SOS), KI-gestuetzt mit Anthropic
// Claude Code, 2026-11-10.
// =================================================================

#include <QtTest/QtTest>
#include <QHostAddress>
#include <QLabel>
#include <QPointer>
#include <QSignalSpy>
#include <QWebSocket>

#include "core/AppSettings.h"
#include "core/TciServer.h"
#include "gui/applets/ClientChainApplet.h"

using namespace Longpath;

class TstTciClientsPanelRuhig : public QObject {
    Q_OBJECT

    static void auffrischen(ClientChainApplet& applet)
    {
        QVERIFY(QMetaObject::invokeMethod(&applet, "refresh", Qt::DirectConnection));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    }

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void zeileBehaeltIhreWidgets()
    {
        TciServer server(nullptr);
        QVERIFY(server.start(QHostAddress(QHostAddress::LocalHost), 0));
        ClientChainApplet applet(&server);

        QWebSocket programm;
        QSignalSpy verbunden(&programm, &QWebSocket::connected);
        programm.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(verbunden.wait(2000));
        QTRY_COMPARE(server.clientCount(), 1);

        auffrischen(applet);
        const QList<QWidget*> zeilen =
            applet.findChildren<QWidget*>(QStringLiteral("tciClientRow"));
        QCOMPARE(zeilen.size(), 1);
        QPointer<QWidget> zeile = zeilen.first();
        QPointer<QLabel> dauer = zeile->findChild<QLabel*>(QStringLiteral("tciDauer"));
        QVERIFY(dauer);
        const QString ersteDauer = dauer->text();
        const int hoehe = applet.sizeHint().height();

        QTest::qWait(1100);
        auffrischen(applet);

        QVERIFY2(!zeile.isNull() && !zeile->isHidden(),
                 "Die Zeile wurde abgerissen, obwohl dasselbe Programm verbunden ist");
        QVERIFY(!dauer.isNull());
        QCOMPARE(applet.findChildren<QWidget*>(QStringLiteral("tciClientRow")),
                 QList<QWidget*>({zeile.data()}));
        QVERIFY2(dauer->text() != ersteDauer,
                 qPrintable(QStringLiteral("Die Dauer steht noch auf %1").arg(ersteDauer)));
        QCOMPARE(applet.sizeHint().height(), hoehe);

        // Das Programm geht: seine Zeile auch.
        programm.close();
        QTRY_COMPARE(server.clientCount(), 0);
        auffrischen(applet);
        QTRY_VERIFY(zeile.isNull() || zeile->isHidden());
        server.stop();
    }
};

QTEST_MAIN(TstTciClientsPanelRuhig)
#include "tst_tci_clients_panel_ruhig.moc"
