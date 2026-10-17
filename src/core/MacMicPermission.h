#pragma once

// Request microphone permission on macOS at app startup.
//
// Hardened-Runtime + Developer-ID signed apps with NSMicrophoneUsageDescription
// in Info.plist will normally surface the system permission dialog the first
// time something opens an input audio unit. In practice that's unreliable for
// us: AudioEngine::ensureTxInputOpen() drives Pa_OpenStream through PortAudio's
// CoreAudio backend, and the device resolver silently returns paNoDevice on
// machines that lack a built-in mic (Mac mini / Mac Studio) or whose only
// inputs are audio interfaces with names that don't match the hardware-mic
// regex. When that happens, AVCaptureDevice never sees an access attempt and
// macOS has no reason to show the prompt.
//
// Calling AVCaptureDevice's requestAccess(forMediaType:AVMediaTypeAudio) at
// app launch deterministically engages TCC regardless of what audio hardware
// is attached, so the user gets the prompt on first launch and the answer is
// cached for every subsequent mic open. No-op on non-macOS platforms.
//
// microphoneAccessGranted() beantwortet dieselbe Frage nachtraeglich, ohne
// etwas anzufordern: darf dieser Prozess JETZT das Mikrofon oeffnen?
//
// Noetig, weil AudioEngine::ensureTxInputOpen() den Eingang eifrig oeffnet —
// und das blockiert, solange TCC die Frage noch stellt. Am 2026-10-03 hat
// das den ganzen Verbindungsaufbau eingefroren: der Dialog stand auf einem
// unsichtbaren Space, PortAudioBus::open() wartete ohne Zeitlimit, und weil
// der Aufruf in der verschachtelten Ereignisschleife von
// RadioModel::connectToRadio steckt, stand mit ihm die gesamte Oberflaeche.
// Siehe docs/architecture/2026-10-03-verbindungshaenger-mikrofon.md.
//
// Auf Nicht-macOS immer true — dort gibt es diese Huerde nicht.
#ifdef Q_OS_MAC
namespace Longpath {
void requestMicrophonePermission();
bool microphoneAccessGranted();
}
#else
namespace Longpath {
inline void requestMicrophonePermission() {}
inline bool microphoneAccessGranted() { return true; }
}
#endif
