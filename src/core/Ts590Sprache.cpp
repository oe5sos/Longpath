// =================================================================
// src/core/Ts590Sprache.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original, siehe Ts590Sprache.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-12 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-13 — Vorlauf nach TX1, Hochpass und fester Pegel je
//                 Ansage; KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ts590Sprache.h"

#include "core/AppSettings.h"
#include "core/CompanionRigController.h"
#include "core/LogCategories.h"
#include "core/TransverterSetup.h"
#include "core/audio/VoiceKeyer.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

bool einstellungAn()
{
    const QString v = AppSettings::instance()
        .value(QStringLiteral("CompanionRigVoiceKeyer"), QStringLiteral("False"))
        .toString().trimmed();
    return v.compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0;
}

bool istSprache(const QString& modus)
{
    const QString m = modus.trimmed().toUpper();
    return m == QStringLiteral("USB") || m == QStringLiteral("LSB")
        || m == QStringLiteral("FM") || m == QStringLiteral("AM");
}

} // namespace

Ts590Sprache::Ts590Sprache(CompanionRigController* rig, WavTxSource* quelle,
                           QObject* parent)
    : QObject(parent), m_rig(rig), m_quelle(quelle)
{
    m_pumpe.setInterval(kPumpeMs);
    connect(&m_pumpe, &QTimer::timeout, this, &Ts590Sprache::pumpe);

    m_hoechstens.setSingleShot(true);
    m_hoechstens.setInterval(kMaxSendenMs);
    connect(&m_hoechstens, &QTimer::timeout, this, [this]() {
        lasLos(tr("TS-590 voice keyer: transmitted for %1 s — switched off.")
                   .arg(m_hoechstens.interval() / 1000));
    });

    if (m_rig) {
        connect(m_rig, &CompanionRigController::catZustandGeaendert,
                this, &Ts590Sprache::pruefeWaehrendDerSendung);
        connect(m_rig, &CompanionRigController::verbindungGeaendert,
                this, &Ts590Sprache::pruefeWaehrendDerSendung);
        connect(m_rig, &CompanionRigController::modusGeaendert,
                this, &Ts590Sprache::pruefeWaehrendDerSendung);
    }
}

Ts590Sprache::~Ts590Sprache()
{
    // Beim Abbau nie einen Sender stehen lassen.
    if (m_getastet && m_rig) { m_rig->stelle(befehlEmpfangen()); }
    m_pumpe.stop();
    if (m_senke) { m_senke->stop(); }
}

bool Ts590Sprache::zustaendig() const
{
    return einstellungAn() && m_rig && m_rig->catBetrieb() && m_rig->istTs590();
}

QString Ts590Sprache::sperrgrund() const
{
    if (!einstellungAn()) {
        return tr("Voice keyer via the TS-590 is off (Setup → Companion rig).");
    }
    if (!m_rig || !m_rig->catBetrieb() || !m_rig->istTs590() || !m_rig->verbunden()) {
        return tr("The TS-590 is not connected.");
    }
    const TransverterSetup& tv = TransverterSetup::current();
    if (!tv.active()) {
        return tr("The TS-590 sends voice memories only through the transverter — "
                  "no transverter is set (Setup → Companion rig → Transverter).");
    }
    if (tv.sperrtSenden()) {
        return tr("The transverter is set to receive only (IFo2).");
    }
    const Ts590Cat::Zustand& z = m_rig->catZustand();
    if (z.drive != 1) {
        return tr("DRV is off at the TS-590: the power would go to the antenna "
                  "socket, not to the transverter.");
    }
    if (z.leistungW < 0) {
        return tr("The TS-590's power is not read yet.");
    }
    if (z.leistungW > kMaxLeistungW) {
        return tr("The TS-590 is set to %1 W; the voice keyer sends at %2 W at most.")
            .arg(z.leistungW).arg(kMaxLeistungW);
    }
    if (!istSprache(m_rig->modus())) {
        return tr("The TS-590 is in %1, not in a voice mode.").arg(m_rig->modus());
    }
    return {};
}

bool Ts590Sprache::legeWeg()
{
    if (m_testZiel) {
        m_ziel = m_testZiel;
    } else {
        // Vorgabe: dasselbe USB-Tongeraet, aus dem der Empfang kommt.
        QString gesucht = AppSettings::instance()
            .value(QStringLiteral("CompanionRigAudioOutput")).toString().trimmed();
        if (gesucht.isEmpty()) {
            gesucht = AppSettings::instance()
                .value(QStringLiteral("CompanionRigAudioInput"),
                       QStringLiteral("USB Audio CODEC")).toString().trimmed();
        }
        QAudioDevice ausgang;
        for (const QAudioDevice& d : QMediaDevices::audioOutputs()) {
            if (!gesucht.isEmpty() && d.description().contains(gesucht, Qt::CaseInsensitive)) {
                ausgang = d;
                break;
            }
        }
        if (ausgang.isNull()) {
            qCWarning(lcConnection) << "TS-590 voice keyer: audio output" << gesucht
                                    << "not found";
            return false;
        }
        QAudioFormat format;
        format.setSampleRate(kRateHz);
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Int16);
        if (!ausgang.isFormatSupported(format)) {
            qCWarning(lcConnection) << "TS-590 voice keyer: 48 kHz stereo not supported by"
                                    << ausgang.description();
            return false;
        }
        m_senke = std::make_unique<QAudioSink>(ausgang, format);
        m_senke->setBufferSize(kRateHz * 2 * 2 / 10);   // 100 ms
        m_ziel = m_senke->start();
        if (!m_ziel) {
            m_senke.reset();
            return false;
        }
    }
    m_pumpe.start();
    return true;
}

void Ts590Sprache::nimmWegZurueck()
{
    m_pumpe.stop();
    if (m_senke) {
        m_senke->stop();
        m_senke.reset();
    }
    m_ziel = nullptr;
}

QString Ts590Sprache::taste(bool an)
{
    if (!m_rig) { return tr("no companion rig"); }
    if (!an) {
        if (!m_getastet) { return {}; }
        m_hoechstens.stop();
        m_rig->stelle(befehlEmpfangen());
        m_getastet = false;
        emit getastetGeaendert(false);
        return {};
    }
    if (m_getastet) { return {}; }
    const QString grund = sperrgrund();
    if (!grund.isEmpty()) { return grund; }
    m_rig->stelle(befehlSenden());
    m_getastet = true;
    m_vorlaufRest = kVorlaufMs * kRateHz / 1000;
    m_hochpass.zuruecksetzen();
    m_hoechstens.start();
    qCInfo(lcConnection) << "TS-590 voice keyer: TX1";
    emit getastetGeaendert(true);
    return {};
}

void Ts590Sprache::lasLos(const QString& grund)
{
    if (!m_getastet) { return; }
    taste(false);
    qCWarning(lcConnection) << "TS-590 voice keyer released:" << grund;
    emit hinweis(grund);
}

void Ts590Sprache::pruefeWaehrendDerSendung()
{
    if (!m_getastet) { return; }
    const QString grund = sperrgrund();
    if (!grund.isEmpty()) { lasLos(grund); }
}

void Ts590Sprache::pumpe()
{
    if (!m_ziel || !m_quelle) { return; }
    // Ohne Taste kein Ton an den 590: nichts soll an seinem Eingang
    // liegen, solange er nicht ueber diesen Weg sendet.
    if (!m_getastet) { return; }
    // Nicht mehr schieben, als die Senke nimmt: sonst laeuft der Ton
    // der Ansage davon und das Ende kommt nach dem Loslassen.
    qint64 frei = m_senke ? m_senke->bytesFree() : qint64(kRateHz * 4 * kPumpeMs / 1000 * 2);
    const int rahmen = static_cast<int>(std::min<qint64>(frei / 4, kRateHz / 10));
    if (rahmen <= 0) { return; }
    m_mono.resize(rahmen);
    int n = 0;
    if (m_vorlaufRest > 0) {
        // Vorlauf: Stille, bis 590 und Transverter umgeschaltet haben.
        // Die Ansage wartet so lange -- sie wird erst hier abgeholt.
        n = std::min(rahmen, m_vorlaufRest);
        std::fill_n(m_mono.data(), n, 0.0f);
        m_vorlaufRest -= n;
    } else {
        if (m_quelle->loadCount() != m_geladen) {
            // Eine neue Ansage: Pegel aus der ganzen Datei.
            m_geladen = m_quelle->loadCount();
            const int rate = m_quelle->rate() > 0 ? m_quelle->rate() : kRateHz;
            m_verstaerkung = AnsageAufbereitung::verstaerkungFuerSendung(m_quelle->samples(), rate);
            m_hochpass.setze(rate);
            qCInfo(lcConnection) << "TS-590 voice keyer: level x" << m_verstaerkung;
        }
        n = m_quelle->pullSamples(m_mono.data(), rahmen);
        if (n <= 0) { return; }
        for (int i = 0; i < n; ++i) {
            m_mono[i] = m_hochpass(m_mono[i]) * m_verstaerkung;
        }
    }
    m_stereo.resize(n * 4);
    auto* p = reinterpret_cast<qint16*>(m_stereo.data());
    for (int i = 0; i < n; ++i) {
        const float v = std::clamp(m_mono[i], -1.0f, 1.0f);
        const auto s = static_cast<qint16>(std::lround(v * 32767.0f));
        p[2 * i] = s;
        p[2 * i + 1] = s;
    }
    m_ziel->write(m_stereo);
}

} // namespace Longpath
