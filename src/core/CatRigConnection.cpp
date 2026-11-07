// =================================================================
// src/core/CatRigConnection.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original, siehe CatRigConnection.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "CatRigConnection.h"

#include "LogCategories.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>

#include <cmath>

namespace Longpath {

namespace {
constexpr double kAbtastrate = 48000.0;
} // namespace

CatRigConnection::CatRigConnection(QObject* parent)
    : RadioConnection(parent)
{
}

CatRigConnection::~CatRigConnection()
{
    stoppeTon();
}

void CatRigConnection::init() {}

void CatRigConnection::connectToRadio(const RadioInfo& info)
{
    // Kein Handschlag: das Geraet haengt an rigctld, und ob es antwortet,
    // meldet CompanionRigController. Hier gibt es nichts abzuwarten.
    m_radioInfo = info;
    setState(ConnectionState::Connected);
    starteTon();
}

void CatRigConnection::disconnect()
{
    stoppeTon();
    setState(ConnectionState::Disconnected);
}

void CatRigConnection::starteTon()
{
    if (m_quelle) { return; }

    QAudioDevice eingang;
    const QString gesucht = m_tonEingang.trimmed();
    for (const QAudioDevice& d : QMediaDevices::audioInputs()) {
        if (!gesucht.isEmpty() && d.description().contains(gesucht, Qt::CaseInsensitive)) {
            eingang = d;
            break;
        }
    }
    if (eingang.isNull()) {
        qCWarning(lcConnection) << "CAT rig: audio input" << gesucht
                                << "not found -- connected without sound";
        return;
    }

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!eingang.isFormatSupported(format)) {
        format.setChannelCount(1);
    }
    if (!eingang.isFormatSupported(format)) {
        qCWarning(lcConnection) << "CAT rig: audio input" << eingang.description()
                                << "does not offer 48 kHz 16 bit -- connected without sound";
        return;
    }

    m_kanaele = format.channelCount();
    m_quelle = new QAudioSource(eingang, format, this);
    // Reserve fuer einen spaeten Weckruf, keine Verzoegerung: der Eingang
    // wird bei jedem Weckruf leergelesen.
    m_quelle->setBufferSize(qsizetype(kBlockProben) * m_kanaele * 2 * 8);
    m_tonGeraet = m_quelle->start();
    if (!m_tonGeraet) {
        qCWarning(lcConnection) << "CAT rig: audio input" << eingang.description()
                                << "would not start -- connected without sound";
        stoppeTon();
        return;
    }
    connect(m_tonGeraet, &QIODevice::readyRead, this, [this]() {
        if (m_tonGeraet) { verarbeiteTon(m_tonGeraet->readAll(), m_kanaele); }
    });
    qCInfo(lcConnection) << "CAT rig: sound from" << eingang.description()
                         << "," << m_kanaele << "channel(s)";
}

void CatRigConnection::stoppeTon()
{
    m_tonGeraet = nullptr;
    if (m_quelle) {
        m_quelle->stop();
        delete m_quelle;
        m_quelle = nullptr;
    }
    m_vorrat.clear();
}

void CatRigConnection::verarbeiteTon(const QByteArray& pcm, int kanaele)
{
    if (kanaele <= 0) { return; }
    const auto* roh = reinterpret_cast<const qint16*>(pcm.constData());
    const int proben = static_cast<int>(pcm.size() / qsizetype(sizeof(qint16))) / kanaele;
    const int alt = static_cast<int>(m_vorrat.size());
    m_vorrat.resize(alt + proben);
    // Der erste Kanal: im Geraet sind beide gleich.
    for (int i = 0; i < proben; ++i) {
        m_vorrat[alt + i] = static_cast<float>(roh[i * kanaele]) / 32768.0f;
    }
    lieferBloecke();
}

void CatRigConnection::schiebeAnDenTraeger(QVector<float>& iq)
{
    const double traeger = m_traegerHz.load();
    const double mitte   = m_mitteHz.load();
    if (traeger <= 0.0 || mitte <= 0.0) { return; }

    const double abstand = traeger - mitte;
    if (std::abs(abstand) >= kAbtastrate / 2.0) { return; }
    if (abstand == 0.0) {
        m_phase = 0.0;
        return;
    }

    // Minus: im Empfangsweg liegt "ueber dem Traeger" bei negativer
    // Frequenz, siehe AudioZuIq.h.
    const double schritt = -2.0 * M_PI * abstand / kAbtastrate;
    const int proben = static_cast<int>(iq.size()) / 2;
    for (int i = 0; i < proben; ++i) {
        const float c = static_cast<float>(std::cos(m_phase));
        const float s = static_cast<float>(std::sin(m_phase));
        const float re = iq[2 * i];
        const float im = iq[2 * i + 1];
        iq[2 * i]     = re * c - im * s;
        iq[2 * i + 1] = re * s + im * c;
        m_phase += schritt;
    }
    m_phase = std::remainder(m_phase, 2.0 * M_PI);
}

void CatRigConnection::lieferBloecke()
{
    m_wandler.setGespiegelt(m_gespiegelt.load());
    int fertig = 0;
    const int vorhanden = static_cast<int>(m_vorrat.size());
    while (vorhanden - fertig >= kBlockProben) {
        QVector<float> block =
            m_wandler.verarbeite(m_vorrat.constData() + fertig, kBlockProben);
        fertig += kBlockProben;
        schiebeAnDenTraeger(block);
        emit iqDataReceived(/*hwReceiverIndex=*/0, block);
        emit frameReceived();
    }
    if (fertig > 0) { m_vorrat.remove(0, fertig); }
}

void CatRigConnection::setReceiverFrequency(int, quint64 frequencyHz)
{
    m_mitteHz.store(static_cast<double>(frequencyHz));
}

void CatRigConnection::setActiveReceiverCount(int) {}
void CatRigConnection::setSampleRate(int) {}
void CatRigConnection::setTxFrequency(quint64) {}
void CatRigConnection::setAttenuator(int) {}
void CatRigConnection::setPreamp(bool) {}
void CatRigConnection::setTxDrive(int) {}
void CatRigConnection::setMox(bool) {}
void CatRigConnection::setAntennaRouting(AntennaRouting) {}
void CatRigConnection::sendTxIq(const float*, int) {}
void CatRigConnection::setTrxRelay(bool) {}
void CatRigConnection::setMicBoost(bool) {}
void CatRigConnection::setLineIn(bool) {}
void CatRigConnection::setMicTipRing(bool) {}
void CatRigConnection::setMicBias(bool) {}
void CatRigConnection::setLineInGain(int) {}
void CatRigConnection::setUserDigOut(quint8) {}
void CatRigConnection::setPuresignalRun(bool) {}
void CatRigConnection::setMicPTTDisabled(bool) {}
void CatRigConnection::setMicXlr(bool) {}
void CatRigConnection::setWatchdogEnabled(bool) {}

} // namespace Longpath
