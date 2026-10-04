#include "core/audio/BusMitFrist.h"

#include "core/LogCategories.h"

#include <atomic>
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

// Steht ein aufgegebener Oeffnungsversuch noch im Hintergrund?
//
// Das ist kein Schoenheitszaehler, sondern eine Sicherung. `Pa_OpenStream`
// darf nicht von zwei Faeden GLEICHZEITIG laufen; PortAudio sichert seine
// Geraeteverwaltung nicht gegen Nebenlaeufigkeit ab. Ohne diese Sperre waere
// genau das die Folge der Frist: der erste Versuch haengt noch in
// Pa_OpenStream, der Hauptfaden geht weiter zum naechsten Geraet -- und
// oeffnet ein zweites Mal, parallel zum ersten. Die Frist haette den Haenger
// gegen etwas Schlimmeres eingetauscht.
//
// Also: haengt noch einer, wird gar nicht erst ein zweiter gestartet. Das ist
// auch sachlich richtig. Haengt die Geraeteverwaltung, wuerde das naechste
// Geraet ohnehin dahinter warten; es sofort zu ueberspringen kostet nichts
// und spart je Geraet eine volle Frist (bei sieben Geraeten waeren das sonst
// 35 s statt 5).
std::atomic<bool> g_einVersuchHaengt{false};

}  // namespace

std::unique_ptr<IAudioBus> oeffneMitFrist(std::unique_ptr<IAudioBus> bus,
                                          const AudioFormat& fmt,
                                          const QString& wofuer,
                                          int fristMs)
{
    if (!bus) { return nullptr; }

    if (g_einVersuchHaengt.load(std::memory_order_acquire)) {
        qCWarning(lcAudio)
            << "[AudioStart:Step] UEBERSPRUNGEN —" << wofuer
            << ": ein frueherer Oeffnungsversuch haengt noch. Ein zweiter "
               "gleichzeitiger Pa_OpenStream ist nicht zulaessig, und hinter "
               "dem haengenden wuerde dieser ohnehin warten.";
        return nullptr;
    }

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
        bool warAufgegeben = false;
        {
            std::lock_guard<std::mutex> sperre(zustand->mutex);
            zustand->geglueckt = ok;
            zustand->fertig = true;
            warAufgegeben = zustand->aufgegeben;
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
        // Nur freigeben, wenn WIR der aufgegebene Versuch waren. Ein Faden,
        // der rechtzeitig fertig wurde, hat die Sperre nie gesetzt und darf
        // sie auch nicht loeschen.
        //
        // Die Reihenfolge ist Absicht: erst aufraeumen, dann freigeben. Sonst
        // koennte der naechste Oeffnungsversuch starten, waehrend der alte
        // Bus noch im Destruktor von close() haengt -- und dann waeren wieder
        // zwei in der Geraeteverwaltung.
        if (warAufgegeben) {
            g_einVersuchHaengt.store(false, std::memory_order_release);
        }
    }).detach();

    std::unique_lock<std::mutex> sperre(zustand->mutex);
    const bool rechtzeitig = zustand->wach.wait_for(
        sperre, std::chrono::milliseconds(fristMs),
        [&] { return zustand->fertig; });

    if (!rechtzeitig) {
        zustand->aufgegeben = true;
        g_einVersuchHaengt.store(true, std::memory_order_release);
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
