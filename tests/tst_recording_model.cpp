// =================================================================
// tests/tst_recording_model.cpp  (Longpath)
// =================================================================
//
// Die Bindeschicht fuer Phase 3M. Was hier festgenagelt wird, ist
// nicht „nimmt sie auf" -- das koennen die sechs Prueflaeufe der
// Klassen selbst, und ohne Verbindung gibt es keine Abtastrate, also
// auch keine Aufnahme. Festgenagelt wird, dass die Bindeschicht
// VERWEIGERT, wo sie verweigern muss, und dass sie dabei sagt, WAS
// fehlt -- weil genau das der Unterschied zwischen einer Flaeche ist,
// die man bedienen kann, und einer, die nur „geht nicht" zeigt.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include <QSignalSpy>
#include <QtTest>

#include "models/RadioModel.h"
#include "models/RecordingModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstRecordingModel : public QObject
{
    Q_OBJECT

private slots:
    void ohneRadioIstSieNichtBereit()
    {
        RecordingModel rec;
        QVERIFY(!rec.ready());

        RadioModel radio;
        QSignalSpy bereit(&rec, &RecordingModel::readyChanged);
        rec.attach(&radio);

        QVERIFY(rec.ready());
        QCOMPARE(bereit.count(), 1);
        QCOMPARE(bereit.takeFirst().at(0).toBool(), true);

        // Zweimal dasselbe Radio ist kein Zustandswechsel.
        rec.attach(&radio);
        QCOMPARE(bereit.count(), 0);

        rec.attach(nullptr);
        QVERIFY(!rec.ready());
        QCOMPARE(bereit.count(), 1);
    }

    void ohneRadioStartetNichtsUndSagtWarum()
    {
        RecordingModel rec;
        QVERIFY(!rec.startAudio());
        QVERIFY(!rec.audioRecording());
        QVERIFY2(rec.lastError().contains(QStringLiteral("radio")),
                 qPrintable(rec.lastError()));

        QVERIFY(!rec.startIq());
        QVERIFY(!rec.iqRecording());
    }

    void ohneScheibeStartetNichts()
    {
        RadioModel radio;
        RecordingModel rec;
        rec.attach(&radio);

        // Ein frisches RadioModel ohne Scheibe: die Bindeschicht darf
        // nicht in den Recorder laufen, sondern muss es benennen.
        if (radio.activeSlice() == nullptr) {
            QVERIFY(!rec.startAudio());
            QVERIFY2(rec.lastError().contains(QStringLiteral("receiver")),
                     qPrintable(rec.lastError()));
        }
    }

    void ohneAbtastrateStartetNichts()
    {
        RadioModel radio;
        if (radio.addSlice() < 0) { QSKIP("keine Scheibe anzulegen"); }
        radio.setActiveSlice(0);
        if (!radio.activeSlice()) { QSKIP("keine aktive Scheibe"); }

        RecordingModel rec;
        rec.attach(&radio);

        // Nicht verbunden -> connectionSampleRateHz() == 0. Eine
        // Aufnahme mit Rate 0 ergibt eine Datei, die niemand abspielen
        // kann; sie muss hier scheitern, nicht im Schreiber.
        QCOMPARE(radio.connectionSampleRateHz(), 0);
        QVERIFY(!rec.startAudio());
        QVERIFY(!rec.audioRecording());
        QVERIFY2(rec.lastError().contains(QStringLiteral("sample rate")),
                 qPrintable(rec.lastError()));
    }

    void iqVerlangtEinenStromUndSagtEsZUERST()
    {
        RadioModel radio;
        if (radio.addSlice() < 0) { QSKIP("keine Scheibe anzulegen"); }
        radio.setActiveSlice(0);
        SliceModel* slice = radio.activeSlice();
        if (!slice) { QSKIP("keine aktive Scheibe"); }

        RecordingModel rec;
        rec.attach(&radio);

        // Eine Scheibe ohne Strom (streamIndex < 0) ist jede, die von
        // einem fremden Geraet gespeist wird -- Kiwi, SunSDR, Flex.
        // Dort rechnet nicht dieses Programm, es kommen fertige Toene
        // an, kein I/Q. Das muss VOR der Ratenpruefung gemeldet
        // werden: die Rate aendert daran nichts, und „verbinde zuerst"
        // waere ein falscher Rat.
        QVERIFY(slice->streamIndex() < 0);
        QVERIFY(!rec.startIq());
        QVERIFY2(rec.lastError().contains(QStringLiteral("I/Q")),
                 qPrintable(rec.lastError()));
        QVERIFY2(!rec.lastError().contains(QStringLiteral("sample rate")),
                 "die Ratenmeldung verdeckt die nuetzlichere");
    }

    void vorschlagTraegtBandUndBetriebsartAuchOhneVerbindung()
    {
        RadioModel radio;
        if (radio.addSlice() < 0) { QSKIP("keine Scheibe anzulegen"); }
        radio.setActiveSlice(0);
        SliceModel* slice = radio.activeSlice();
        if (!slice) { QSKIP("keine aktive Scheibe"); }

        slice->setFrequency(14'205'000.0);
        slice->setDspMode(DSPMode::LSB);

        RecordingModel rec;
        rec.attach(&radio);

        const QString p = rec.suggestedPath(QStringLiteral("wav"));

        // Band und Betriebsart haengen an der SCHEIBE, nicht an der
        // Verbindung. Als beides aus einem Weg kam, verlor der
        // Dateiname sie, sobald nichts verbunden war -- und genau dann
        // legt man Aufnahmen zum Vergleich an.
        QVERIFY2(p.contains(QStringLiteral("20m")), qPrintable(p));
        QVERIFY2(p.contains(QStringLiteral("LSB")), qPrintable(p));
        QVERIFY2(p.endsWith(QStringLiteral(".wav")), qPrintable(p));
        // UTC, sonst traegt eine Aufnahme ueber Mitternacht denselben
        // Namen zweimal.
        QVERIFY2(p.contains(QStringLiteral("Z")), qPrintable(p));
        QVERIFY2(p.startsWith(rec.recordingFolder()), qPrintable(p));
    }

    void derOrdnerIstDerselbeWieBeiDerQsoAufnahme()
    {
        RecordingModel rec;
        // QsoRecorderApplet.cpp:161 legt unter dataDir()/recordings ab.
        // Zwei Aufnahmearten an zwei Orten waere eine Suche zuviel.
        QVERIFY(rec.recordingFolder().endsWith(QStringLiteral("/recordings")));
        QVERIFY(QDir(rec.recordingFolder()).exists());
    }

    void derZeitplanRuftDieAufnahmeUndNichtsAnderes()
    {
        RecordingModel rec;
        // Kein Radio: der Start MUSS scheitern. Genau das macht den
        // Prueffall zum Faenger -- waere der Zeitplan nicht
        // angeschlossen, blieb lastError() leer.
        QVERIFY(rec.lastError().isEmpty());

        ScheduledRecording e;
        e.startAtUtc      = QDateTime::currentDateTimeUtc().addSecs(-5);
        e.durationMinutes = 0;
        e.kind            = ScheduledRecordingKind::Audio;
        e.path            = QString();
        e.enabled         = true;
        const int id = rec.schedule().add(e);
        QVERIFY(id >= 0);
        QCOMPARE(rec.scheduleCount(), 1);

        rec.schedule().checkNow(QDateTime::currentDateTimeUtc());

        QVERIFY2(!rec.lastError().isEmpty(),
                 "der Zeitplan hat die Aufnahme nicht gerufen");
        QVERIFY2(rec.lastError().contains(QStringLiteral("radio")),
                 qPrintable(rec.lastError()));
    }

    void dasVerlustzeichenBleibtStehen()
    {
        RecordingModel rec;
        QSignalSpy verlust(&rec, &RecordingModel::audioSamplesLostChanged);
        QVERIFY(!rec.audioSamplesLost());

        emit rec.audio().samplesLost();
        QCOMPARE(rec.audioSamplesLost(), true);
        QCOMPARE(verlust.count(), 1);

        // Zweimal derselbe Verlust ist kein zweites Ereignis -- und das
        // Zeichen darf nicht wieder verschwinden: eine Luecke in der
        // Datei heilt nicht.
        emit rec.audio().samplesLost();
        QCOMPARE(rec.audioSamplesLost(), true);
        QCOMPARE(verlust.count(), 1);
    }

    void dieWiedergabeIstEhrlichAbwesend()
    {
        RecordingModel rec;
        // PlaybackRadioConnection braucht einen Zweig in
        // RadioConnection::create() (Plan §6) -- eine Entscheidung am
        // Verbindungsweg, nicht in einer Bindeschicht. Solange die
        // fehlt, sagt die Eigenschaft das, statt einen Zustand zu
        // erfinden, auf den eine Flaeche dann wartet.
        QVERIFY(!rec.playbackAvailable());
        QVERIFY(!rec.playing());
        QVERIFY(rec.playbackFile().isEmpty());
    }
};

QTEST_MAIN(TstRecordingModel)
#include "tst_recording_model.moc"
