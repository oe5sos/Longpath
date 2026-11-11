// no-port-check: Pruefstand fuer Longpath-eigene Oberflaeche.
// =================================================================
// tests/tst_bandstapel_im_fenster.cpp  (Longpath)
// =================================================================
//
// Der Bandstapel im ECHTEN Hauptfenster, offscreen. Betreiber am
// 2026-11-11: „bitte teste dann selbst". Ohne sein laufendes Fenster und
// ohne Funkgeraet: die Verbindung wird dem Modell vorgetaeuscht
// (setLastRadioInfoForTest + setConnectionStateForTest), alles andere
// ist das Programm, wie es laeuft.
//
// Gefahren wird die Bedienung:
//   · das Applet steht in der Spalte und sagt ohne Geraet, warum nicht
//   · die Bandtaste (MainWindow::bandUmschalten, der EINE Weg aller
//     Bandknoepfe) landet auf dem ersten Sprecheintrag, nochmal = weiter
//   · der Stand im Applet folgt, der Hinweis erscheint
//   · ein Klick auf eine Applet-Zeile fuehrt hin (QMouseEvent)
//   · die Marken liegen im Spektrum des Panadapters, die Stelle hell,
//     und ein Klick auf eine Marke fuehrt hin (QMouseEvent)
//   · Band > Band Stacking... holt das ausgeblendete Applet zurueck
//
// Mit LONGPATH_GRAB_DIR legt er ein Bild des Fensters ab.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>

#include "FensterFrist.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/BandStackApplet.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackController.h"

using namespace Longpath;

namespace {

void klick(QWidget* w, const QPoint& pos)
{
    const QPointF lokal(pos);
    const QPointF global(w->mapToGlobal(pos));
    QMouseEvent druck(QEvent::MouseButtonPress, lokal, global, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, lokal, global, Qt::LeftButton, Qt::NoButton,
                    Qt::NoModifier);
    QApplication::sendEvent(w, &los);
}

QString idBei(BandStackController* stapel, Band band, qint64 hz)
{
    for (const BandStackEntry& e : stapel->store().entries(band)) {
        if (e.frequencyHz == hz) { return e.id; }
    }
    return {};
}

QStringList toastTexte(QWidget* mw)
{
    QStringList out;
    for (StatusToast* t : mw->findChildren<StatusToast*>()) {
        for (QLabel* l : t->findChildren<QLabel*>()) {
            if (!l->text().isEmpty()) { out << l->text(); }
        }
    }
    return out;
}

} // namespace

class TstBandstapelImFenster : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::instance().setValue(QStringLiteral("Region"), QStringLiteral("Europe"));
    }

    void dieBedienungImEchtenFenster()
    {
        auto* mw = new MainWindow();
        mw->resize(1600, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        RadioModel* rm = mw->radioModelForTest();
        QVERIFY(rm != nullptr);
        BandStackController* stapel = rm->bandStack();
        QVERIFY(stapel != nullptr);

        // ── Das Applet steht in der Spalte ─────────────────────────────
        AppletVisibilityController* vis = mw->appletVisibilityForTest();
        QVERIFY(vis != nullptr);
        QVERIFY2(vis->isEffectivelyVisible(QStringLiteral("BandStack")),
                 "Das Bandstapel-Applet ist ab Werk nicht sichtbar");
        auto* applet = mw->findChild<BandStackApplet*>();
        QVERIFY(applet != nullptr);
        // Ohne Geraet: der Grund statt einer leeren Liste.
        QVERIFY2(applet->erklaerungForTest()->text().contains(QStringLiteral("Connect")),
                 qPrintable(applet->erklaerungForTest()->text()));

        // ── Ein Geraet vortaeuschen ────────────────────────────────────
        if (rm->slices().isEmpty()) {
            rm->addSlice();
        }
        SliceModel* scheibe = rm->activeSlice();
        QVERIFY(scheibe != nullptr);
        scheibe->setFrequency(14225000.0);
        scheibe->setDspMode(DSPMode::USB);
        RadioInfo info;
        info.macAddress = QStringLiteral("02:00:00:00:BA:FE");
        rm->setLastRadioInfoForTest(info);
        rm->setConnectionStateForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(stapel->isReady(), kFensterFrist);
        QCoreApplication::processEvents();
        const int id = scheibe->sliceIndex();

        // ── Bandtaste: der eine Weg aller Bandknoepfe ──────────────────
        mw->bandUmschalten(Band::Band40m);
        QCoreApplication::processEvents();
        QCOMPARE(qRound64(scheibe->frequency()), qint64(7090000));
        QCOMPARE(scheibe->dspMode(), DSPMode::LSB);
        QCOMPARE(applet->standForTest()->text(), QStringLiteral("40m 4/5"));
        QVERIFY2(toastTexte(mw).join(QLatin1Char('|')).contains(QStringLiteral("40m: 7.090 LSB, 4 of 5")),
                 qPrintable(toastTexte(mw).join(QLatin1Char('|'))));

        // Nochmal dieselbe Taste: der naechste Eintrag.
        mw->bandUmschalten(Band::Band40m);
        QCoreApplication::processEvents();
        QCOMPARE(qRound64(scheibe->frequency()), qint64(7100000));
        QCOMPARE(applet->standForTest()->text(), QStringLiteral("40m 5/5"));

        // ── Klick auf eine Zeile im Applet ─────────────────────────────
        BandStackListe* liste = applet->listeForTest();
        const QRect zeile = liste->zeileRectForTest(idBei(stapel, Band::Band40m, 7045000));
        QVERIFY(zeile.isValid());
        klick(liste, zeile.center());
        QCoreApplication::processEvents();
        QCOMPARE(qRound64(scheibe->frequency()), qint64(7045000));
        QCOMPARE(applet->standForTest()->text(), QStringLiteral("40m 3/5"));

        // ── Die Marken im Spektrum ─────────────────────────────────────
        SpectrumWidget* sw = nullptr;
        for (SpectrumWidget* s : mw->findChildren<SpectrumWidget*>()) {
            if (!s->bandStackMarks().isEmpty()) { sw = s; break; }
        }
        QVERIFY2(sw != nullptr, "Kein Panadapter traegt Bandstapel-Marken");
        int hell = 0;
        QString hellId;
        for (const SpectrumWidget::BandStackMark& m : sw->bandStackMarks()) {
            if (m.place) { ++hell; hellId = m.id; }
        }
        QCOMPARE(hell, 1);
        QCOMPARE(hellId, idBei(stapel, Band::Band40m, 7045000));

        // Irgendeine sichtbare Marke ausser der eigenen Stelle anklicken.
        // Liegt keine im Bild, das Bild auf das ganze Band ziehen.
        const auto sucheMarke = [&](QString* id) {
            for (int y = sw->height() - 1; y > 0; --y) {
                for (int x = 0; x < sw->width(); ++x) {
                    const QString m = sw->bandStackMarkAtForTest(x, y);
                    if (!m.isEmpty() && m != hellId) { *id = m; return QPoint(x, y); }
                }
            }
            return QPoint(-1, -1);
        };
        QString ziel;
        QPoint wo = sucheMarke(&ziel);
        if (wo.x() < 0) {
            sw->setFrequencyRange(7100000.0, 200000.0);
            QCoreApplication::processEvents();
            wo = sucheMarke(&ziel);
        }
        QVERIFY2(wo.x() >= 0, "Keine anklickbare Marke im Spektrum");
        const qint64 zielHz = stapel->store().entry(ziel)->frequencyHz;
        qInfo() << "Markenklick auf" << zielHz << "bei" << wo;
        klick(sw, wo);
        QCoreApplication::processEvents();
        QCOMPARE(qRound64(scheibe->frequency()), zielHz);

        // ── Band > Band Stacking... holt das Applet zurueck ────────────
        vis->setVisible(QStringLiteral("BandStack"), false);
        QVERIFY(!vis->isEffectivelyVisible(QStringLiteral("BandStack")));
        QAction* eintrag = nullptr;
        for (QAction* b : mw->findChildren<QAction*>()) {
            if (b->text().remove(QLatin1Char('&')) == QStringLiteral("Band Stacking...")) {
                eintrag = b;
            }
        }
        QVERIFY(eintrag != nullptr);
        QVERIFY(eintrag->isEnabled());
        eintrag->trigger();
        QCoreApplication::processEvents();
        QVERIFY(vis->isEffectivelyVisible(QStringLiteral("BandStack")));

        // ── Bild des Fensters ──────────────────────────────────────────
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()) {
            QTest::qWait(300);
            const QImage bild = mw->grab().toImage();
            QVERIFY(bild.save(QDir(dir).filePath(QStringLiteral("bandstapel-im-fenster.png"))));
            const QImage nah = applet->grab().toImage();
            QVERIFY(nah.save(QDir(dir).filePath(QStringLiteral("bandstapel-applet-im-fenster.png"))));
        }

        rm->setConnectionStateForTest(ConnectionState::Disconnected);
        delete mw;
        Q_UNUSED(id);
    }
};

QTEST_MAIN(TstBandstapelImFenster)
#include "tst_bandstapel_im_fenster.moc"
