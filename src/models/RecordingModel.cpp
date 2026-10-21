// =================================================================
// src/models/RecordingModel.cpp  (Longpath)
// =================================================================
// Siehe RecordingModel.h fuer das Warum.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include "RecordingModel.h"

#include <QDateTime>
#include <QDir>
#include <QLocale>

#include "Band.h"
#include "RadioModel.h"
#include "SliceModel.h"
#include "core/AppSettings.h"

namespace Longpath {

RecordingModel::RecordingModel(QObject* parent)
    : QObject(parent)
{
    wireControllers();
}

RecordingModel::~RecordingModel() = default;

void RecordingModel::wireControllers()
{
    connect(&m_wav, &WavRecorderController::recordingChanged,
            this, [this](bool on) {
        if (!on) { m_audioSeconds = m_wav.recorder().recordedSeconds(); }
        emit audioRecordingChanged(on);
        emit audioSecondsChanged(m_audioSeconds);
    });
    connect(&m_wav, &WavRecorderController::secondsChanged,
            this, [this](double s) {
        m_audioSeconds = s;
        emit audioSecondsChanged(s);
    });
    connect(&m_wav, &WavRecorderController::samplesLost,
            this, [this]() {
        // Einmal gesetzt, bleibt es fuer diese Aufnahme stehen: eine
        // Luecke verschwindet nicht wieder, und eine Flaeche, die das
        // Zeichen nach einer Sekunde ausblendet, verschweigt sie.
        if (m_audioLost) { return; }
        m_audioLost = true;
        emit audioSamplesLostChanged(true);
    });

    connect(&m_iq, &IqRecorderController::recordingChanged,
            this, [this](bool on) {
        if (!on) { m_iqSeconds = m_iq.recorder().recordedSeconds(); }
        emit iqRecordingChanged(on);
        emit iqSecondsChanged(m_iqSeconds);
    });
    connect(&m_iq, &IqRecorderController::secondsChanged,
            this, [this](double s) {
        m_iqSeconds = s;
        emit iqSecondsChanged(s);
    });
    connect(&m_iq, &IqRecorderController::samplesLost,
            this, [this]() {
        if (m_iqLost) { return; }
        m_iqLost = true;
        emit iqSamplesLostChanged(true);
    });

    // ── Der Zeitplan ─────────────────────────────────────────────────
    //
    // Er sagt nur „jetzt waere eine fällig" und nennt die Art; das
    // Starten bleibt hier, damit die Angaben aus dem Radio auf
    // demselben Weg entstehen wie bei einem Druck auf den Knopf. Ein
    // zweiter Weg waere ein zweiter Ort, an dem die Frequenz falsch
    // sein kann.
    connect(&m_schedule, &RecordingScheduler::recordingDue,
            this, [this](int, Longpath::ScheduledRecording entry) {
        if (entry.kind == ScheduledRecordingKind::Iq) {
            startIq(entry.path);
        } else {
            startAudio(entry.path);
        }
    });
    connect(&m_schedule, &RecordingScheduler::recordingShouldStop,
            this, [this](int id) {
        const std::optional<ScheduledRecording> e = m_schedule.entry(id);
        if (e && e->kind == ScheduledRecordingKind::Iq) { stopIq(); }
        else                                            { stopAudio(); }
    });
}

void RecordingModel::attach(RadioModel* radio)
{
    if (m_radio == radio) { return; }

    const bool warReady = ready();
    m_radio = radio;

    m_wav.attach(radio ? radio->audioEngine() : nullptr);
    m_iq.attach(radio);

    // ── Den Takt erst hier anwerfen ──────────────────────────────────
    //
    // Ohne das stand er still: `recordingDue` war verdrahtet, aber
    // niemand rief `start()`, also lief der Zeitgeber nie und
    // gespeicherte Eintraege wurden nie gelesen. `scheduleRunning()`
    // konnte darum NIE true werden -- die Anzeige „ARMED" war tot.
    //
    // Der Prueffall dazu rief `checkNow()` selbst auf. Er belegte die
    // Verdrahtung und nicht den Betrieb: ein Waechter, wo ein Faenger
    // gebraucht wird.
    //
    // An `attach` und nicht an den Baukasten, weil ein faelliger
    // Eintrag ohne Radio nur eine Fehlermeldung erzeugen wuerde.
    if (radio) {
        m_schedule.loadFromSettings();
        m_schedule.start();
    } else {
        m_schedule.stop();
    }

    if (ready() != warReady) { emit readyChanged(ready()); }
    emit scheduleRunningChanged(m_schedule.isRunning());
    emit scheduleCountChanged(scheduleCount());
}

bool RecordingModel::sliceInfo(QString* freq, QString* mode,
                               QString* band) const
{
    if (!m_radio) { return false; }

    const SliceModel* slice = m_radio->activeSlice();
    if (!slice) { return false; }

    const double hz = slice->frequency();

    // Dieselbe Schreibweise wie im Kommentar der Infostruktur
    // („14.205.000"): Punkte als Tausendertrenner, keine Einheit.
    *freq = QLocale::c().toString(static_cast<qlonglong>(hz))
                .replace(QLatin1Char(','), QLatin1Char('.'));
    *mode = SliceModel::modeName(slice->dspMode());
    *band = bandLabel(bandFromFrequency(hz));
    return true;
}

QString RecordingModel::recordingFolder() const
{
    // Derselbe Ort wie bei der QSO-Aufnahme
    // (QsoRecorderApplet.cpp:161) -- nicht ein zweiter, sonst sucht der
    // Betreiber seine Aufnahmen an zwei Stellen.
    const QString dir = AppSettings::dataDir() + QStringLiteral("/recordings");
    QDir().mkpath(dir);
    return dir;
}

QString RecordingModel::suggestedPath(const QString& suffix) const
{
    QString freq;
    QString mode;
    QString band;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString stamp = now.toString(QStringLiteral("yyyyMMdd-HHmmss"))
                          + QStringLiteral("Z");

    if (!sliceInfo(&freq, &mode, &band)) {
        return recordingFolder() + QLatin1Char('/') + stamp
               + QLatin1Char('.') + suffix;
    }
    return recordingFolder() + QLatin1Char('/') + stamp
           + QLatin1Char('_') + band + QLatin1Char('_') + mode
           + QLatin1Char('.') + suffix;
}

void RecordingModel::setLastError(const QString& message)
{
    if (m_lastError == message) { return; }
    m_lastError = message;
    emit lastErrorChanged(m_lastError);
}

bool RecordingModel::startAudio(const QString& path)
{
    if (!m_radio) {
        setLastError(tr("No radio — connect first."));
        return false;
    }
    if (m_wav.isRecording()) { return false; }

    WavRecordingInfo info;
    if (!sliceInfo(&info.frequency, &info.mode, &info.band)) {
        setLastError(tr("No active receiver."));
        return false;
    }
    // Die Rate der Verbindung, nicht die der Tonausgabe: der Abgriff
    // sitzt VOR dem Mischer (AudioEngine.h), also in der Rate, in der
    // die Kette rechnet.
    const int rate = m_radio->connectionSampleRateHz();
    if (rate <= 0) {
        setLastError(tr("The sample rate is not known yet — "
                        "connect first."));
        return false;
    }
    info.utcStart   = QDateTime::currentDateTimeUtc();
    info.sampleRate = rate;

    const SliceModel* slice = m_radio->activeSlice();
    m_wav.setSliceId(slice ? slice->streamIndex() : -1);
    m_wav.setSampleRate(rate);

    const QString ziel = path.isEmpty()
                             ? suggestedPath(QStringLiteral("wav"))
                             : path;

    QString fehler;
    if (!m_wav.start(ziel, info, &fehler)) {
        setLastError(fehler.isEmpty()
                         ? tr("Could not start the recording.")
                         : fehler);
        return false;
    }

    setLastError(QString());
    m_audioLost    = false;
    m_audioSeconds = 0.0;
    m_audioFile    = ziel;
    emit audioSamplesLostChanged(false);
    emit audioFileChanged(m_audioFile);
    return true;
}

void RecordingModel::stopAudio()
{
    m_wav.stop();
}

bool RecordingModel::startIq(const QString& path)
{
    if (!m_radio) {
        setLastError(tr("No radio — connect first."));
        return false;
    }
    if (m_iq.isRecording()) { return false; }

    // Zuerst der Strom, dann die Rate: eine Scheibe von einem fremden
    // Geraet bekommt NIE I/Q, egal welche Rate die Verbindung meldet.
    // Die Meldung darueber ist die nuetzlichere, also darf die
    // Ratenpruefung sie nicht verdecken.
    const SliceModel* slice = m_radio->activeSlice();
    if (!slice || slice->streamIndex() < 0) {
        // Ohne Strom gibt es kein I/Q. Das trifft jede Scheibe, die
        // von einem fremden Geraet gespeist wird (Kiwi, SunSDR, Flex):
        // dort rechnet nicht dieses Programm, es kommen fertige Toene
        // und fertige Bildpunkte an, kein I/Q.
        setLastError(tr("This receiver has no I/Q stream — I/Q "
                        "recording needs a direct radio."));
        return false;
    }

    IqRecordingInfo info;
    if (!sliceInfo(&info.frequency, &info.mode, &info.band)) {
        setLastError(tr("No active receiver."));
        return false;
    }
    const int rate = m_radio->connectionSampleRateHz();
    if (rate <= 0) {
        setLastError(tr("The sample rate is not known yet — "
                        "connect first."));
        return false;
    }
    info.utcStart   = QDateTime::currentDateTimeUtc();
    info.sampleRate = rate;

    m_iq.setStreamIndex(slice->streamIndex());
    m_iq.setSampleRate(rate);

    const QString ziel = path.isEmpty()
                             ? suggestedPath(QStringLiteral("wav"))
                             : path;

    QString fehler;
    if (!m_iq.start(ziel, info, &fehler)) {
        setLastError(fehler.isEmpty()
                         ? tr("Could not start the I/Q recording.")
                         : fehler);
        return false;
    }

    setLastError(QString());
    m_iqLost    = false;
    m_iqSeconds = 0.0;
    m_iqFile    = ziel;
    emit iqSamplesLostChanged(false);
    emit iqFileChanged(m_iqFile);
    return true;
}

void RecordingModel::stopIq()
{
    m_iq.stop();
}

}  // namespace Longpath
