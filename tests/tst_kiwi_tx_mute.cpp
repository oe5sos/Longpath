// SPDX-License-Identifier: GPL-3.0-or-later
//
// Waehrend wir senden, ist der Ton des KiwiSDR aus.
//
// KiwiSDR Stufe 7a, 2026-08-23. Der Grund ist nicht Rueckkopplung —
// der Kiwi steht irgendwo im Netz —, sondern dass ein Empfaenger in
// Reichweite die eigene Aussendung hoert und sie einem mit ein bis
// zwei Sekunden Verzug ins Ohr legt.
//
// Gemessen wird am AudioEngine, also an der Stelle, an der der Ton
// tatsaechlich zugelassen oder verworfen wird — nicht an einer
// Merkervariablen im MainWindow.

#include <QtTest>

#include "core/AudioEngine.h"
#include "gui/MainWindow.h"
#include "core/KiwiSdrManager.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;


namespace {

// ── Hermetisch machen ────────────────────────────────────────────────
//
// Der KiwiSdrManager legt seine Profile in den Einstellungen ab und
// liest sie beim Start wieder ein. Ohne dieses Aufraeumen findet die
// zweite Pruefung das Profil der ersten wieder — OHNE Scheibe, denn
// die Zuordnung wird nicht mitgespeichert — und profiles().first()
// liefert dann das falsche.
//
// Genau daran ist der erste Lauf gescheitert, und zwar mit einer
// Meldung, die in die Irre fuehrte ("Beim Abstimmen bleibt der
// KiwiSDR hoerbar"): es lag nicht an TUNE, sondern daran, dass die
// Pruefung auf ein Profil sah, das nie zugeordnet worden war.
void clearProfiles(KiwiSdrManager* mgr)
{
    if (!mgr) { return; }
    const QVector<KiwiSdrAntennaProfile> existing = mgr->profiles();
    for (const KiwiSdrAntennaProfile& p : existing) {
        mgr->removeProfile(p.id);
    }
}

} // namespace

class TstKiwiTxMute : public QObject
{
    Q_OBJECT

private slots:
    void beimSendenIstDerKiwiStumm()
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
        const int sliceId = mgr->assignedSliceForProfile(profiles.first().id);
        QVERIFY2(sliceId >= 0,
                 "Der Empfaenger wurde keiner Scheibe zugeordnet — dann "
                 "prueft der Rest dieser Pruefung nichts");

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);

        // Ausgangslage: der Ton ist zugelassen.
        audio->setKiwiSdrAudioSourceEnabled(sliceId, true);
        QVERIFY(audio->kiwiSdrAudioEnabled(sliceId));

        model->transmitModel().setMox(true);
        qInfo() << "MOX an, Kiwi-Ton zugelassen:"
                << audio->kiwiSdrAudioEnabled(sliceId);
        QVERIFY2(!audio->kiwiSdrAudioEnabled(sliceId),
                 "Der KiwiSDR bleibt beim Senden hoerbar");

        model->transmitModel().setMox(false);
        // Ohne resumeAudioAfterTxDelay (Vorgabe) kommt der Ton sofort
        // zurueck.
        qInfo() << "MOX aus, Kiwi-Ton zugelassen:"
                << audio->kiwiSdrAudioEnabled(sliceId);
        QVERIFY2(audio->kiwiSdrAudioEnabled(sliceId),
                 "Nach dem Senden bleibt der KiwiSDR stumm");

        mw->close();
    }

    void tuneZaehltAuch()
    {
        // Beim Abstimmen wird gesendet — ein Dauertraeger sogar. Wer
        // das vergisst, hoert sich beim Tunen selbst.
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        clearProfiles(mgr);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Gmunden"),
                                      QStringLiteral("kiwi.example.at:8073"));
        QVERIFY(!mgr->profiles().isEmpty());
        const int sliceId =
            mgr->assignedSliceForProfile(mgr->profiles().first().id);
        QVERIFY2(sliceId >= 0, "Nicht zugeordnet — der Rest prueft nichts");
        RadioModel* model = mw->radioModelForTest();
        AudioEngine* audio = model->audioEngine();
        audio->setKiwiSdrAudioSourceEnabled(sliceId, true);

        model->transmitModel().setTune(true);
        qInfo() << "TUNE an, Kiwi-Ton zugelassen:"
                << audio->kiwiSdrAudioEnabled(sliceId);
        QVERIFY2(!audio->kiwiSdrAudioEnabled(sliceId),
                 "Beim Abstimmen bleibt der KiwiSDR hoerbar");

        mw->close();
    }
};

QTEST_MAIN(TstKiwiTxMute)
#include "tst_kiwi_tx_mute.moc"
