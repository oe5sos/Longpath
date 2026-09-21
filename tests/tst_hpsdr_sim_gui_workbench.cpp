// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_hpsdr_sim_gui_workbench.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// WERKBANK mit Oberflaeche: baut das echte MainWindow, verbindet es mit
// dem HPSDR-Simulator (hpsdrsim, GPL — Messgeraet, keine Quelle) und
// legt Bildschirmfotos ab, damit man sieht, was ein Betreiber eines
// Hermes/Hermes-Lite saehe — ohne ein Geraet zu besitzen. Gegenstueck
// zu tst_hpsdr_sim_workbench (nur Modell, kein Fenster).
//
// Laeuft NUR mit LONGPATH_HPSDRSIM=host:port (sonst QSKIP). Bilder gehen
// nach LONGPATH_GRAB_DIR (sonst keine). GPU-Flaechen (QRhiWidget) werden
// per grabFramebuffer() eingelesen und ueber das Fensterbild gelegt —
// QWidget::grab() liefert fuer sie nur Schwarz.

#include <QtTest>

#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "gui/MainWindow.h"
#include "gui/SetupDialog.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QDir>
#include <QHostAddress>
#include <QPainter>
#include <QSignalSpy>
#include <QTabWidget>
#ifdef NEREUS_GPU_SPECTRUM
#include <QRhiWidget>
#endif

using namespace Longpath;

namespace {

QImage grabWindow(QWidget* top)
{
    QImage base = top->grab().toImage();
#ifdef NEREUS_GPU_SPECTRUM
    QPainter p(&base);
    const QList<QRhiWidget*> gpu = top->findChildren<QRhiWidget*>();
    for (QRhiWidget* w : gpu) {
        if (!w->isVisible()) { continue; }
        const QImage fb = w->grabFramebuffer();
        if (fb.isNull()) { continue; }
        const QPoint at = w->mapTo(top, QPoint(0, 0));
        p.drawImage(QRect(at, w->size()), fb);
    }
#endif
    return base;
}

void saveGrab(QWidget* top, const QString& name)
{
    const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
    if (dir.isEmpty()) { return; }
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + name + QStringLiteral(".png");
    const QImage img = grabWindow(top);
    if (img.save(path)) {
        qInfo().noquote() << "GRAB" << path << img.size();
    } else {
        qWarning().noquote() << "GRAB FAILED" << path;
    }
}

} // namespace

class TstHpsdrSimGuiWorkbench : public QObject { Q_OBJECT
private slots:
    void mainwindow_against_simulator()
    {
        const QString target = qEnvironmentVariable("LONGPATH_HPSDRSIM");
        if (target.isEmpty()) {
            QSKIP("LONGPATH_HPSDRSIM nicht gesetzt — Werkbank, kein CI-Test.");
        }
        const QStringList hp = target.split(QLatin1Char(':'));
        const QHostAddress addr(hp.value(0, QStringLiteral("127.0.0.1")));
        const quint16 port = static_cast<quint16>(hp.value(1, QStringLiteral("1024")).toUInt());

        auto* mw = new MainWindow();
        mw->resize(1680, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));
        QTest::qWait(800);
        saveGrab(mw, QStringLiteral("01-vor-verbindung"));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);

        // ── Discovery ────────────────────────────────────────────────
        RadioDiscovery disc;
        QSignalSpy found(&disc, &RadioDiscovery::radioDiscovered);
        disc.probeAddress(addr, port);
        QTRY_VERIFY_WITH_TIMEOUT(found.count() > 0, 8000);
        const RadioInfo info = found.first().first().value<RadioInfo>();
        qInfo() << "DISCOVERED" << info.displayName() << "board" << int(info.boardType);

        // ── Verbinden — derselbe Aufruf wie der Connect-Knopf ────────
        QSignalSpy iq(model, &RadioModel::rawIqData);
        model->connectToRadio(info);
        QTRY_VERIFY_WITH_TIMEOUT(model->connectionState() == ConnectionState::Connected, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= 60, 10000);
        QTest::qWait(1500);   // Spektrum/Wasserfall ein paar Bilder laufen lassen
        saveGrab(mw, QStringLiteral("02-verbunden"));

        // Was das Fenster jetzt zeigt: Titel, Applets, Verbindungstext.
        qInfo() << "TITLE" << mw->windowTitle();
        QStringList visibleNames;
        for (QWidget* w : mw->findChildren<QWidget*>()) {
            if (w->isVisible() && !w->objectName().isEmpty()
                && w->objectName().contains(QLatin1String("Applet"), Qt::CaseInsensitive)) {
                visibleNames << w->objectName();
            }
        }
        visibleNames.removeDuplicates();
        qInfo().noquote() << "APPLETS" << visibleNames.join(QStringLiteral(", "));

        // ── Abstimmen ueber die Scheibe, wie ein Mausrad es taete ────
        if (SliceModel* s = model->activeSlice()) {
            s->setFrequency(7'100'000.0);
            QTest::qWait(1500);
            saveGrab(mw, QStringLiteral("03-40m"));
        }

        // ── Setup → Hardware: die Reiter, die es nur fuer dieses Board gibt
        {
            auto* dlg = new SetupDialog(model, mw);
            dlg->resize(1100, 760);
            dlg->selectPage(QStringLiteral("Hardware Config"));
            dlg->show();
            QVERIFY(QTest::qWaitForWindowExposed(dlg, 10000));
            QTest::qWait(600);
            QStringList tabs;
            for (QTabWidget* tw : dlg->findChildren<QTabWidget*>()) {
                for (int i = 0; i < tw->count(); ++i) {
                    if (tw->isTabVisible(i)) { tabs << tw->tabText(i); }
                }
            }
            qInfo().noquote() << "HARDWARE TABS" << tabs.join(QStringLiteral(" | "));
            saveGrab(dlg, QStringLiteral("05-setup-hardware"));
            // Den HL2-Reiter zeigen, damit man sieht, was dort steht.
            for (QTabWidget* tw : dlg->findChildren<QTabWidget*>()) {
                for (int i = tw->count() - 1; i >= 0; --i) {
                    if (tw->isTabVisible(i) && tw->tabText(i).contains(QLatin1String("HL2"))) {
                        tw->setCurrentIndex(i);
                        QTest::qWait(400);
                        saveGrab(dlg, QStringLiteral("06-setup-hardware-hl2"));
                        break;
                    }
                }
            }
            dlg->close();
            dlg->deleteLater();
        }

        // ── TUNE mit offener Diagnoseseite „Radio Status" ─────────────
        if (!qEnvironmentVariableIsSet("LONGPATH_HPSDRSIM_NO_TX")) {
            auto* dlg = new SetupDialog(model, mw);
            dlg->resize(1100, 760);
            dlg->selectPage(QStringLiteral("Radio Status"));
            dlg->show();
            QVERIFY(QTest::qWaitForWindowExposed(dlg, 10000));
            model->setTune(true);
            QTest::qWait(1500);
            saveGrab(mw, QStringLiteral("07-tune"));
            saveGrab(dlg, QStringLiteral("08-radio-status-bei-tune"));
            model->setTune(false);
            QTRY_VERIFY_WITH_TIMEOUT(!model->mox(), 8000);
            QTest::qWait(600);
            saveGrab(dlg, QStringLiteral("09-radio-status-nach-tune"));
            dlg->close();
            dlg->deleteLater();
        }

        // ── Trennen ──────────────────────────────────────────────────
        model->disconnectFromRadio();
        QTRY_COMPARE_WITH_TIMEOUT(model->connectionState(), ConnectionState::Disconnected, 8000);
        QTest::qWait(500);
        saveGrab(mw, QStringLiteral("04-getrennt"));
        mw->close();
    }
};

QTEST_MAIN(TstHpsdrSimGuiWorkbench)
#include "tst_hpsdr_sim_gui_workbench.moc"
