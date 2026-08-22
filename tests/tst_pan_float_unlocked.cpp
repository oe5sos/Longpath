// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Abloese-Schalter des Panadapters funktioniert in beide
// Richtungen.
//
// Vorgeschichte: am 2026-08-21 war das Abloesen einen Tag lang
// GESPERRT (Absturz beim Beenden, upstream #2495; diese Datei hiess
// tst_pan_float_locked und bewachte die Sperre samt Rueckweg). Am
// 2026-08-22 wurden die vier bei der Portierung verlorenen
// AetherSDR-Schutzmethoden rueckportiert; der Absturz-Repro (Beenden
// mit abgeloestem Panadapter, tst_real_pan_float_state) ueberlebt
// seither, und die Sperre fiel.

#include <QtTest>
#include <QMenu>
#include <QPushButton>

#include "gui/PanadapterApplet.h"

using namespace Longpath;

class TstPanFloatUnlocked : public QObject
{
    Q_OBJECT

private:
    static QPushButton* floatButton(PanadapterApplet& a)
    {
        for (QPushButton* b : a.findChildren<QPushButton*>()) {
            const QString t = b->text();
            if (t == QString::fromUtf8("\u2197")
                || t == QString::fromUtf8("\u2199")) {
                return b;
            }
        }
        return nullptr;
    }

private slots:
    void theButtonDetaches()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.setFloatingIndicator(false);

        QPushButton* b = floatButton(applet);
        QVERIFY(b);
        QVERIFY2(b->isEnabled(), "Die Sperre ist zurueck?");

        QSignalSpy out(&applet, &PanadapterApplet::floatRequested);
        b->click();
        QCOMPARE(out.count(), 1);
    }

    void theSameButtonComesBack()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.setFloatingIndicator(true);

        QPushButton* b = floatButton(applet);
        QVERIFY(b && b->isEnabled());

        QSignalSpy back(&applet, &PanadapterApplet::dockRequested);
        b->click();
        QCOMPARE(back.count(), 1);
    }

    void theMenuEntryWorksToo()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QMenu* m = applet.buildContextMenuForTesting();
        QVERIFY(m);

        QAction* floatAct = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text().contains(QStringLiteral("Float"))) { floatAct = a; }
        }
        QVERIFY(floatAct);
        QVERIFY2(floatAct->isEnabled(), "Der Menueeintrag ist noch gesperrt");
    }
};

QTEST_MAIN(TstPanFloatUnlocked)
#include "tst_pan_float_unlocked.moc"
