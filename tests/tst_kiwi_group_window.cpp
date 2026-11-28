// SPDX-License-Identifier: GPL-3.0-or-later
//
// "Wo werde ich gehoert" im Hauptfenster (Entwurf B + C, 2026-11-25):
//   * ein Mithoerer bekommt KEINE Scheibe (keine neue, keine zugeordnete)
//     und damit keinen Weg zur Sicherheitsschranke,
//   * hoechstens fuenf Kiwis insgesamt, der von "Mein Signal" eingerechnet,
//   * ein Kiwi, der schon eine Scheibe speist, wird nicht Mithoerer,
//   * Wasserfallzeilen kommen in der Kachel an, Entfernen trennt den Kiwi,
//   * der Knopf im Applet oeffnet das Fenster.
//
// Ohne Netz: die Endpunkte liegen auf 127.0.0.1, Zeilen werden als Signal
// des Managers eingespeist.
//
// Longpath-eigener Pruefstand, kein Port.
#include <QSignalSpy>
#include <QAbstractButton>
#include <QLabel>
#include <QtTest>
#include <QSignalBlocker>
#include <QListWidget>
#include <QPushButton>
#include <QPointer>
#include <QRegularExpression>
#include <QToolButton>
#include <QSlider>
#include <QTcpServer>
#include <memory>
#include <vector>
#include <QScopeGuard>

#include "core/AudioEngine.h"
#include "core/KiwiGroup.h"
#include "core/KiwiSdrManager.h"
#include "gui/KiwiHearingDialog.h"
#include "gui/KiwiWaterfallPanel.h"
#include "gui/KiwiWaterfallStripWidget.h"
#include "gui/KiwiOwnSignalStrip.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/widgets/KiwiLostOverlay.h"
#include "gui/applets/KiwiSdrApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

void clearProfiles(KiwiSdrManager* mgr)
{
    for (const KiwiSdrAntennaProfile& p : mgr->profiles()) { mgr->removeProfile(p.id); }
}

QString profile(KiwiSdrManager* mgr, const char* name, int port)
{
    return mgr->addProfile(QString::fromLatin1(name),
                           QStringLiteral("127.0.0.1:%1").arg(port));
}

} // namespace

class TstKiwiGroupWindow : public QObject
{
    Q_OBJECT
private slots:
    void mithoererBekommtKeineScheibe()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        QVERIFY(mgr && model);
        clearProfiles(mgr);

        const int slicesBefore = model->slices().size();
        const QString a = profile(mgr, "Linz", 9);
        QVERIFY(mw->addKiwiListenerForTest(a, 60.0, true, 48.3, 14.3));
        QVERIFY(mgr->isProfileListener(a));
        QCOMPARE(mgr->assignedSliceForProfile(a), -1);       // keine Zuordnung
        QCOMPARE(model->slices().size(), slicesBefore);      // keine neue Scheibe
        QCOMPARE(mw->kiwiGroupForTest().size(), 1);

        // Entfernen trennt wieder.
        mw->removeKiwiListenerForTest(a);
        QVERIFY(!mgr->isProfileListener(a));
        QCOMPARE(mw->kiwiGroupForTest().size(), 0);
    }

    void hoechstensFuenfInsgesamt()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);

        // Ohne "Mein Signal": fuenf Mithoerer, der sechste nicht.
        QStringList ids;
        for (int i = 0; i < 6; ++i) { ids << profile(mgr, qPrintable(QStringLiteral("K%1").arg(i)), 10 + i); }
        for (int i = 0; i < 5; ++i) { QVERIFY(mw->addKiwiListenerForTest(ids[i])); }
        QVERIFY(!mw->addKiwiListenerForTest(ids[5]));
        QCOMPARE(mw->kiwiGroupForTest().size(), 5);

        // Wird einer von ihnen der Kiwi von "Mein Signal", zaehlt er nur
        // einmal -- und die Gruppe bleibt bei insgesamt fuenf.
        mw->setKiwiOwnSignalProfileForTest(ids[5], 10.0);
        QVERIFY(mw->kiwiGroupForTest().size() <= 4);
        QVERIFY(!mw->kiwiGroupForTest().contains(ids[5]));
        QVERIFY(!mgr->isProfileListener(ids[4]) || mw->kiwiGroupForTest().contains(ids[4]));
        // Wer herausgefallen ist, ist auch nicht mehr Mithoerer.
        int listeners = 0;
        for (const QString& id : ids) { if (mgr->isProfileListener(id)) { ++listeners; } }
        QCOMPARE(listeners, mw->kiwiGroupForTest().size());
    }

    void keinKiwiMitScheibeWirdMithoerer()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Mit Scheibe"),
                                      QStringLiteral("127.0.0.1:20"));
        const QString pid = mgr->profiles().first().id;
        QVERIFY(mgr->assignedSliceForProfile(pid) >= 0);
        QVERIFY(!mw->addKiwiListenerForTest(pid));
        QVERIFY(!mgr->isProfileListener(pid));
        QCOMPARE(mw->kiwiGroupForTest().size(), 0);
    }

    // Im echten Hauptfenster (Applet in der Spalte), nicht allein: waechst
    // die Liste, wenn das Fenster hoeher wird? (Bild von Martin 2026-11-25:
    // drei Zeilen und darunter leerer Platz.)
    void listeImHauptfensterWaechst()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 700);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        for (int i = 0; i < 12; ++i) {
            profile(mgr, qPrintable(QStringLiteral("Kiwi%1").arg(i)), 40 + i);
        }
        auto* list = mw->kiwiSdrAppletForTest()->findChild<QListWidget*>();
        QVERIFY(list);
        const auto visibleRows = [&]() {
            int n = 0;
            const QRect view = list->viewport()->rect();
            for (int i = 0; i < list->count(); ++i) {
                if (view.contains(list->visualItemRect(list->item(i)))) { ++n; }
            }
            return n;
        };
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 12, 3000);   // Aufbau wird gebuendelt
        const int klein = visibleRows();
        mw->resize(1400, 1100);
        QTest::qWait(150);
        const int gross = visibleRows();
        qInfo() << "Hauptfenster: Zeilen klein" << klein << "gross" << gross
                << "Listenhoehe" << list->height() << "Applet sichtbar"
                << mw->kiwiSdrAppletForTest()->isVisible();
        QVERIFY2(gross > klein, "hoeheres Hauptfenster muss mehr Zeilen zeigen");

    }

    // "Nur Kiwi hoeren": nur Wiedergabe, nie ein Mute an der Scheibe.
    void soloHoertNurDenKiwiUndRuehrtKeineScheibeAn()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Linz"),
                                      QStringLiteral("127.0.0.1:60"));
        const QString pid = mgr->profiles().first().id;
        const int kiwiSlice = mgr->assignedSliceForProfile(pid);
        QVERIFY(kiwiSlice >= 0);
        const int txId = model->addSlice();
        SliceModel* radio = model->sliceById(txId);
        QVERIFY(radio && !radio->muted());

        // Ohne "Mein Signal": abgelehnt, nichts stumm.
        mw->setKiwiSoloForTest(true);
        QVERIFY(!mw->kiwiSoloOnForTest());
        QCOMPARE(audio->soloSlice(), -1);

        mw->setKiwiOwnSignalProfileForTest(pid, 10.0);
        mw->setKiwiOwnSignalForTest(true);
        QVERIFY(mw->kiwiOwnSignalOnForTest());
        mw->setKiwiSoloForTest(true);
        QVERIFY(mw->kiwiSoloOnForTest());
        QCOMPARE(audio->soloSlice(), kiwiSlice);
        // Die Scheibe des Funkgeraets bleibt unberuehrt.
        QVERIFY(!radio->muted());
        auto* btn = mw->kiwiSdrAppletForTest()->findChild<QPushButton*>(
            QStringLiteral("kiwiSoloButton"));
        QVERIFY(btn && btn->isChecked());

        // Aus -> alles wieder da.
        mw->setKiwiSoloForTest(false);
        QCOMPARE(audio->soloSlice(), -1);
        QVERIFY(!btn->isChecked());

        // Faellt der Kiwi aus (Fehler), geht Solo von selbst aus: sonst Stille.
        mw->setKiwiSoloForTest(true);
        QVERIFY(mw->kiwiSoloOnForTest());
        emit mgr->profileStateChanged(pid, KiwiSdrClient::State::Error,
                                      QStringLiteral("kein Tonstrom"));
        QVERIFY(!mw->kiwiSoloOnForTest());
        QCOMPARE(audio->soloSlice(), -1);

        // "Mein Signal" aus -> Solo aus, sonst bliebe alles stumm.
        mw->setKiwiSoloForTest(true);
        QCOMPARE(audio->soloSlice(), kiwiSlice);
        mw->setKiwiOwnSignalForTest(false);
        QCOMPARE(audio->soloSlice(), -1);
        QVERIFY(!mw->kiwiSoloOnForTest());
    }

    // Ein Endpunkt, ein Profil: dreimal "hinzufuegen" belegt keine drei Kanaele.
    void einEndpunktEinProfil()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        const QString a = mgr->addProfile(QStringLiteral("Bern"), QStringLiteral("127.0.0.1:70"));
        const QString b = mgr->addProfile(QStringLiteral("Bern 2"), QStringLiteral("127.0.0.1:70"));
        const QString c = mgr->addProfile(QStringLiteral("Bern 3"), QStringLiteral("127.0.0.1:70"));
        QVERIFY(!a.isEmpty());
        QCOMPARE(b, a);
        QCOMPARE(c, a);
        QCOMPARE(mgr->profiles().size(), 1);
        // Gegenprobe: ein anderer Endpunkt ist ein anderes Profil.
        QVERIFY(mgr->addProfile(QStringLiteral("Anders"), QStringLiteral("127.0.0.1:71")) != a);
        QCOMPARE(mgr->profiles().size(), 2);
    }

    // Ton je Kiwi im Wasserfall-Streifen: eine EIGENE ungebundene Scheibe, nie
    // die des Funkgeraets; aus -> Scheibe weg.
    void tonJeKiwiSchaltetEineEigeneScheibe()
    {
        // Die Scheiben-Plakette der Liste hatte ein unausgefuelltes %2
        // (Betreiberlog 2026-11-25: "Could not parse stylesheet").
        QTest::failOnWarning(QRegularExpression(QStringLiteral("Could not parse stylesheet")));
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        clearProfiles(mgr);
        const int radioSlice = model->addSlice();
        SliceModel* radio = model->sliceById(radioSlice);
        QVERIFY(radio);
        const double radioFreq = radio->frequency();

        const QString id = profile(mgr, "Linz", 80);
        mgr->setWaterfallPreviewEnabled(id, true);
        KiwiWaterfallPanel* panel = mw->kiwiWaterfallPanelForTest();
        QVERIFY(panel && panel->stripForTest(id));
        auto* strip = panel->stripForTest(id);
        QVERIFY(!strip->soundActive());
        QCOMPARE(mgr->assignedSliceForProfile(id), -1);

        const int slicesBefore = model->slices().size();
        QTest::mouseClick(strip->soundButtonForTest(), Qt::LeftButton);   // Ton an
        const int sid = mgr->assignedSliceForProfile(id);
        QVERIFY(sid >= 0);
        QVERIFY(sid != radioSlice);                          // nicht die des Funkgeraets
        QCOMPARE(model->slices().size(), slicesBefore + 1);
        QVERIFY(model->sliceById(sid)->streamIndex() < 0);   // ungebunden
        QVERIFY(strip->soundActive());
        QCOMPARE(radio->frequency(), radioFreq);             // Funkgeraet unberuehrt

        QTest::mouseClick(strip->soundButtonForTest(), Qt::LeftButton);   // Ton aus
        QCOMPARE(mgr->assignedSliceForProfile(id), -1);
        QCOMPARE(model->slices().size(), slicesBefore);      // unsere Scheibe ist weg
        QVERIFY(!strip->soundActive());
        QVERIFY(model->sliceById(radioSlice) != nullptr);    // die andere steht noch
    }

    // Plus/Minus im Streifen veraendern den Bereich; "Alle Ton" schaltet alle
    // verbundenen Kiwis, hoechstens fuenf.
    void zoomUndAlleTon()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        clearProfiles(mgr);
        const int radioSlice = model->addSlice();
        QVERIFY(radioSlice >= 0);

        // Stumme Gegenstellen: sie nehmen die Verbindung an und antworten nie,
        // sodass jeder Kiwi im Zustand "Verbindet" bleibt (ein geschlossener
        // Port wuerde sofort auf Fehler laufen, und der Test hinge am Takt).
        std::vector<std::unique_ptr<QTcpServer>> stumm;
        QStringList ids;
        for (int i = 0; i < 7; ++i) {
            stumm.push_back(std::make_unique<QTcpServer>());
            QVERIFY(stumm.back()->listen(QHostAddress::LocalHost, 0));
            ids << profile(mgr, qPrintable(QStringLiteral("Z%1").arg(i)),
                           stumm.back()->serverPort());
            mgr->setWaterfallPreviewEnabled(ids.last(), true);
        }
        KiwiWaterfallPanel* panel = mw->kiwiWaterfallPanelForTest();
        QVERIFY(panel->stripForTest(ids[0]));

        // Zoom: Plus halbiert, Minus verdoppelt, in den Grenzen 5..200 kHz.
        QCOMPARE(mw->kiwiViewSpanForTest(ids[0]), 0.0);
        QTest::mouseClick(panel->stripForTest(ids[0])->zoomInButtonForTest(), Qt::LeftButton);
        QCOMPARE(mw->kiwiViewSpanForTest(ids[0]), 20000.0);      // aus 40 kHz
        QTest::mouseClick(panel->stripForTest(ids[0])->zoomInButtonForTest(), Qt::LeftButton);
        QCOMPARE(mw->kiwiViewSpanForTest(ids[0]), 10000.0);
        for (int i = 0; i < 8; ++i) { mw->zoomKiwiStripForTest(ids[0], +1); }
        QCOMPARE(mw->kiwiViewSpanForTest(ids[0]), 5000.0);       // nicht kleiner
        for (int i = 0; i < 10; ++i) { mw->zoomKiwiStripForTest(ids[0], -1); }
        QCOMPARE(mw->kiwiViewSpanForTest(ids[0]), 200000.0);     // nicht groesser

        // Vergroessern: ein Streifen fuellt das Fenster, die anderen treten zurueck.
        QTest::mouseClick(panel->stripForTest(ids[0])->expandButtonForTest(), Qt::LeftButton);
        QVERIFY(!panel->stripForTest(ids[1])->isHidden() == false);
        QVERIFY(!panel->stripForTest(ids[0])->isHidden());
        QTest::mouseClick(panel->stripForTest(ids[0])->expandButtonForTest(), Qt::LeftButton);
        QVERIFY(!panel->stripForTest(ids[1])->isHidden());
    }

    // 2026-11-28, Betreiber: "Kiwi Wasserfaelle geht nach einer Sekunde immer
    // automatisch zu". Gemerkte WF-Haken bauen beim Start keinen Streifen; der
    // Sekundentakt schloss das leere Fenster, so oft man es auch holte.
    void dasGeholteFensterBleibtOffen()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        AppletVisibilityController* vis = mw->appletVisibilityForTest();
        const QString win = QStringLiteral("KiwiWaterfalls");
        vis->ensureVisible(win);
        // Beim Betreiber ist es ein eigenes Fenster; offscreen steht das
        // Panel in der Spalte, die dafuer sichtbar sein muss.
        KiwiWaterfallPanel* shown = mw->kiwiWaterfallPanelForTest();
        for (QWidget* w = shown->parentWidget(); w; w = w->parentWidget()) {
            if (w->isHidden()) { w->show(); }
        }
        QTRY_VERIFY(shown->isVisible());
        QTest::qWait(1600);
        QVERIFY2(vis->isEffectivelyVisible(win), "das Fenster ging von selbst zu");

        // Mit dem letzten Streifen geht es weiterhin zu.
        const QString id = profile(mgr, "Kurz", 85);
        mgr->setWaterfallPreviewEnabled(id, true);
        QVERIFY(vis->isEffectivelyVisible(win));
        mgr->setWaterfallPreviewEnabled(id, false);
        QVERIFY(!vis->isEffectivelyVisible(win));
    }

    // Der Haken vom letzten Mal: verbunden oder nicht, wer das Fenster holt,
    // sieht den Kiwi.
    void gemerkterKiwiStehtImGeholtenFenster()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        AppletVisibilityController* vis = mw->appletVisibilityForTest();
        const QString win = QStringLiteral("KiwiWaterfalls");
        vis->setVisible(win, false);
        KiwiWaterfallPanel* panel = mw->kiwiWaterfallPanelForTest();

        QTcpServer stumm;
        QVERIFY(stumm.listen(QHostAddress::LocalHost, 0));
        const QString laeuft = profile(mgr, "Laeuft", stumm.serverPort());
        const QString aus = profile(mgr, "Aus", 86);
        {
            // Wie nach dem Start: Haken gesetzt, kein Streifen.
            const QSignalBlocker block(mgr);
            mgr->setWaterfallPreviewEnabled(laeuft, true);
            mgr->setWaterfallPreviewEnabled(aus, true);
        }
        mgr->disconnectProfile(aus);
        QTest::qWait(500);   // spaete Meldungen des Client-Fadens abwarten
        mgr->setStateForTest(aus, KiwiSdrClient::State::Disconnected);
        mgr->setStateForTest(laeuft, KiwiSdrClient::State::Connected);
        QCOMPARE(mgr->state(aus), KiwiSdrClient::State::Disconnected);
        QVERIFY(!panel->stripForTest(laeuft));
        QVERIFY(!vis->isEffectivelyVisible(win));   // nichts springt von selbst auf

        vis->ensureVisible(win);
        QTRY_VERIFY(panel->stripForTest(laeuft));
        QVERIFY(!panel->stripForTest(aus));         // Getrennte bleiben weg

        // "Fenster oeffnen" im KiwiSDR-Panel holt auch einen getrennten.
        emit mw->kiwiSdrAppletForTest()->openWindowRequested(aus);
        QTRY_VERIFY(panel->stripForTest(aus));
        QTest::qWait(1600);
        QVERIFY(vis->isEffectivelyVisible(win));
    }

    // 2026-11-28: ohne Funkgeraet stand ein neuer Kiwi auf 15000 kHz, waehrend
    // der Panadapter 14,225 MHz zeigte.
    void ohneFunkgeraetFolgtDerKiwiDerAnzeige()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        QVERIFY(mw->radioModelForTest()->slices().isEmpty());   // kein Geraet
        SpectrumWidget* pan = mw->activeSpectrumWidget();
        QVERIFY(pan);
        pan->setFrequencyRange(14.225e6, 40000.0);

        QTcpServer stumm;
        QVERIFY(stumm.listen(QHostAddress::LocalHost, 0));
        const QString id = profile(mgr, "Vorschau", stumm.serverPort());
        mgr->setWaterfallPreviewEnabled(id, true);
        QTRY_COMPARE_WITH_TIMEOUT(mgr->primedFrequencyMhzForTest(id), 14.225, 4000);
        pan->setFrequencyRange(7.100e6, 40000.0);
        QTRY_COMPARE_WITH_TIMEOUT(mgr->primedFrequencyMhzForTest(id), 7.1, 4000);
    }

    // "Radio stumm": nur mit einem hoerbaren Kiwi, nur Wiedergabe, geht von
    // selbst aus, wenn der letzte Kiwi-Ton wegfaellt.
    void radioStummNurMitKiwiTon()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        RadioModel* model = mw->radioModelForTest();
        AudioEngine* audio = model->audioEngine();
        clearProfiles(mgr);
        const int radioSlice = model->addSlice();
        SliceModel* radio = model->sliceById(radioSlice);
        QVERIFY(radio && !radio->muted());

        // Der Ton-Schalter im Panadapter-Kopf schaltet nur das Funkgeraet; ein
        // Kiwi laeuft allein weiter (Betreiber 2026-11-25).
        auto* panSound = mw->findChild<QAbstractButton*>(
            QStringLiteral("panHeaderSoundToggle"));
        QVERIFY(panSound && panSound->isChecked());
        panSound->click();
        QVERIFY(mw->radioMutedForTest());
        QVERIFY(audio->muteNonKiwi());
        panSound->click();
        QVERIFY(!audio->muteNonKiwi());

        // Radio stumm geht auch ohne Kiwi-Ton (Betreiber 2026-11-25).
        mw->setRadioMutedForTest(true);
        QVERIFY(mw->radioMutedForTest());
        QVERIFY(audio->muteNonKiwi());
        mw->setRadioMutedForTest(false);
        QVERIFY(!audio->muteNonKiwi());

        const QString id = profile(mgr, "Linz", 120);
        mgr->setWaterfallPreviewEnabled(id, true);
        mw->setKiwiSoundForTest(id, true);
        QVERIFY(audio->kiwiAudioSliceCount() >= 1);
        mw->setRadioMutedForTest(true);
        QVERIFY(mw->radioMutedForTest());
        QVERIFY(audio->muteNonKiwi());
        QVERIFY(!radio->muted());                 // die Scheibe selbst bleibt unberuehrt

        // Kiwi-Ton weg: "Radio stumm" bleibt, wie der Betreiber es eingestellt hat.
        mw->setKiwiSoundForTest(id, false);
        QVERIFY(mw->radioMutedForTest());
    }

    // Aus dem Verzeichnis gewaehlter Kiwi: ist "Mein Signal"-Kiwi UND steht im
    // Fenster KIWI WASSERFAELLE.
    void gewaehlterKiwiErscheintImWasserfallFenster()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        mw->adoptPickedKiwiForTest(QStringLiteral("127.0.0.1:130"), QStringLiteral("Gewaehlt"));
        const QString id = mgr->profileIdForEndpoint(QStringLiteral("127.0.0.1:130"));
        QVERIFY(!id.isEmpty());
        QVERIFY(mw->kiwiOwnSignalOnForTest());
        QVERIFY2(mw->kiwiWaterfallPanelForTest()->stripForTest(id) != nullptr,
                 "der gewaehlte Kiwi muss im Wasserfall-Fenster stehen");
        QVERIFY(mgr->waterfallPreviewEnabled(id));
        // Zweimal gewaehlt: weiter genau ein Profil.
        mw->adoptPickedKiwiForTest(QStringLiteral("127.0.0.1:130"), QStringLiteral("Nochmal"));
        QCOMPARE(mgr->profiles().size(), 1);
    }

    void zeilenKommenAnUndDasFensterOeffnetSich()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        const QString a = profile(mgr, "Linz", 30);
        QVERIFY(mw->addKiwiListenerForTest(a, 60.0, true, 48.3, 14.3));

        // Der Knopf im Applet.
        auto* btn = mw->kiwiSdrAppletForTest()->findChild<QPushButton*>(
            QStringLiteral("kiwiHearingButton"));
        QVERIFY(btn);
        QVERIFY(mw->kiwiHearingDialogForTest() == nullptr);
        btn->click();
        KiwiHearingDialog* dlg = mw->kiwiHearingDialogForTest();
        QVERIFY(dlg);
        QVERIFY(dlg->isVisible());
        QCOMPARE(dlg->tileCount(), 1);
        QVERIFY(dlg->tileVerdictText(a).contains(QStringLiteral("waiting")));

        // Zeilen aus dem Manager laufen in die Kachel.
        QVector<float> bins(1024, -110.0f);
        for (int i = 0; i < 3; ++i) {
            emit mgr->waterfallRowReady(a, QStringLiteral("kiwi-group"), bins,
                                        14.185, 14.225, quint32(i));
        }
        QCOMPARE(dlg->miniRowsForTest(a), 3);
        // Fremde Kennung: nichts.
        emit mgr->waterfallRowReady(QStringLiteral("fremd"), QString(), bins,
                                    14.185, 14.225, 0);
        QCOMPARE(dlg->miniRowsForTest(a), 3);

        // Vor dem Senden ist nichts zu messen; nach einer Sendung OHNE Bild
        // im Sendefenster heisst es "unbekannt", nicht "hoert nichts".
        QCOMPARE(mw->kiwiGroupForTest().result(a).verdict,
                 KiwiHearMeMeter::Verdict::Waiting);
        mw->kiwiGroupTxEdgeForTest(true);
        mw->kiwiGroupTxEdgeForTest(false);
        QCOMPARE(mw->kiwiGroupForTest().result(a).verdict,
                 KiwiHearMeMeter::Verdict::Unknown);

        // Schliessen-Knopf entfernt den Mithoerer.
        dlg->removeButtonForTest(a)->click();
        QCOMPARE(mw->kiwiGroupForTest().size(), 0);
        QVERIFY(!mgr->isProfileListener(a));
    }

    // Betreiber 2026-11-25: der Kiwi `93` trennt nach 10 s (Programm-Limit).
    // Das soll an seiner Zeile stehen, auch nach dem naechsten Versuch.
    void rauswurfStehtAnDerZeile()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        profile(mgr, "Voll", 77);
        const QString id = mgr->profiles().constFirst().id;
        const auto warnings = [&]() {
            return mw->kiwiSdrAppletForTest()
                ->findChildren<QLabel*>(QStringLiteral("kiwiRowWarning")).size();
        };
        emit mgr->profileStateChanged(id, KiwiSdrClient::State::Connecting, QString());
        // Belegt schon beim Verbinden ist kein Rauswurf.
        emit mgr->profileStateChanged(id, KiwiSdrClient::State::Busy, QStringLiteral("x"));
        QTest::qWait(500);
        QCOMPARE(warnings(), 0);
        emit mgr->profileStateChanged(id, KiwiSdrClient::State::Connected, QString());
        emit mgr->profileStateChanged(id, KiwiSdrClient::State::Busy, QStringLiteral("x"));
        QTRY_VERIFY_WITH_TIMEOUT(warnings() >= 1, 3000);
    }

    void tonAnVerbindetEinenGetrenntenKiwiNeu()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        mw->radioModelForTest()->addSlice();
        clearProfiles(mgr);
        const QString id = profile(mgr, "Neu", 91);
        mw->setKiwiSoundForTest(id, true);
        mgr->disconnectProfile(id);                 // wie nach einem Rauswurf
        QTRY_COMPARE_WITH_TIMEOUT(mgr->state(id),
                                  KiwiSdrClient::State::Disconnected, 3000);
        QSignalSpy spy(mgr, &KiwiSdrManager::profileStateChanged);
        mw->setKiwiSoundForTest(id, true);
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);   // neuer Versuch
    }

    // Betreiber 2026-11-25: "deutlich anzeigen" -- ein getrennter Kiwi zeigt
    // an beiden Anzeigen gross den Grund und einen Knopf.
    void getrennterKiwiStehtGrossDaMitKnopf()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        mw->adoptPickedKiwiForTest(QStringLiteral("127.0.0.1:131"), QStringLiteral("Weg"));
        const QString id = mgr->profileIdForEndpoint(QStringLiteral("127.0.0.1:131"));
        auto* strip = mw->kiwiWaterfallPanelForTest()->stripForTest(id);
        QVERIFY(strip);
        KiwiLostOverlay* inWindow = strip->lostOverlayForTest();
        KiwiLostOverlay* below = mw->kiwiOwnStripForTest()->lostOverlayForTest();
        // Der Zustand kommt vom Manager: getrennt -> beide Anzeigen melden es.
        mgr->disconnectProfile(id);
        QTRY_VERIFY_WITH_TIMEOUT(inWindow->isLost() && below->isLost(), 3000);
        QVERIFY(!inWindow->title().isEmpty());
        QCOMPARE(below->title(), inWindow->title());
        QVERIFY(inWindow->chooseButtonForTest());
        QVERIFY(below->chooseButtonForTest());

        // Neu verbinden -> die Anzeige geht weg.
        mgr->connectProfile(id);
        QTRY_COMPARE_WITH_TIMEOUT(mgr->state(id),
                                  KiwiSdrClient::State::Connecting, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!inWindow->isLost() && !below->isLost(), 3000);

        // Fuers Auge (nur mit LONGPATH_TEST_BILDER).
        const QString dir = qEnvironmentVariable("LONGPATH_TEST_BILDER");
        if (!dir.isEmpty()) {
            mgr->disconnectProfile(id);
            QTRY_VERIFY_WITH_TIMEOUT(inWindow->isLost(), 3000);
            QVector<float> bins(1024, -105.0f);
            for (int k = 300; k < 330; ++k) { bins[k] = -60.0f; }
            for (int r = 0; r < 60; ++r) {
                strip->setRowIntervalMs(0);
                strip->pushRow(bins, 7.080e6, 7.120e6);
                mw->kiwiOwnStripForTest()->setRowIntervalMs(0);
                mw->kiwiOwnStripForTest()->pushRow(bins, 7.080e6, 7.120e6);
            }
            strip->resize(560, 220);
            QTest::qWait(100);
            strip->grab().save(dir + QStringLiteral("/kiwi-getrennt-fenster.png"));
            auto* own = mw->kiwiOwnStripForTest();
            own->setView(7.1e6, 40000.0);
            own->resize(900, 176);
            QTest::qWait(100);
            own->grab().save(dir + QStringLiteral("/kiwi-getrennt-unten.png"));
            auto* applet = mw->kiwiSdrAppletForTest();
            applet->resize(360, 520);
            QTest::qWait(100);
            applet->grab().save(dir + QStringLiteral("/kiwi-liste.png"));
        }
    }

    void ersetzterKiwiWirdWeggeraeumt()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        mw->radioModelForTest()->addSlice();
        clearProfiles(mgr);
        const QString id = profile(mgr, "Alt", 132);
        const int slicesBefore = mw->radioModelForTest()->slices().size();
        mgr->setWaterfallPreviewEnabled(id, true);
        mw->setKiwiSoundForTest(id, true);
        QVERIFY(mgr->assignedSliceForProfile(id) >= 0);
        mw->retireKiwiForTest(id);
        QCOMPARE(mgr->assignedSliceForProfile(id), -1);
        QVERIFY(!mgr->waterfallPreviewEnabled(id));
        QCOMPARE(mw->radioModelForTest()->slices().size(), slicesBefore);
        QVERIFY(mw->kiwiWaterfallPanelForTest()->stripForTest(id) == nullptr);
        QVERIFY(mgr->hasProfile(id));   // der Eintrag in der Liste bleibt
    }

    // Betreiber 2026-11-25: "durch Plus und Minus aendert sich aber nichts" --
    // am Kiwi von "Mein Signal" galt weiter die Achse des Panadapters.
    void plusMinusAendertDenStreifenUnten()
    {
        auto* mw = new MainWindow();
        mw->resize(1400, 900);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        const int sid = mw->radioModelForTest()->addSlice();
        clearProfiles(mgr);
        mw->radioModelForTest()->sliceById(sid)->setFrequency(7.1e6);
        mw->adoptPickedKiwiForTest(QStringLiteral("127.0.0.1:135"), QStringLiteral("Zoom"));
        const QString id = mgr->profileIdForEndpoint(QStringLiteral("127.0.0.1:135"));
        QVERIFY(mw->kiwiOwnSignalOnForTest());
        KiwiOwnSignalStrip* strip = mw->kiwiOwnStripForTest();
        const double wide = strip->viewWidthHzForTest();
        QVERIFY(wide > 0.0);

        mw->zoomKiwiStripForTest(id, +1);
        QCOMPARE(strip->viewWidthHzForTest(), wide / 2.0);
        // Die Sendefrequenz steht in der Mitte (die Marke bleibt sichtbar).
        QVERIFY(strip->markX() >= 0);
        // Ein spaeterer Abgleich (Frequenzwechsel) setzt den Zoom nicht zurueck.
        mw->radioModelForTest()->sliceById(sid)->setFrequency(7.15e6);
        QTRY_COMPARE_WITH_TIMEOUT(strip->viewCenterHzForTest(), 7.15e6, 3000);
        QCOMPARE(strip->viewWidthHzForTest(), wide / 2.0);

        mw->zoomKiwiStripForTest(id, -1);
        QCOMPARE(strip->viewWidthHzForTest(), wide);
        QCOMPARE(mw->kiwiViewSpanForTest(id), 0.0);   // wieder der Panadapter
    }

    void lautstaerkeJeKiwiBleibtGemerkt()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });
        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        const QString a = profile(mgr, "Laut", 133);
        const QString b = profile(mgr, "Normal", 134);
        auto* applet = mw->kiwiSdrAppletForTest();
        const auto sliderFor = [&](const QString& name) -> QSlider* {
            for (QSlider* s : applet->findChildren<QSlider*>(QStringLiteral("kiwiRowVolume"))) {
                if (s->accessibleName().endsWith(name)) { return s; }
            }
            return nullptr;
        };
        QTRY_VERIFY_WITH_TIMEOUT(sliderFor(QStringLiteral("Laut")), 3000);
        QSlider* s = sliderFor(QStringLiteral("Laut"));
        // Waehrend des Ziehens ueberlebt der Regler einen Neubau der Liste.
        s->setSliderDown(true);
        QPointer<QSlider> alive(s);
        s->setValue(12);
        emit mgr->profileStateChanged(a, KiwiSdrClient::State::Connecting, QString());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(alive);
        QCOMPARE(alive->value(), 12);
        alive->setSliderDown(false);
        QCOMPARE(mw->kiwiVolumeDbForTest(a), 12);
        QCOMPARE(mw->kiwiVolumeDbForTest(b), 0);
        // Nach dem Neubau steht der Wert wieder am Regler.
        QTRY_VERIFY_WITH_TIMEOUT(sliderFor(QStringLiteral("Laut"))
                                 && sliderFor(QStringLiteral("Laut"))->value() == 12, 3000);
    }
};

QTEST_MAIN(TstKiwiGroupWindow)
#include "tst_kiwi_group_window.moc"
