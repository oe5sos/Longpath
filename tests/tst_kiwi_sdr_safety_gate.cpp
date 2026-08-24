// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sicherheitsschranke fuer KiwiSDR, uebertragen aus der SunSDR-
// Durchsicht (2026-08-24, siehe docs/architecture/2026-08-24-sunsdr-
// tci-client-design.md). Ein KiwiSDR ist wie SunSDR ein fremdes,
// netzwerkbasiertes Empfangsgeraet -- eine Scheibe mit echter
// DDC-Bindung (streamIndex() >= 0, ein ECHTES, moeglicherweise
// sendefaehiges Funkgeraet) darf niemals von ihm gefuettert oder
// beeinflusst werden.
//
// Eine unabhaengige Durchsicht (general-purpose Agent, jede Kernaussage
// gegen den Quellcode selbst nachgeprueft) fand vier Luecken -- genau
// dieselbe Fehlerklasse wie bei SunSDR, denn addKiwiSdrReceiver() war
// buchstaeblich die Vorlage, von der SunSDR's Scheiben-Rueckfall
// abgeschrieben wurde (MainWindow_SunSdr.cpp:20-28), nur wurde der
// spaetere SunSDR-Fix nie zurueckuebertragen:
//
//   1. Der Scheiben-Rueckfall nahm die aktive Scheibe blind, auch mit
//      echter DDC-Bindung.
//   2. Kein zentraler "sicher zu fuettern"-Wachposten -- Ton- und
//      Panadapter-Pfad hatten je eigene, unvollstaendige Pruefungen
//      (der Panadapter-Pfad gar keine).
//   3. Keine Freigabe beim Loeschen der zugeordneten Scheibe --
//      RadioModel::addSlice()'s Wiederverwendung der niedrigsten
//      freien Kennung haette eine voellig unbeteiligte neue Scheibe
//      die Zuordnung erben lassen koennen.
//   4. Keine Freigabe, wenn ein echtes Funkgeraet die zugeordnete
//      Scheibe SPAETER uebernimmt (RadioModel::bindUnboundSlices()).
//
// Anders als SunSDR (ein Geraet, eine feste Zielscheibe) kann ein
// KiwiSDR mehrere Scheiben gleichzeitig speisen -- die Pruefungen hier
// gehen darum ueber assignedSliceForProfile()/assignedProfileForSlice(),
// nicht ueber eine einzelne gespeicherte Kennung.

#include <QtTest>

#include "core/AudioEngine.h"
#include "core/KiwiSdrManager.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

// Siehe tst_kiwi_tx_mute.cpp: der KiwiSdrManager legt Profile in den
// Einstellungen ab und liest sie beim naechsten Start wieder ein --
// ohne dieses Aufraeumen findet eine spaetere Pruefung ein Profil
// einer frueheren wieder.
void clearProfiles(KiwiSdrManager* mgr)
{
    if (!mgr) { return; }
    const QVector<KiwiSdrAntennaProfile> existing = mgr->profiles();
    for (const KiwiSdrAntennaProfile& p : existing) {
        mgr->removeProfile(p.id);
    }
}

} // namespace

class TstKiwiSdrSafetyGate : public QObject
{
    Q_OBJECT

private slots:
    void hinzufuegenUeberspringtEineEchteScheibeUndLegtEineNeueAn()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        QVERIFY(mgr);
        clearProfiles(mgr);

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        // Einzige vorhandene Scheibe: wird automatisch aktiv (addSlice(),
        // erste Scheibe ueberhaupt) UND bekommt eine nachgestellte echte
        // DDC-Bindung.
        const int realId = model->addSlice();
        SliceModel* realSlice = model->sliceById(realId);
        QVERIFY(realSlice);
        realSlice->setStreamIndex(0);
        const double realFreqBefore = realSlice->frequency();

        mw->addKiwiSdrReceiverForTest(QStringLiteral("Gmunden"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QVector<KiwiSdrAntennaProfile> profiles = mgr->profiles();
        QVERIFY(!profiles.isEmpty());
        const int kiwiId = mgr->assignedSliceForProfile(profiles.first().id);

        QVERIFY2(kiwiId != realId,
                 "KiwiSDR hat die echte, gebundene Scheibe als Ziel genommen");
        QVERIFY2(kiwiId >= 0, "KiwiSDR hat gar keine Zielscheibe bekommen");
        SliceModel* kiwiSlice = model->sliceById(kiwiId);
        QVERIFY(kiwiSlice);
        QVERIFY2(kiwiSlice->streamIndex() < 0,
                 "die neu angelegte KiwiSDR-Scheibe hat selbst eine "
                 "DDC-Bindung -- Testannahme verletzt");

        // Die echte Scheibe ist unberuehrt -- keine Frequenzaenderung,
        // keine KiwiSDR-Zuordnung.
        QCOMPARE(realSlice->frequency(), realFreqBefore);
        QVERIFY2(mgr->assignedProfileForSlice(realId).isEmpty(),
                 "der echten Scheibe wurde trotzdem ein KiwiSDR-Profil "
                 "zugeordnet");

        mw->close();
    }

    void zugeordneteScheibeWirdBeiEchterDdcBindungSpaeterFreigegeben()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        QVERIFY(mgr);
        clearProfiles(mgr);

        mw->addKiwiSdrReceiverForTest(QStringLiteral("Gmunden"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QVector<KiwiSdrAntennaProfile> profiles = mgr->profiles();
        QVERIFY(!profiles.isEmpty());
        const QString profileId = profiles.first().id;
        const int sliceId = mgr->assignedSliceForProfile(profileId);
        QVERIFY2(sliceId >= 0, "nicht zugeordnet -- der Rest prueft nichts");

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        audio->setKiwiSdrAudioSourceEnabled(sliceId, true);
        QVERIFY(audio->kiwiSdrAudioEnabled(sliceId));

        // Ein echtes Funkgeraet uebernimmt die Scheibe -- ohne echte
        // Hardware ist setStreamIndex() der einzige Weg, genau den
        // Zustandswechsel herzustellen, den bindUnboundSlices() ausloest.
        slice->setStreamIndex(0);

        QVERIFY2(mgr->assignedProfileForSlice(sliceId).isEmpty(),
                 "die Zuordnung blieb nach der Uebernahme stehen");
        QVERIFY2(!audio->kiwiSdrAudioEnabled(sliceId),
                 "KiwiSDR-Ton lief nach der Uebernahme weiter");

        mw->close();
    }

    void scheibeLoeschenGibtDieZuordnungFreiUndVerhindertKennungsWiederverwendung()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        QVERIFY(mgr);
        clearProfiles(mgr);

        mw->addKiwiSdrReceiverForTest(QStringLiteral("Gmunden"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QVector<KiwiSdrAntennaProfile> profiles = mgr->profiles();
        QVERIFY(!profiles.isEmpty());
        const QString profileId = profiles.first().id;
        const int kiwiId = mgr->assignedSliceForProfile(profileId);
        QVERIFY2(kiwiId >= 0, "nicht zugeordnet -- der Rest prueft nichts");

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        audio->setKiwiSdrAudioSourceEnabled(kiwiId, true);
        QVERIFY(audio->kiwiSdrAudioEnabled(kiwiId));

        // removeSlice() weigert sich, die letzte Scheibe zu loeschen --
        // eine zweite anlegen, damit die KiwiSDR-Scheibe wirklich weg kann.
        const int otherId = model->addSlice();
        QVERIFY(model->sliceById(otherId));

        model->removeSlice(kiwiId);
        QVERIFY2(!model->sliceById(kiwiId),
                 "Testannahme verletzt: Scheibe wurde nicht geloescht");
        QVERIFY2(mgr->assignedProfileForSlice(kiwiId).isEmpty(),
                 "die Zuordnung blieb nach dem Loeschen stehen");
        QVERIFY2(!audio->kiwiSdrAudioEnabled(kiwiId),
                 "KiwiSDR-Ton lief nach dem Loeschen weiter");

        // Eine neue Scheibe kann jetzt dieselbe Kennung wiederbekommen
        // (niedrigste freie zuerst) -- gegenpruefen, dass das tatsaechlich
        // passiert, sonst prueft der Rest hier den falschen Fall.
        const int reusedId = model->addSlice();
        SliceModel* reusedSlice = model->sliceById(reusedId);
        QVERIFY(reusedSlice);
        if (reusedId != kiwiId) {
            QSKIP("Kennung wurde nicht wiederverwendet -- Testvoraussetzung "
                  "diesmal nicht gegeben, nichts zu pruefen");
        }

        // Die wiederverwendete Scheibe darf KEIN KiwiSDR-Profil erben.
        QVERIFY2(mgr->assignedProfileForSlice(reusedId).isEmpty(),
                 "die wiederverwendete Scheibe hat eine KiwiSDR-Zuordnung "
                 "geerbt, obwohl sie nie damit verbunden wurde");
        QVERIFY2(!audio->kiwiSdrAudioEnabled(reusedId),
                 "die wiederverwendete Scheibe hat KiwiSDR-Ton geerbt");

        mw->close();
    }
};

QTEST_MAIN(TstKiwiSdrSafetyGate)
#include "tst_kiwi_sdr_safety_gate.moc"
