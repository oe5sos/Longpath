// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_qso_mitschnitt_hoert_mich.cpp  (Longpath)
// =================================================================
//
// Die eigene Spur im QSO-Mitschnitt war STILL.
//
// Der Betreiber am 2026-08-19: „wichtig ist, dass man sich selbst aber
// auch die andere station hört!" Daraufhin entstand der
// QsoRecorder -- eine Stereodatei, links die Gegenstation, rechts die
// eigene Stimme, der Empfang als Uhr. Alles gebaut, alles von
// Prueflaeufen gedeckt.
//
// Nur: die rechte Spur bekam nie etwas. Der Abgriff im Sendefaden
// steht mit Vorgabe AUS (`TxWorkerThread::m_voiceTapUsers`, vormals
// ein bool), und eingeschaltet haben ihn allein das
// Sprachpruef-Fenster und das DVK-Applet. Waehrend eines
// QSO-Mitschnitts feuerte das Signal also gar nicht.
//
// Der vorhandene Prueflauf (`tst_qso_recorder_controller`) schrieb
// DIREKT in die Ringe. Er belegte den Mitschneider und nie die
// Verdrahtung -- ein Waechter, wo ein Faenger gebraucht wird.
//
// Dazu, aus demselben Wunsch: die eigene Spur hing an
// `preStripAudioReady`, dem rohen Mikrofon vor Entzerrer, Leveler,
// CFC und ALC. Man hoerte sich also NICHT so, wie die Gegenstation
// einen gehoert hat.
//
// Modification history (Longpath):
//   2026-10-22 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QTemporaryDir>

#include "core/TxWorkerThread.h"
#include "core/ZeitUtc.h"
#include "core/audio/QsoRecorder.h"
#include "core/audio/QsoRecorderController.h"

using namespace Longpath;

class TstQsoMitschnittHoertMich : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString pfad(const QString& n) const
    {
        return m_dir.path() + QLatin1Char('/') + n;
    }

    static QsoRecordingInfo einQso()
    {
        QsoRecordingInfo i;
        i.utcStart  = Longpath::Zeit::utc(QDate(2026, 10, 22), QTime(4, 0));
        i.frequency = QStringLiteral("14.205.000");
        i.mode      = QStringLiteral("USB");
        i.band      = QStringLiteral("20m");
        return i;
    }

private slots:

    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void derMitschnittOeffnetDasTorFuerDieEigeneStimme()
    {
        TxWorkerThread worker;
        QsoRecorderController c;
        c.setSampleRate(1000);
        c.attach(nullptr, &worker);

        QVERIFY2(!worker.voiceTapEnabled(),
                 "vor dem Mitschnitt soll der Abgriff ruhen");

        QVERIFY(c.start(pfad(QStringLiteral("tor.wav")), einQso()));
        QVERIFY2(worker.voiceTapEnabled(),
                 "der Abgriff laeuft nicht — die eigene Spur bleibt still");

        c.stop();
        QVERIFY2(!worker.voiceTapEnabled(),
                 "nach dem Mitschnitt soll er wieder ruhen");
    }

    void einFremdesFensterSchaltetIhnNichtAb()
    {
        // Wer waehrend eines laufenden Mitschnitts das
        // Sprachpruef-Fenster oeffnet und wieder zumacht, darf dabei
        // nicht die eigene Stimme aus der Aufnahme verlieren. Mit einem
        // einzelnen bool passierte genau das.
        TxWorkerThread worker;
        QsoRecorderController c;
        c.setSampleRate(1000);
        c.attach(nullptr, &worker);
        QVERIFY(c.start(pfad(QStringLiteral("fremd.wav")), einQso()));

        worker.setVoiceTapUser(TxWorkerThread::VoiceTapUser::VoiceCheck, true);
        worker.setVoiceTapUser(TxWorkerThread::VoiceTapUser::VoiceCheck, false);
        QVERIFY2(worker.voiceTapEnabled(),
                 "ein fremdes Fenster hat den Abgriff mitgenommen");

        // Und zweimal abschalten darf nicht unterlaufen -- das
        // Sprachpruef-Fenster tut das wirklich (Abbau UND stop()).
        worker.setVoiceTapUser(TxWorkerThread::VoiceTapUser::VoiceCheck, false);
        QVERIFY(worker.voiceTapEnabled());

        c.stop();
        QVERIFY(!worker.voiceTapEnabled());
    }

    void meineSpurTraegtDenAufbereitetenTon()
    {
        // Die Verdrahtung, nicht der Mitschneider: ein Block auf
        // postStripAudioReady muss in der Sprechspur landen, einer auf
        // preStripAudioReady NICHT. Sonst hoert man sich roh.
        TxWorkerThread worker;
        QsoRecorderController c;
        c.setSampleRate(1000);
        c.attach(nullptr, &worker);
        QVERIFY(c.start(pfad(QStringLiteral("post.wav")), einQso()));

        const std::vector<float> block(200, 0.5f);

        QVERIFY(QMetaObject::invokeMethod(
            &worker, "preStripAudioReady", Qt::DirectConnection,
            Q_ARG(const float*, block.data()), Q_ARG(int, 200)));
        QCOMPARE(c.txRing().available(), 0);

        QVERIFY(QMetaObject::invokeMethod(
            &worker, "postStripAudioReady", Qt::DirectConnection,
            Q_ARG(const float*, block.data()), Q_ARG(int, 200)));
        QCOMPARE(c.txRing().available(), 200);

        c.stop();
    }
};

QTEST_MAIN(TstQsoMitschnittHoertMich)
#include "tst_qso_mitschnitt_hoert_mich.moc"
