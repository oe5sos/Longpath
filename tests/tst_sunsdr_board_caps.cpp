// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die SunSDR2 QRP muss mit IHRER Abtastrate gefahren werden, nicht mit
// der von Atlas.
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Fehler, den das hier festnagelt (2026-09-23): defaultModelForBoard()
// kennt nur OpenHPSDR-Baugruppen. Die SunSDR steht in keiner
// HPSDRModel-Liste, also faellt die Aufloesung auf HPSDRModel::HPSDR und
// profileForModel() setzt effectiveBoard = Atlas. Ab da entscheiden
// Atlas' Kenndaten ueber die Abtastrate: 192 000 Hz fuer ein Geraet, das
// 48 000 liefert.
//
// Im Protokoll einer laufenden QRP-Verbindung sah das so aus:
//
//     HardwareProfile: model= HPSDR (Atlas/Metis) effectiveBoard= 0
//     Connecting with sampleRate= 192000
//
// Das hat zwei Tage Fehlersuche gekostet: die 48 000 in der QRP-Zeile
// von BoardCapabilities wurden nie gelesen, also war jede Fassung, die
// „mit 48 kHz" gehoert wurde, in Wahrheit eine mit 192 kHz. Ohne diesen
// Pruefstand faellt so etwas beim naechsten Umbau still wieder zurueck.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-23 — Created for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Der Eintrag, wie ihn "Add Manually" fuer die QRP anlegt und wie er
// am 2026-09-23 in der Einstellungsdatei des Betreibers stand:
//   boardType 13 (SunSdr2Qrp), modelOverride 0.
// modelOverride 0 ist NICHT "keine Wahl" -- HPSDRModel::FIRST ist -1,
// die 0 ist HPSDRModel::HPSDR, also der Atlas/Metis-Bausatz. Genau
// deshalb stand im Protokoll "model= HPSDR (Atlas/Metis)".
RadioInfo infoFor(HPSDRHW board, ProtocolVersion protocol,
                  HPSDRModel modelOverride = HPSDRModel::HPSDR)
{
    RadioInfo info;
    info.address       = QHostAddress(QStringLiteral("192.168.16.200"));
    info.port          = 50001;
    info.boardType     = board;
    info.protocol      = protocol;
    info.modelOverride = modelOverride;
    info.macAddress    = QStringLiteral("MANUAL:192.168.16.200:1024");
    info.name          = QStringLiteral("SUNSDR QRP");
    return info;
}

} // namespace

class TstSunSdrBoardCaps : public QObject {
    Q_OBJECT

private slots:

    void theQrpIsRunAtItsOwnRate()
    {
        RadioModel model;
        model.applyHardwareProfileForTest(
            infoFor(HPSDRHW::SunSdr2Qrp, ProtocolVersion::SunSdr));

        QCOMPARE(model.boardCapabilities().board, HPSDRHW::SunSdr2Qrp);
        QCOMPARE(model.boardCapabilities().maxSampleRate, 48000);
    }

    void withoutTheExceptionItIsAtlasAgain()
    {
        // Gegenprobe, damit der Fall oben nicht aus Versehen etwas
        // Selbstverstaendliches behauptet: DERSELBE Eintrag, nur ohne
        // das SunSDR-Protokoll, landet weiterhin bei Atlas und dessen
        // 192 kHz -- der Zustand, der zwei Tage Fehlersuche gekostet hat.
        RadioModel model;
        model.applyHardwareProfileForTest(
            infoFor(HPSDRHW::SunSdr2Qrp, ProtocolVersion::Protocol1));

        QCOMPARE(model.boardCapabilities().board, HPSDRHW::Atlas);
        QCOMPARE(model.boardCapabilities().maxSampleRate, 192000);
    }

    void anOrdinaryBoardIsUntouched()
    {
        // Die Ausnahme darf nur die SunSDR betreffen.
        RadioModel model;
        model.applyHardwareProfileForTest(
            infoFor(HPSDRHW::HermesLite, ProtocolVersion::Protocol1,
                    HPSDRModel::FIRST));

        QCOMPARE(model.boardCapabilities().board, HPSDRHW::HermesLite);
    }
};

QTEST_MAIN(TstSunSdrBoardCaps)
#include "tst_sunsdr_board_caps.moc"
