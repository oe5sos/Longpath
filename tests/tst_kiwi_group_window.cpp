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
#include <QtTest>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
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
#include "gui/MainWindow.h"
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
        QTest::qWait(100);
        QCOMPARE(list->count(), 12);
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

        // Alle Ton: nur verbundene (hier "Verbindet"), hoechstens fuenf, je eigene Scheibe.
        const int slicesBefore = model->slices().size();
        // Wie viele laufen gerade? (Gegenstellen auf 127.0.0.1 antworten mit
        // "abgelehnt", der Zustand ist vom Takt abhaengig -- der Test prueft
        // deshalb die Regel, nicht eine feste Zahl.)
        int laufend = 0;
        for (const QString& id : ids) {
            const KiwiSdrClient::State st = mgr->state(id);
            if (st != KiwiSdrClient::State::Disconnected && st != KiwiSdrClient::State::Error) {
                ++laufend;
            }
        }
        QTest::mouseClick(panel->allSoundOnForTest(), Qt::LeftButton);
        int hoerbar = 0;
        for (const QString& id : ids) {
            if (mgr->assignedSliceForProfile(id) >= 0) { ++hoerbar; }
        }
        QVERIFY2(hoerbar == qMin(laufend, 5), qPrintable(QString::number(hoerbar)));
        QCOMPARE(model->slices().size(), slicesBefore + hoerbar);
        QVERIFY(model->sliceById(radioSlice) != nullptr);

        QTest::mouseClick(panel->allSoundOffForTest(), Qt::LeftButton);
        for (const QString& id : ids) { QCOMPARE(mgr->assignedSliceForProfile(id), -1); }
        QCOMPARE(model->slices().size(), slicesBefore);
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

        // Ohne Kiwi-Ton: abgelehnt.
        mw->setRadioMutedForTest(true);
        QVERIFY(!mw->radioMutedForTest());
        QVERIFY(!audio->muteNonKiwi());

        const QString id = profile(mgr, "Linz", 120);
        mgr->setWaterfallPreviewEnabled(id, true);
        mw->setKiwiSoundForTest(id, true);
        QVERIFY(audio->kiwiAudioSliceCount() >= 1);
        mw->setRadioMutedForTest(true);
        QVERIFY(mw->radioMutedForTest());
        QVERIFY(audio->muteNonKiwi());
        QVERIFY(!radio->muted());                 // die Scheibe selbst bleibt unberuehrt

        // Letzter Kiwi-Ton weg -> Radio wieder an.
        mw->setKiwiSoundForTest(id, false);
        QVERIFY(!mw->radioMutedForTest());
        QVERIFY(!audio->muteNonKiwi());
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
};

QTEST_MAIN(TstKiwiGroupWindow)
#include "tst_kiwi_group_window.moc"
