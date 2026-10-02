// no-port-check: Longpath-eigener Prüfstand. Thetis hat diesen Fehler nicht,
// weil es einen eigenen, kanalweisen Resampler mitbringt; hier wird WDSPs
// RESAMPLEF benutzt, und der kennt keine Kanäle.
//
// ── Der Fehler ───────────────────────────────────────────────────────────────
//
// TciServer::drainAudio übergab den VERSCHRÄNKTEN Stereopuffer (L,R,L,R,…) als
// EINEN Strom an xresampleFV. WDSPs xresampleF (third_party/wdsp/src/resample.c)
// rechnet aber einkanalig und reell: ein Ringpuffer, ein Wert je Abtastung,
// kein Kanalbegriff. Folge bei jeder Rate ausser 48000 (nur dort überspringt
// der Code den Umtaster ganz):
//
//   1. L und R laufen durch denselben FIR und vermischen sich.
//   2. Die Tonhöhe stimmt nicht: der Resampler sieht doppelt so viele Werte
//      wie es Abtastungen gibt, rechnet also faktisch von 96 kHz herunter.
//
// Betrifft jeden Stereo-Client, der eine andere Rate verlangt — die Handfunke
// (12 kHz wären der billige Ton fürs Telefon), aber auch fremde Clients.
//
// Dieser Prüfstand zeigt das an WDSP selbst, ohne Server: zwei Kanäle, die
// sich nicht ähnlicher sein könnten als +1 und −1.

#include <QtTest/QtTest>
#include <vector>
#include <cmath>

extern "C" {
void* create_resampleFV(int in_rate, int out_rate);
void  xresampleFV(float* input, float* output, int numsamps, int* outsamps, void* ptr);
void  destroy_resampleFV(void* ptr);
}

class TestTciAudioResampleChannels : public QObject {
    Q_OBJECT

private:
    // Das, was der Server tun MUSS: je Kanal ein eigener Resampler.
    static void resampleStereo(const float* verschraenkt, int rahmen,
                               void* resL, void* resR,
                               std::vector<float>& aus) {
        std::vector<float> l(rahmen), r(rahmen);
        for (int i = 0; i < rahmen; ++i) { l[i] = verschraenkt[i*2]; r[i] = verschraenkt[i*2+1]; }
        std::vector<float> lo(rahmen * 8), ro(rahmen * 8);
        int nl = 0, nr = 0;
        xresampleFV(l.data(), lo.data(), rahmen, &nl, resL);
        xresampleFV(r.data(), ro.data(), rahmen, &nr, resR);
        const int n = std::min(nl, nr);
        aus.resize(n * 2);
        for (int i = 0; i < n; ++i) { aus[i*2] = lo[i]; aus[i*2+1] = ro[i]; }
    }

private slots:

    void einkanaliger_resampler_vermischt_verschraenktes_stereo() {
        // L konstant +1, R konstant −1. Zwei Kanäle, die sich weniger ähnlich
        // nicht sein könnten. Bleiben sie getrennt, muss das auch am Ausgang
        // so aussehen.
        const int rahmen = 4096;
        std::vector<float> ein(rahmen * 2);
        for (int i = 0; i < rahmen; ++i) { ein[i*2] = 1.0f; ein[i*2+1] = -1.0f; }

        void* res = create_resampleFV(48000, 12000);
        QVERIFY(res != nullptr);

        std::vector<float> aus(rahmen * 8);
        int n = 0;
        xresampleFV(ein.data(), aus.data(), rahmen * 2, &n, res);   // DER FEHLER
        QVERIFY(n > 0);

        // Der eingeschwungene Teil: die ersten Werte tragen noch die
        // Filtereinschwingung, die interessiert hier nicht.
        double summeBetrag = 0.0;
        int gezaehlt = 0;
        for (int i = n / 2; i < n; ++i) { summeBetrag += std::fabs(aus[i]); ++gezaehlt; }
        const double mittel = summeBetrag / std::max(1, gezaehlt);

        // Getrennt gerechnet müsste jeder Wert bei ±1 liegen. Durch EINEN
        // reellen FIR gejagt heben sich +1 und −1 gegenseitig auf.
        QVERIFY2(mittel < 0.2,
                 qPrintable(QStringLiteral(
                     "Erwartet: der einkanalige Resampler mischt L und R zu nahe 0 "
                     "(Mittel |x| = %1). Ist dieser Prüfpunkt rot, rechnet WDSP "
                     "plötzlich kanalweise — dann ist der Fix im Server neu zu "
                     "bewerten.").arg(mittel)));

        destroy_resampleFV(res);
    }

    void je_kanal_ein_resampler_haelt_sie_getrennt() {
        const int rahmen = 4096;
        std::vector<float> ein(rahmen * 2);
        for (int i = 0; i < rahmen; ++i) { ein[i*2] = 1.0f; ein[i*2+1] = -1.0f; }

        void* resL = create_resampleFV(48000, 12000);
        void* resR = create_resampleFV(48000, 12000);
        QVERIFY(resL && resR);

        std::vector<float> aus;
        resampleStereo(ein.data(), rahmen, resL, resR, aus);
        QVERIFY(aus.size() >= 64);

        const int rahmenAus = static_cast<int>(aus.size()) / 2;
        double mL = 0.0, mR = 0.0;
        int z = 0;
        for (int i = rahmenAus / 2; i < rahmenAus; ++i) { mL += aus[i*2]; mR += aus[i*2+1]; ++z; }
        mL /= std::max(1, z); mR /= std::max(1, z);

        QVERIFY2(mL > 0.9, qPrintable(QStringLiteral("L muss bei +1 bleiben, ist %1").arg(mL)));
        QVERIFY2(mR < -0.9, qPrintable(QStringLiteral("R muss bei −1 bleiben, ist %1").arg(mR)));

        destroy_resampleFV(resL);
        destroy_resampleFV(resR);
    }

    void kanalweise_rechnung_trifft_die_richtige_laenge() {
        // Der zweite Teil des Fehlers: mit verschränktem Puffer sieht der
        // Resampler doppelt so viele Werte wie es Abtastungen gibt und rechnet
        // faktisch von 96 kHz herunter — der Ton wäre eine Oktave daneben.
        const int rahmen = 4800;                 // 100 ms bei 48 kHz
        std::vector<float> ein(rahmen * 2, 0.0f);

        void* resL = create_resampleFV(48000, 12000);
        void* resR = create_resampleFV(48000, 12000);
        std::vector<float> aus;
        resampleStereo(ein.data(), rahmen, resL, resR, aus);

        // 48000 -> 12000 ist Faktor 4: aus 4800 Rahmen werden rund 1200.
        const int rahmenAus = static_cast<int>(aus.size()) / 2;
        QVERIFY2(std::abs(rahmenAus - 1200) <= 8,
                 qPrintable(QStringLiteral("Erwartet rund 1200 Rahmen, sind %1").arg(rahmenAus)));

        destroy_resampleFV(resL);
        destroy_resampleFV(resR);
    }

    void ein_ton_bleibt_ein_ton() {
        // Schärfer als konstante Pegel: ein 1-kHz-Ton links, Stille rechts.
        // Vermischen sich die Kanäle, leckt der Ton nach rechts.
        const int rahmen = 9600;
        std::vector<float> ein(rahmen * 2);
        for (int i = 0; i < rahmen; ++i) {
            ein[i*2]     = static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * i / 48000.0));
            ein[i*2 + 1] = 0.0f;
        }

        void* resL = create_resampleFV(48000, 12000);
        void* resR = create_resampleFV(48000, 12000);
        std::vector<float> aus;
        resampleStereo(ein.data(), rahmen, resL, resR, aus);

        const int rahmenAus = static_cast<int>(aus.size()) / 2;
        QVERIFY(rahmenAus > 100);
        double leistungL = 0.0, leistungR = 0.0;
        int z = 0;
        for (int i = rahmenAus / 2; i < rahmenAus; ++i) {
            leistungL += double(aus[i*2]) * aus[i*2];
            leistungR += double(aus[i*2+1]) * aus[i*2+1];
            ++z;
        }
        leistungL /= std::max(1, z); leistungR /= std::max(1, z);

        QVERIFY2(leistungL > 0.4, "Links muss der Ton ankommen");
        QVERIFY2(leistungR < 1e-6,
                 qPrintable(QStringLiteral("Rechts muss still bleiben, Leistung ist %1")
                                .arg(leistungR)));

        destroy_resampleFV(resL);
        destroy_resampleFV(resR);
    }
};

QTEST_MAIN(TestTciAudioResampleChannels)
#include "tst_tci_audio_resample_channels.moc"
