// SPDX-License-Identifier: GPL-3.0-or-later
//
// Jedes Applet, das im Panel haengt, ist auch im Auswaehler zu finden.
//
// Anlass, 2026-08-23: "wo ist die applet mitschrift". Sie war gebaut,
// ins Panel gehaengt — und unsichtbar. Es fehlten ZWEI Anmeldungen:
// der Eintrag in m_appletsById und der Aufruf von registerApplet.
// Dasselbe galt fuer das KiwiSDR-Applet und fuer SWR/Leistung.
//
// Das ist die teuerste Fehlerart dieses Projekts, und sie ist an
// diesem einen Tag DREIMAL aufgetreten: der KiwiSDR war vollstaendig
// gebaut und ohne Menuepunkt, die Zoomknoepfe waren Signale ohne
// Empfaenger, und jetzt drei Applets ohne Eintrag. Gebaut und nicht
// erreichbar ist so gut wie nicht gebaut — und keine Einzelpruefung
// sieht es, weil jeder Baustein fuer sich in Ordnung ist.
//
// Diese Pruefung vergleicht darum die beiden Listen gegeneinander,
// statt einzelne Namen abzufragen. Ein Applet, das jemand kuenftig
// hinzufuegt und anzumelden vergisst, faellt hier auf, ohne dass
// jemand diese Datei anfassen muss.

#include <QtTest>

#include "gui/MainWindow.h"
#include "gui/applets/AppletWidget.h"

using namespace Longpath;

class TstEveryAppletIsReachable : public QObject
{
    Q_OBJECT

private slots:
    void keinAppletOhneEintrag()
    {
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        // Alle Applets, die wirklich im Fenster stecken.
        const QList<AppletWidget*> inPanel = mw->findChildren<AppletWidget*>();
        QVERIFY2(!inPanel.isEmpty(), "Gar keine Applets gefunden — dann "
                                     "prueft diese Pruefung nichts");

        QStringList missing;
        for (AppletWidget* a : inPanel) {
            if (!a) { continue; }
            const QString id = a->appletId();
            if (id.isEmpty()) { continue; }
            // Der Auswaehler findet ein Applet ueber die Kennungskarte.
            // Liefert sie nichts, gibt es keinen Weg dorthin.
            if (!mw->appletIsRegisteredForTest(a)) {
                missing << QStringLiteral("%1 (%2)").arg(a->appletTitle(), id);
            }
        }

        if (!missing.isEmpty()) {
            qWarning().noquote() << "Nicht im Auswaehler:"
                                 << missing.join(QStringLiteral(", "));
        } else {
            qInfo() << "alle" << inPanel.size() << "Applets erreichbar";
        }
        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Diese Applets sind gebaut, aber ueber den Auswaehler "
                     "nicht erreichbar: %1").arg(missing.join(QStringLiteral(", ")))));
        mw->close();
    }
};

QTEST_MAIN(TstEveryAppletIsReachable)
#include "tst_every_applet_is_reachable.moc"
