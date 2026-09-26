// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original design workbench, no Thetis logic.
//
// ENTWURFSWERKBANK, kein CI-Test (2026-09-26). Gibt die echten Bauteile
// als Bilder aus, aus denen die Blaetter "Loggen und Rotor im Logbuch"
// zusammengesetzt werden: den Rotor-Kompass (Rose und Band) im Zustand
// "dreht von 74° nach 11°" und das Rotor/Log-Feld, wie es im
// Rotorfenster steht. Laeuft nur mit LONGPATH_ENTWURF_DIR; dazu
// LONGPATH_CONFIG_DIR auf einen leeren Ordner, damit keine echte
// Rotor-Einstellung greift.

#include <QtTest>

#include "gui/widgets/RotorDialWidget.h"
#include "gui/widgets/RotorLogbookPanel.h"

#include <QLineEdit>

using namespace Longpath;

class TstLogbuchEntwurfWerkbank : public QObject { Q_OBJECT
private slots:
    void bauteile()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_ENTWURF_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_ENTWURF_DIR nicht gesetzt -- Entwurfswerkbank."); }
        if (qEnvironmentVariable("LONGPATH_CONFIG_DIR").isEmpty()) {
            QSKIP("LONGPATH_CONFIG_DIR fehlt -- sonst liest das Rotorfeld die echten Einstellungen.");
        }
        QDir().mkpath(dir);

        RotorDialWidget d;
        d.setActualBearing(74.0);     // Parkstellung der Station
        d.setTargetBearing(11.0);     // OE5VVM, Laakirchen
        d.setState(RotorDialWidget::State::Turning);
        for (int side : {70, 80, 90, 100, 130, 145, 160, 200, 260}) {
            d.resize(side, side);
            d.show();
            QVERIFY(QTest::qWaitForWindowExposed(&d));
            QTest::qWait(100);
            QVERIFY(d.grab().save(dir + QStringLiteral("/dial_rose_%1.png").arg(side)));
        }
        d.hide();

        RotorDialWidget tape;
        tape.setShape(RotorDialWidget::Shape::Tape);
        tape.setActualBearing(74.0);
        tape.setTargetBearing(11.0);
        tape.setState(RotorDialWidget::State::Turning);
        for (int w : {250, 300, 340}) {
            tape.resize(w, w == 250 ? 100 : 120);
            tape.show();
            QVERIFY(QTest::qWaitForWindowExposed(&tape));
            QTest::qWait(100);
            QVERIFY(tape.grab().save(dir + QStringLiteral("/dial_tape_%1.png").arg(w)));
        }
        tape.hide();

        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        panel.resize(360, 760);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        for (QLineEdit* e : panel.findChildren<QLineEdit*>()) {
            const QString ph = e->placeholderText();
            if (ph == QStringLiteral("callsign"))      { e->setText(QStringLiteral("OE5VVM")); }
            if (ph == QStringLiteral("your locator"))  { e->setText(QStringLiteral("JN67VV")); }
            if (ph == QStringLiteral("their locator")) { e->setText(QStringLiteral("JN67")); }
        }
        if (RotorDialWidget* pd = panel.dial()) {
            pd->setShape(RotorDialWidget::Shape::Rose);   // wie beim Betreiber
            pd->setActualBearing(74.0);
            pd->setTargetBearing(11.0);
            pd->setState(RotorDialWidget::State::Turning);
        }
        QTest::qWait(300);
        QVERIFY(panel.grab().save(dir + QStringLiteral("/rotor_log_panel.png")));
        // Gestaucht, wie es in eine Spalte des Logbuchs passen muesste.
        for (int h : {400, 480}) {
            panel.resize(360, h);
            QTest::qWait(300);
            QVERIFY(panel.grab().save(dir + QStringLiteral("/rotor_log_panel_%1.png").arg(h)));
        }
    }
};

QTEST_MAIN(TstLogbuchEntwurfWerkbank)
#include "tst_logbuch_entwurf_werkbank.moc"
