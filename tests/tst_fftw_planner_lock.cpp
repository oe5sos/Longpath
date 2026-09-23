// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_fftw_planner_lock.cpp  (Longpath)
// =================================================================
//
// FFTWs Planer ist nicht threadsicher — je Genauigkeit einer, beide
// prozessweit. Bis zum 2026-09-22 hatte Longpath gar keine Sperre
// darum: FFTEngine (ein Panadapter je Scheibe, jeder auf seinem Faden),
// WidebandFftEngine, RxChannels Filterbild und WDSP (dessen
// WDSPwisdom() hier auf einem eigenen Faden laeuft) planten frei
// nebeneinander. In den Quellen stand sogar, FFTW_ESTIMATE weiche „dem
// globalen FFTW-Mutex" aus — das tut es nicht.
//
// Dieser Prüfstand braucht weder Netz noch Geraet:
//   1. Die Sperren sind prozessweit und je Genauigkeit EINE — zwei
//      Mutexe ueber einem Planer serialisieren nichts.
//   2. Sie sperren wirklich (ein zweiter Faden kommt erst hinein,
//      wenn der erste losgelassen hat).
//   3. Unter der Sperre halten viele Faeden gleichzeitiges Planen,
//      Belegen und Freigeben aus — genau das Muster, das der
//      Panadapter beim Groessenwechsel faehrt.
// =================================================================

#include <QtTest/QtTest>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "core/dsp/FftwPlannerLock.h"

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

using namespace Longpath;

class TstFftwPlannerLock : public QObject { Q_OBJECT
private slots:

    void oneMutexPerPrecision()
    {
        QVERIFY2(&fftwPlannerMutex() == &fftwPlannerMutex(),
                 "Die Sperre der doppelten Genauigkeit ist nicht prozessweit dieselbe");
        QVERIFY2(&fftwfPlannerMutex() == &fftwfPlannerMutex(),
                 "Die Sperre der einfachen Genauigkeit ist nicht prozessweit dieselbe");
        QVERIFY2(&fftwPlannerMutex() != reinterpret_cast<std::mutex*>(&fftwfPlannerMutex()),
                 "Beide Genauigkeiten teilen sich eine Sperre — sie haben getrennte Planer");
    }

    void theLockActuallyExcludes()
    {
        std::atomic<bool> secondGotIn{false};
        auto held = fftwPlannerLock();
        std::thread t([&]() {
            auto lock = fftwPlannerLock();
            secondGotIn.store(true);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        QVERIFY2(!secondGotIn.load(),
                 "Ein zweiter Faden kam in die Sperre, waehrend sie gehalten wurde");
        held.unlock();
        t.join();
        QVERIFY(secondGotIn.load());
    }

    void manyThreadsPlanAndFreeUnderTheLock()
    {
#ifndef HAVE_FFTW3
        QSKIP("ohne FFTW3 gebaut");
#else
        // Vier Faeden, die tun, was der Panadapter beim Groessenwechsel
        // tut: belegen, planen, verwerfen, freigeben. Ohne Sperre ist das
        // undefiniert; mit ihr muss es einfach durchlaufen.
        constexpr int kThreads = 4;
        constexpr int kRounds  = 12;
        std::vector<std::thread> threads;
        std::atomic<int> done{0};
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&, t]() {
                for (int r = 0; r < kRounds; ++r) {
                    const int n = 1024 << (r % 3);
                    if (t % 2 == 0) {
                        auto lock = fftwfPlannerLock();
                        auto* in  = fftwf_alloc_complex(n);
                        auto* out = fftwf_alloc_complex(n);
                        fftwf_plan p = fftwf_plan_dft_1d(n, in, out,
                                                         FFTW_FORWARD, FFTW_ESTIMATE);
                        fftwf_destroy_plan(p);
                        fftwf_free(in);
                        fftwf_free(out);
                    } else {
                        auto lock = fftwPlannerLock();
                        auto* in  = fftw_alloc_complex(n);
                        auto* out = fftw_alloc_complex(n);
                        fftw_plan p = fftw_plan_dft_1d(n, in, out,
                                                       FFTW_FORWARD, FFTW_ESTIMATE);
                        fftw_destroy_plan(p);
                        fftw_free(in);
                        fftw_free(out);
                    }
                }
                ++done;
            });
        }
        for (auto& th : threads) { th.join(); }
        QCOMPARE(done.load(), kThreads);
#endif
    }
};

QTEST_MAIN(TstFftwPlannerLock)
#include "tst_fftw_planner_lock.moc"
