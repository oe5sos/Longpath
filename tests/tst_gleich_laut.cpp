// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_gleich_laut.cpp  (Longpath)
// =================================================================
//
// Der Vergleich muss bei gleicher Lautstärke stattfinden.
//
// Am 2026-10-14, vier Vorfälle an einem Tag, zuletzt am Abend:
// „bei dx punchy usw. ist schon wieder das total laute übersteuern",
// „schon wieder bei fast allen", „muss sofort abdrehen und longpath
// zu machen".
//
// Es war keine kaputte Voreinstellung. Es ist der Bau der Sache:
// jede ordentliche Kette endet an der Decke des Begrenzers, das rohe
// Mikrofon liegt zwanzig bis vierzig Dezibel darunter. Also prüft
// dieser Prüfstand genau das, was sein Ohr erlebt hat -- erst roh,
// dann umgeschaltet -- und dazu die Gegenprobe: OHNE den Ausgleich
// muss derselbe Fall durchfallen, sonst prüft er nichts.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include "core/strip/LoudnessMatch.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

constexpr double kRate = 48000.0;

// Sprachähnlich: Silben mit Pausen, damit der Effektivwert nicht der
// eines Dauertons ist.
std::vector<float> rede(int frames, float amp)
{
    std::vector<float> v(size_t(frames), 0.0f);
    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / kRate;
        const double huelle = (std::fmod(t, 0.5) < 0.3) ? 1.0 : 0.0;
        v[size_t(i)] = float(amp * huelle
                             * (0.7 * std::sin(2.0 * M_PI * 180.0 * t)
                              + 0.3 * std::sin(2.0 * M_PI * 1300.0 * t)));
    }
    return v;
}

float effektivDb(const std::vector<float>& v, size_t ab)
{
    double s = 0.0;
    size_t n = 0;
    for (size_t i = ab; i < v.size(); ++i) { s += double(v[i]) * v[i]; ++n; }
    const double r = n > 0 ? std::sqrt(s / double(n)) : 0.0;
    return r > 1e-9 ? float(20.0 * std::log10(r)) : -120.0f;
}

} // namespace

class TstGleichLaut : public QObject
{
    Q_OBJECT
private slots:

    // Eine Kette, die 24 dB drauflegt, darf im Kopfhoerer nicht
    // 24 dB lauter sein.
    void eine_kette_wird_nicht_lauter_als_das_rohe_signal()
    {
        constexpr float kKette = 24.0f;      // was die Kette drauflegt
        const auto roh = rede(int(kRate * 6), 0.05f);

        LoudnessMatch m;
        m.prepare(kRate);
        m.setEnabled(true);

        std::vector<float> aus(roh.size(), 0.0f);
        constexpr int kBlock = 128;
        const float g = std::pow(10.0f, kKette / 20.0f);
        for (size_t at = 0; at + kBlock <= roh.size(); at += kBlock) {
            m.feedInput(roh.data() + at, kBlock);
            for (int i = 0; i < kBlock; ++i) {
                aus[at + size_t(i)] = roh[at + size_t(i)] * g;
            }
            m.applyOutput(aus.data() + at, kBlock);
        }

        // Die letzten zwei Sekunden beurteilen: davor darf er suchen.
        const size_t ab = size_t(kRate * 4);
        const float rohDb = effektivDb(roh, ab);
        const float ausDb = effektivDb(aus, ab);

        // Die Gegenprobe zuerst: OHNE Ausgleich waeren es die vollen
        // 24 dB. Ohne diese Zeile belegt der Fall nichts.
        QVERIFY2(std::fabs((rohDb + kKette) - effektivDb(
                     [&] {
                         std::vector<float> o(roh.size());
                         for (size_t i = 0; i < roh.size(); ++i) {
                             o[i] = roh[i] * g;
                         }
                         return o;
                     }(), ab)) < 0.1f,
                 "die Gegenprobe selbst stimmt nicht");

        QVERIFY2(std::fabs(ausDb - rohDb) < 1.5f,
                 qPrintable(QStringLiteral("roh %1 dBFS, durch die Kette "
                                           "%2 dBFS -- %3 dB Unterschied")
                                .arg(double(rohDb), 0, 'f', 1)
                                .arg(double(ausDb), 0, 'f', 1)
                                .arg(double(ausDb - rohDb), 0, 'f', 1)));
    }

    // Und das Wichtigste fuer seine Ohren: der Sprung beim
    // Umschalten darf nicht erst nach Sekunden eingefangen werden.
    void der_sprung_beim_umschalten_ist_nach_100_ms_vorbei()
    {
        const auto roh = rede(int(kRate * 4), 0.05f);
        LoudnessMatch m;
        m.prepare(kRate);
        m.setEnabled(true);

        constexpr int kBlock = 128;
        // Erst zwei Sekunden ohne Zusatz -- so steht es da, wenn er
        // roh hoert.
        std::vector<float> a(roh.begin(), roh.begin() + size_t(kRate * 2));
        for (size_t at = 0; at + kBlock <= a.size(); at += kBlock) {
            m.feedInput(a.data() + at, kBlock);
            m.applyOutput(a.data() + at, kBlock);
        }

        // Jetzt die Voreinstellung: schlagartig 24 dB mehr.
        const float g = std::pow(10.0f, 24.0f / 20.0f);
        std::vector<float> b(roh.begin() + size_t(kRate * 2), roh.end());
        std::vector<float> ein = b;
        for (size_t at = 0; at + kBlock <= b.size(); at += kBlock) {
            m.feedInput(b.data() + at, kBlock);   // Eingang bleibt roh
            for (int i = 0; i < kBlock; ++i) { ein[at + size_t(i)] *= g; }
            m.applyOutput(ein.data() + at, kBlock);
        }

        // ── Ab 30 ms, und warum nicht ab null ───────────────────
        //
        // Lautheit ohne Zeit gibt es nicht: die Messung braucht ihre
        // 20 ms, um den neuen Pegel ueberhaupt zu sehen. Was bleibt,
        // ist ein kurzer Anriss von rund zwei Hundertstelsekunden --
        // ein Knacken, kein Dauerpegel. Gefordert wird hier das, was
        // sein Ohr als „laut" erlebt: dass es nach 30 ms vorbei ist.
        const size_t vonMs = size_t(kRate * 0.03);
        const size_t bis   = size_t(kRate * 0.1);
        float spitze = 0.0f;
        for (size_t i = vonMs; i < bis && i < ein.size(); ++i) {
            spitze = std::max(spitze, std::fabs(ein[i]));
        }
        float rohSpitze = 0.0f;
        for (size_t i = vonMs; i < bis && i < b.size(); ++i) {
            rohSpitze = std::max(rohSpitze, std::fabs(b[i]));
        }
        const float mehrDb = 20.0f * std::log10(
            std::max(spitze, 1e-9f) / std::max(rohSpitze, 1e-9f));
        QVERIFY2(mehrDb < 8.0f,
                 qPrintable(QStringLiteral("in den ersten 100 ms stand es "
                                           "noch %1 dB über dem rohen Signal")
                                .arg(double(mehrDb), 0, 'f', 1)));
    }

    // ── Der Ausgleich darf nicht je Silbe pumpen ────────────────
    //
    // „weiters höre ich mich öfters immer ein bisschen abgehackt"
    // (2026-10-14). Bevor dafür fremde Einstellungen verdächtigt
    // werden, prüft dieser Fall den eigenen neuen Code: ein
    // Ausgleich, der in 50 ms fällt und bei mehr als 6 dB sofort
    // setzt, könnte Silbenanfänge ducken -- und das klingt genau so.
    //
    // Gemessen wird der Ausschlag der Verstärkung INNERHALB einer
    // halben Sekunde. Über Minuten darf sie wandern, das ist ihre
    // Aufgabe; was innerhalb eines Wortes passiert, hört man.
    void der_ausgleich_pumpt_nicht_je_silbe()
    {
        const auto roh = rede(int(kRate * 8), 0.05f);
        LoudnessMatch m;
        m.prepare(kRate);
        m.setEnabled(true);

        // Eine Kette mit fester Verstärkung: was der Ausgleich hier
        // noch bewegt, bewegt er aus eigenem Antrieb.
        const float g = std::pow(10.0f, 20.0f / 20.0f);
        constexpr int kBlock = 128;
        std::vector<float> spur;              // gainDb je Block
        std::vector<float> aus = roh;
        for (size_t at = 0; at + kBlock <= roh.size(); at += kBlock) {
            m.feedInput(roh.data() + at, kBlock);
            for (int i = 0; i < kBlock; ++i) { aus[at + size_t(i)] *= g; }
            m.applyOutput(aus.data() + at, kBlock);
            spur.push_back(m.gainDb());
        }

        // Die ersten zwei Sekunden sind das Einschwingen.
        const size_t ab = size_t(2.0 * kRate / kBlock);
        const size_t fenster = size_t(0.5 * kRate / kBlock);
        float groessterHub = 0.0f;
        for (size_t i = ab; i + fenster < spur.size(); ++i) {
            float lo = spur[i], hi = spur[i];
            for (size_t k = i; k < i + fenster; ++k) {
                lo = std::min(lo, spur[k]);
                hi = std::max(hi, spur[k]);
            }
            groessterHub = std::max(groessterHub, hi - lo);
        }
        QVERIFY2(groessterHub < 1.5f,
                 qPrintable(QStringLiteral("der Ausgleich bewegt sich um "
                                           "%1 dB innerhalb einer halben "
                                           "Sekunde -- das pumpt hörbar")
                                .arg(double(groessterHub), 0, 'f', 2)));
        qInfo().noquote() << QStringLiteral("   größter Hub in 0,5 s: %1 dB")
            .arg(double(groessterHub), 0, 'f', 2);
    }

    // Ausgeschaltet heisst ausgeschaltet.
    void ausgeschaltet_wird_nichts_angefasst()
    {
        auto roh = rede(int(kRate * 2), 0.05f);
        const auto vorher = roh;
        LoudnessMatch m;
        m.prepare(kRate);
        m.setEnabled(false);
        constexpr int kBlock = 128;
        for (size_t at = 0; at + kBlock <= roh.size(); at += kBlock) {
            m.feedInput(roh.data() + at, kBlock);
            m.applyOutput(roh.data() + at, kBlock);
        }
        // Die Zahl laeuft nach 0 dB, also bleibt das Signal gleich.
        QVERIFY2(std::fabs(effektivDb(roh, size_t(kRate))
                           - effektivDb(vorher, size_t(kRate))) < 0.1f,
                 "der ausgeschaltete Ausgleich hat trotzdem geregelt");
    }

    // In der Stille wird nicht geregelt -- sonst zieht er das
    // Grundgeraeusch hoch, und genau das war der Fehler vom Vormittag.
    void in_der_stille_bewegt_sich_nichts()
    {
        LoudnessMatch m;
        m.prepare(kRate);
        m.setEnabled(true);
        // -80 dBFS Rauschen, ein und aus.
        std::vector<float> leise(size_t(kRate * 3), 0.0f);
        for (size_t i = 0; i < leise.size(); ++i) {
            leise[i] = 1.0e-4f * float(std::sin(double(i) * 0.1));
        }
        std::vector<float> aus = leise;
        constexpr int kBlock = 128;
        for (size_t at = 0; at + kBlock <= leise.size(); at += kBlock) {
            m.feedInput(leise.data() + at, kBlock);
            m.applyOutput(aus.data() + at, kBlock);
        }
        QVERIFY2(std::fabs(m.gainDb()) < 0.5f,
                 qPrintable(QStringLiteral("in der Stille auf %1 dB gelaufen")
                                .arg(double(m.gainDb()), 0, 'f', 1)));
    }
};

QTEST_APPLESS_MAIN(TstGleichLaut)
#include "tst_gleich_laut.moc"
