#include "core/audio/BusMitFrist.h"

#include "core/LogCategories.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <QLoggingCategory>

namespace Longpath::Audio {
namespace {

/// Gemeinsamer Zustand zwischen Aufrufer und Öffnungsfaden.
struct BusOeffnung {
    std::mutex mutex;
    std::condition_variable wach;
    bool fertig = false;
    bool aufgegeben = false;
    bool geglueckt = false;
    std::unique_ptr<IAudioBus> bus;   // unter `mutex`
};

}  // namespace

std::unique_ptr<IAudioBus> oeffneMitFrist(std::unique_ptr<IAudioBus> bus,
                                          const AudioFormat& fmt,
                                          const QString& wofuer,
                                          int fristMs)
{
    if (!bus) { return nullptr; }

    auto zustand = std::make_shared<BusOeffnung>();
    zustand->bus = std::move(bus);

    // Abgehängt (`detach`) mit Absicht: dieser Faden kann beliebig lange
    // stehen, und ein QThread, auf den niemand mehr wartet, ließe sich nicht
    // aufräumen. Er hält den Zustand über den shared_ptr am Leben und räumt
    // den Bus selbst weg, wenn der Aufrufer schon weitergegangen ist. Je
    // Verbindungsaufbau kann das höchstens zweimal vorkommen (Lautsprecher
    // und Mikrofon).
    std::thread([zustand, fmt]() {
        IAudioBus* roh = nullptr;
        {
            std::lock_guard<std::mutex> sperre(zustand->mutex);
            roh = zustand->bus.get();
        }
        const bool ok = roh && roh->open(fmt);
        std::unique_ptr<IAudioBus> wegwerfen;
        {
            std::lock_guard<std::mutex> sperre(zustand->mutex);
            zustand->geglueckt = ok;
            zustand->fertig = true;
            if (zustand->aufgegeben) {
                // Niemand wartet mehr — selbst wegräumen. Der Destruktor
                // läuft bewusst AUSSERHALB der Sperre: close() kann seinerseits
                // dauern, und solange niemand mehr auf den Zustand schaut,
                // braucht er die Sperre nicht.
                wegwerfen = std::move(zustand->bus);
            }
        }
        zustand->wach.notify_all();
        wegwerfen.reset();
    }).detach();

    std::unique_lock<std::mutex> sperre(zustand->mutex);
    const bool rechtzeitig = zustand->wach.wait_for(
        sperre, std::chrono::milliseconds(fristMs),
        [&] { return zustand->fertig; });

    if (!rechtzeitig) {
        zustand->aufgegeben = true;
        qCWarning(lcAudio)
            << "[AudioStart:Step] ZEITLIMIT —" << wofuer << "hat in" << fristMs
            << "ms nicht geoeffnet; es geht OHNE dieses Geraet weiter. Der "
               "Oeffnungsversuch laeuft im Hintergrund aus und raeumt sich "
               "selbst auf.";
        return nullptr;
    }
    if (!zustand->geglueckt) {
        // Sauber gescheitert (kein Zeitlimit): hier gehört der Bus noch uns,
        // also lässt sich sein Fehlertext noch holen. Nach einem Zeitlimit
        // ginge das NICHT — dort gehört er dem Öffnungsfaden.
        qCWarning(lcAudio) << "IAudioBus open failed:" << wofuer << "—"
                           << (zustand->bus ? zustand->bus->errorString()
                                            : QStringLiteral("(kein Bus)"));
        return nullptr;
    }
    return std::move(zustand->bus);
}

}  // namespace Longpath::Audio
