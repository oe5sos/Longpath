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

namespace Longpath {

CatRigConnection::CatRigConnection(QObject* parent)
    : RadioConnection(parent)
{
}

CatRigConnection::~CatRigConnection() = default;

void CatRigConnection::init() {}

void CatRigConnection::connectToRadio(const RadioInfo& info)
{
    // Kein Handschlag: das Geraet haengt an rigctld, und ob es antwortet,
    // meldet CompanionRigController. Hier gibt es nichts abzuwarten.
    m_radioInfo = info;
    setState(ConnectionState::Connected);
}

void CatRigConnection::disconnect()
{
    setState(ConnectionState::Disconnected);
}

void CatRigConnection::setReceiverFrequency(int, quint64) {}
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
