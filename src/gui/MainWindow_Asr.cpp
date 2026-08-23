// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/MainWindow_Asr.cpp  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Verbindung zwischen Ton und Mitschrift ──────────────────────
//
// Der Betreiber hat am 2026-08-23 den OERTLICHEN Weg gewaehlt: ein
// Whisper-Dienst auf seinem eigenen Rechner. Der Ton verlaesst die
// Maschine nicht.
//
// Hier steht, was drei Dinge zusammenbringt: den Abgriff an der
// Tonmaschine, den Dienst, der daraus Text macht, und das Applet, das
// ihn zeigt.
//
// ── Warum das Einschalten so viel prueft ────────────────────────────
//
// Weil hier praktisch alles fehlen kann und jedes Fehlen anders
// aussieht: keine Adresse eingetragen, Dienst laeuft nicht, keine
// Scheibe zum Abhoeren. Jeder dieser Faelle muss dem Betreiber SAGEN,
// was fehlt — ein Haken, der sich stumm wieder ausschaltet, ist die
// schlechteste aller Antworten.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/MainWindow.h"

#include "asr/AsrService.h"
#include "asr/RemoteAsrBackend.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/LogCategories.h"
#include "core/audio/AudioTapRing.h"
#include "gui/applets/AsrApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace Longpath {

namespace {
// Eine halbe Sekunde Stereo bei 48 kHz. Reichlich: der Zeitgeber des
// Dienstes leert zwanzigmal je Sekunde.
constexpr int kAsrRingFloats = 48000;
} // namespace

void MainWindow::setAsrEnabled(bool on)
{
    if (!on) {
        if (m_asrService) { m_asrService->stop(); }
        if (m_radioModel && m_radioModel->audioEngine()) {
            m_radioModel->audioEngine()->setAsrTap(nullptr, -1);
        }
        m_asrTapSlice = -1;
        return;
    }

    auto& st = AppSettings::instance();
    const QString url = st.value(QStringLiteral("AsrEndpointUrl"),
                                 QStringLiteral("http://127.0.0.1:8080/inference"))
                            .toString().trimmed();
    if (url.isEmpty()) {
        if (m_asrApplet) {
            emit m_asrApplet->enableRequested(false);
        }
        qCWarning(lcDsp) << "Spracherkennung: keine Adresse eingetragen "
                            "(AsrEndpointUrl)";
        return;
    }

    SliceModel* slice = m_radioModel ? m_radioModel->activeSlice() : nullptr;
    if (!slice) {
        const QList<SliceModel*> slices =
            m_radioModel ? m_radioModel->slices() : QList<SliceModel*>{};
        if (!slices.isEmpty()) { slice = slices.first(); }
    }
    if (!slice) {
        qCWarning(lcDsp) << "Spracherkennung: keine Scheibe zum Abhoeren";
        return;
    }

    if (!m_asrTapRing) {
        m_asrTapRing = std::make_unique<AudioTapRing>();
        m_asrTapRing->resize(kAsrRingFloats);
    }
    m_asrTapRing->reset();

    if (!m_asrService) {
        m_asrService = new AsrService(this);
        if (m_asrApplet) { m_asrApplet->setService(m_asrService); }
    }

    RemoteAsrConfig cfg;
    cfg.url = url;
    cfg.apiKey = st.value(QStringLiteral("AsrApiKey"), QString()).toString();
    cfg.language = st.value(QStringLiteral("AsrLanguage"),
                            QStringLiteral("de")).toString();
    cfg.model = st.value(QStringLiteral("AsrModel"),
                         QStringLiteral("whisper-1")).toString();
    m_asrService->setBackend(std::make_unique<RemoteAsrBackend>(cfg));
    m_asrService->setSource(m_asrTapRing.get(), 48000);

    // Der Abgriff ZULETZT: erst wenn der Dienst steht, darf der
    // Tonfaden schreiben. Umgekehrt liefe der Ring voll, bevor jemand
    // ihn leert, und der erste Sprechabschnitt waere zerhackt.
    m_asrService->start();
    if (!m_asrService->isRunning()) { return; }

    m_asrTapSlice = slice->sliceIndex();
    if (m_radioModel->audioEngine()) {
        m_radioModel->audioEngine()->setAsrTap(m_asrTapRing.get(),
                                               m_asrTapSlice);
    }
    qCInfo(lcDsp).nospace()
        << "Spracherkennung an: " << url << ", Scheibe " << m_asrTapSlice
        << ", Sprache " << cfg.language;
}

} // namespace Longpath
