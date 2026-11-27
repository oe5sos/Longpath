// SPDX-License-Identifier: GPL-3.0-or-later
//
// „Mein Signal am Kiwi“ (Entwurf C, 2026-11-24) im Hauptfenster:
//   * der Kiwi folgt der SENDEfrequenz (Split-/XIT-Scheibe), nicht der
//     Empfangsfrequenz seiner eigenen Scheibe,
//   * der Ton bleibt beim Senden -- und ohne den Modus ist er weiter
//     stumm (Gegenprobe),
//   * der Streifen erscheint, das Applet zeigt den Zustand,
//   * "hoert er mich" aus Zeilen, die der Manager liefert,
//   * Aufnahme erzeugt eine Datei,
//   * die Sicherheitsschranke: uebernimmt ein echtes Funkgeraet die
//     Scheibe, ist der Modus aus und der Kiwi steuert nichts mehr.
//
// Laeuft offscreen wie tst_kiwi_tracking_follows_the_slice (dessen Hauptfenster
// stuerzt nicht ab) -- ohne die Wache aus HeadlessMacGuard.h.
//
// Longpath-eigener Pruefstand, kein Port.
#include <QtTest>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QToolButton>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/KiwiRecording.h"
#include "core/KiwiSdrManager.h"
#include "gui/KiwiOwnSignalStrip.h"
#include "gui/MainWindow.h"
#include "gui/applets/KiwiSdrApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

namespace {

void clearProfiles(KiwiSdrManager* mgr)
{
    for (const KiwiSdrAntennaProfile& p : mgr->profiles()) { mgr->removeProfile(p.id); }
}

QVector<float> bins(double bumpDb)
{
    QVector<float> v(1024, -110.0f);
    if (bumpDb > 0.0) {
        for (int i = 505; i < 520; ++i) { v[i] = float(-110.0 + bumpDb); }
    }
    return v;
}

} // namespace

class TstKiwiOwnSignalWindow : public QObject
{
    Q_OBJECT

private slots:
    void dieSendefrequenzFuehrtDenKiwi()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        QVERIFY(mgr && model);
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Linz"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QString pid = mgr->profiles().first().id;
        const int kiwiSlice = mgr->assignedSliceForProfile(pid);
        QVERIFY2(kiwiSlice >= 0, "Kiwi nicht zugeordnet");

        // Die Sendescheibe des Betreibers: ein anderer Slice, nicht vom Kiwi.
        const int txId = model->addSlice();
        SliceModel* tx = model->sliceById(txId);
        QVERIFY(tx && txId != kiwiSlice);
        tx->setFrequency(14.205e6);
        tx->setTxSlice(true);
        model->sliceById(kiwiSlice)->setFrequency(7.100e6);   // Empfang woanders

        // Ohne den Modus: kein Streifen.
        QVERIFY(mw->kiwiOwnStripForTest());
        QVERIFY(mw->kiwiOwnStripForTest()->isHidden());

        QSignalSpy tracked(mgr, &KiwiSdrManager::sliceTrackingUpdated);
        mw->setKiwiOwnSignalProfileForTest(pid, 38.0);
        mw->setKiwiOwnSignalForTest(true);
        QVERIFY(mw->kiwiOwnSignalOnForTest());
        QVERIFY(!mw->kiwiOwnStripForTest()->isHidden());
        QVERIFY(tracked.count() >= 1);
        QCOMPARE(tracked.last().at(0).toInt(), kiwiSlice);
        QCOMPARE(tracked.last().at(1).toDouble(), 14.205);   // Sende-, nicht Empfangsfrequenz
        QCOMPARE(mw->kiwiOwnStripForTest()->markX() >= -1, true);

        // XIT verschiebt die Sendefrequenz -- der Kiwi folgt.
        tracked.clear();
        tx->setXitHz(1000);
        tx->setXitEnabled(true);
        QVERIFY(tracked.count() >= 1);
        QCOMPARE(tracked.last().at(1).toDouble(), 14.206);

        // Der Empfang der Kiwi-Scheibe bewegt ihn im Modus NICHT.
        tracked.clear();
        model->sliceById(kiwiSlice)->setFrequency(7.200e6);
        for (int i = 0; i < tracked.count(); ++i) {
            QCOMPARE(tracked.at(i).at(1).toDouble(), 14.206);
        }

        // Applet zeigt den Zustand.
        auto* btn = mw->kiwiSdrAppletForTest()->findChild<QPushButton*>(
            QStringLiteral("kiwiOwnSignalButton"));
        QVERIFY(btn);
        QVERIFY(btn->isChecked());

        // Aus: der Kiwi folgt wieder seiner Scheibe.
        tracked.clear();
        mw->setKiwiOwnSignalForTest(false);
        QVERIFY(mw->kiwiOwnStripForTest()->isHidden());
        QVERIFY(!btn->isChecked());
        QVERIFY(tracked.count() >= 1);
        QCOMPARE(tracked.last().at(1).toDouble(), 7.2);
    }

    void derTonBleibtImModusBeimSenden()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Linz"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QString pid = mgr->profiles().first().id;
        const int sliceId = mgr->assignedSliceForProfile(pid);
        QVERIFY(sliceId >= 0);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        audio->setKiwiSdrAudioSourceEnabled(sliceId, true);

        // Gegenprobe: ohne den Modus ist er beim Senden stumm (bisheriges Verhalten).
        model->transmitModel().setMox(true);
        QVERIFY(!audio->kiwiSdrAudioEnabled(sliceId));
        model->transmitModel().setMox(false);
        QVERIFY(audio->kiwiSdrAudioEnabled(sliceId));

        // Im Modus bleibt er an -- auch wenn der Modus MITTEN in der Sendung
        // eingeschaltet wird.
        mw->setKiwiOwnSignalProfileForTest(pid);
        mw->setKiwiOwnSignalForTest(true);
        model->transmitModel().setMox(true);
        QVERIFY2(audio->kiwiSdrAudioEnabled(sliceId),
                 "Im Modus Mein Signal wurde der Kiwi beim Senden stumm");
        mw->setKiwiOwnSignalForTest(false);
        QVERIFY2(!audio->kiwiSdrAudioEnabled(sliceId),
                 "Nach dem Ausschalten gilt die Sendesperre wieder");
        mw->setKiwiOwnSignalForTest(true);
        QVERIFY(audio->kiwiSdrAudioEnabled(sliceId));
        model->transmitModel().setMox(false);
    }

    void hoertErMich_ausZeilenDesManagers()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Linz"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QString pid = mgr->profiles().first().id;
        const int txId = model->addSlice();
        SliceModel* tx = model->sliceById(txId);
        tx->setFrequency(14.205e6);
        tx->setTxSlice(true);
        mw->setKiwiOwnSignalProfileForTest(pid, 38.0);
        mw->setKiwiOwnSignalForTest(true);
        KiwiOwnSignalStrip* strip = mw->kiwiOwnStripForTest();
        QCOMPARE(strip->statusText(), QStringLiteral("waiting for transmission"));

        // Gesendet, aber kein Bild: unbekannt -- NICHT "hoert nichts".
        model->transmitModel().setMox(true);
        QCOMPARE(strip->statusText(), QStringLiteral("unknown"));

        // Der Kiwi liefert Zeilen, auf der Marke steht ein Buckel.
        AppSettings::instance().setValue(
            QStringLiteral("KiwiWaterfall/RowIntervalMs"), QStringLiteral("0"));
        for (int i = 0; i < 40; ++i) {
            emit mgr->waterfallRowReady(pid, QStringLiteral("p"), bins(25.0),
                                        14.185, 14.225, quint32(i));
            QTest::qWait(40);
        }
        QCOMPARE(strip->statusText().left(7), QStringLiteral("hears m"));
        QVERIFY(strip->rowCount() >= 30);

        // Zum Ansehen (Vergleich mit dem Entwurf): das ganze Fenster und die
        // Spalte des Panadapters als Bild, wenn LONGPATH_STRIP_PNG gesetzt ist.
        // Ohne QRhi (offscreen) bleibt die Panadapter-Flaeche selbst leer --
        // geprueft wird hier der PLATZ des Streifens, nicht das Spektrum.
        const QString out = qEnvironmentVariable("LONGPATH_STRIP_PNG");
        if (!out.isEmpty()) {
            for (int i = 0; i < 90; ++i) {
                emit mgr->waterfallRowReady(pid, QStringLiteral("p"), bins(25.0),
                                            14.185, 14.225, quint32(100 + i));
            }
            QTest::qWait(100);
            QVERIFY(mw->grab().save(out + "-fenster.png"));
            if (QWidget* pane = strip->parentWidget()) {
                QVERIFY(pane->grab().save(out + "-spalte.png"));
            }
            if (KiwiSdrApplet* ap = mw->kiwiSdrAppletForTest()) {
                ap->resize(360, ap->sizeHint().height());
                QVERIFY(ap->grab().save(out + "-applet.png"));
            }
            qInfo() << "Streifen" << strip->geometry() << "sichtbar"
                    << strip->isVisible() << "Spalte" << strip->parentWidget()->size();
        }
        model->transmitModel().setMox(false);
    }

    // Betreiber 2026-11-25: "unter dem Panadapter ist der Kiwi schoen, aber dort
    // kann ich nichts aendern" -- der Streifen hat Ton und Zoom im Kopf.
    void derStreifenHatTonUndZoom()
    {
        KiwiOwnSignalStrip strip;
        strip.resize(600, 176);
        strip.show();   // erst sichtbar bekommt es die Groessenmeldung
        QToolButton* ton = strip.soundButtonForTest();
        QVERIFY(ton);
        QSignalSpy tonSpy(&strip, &KiwiOwnSignalStrip::soundToggled);
        QSignalSpy zoomSpy(&strip, &KiwiOwnSignalStrip::zoomRequested);
        QTest::mouseClick(ton, Qt::LeftButton);
        QCOMPARE(tonSpy.count(), 1);
        QVERIFY(tonSpy.at(0).at(0).toBool());
        QVERIFY(strip.soundActiveForTest());
        // Von aussen gesetzt: kein Echo.
        strip.setSoundActive(false);
        QCOMPARE(tonSpy.count(), 1);
        const auto buttons = strip.findChildren<QToolButton*>();
        QCOMPARE(buttons.size(), 3);
        for (QToolButton* b : buttons) {
            QVERIFY(b->geometry().right() <= strip.width());
            QVERIFY(b->geometry().bottom() < 26);   // im Kopf
        }
        Q_UNUSED(zoomSpy);
    }

    // Betreiber 2026-11-25: "alle Wasserfaelle laufen zu schnell".
    void derStreifenHaeltDasEingestellteTempo()
    {
        KiwiOwnSignalStrip strip;
        strip.setRowIntervalMs(250);
        const QVector<float> row(256, -100.0f);
        for (int i = 0; i < 20; ++i) {
            strip.pushRow(row, 14.19e6, 14.21e6);
        }
        QCOMPARE(strip.rowCount(), 1);
        QTest::qWait(300);
        strip.pushRow(row, 14.19e6, 14.21e6);
        QCOMPARE(strip.rowCount(), 2);
    }

    void aufnahmeErzeugtEineDatei()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Linz"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QString pid = mgr->profiles().first().id;
        mw->setKiwiOwnSignalProfileForTest(pid);
        mw->setKiwiOwnSignalForTest(true);

        mw->startKiwiRecordingForTest();
        KiwiRecording* rec = mw->kiwiRecordingForTest();
        QVERIFY(rec && rec->isRecording());
        QByteArray pcm(24000 * 8, 0);
        emit mgr->decodedAudioReady(pid, pcm);
        emit mgr->decodedAudioReady(QStringLiteral("fremd"), pcm);   // anderes Profil: nicht
        const QString wav = rec->wavPath();
        mw->stopKiwiRecordingForTest();
        QVERIFY(QFileInfo::exists(wav));
        QCOMPARE(QFileInfo(wav).size(), qint64(44 + 24000 * 8));
    }

    void eineUebernommeneScheibeSperrtUndBeendetDenModus()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Linz"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QString pid = mgr->profiles().first().id;
        const int sliceId = mgr->assignedSliceForProfile(pid);
        mw->setKiwiOwnSignalProfileForTest(pid);
        mw->setKiwiOwnSignalForTest(true);
        QVERIFY(mw->kiwiOwnSignalOnForTest());

        const int txId = model->addSlice();
        SliceModel* tx = model->sliceById(txId);
        tx->setFrequency(14.205e6);
        tx->setTxSlice(true);

        // Ein echtes Funkgeraet bindet die Scheibe des Kiwi: die Schranke
        // (kiwiControllableSlice) sperrt jede Nachfuehrung -- auch die der
        // SENDEfrequenz. (Die Zuordnung selbst loest erst die Bindungsmeldung
        // des RadioModel, siehe releaseKiwiSdrSlice.)
        QSignalSpy tracked(mgr, &KiwiSdrManager::sliceTrackingUpdated);
        model->sliceById(sliceId)->setStreamIndex(0);
        tracked.clear();
        tx->setFrequency(14.210e6);
        model->sliceById(sliceId)->setFrequency(14.060e6);
        QCOMPARE(tracked.count(), 0);

        // Fiele die Zuordnung (releaseKiwiSdrSlice), ist der Modus aus.
        mgr->clearSliceAssignment(sliceId);
        QVERIFY(!mw->kiwiOwnSignalOnForTest());
        QVERIFY(mw->kiwiOwnStripForTest()->isHidden());
    }
};

QTEST_MAIN(TstKiwiOwnSignalWindow)
#include "tst_kiwi_own_signal_window.moc"
