// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_bandbreiten_genau.cpp  (Longpath)
// =================================================================
// „bitte kontrolliere genau die bandbreiten! mir kommt vor, dass
//  diese nicht stimmen, 4k hört sich eher an wie 2700k, nur roh,
//  also volle bandbreite hört sich richtig gut an."
//
// Zwei Fragen, die man nicht zusammen beantworten kann:
//
//   1. Sitzen die Ecken der vier Kanäle dort, wo die Aufschrift es
//      behauptet? Das ist eine Frage an BandLimit, und sie wird mit
//      reinen Tönen gemessen -- keine Filterrechnung auf der
//      Prüfstandseite, nur Sinus hinein und Effektivwert heraus.
//      Ein Prüfstand, der mit derselben Formel misst, mit der das
//      Gerät rechnet, bestätigt nur sich selbst.
//
//   2. Kommt oben überhaupt noch etwas an, wenn der Kanal offen
//      ist? Das ist eine Frage an die KETTE, und sie wird mit einer
//      nachgebauten Stimme und einer eigenen Fourier-Zerlegung
//      gemessen. „4 k klingt wie 2700" kann nämlich auch heissen,
//      dass der Kanal richtig öffnet und die Kette nichts
//      hineinschickt.
//
// Beide Antworten stehen im Protokoll, mit Zahlen. Geprüft wird
// nur, was ohne Zweifel falsch wäre -- eine Schwelle, die der erste
// Lauf reisst, prüft nichts.
//
// Modification history (Longpath):
//   2026-10-10 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/strip/AetherPreset.h"
#include "core/strip/BandLimit.h"
#include "core/strip/ClientDeEss.h"
#include "core/strip/ClientEq.h"
#include "core/Loudness.h"
#include "core/strip/ClientFinalLimiter.h"
#include "core/strip/StripLiveMonitor.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr double kRate = 48000.0;

double dB(double v) { return 20.0 * std::log10(std::max(v, 1e-12)); }

// ── Das erste Messgerät: reiner Ton, Effektivwert ────────────────────
//
// Die Verstärkung eines linearen Filters bei einer Frequenz ist das
// Verhältnis der Effektivwerte, nichts weiter. Kein Fenster, keine
// Zerlegung, keine Annahme -- und damit nichts, was mit BandLimit
// denselben Fehler teilen könnte.
//
// Das Messfenster wird auf eine ganze Zahl von Perioden gelegt. Ohne
// das hat der Effektivwert bei 50 Hz einen Rest von rund 0,07 dB aus
// der angeschnittenen letzten Periode -- bei einer -3-dB-Suche ist
// das die Stelle hinter dem Komma, um die es geht.
double kanalDb(int kanal, double hz)
{
    BandLimit b;
    b.prepare(kRate);
    b.setChannel(kanal);

    const int perioden = std::max(8, int(0.40 * hz));
    const int mess     = int(std::lround(perioden * kRate / hz));
    const int ruhe     = int(kRate * 0.8);      // Einschwingen der Kaskade

    std::vector<float> x(size_t(ruhe + mess));
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = float(std::sin(2.0 * M_PI * hz * double(i) / kRate));
    }
    for (size_t at = 0; at + 240 <= x.size(); at += 240) {
        b.process(x.data() + at, 240);
    }

    double s = 0.0;
    for (int i = 0; i < mess; ++i) {
        const double v = double(x[size_t(ruhe + i)]);
        s += v * v;
    }
    // Hinein ging Amplitude 1, also Effektivwert 1/√2.
    return 10.0 * std::log10(std::max(s / mess, 1e-14) / 0.5);
}

// Die Frequenz, bei der der Kanal um `zielDb` abfällt. Halbierung,
// weil die Flanke in ihrem Bereich streng einsinnig ist.
double ecke(int kanal, double von, double bis, double zielDb, bool steigend)
{
    for (int i = 0; i < 26; ++i) {
        const double m = std::sqrt(von * bis);            // geometrische Mitte
        const bool drunter = kanalDb(kanal, m) < zielDb;
        if (drunter == steigend) { von = m; } else { bis = m; }
    }
    return std::sqrt(von * bis);
}

// ── Das zweite Messgerät: eigene Fourier-Zerlegung ───────────────────
//
// Radix-2, iterativ, hier im Prüfstand und nicht aus dem Programm.
// Damit lassen sich Bandenergien einer Stimme absolut vergleichen,
// und zwar ohne das LTAS des Programms zu benutzen -- das bezieht
// sich auf seinen eigenen Mittelwert und kann eine Kippe, die die
// GANZE Stimme betrifft, grundsätzlich nicht sehen.
void fft(std::vector<double>& re, std::vector<double>& im)
{
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) { j ^= bit; }
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / double(len);
        const size_t h = len / 2;
        for (size_t i = 0; i < n; i += len) {
            for (size_t k = 0; k < h; ++k) {
                const double c = std::cos(ang * double(k));
                const double s = std::sin(ang * double(k));
                const double ur = re[i + k];
                const double ui = im[i + k];
                const double vr = re[i + k + h] * c - im[i + k + h] * s;
                const double vi = re[i + k + h] * s + im[i + k + h] * c;
                re[i + k]     = ur + vr;
                im[i + k]     = ui + vi;
                re[i + k + h] = ur - vr;
                im[i + k + h] = ui - vi;
            }
        }
    }
}

// Das Betragsquadrat über alle Teilbänder eines Signals, Hann
// gefenstert, 2^18 Werte (5,5 s bei 48 kHz).
std::vector<double> spektrum(const std::vector<float>& v)
{
    constexpr size_t kN = 1u << 18;
    std::vector<double> re(kN, 0.0);
    std::vector<double> im(kN, 0.0);
    const size_t n = std::min(kN, v.size());
    for (size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(i)
                                              / double(n - 1));
        re[i] = double(v[i]) * w;
    }
    fft(re, im);
    std::vector<double> p(kN / 2, 0.0);
    for (size_t k = 0; k < kN / 2; ++k) {
        p[k] = re[k] * re[k] + im[k] * im[k];
    }
    return p;
}

// Energie zwischen zwei Frequenzen, in dB. Absolut, nicht bezogen.
double bandDb(const std::vector<double>& p, double vonHz, double bisHz)
{
    const double binHz = kRate / double(p.size() * 2);
    const size_t a = size_t(std::max(0.0, std::floor(vonHz / binHz)));
    const size_t b = std::min(p.size(), size_t(std::ceil(bisHz / binHz)));
    double s = 0.0;
    for (size_t k = a; k < b; ++k) { s += p[k]; }
    return 10.0 * std::log10(std::max(s, 1e-20));
}

// ── Die nachgebaute Stimme, diesmal mit Zischlaut ────────────────────
//
// Der vorige Messlauf hat den De-Esser mit 0,0 dB überall gemessen
// und es offen gelassen: Fehler oder Prüfsignal? Das Signal hatte
// oberhalb von 4 kHz praktisch nichts, und ein De-Esser, der nichts
// findet, tut richtig nichts. Also bekommt die Stimme hier ein
// „ss" -- schmalbandiges Rauschen um 6,5 kHz, am Ende jeder Silbe.
std::vector<float> stimme(double sekunden, bool mitZischen,
                          double rauschDbFs = -48.0)
{
    const int n = int(sekunden * kRate);
    std::vector<float> v(size_t(n), 0.0f);

    // ── Stimmbänder ──────────────────────────────────────────────
    //
    // Ein Pulszug bei 110 Hz, aber nicht der reine: zwei Dinge, die
    // der erste Lauf dieses Prüfstands gelehrt hat.
    //
    //   * Die Tonhöhe schwankt. Ein exakt periodischer Pulszug hat
    //     ein KAMMSPEKTRUM -- Linien bei 110, 220, 330 Hz und
    //     dazwischen nichts. Das Drittelband um 250 Hz (224..282 Hz)
    //     liegt dann in einer Lücke, und was dort gemessen wird, ist
    //     nicht die Stimme, sondern wieviel Verzerrung die Kette in
    //     die Lücke hineinlegt. Genau so kam im ersten Lauf ein
    //     „+16,7 dB bei 250 Hz" heraus. Drei Prozent Schwankung mit
    //     4,3 Hz schmieren die Linien zu Bändern -- und so klingt
    //     eine Stimme ohnehin.
    //
    //   * Die Quelle fällt ab. Ein Rechteckpuls ist bis zur
    //     Nyquistgrenze gleich laut; eine Stimmlippe fällt mit rund
    //     12 dB je Oktave. Ohne diesen Abfall hat das Prüfsignal bei
    //     6 kHz so viel Energie wie bei 300, und ein De-Esser findet
    //     dort nie einen Zischlaut, weil alles zischt.
    double phase = 0.0;
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / kRate;
        const double f0 = 110.0 * (1.0 + 0.03 * std::sin(2.0 * M_PI * 4.3 * t));
        phase += f0 / kRate;
        if (phase >= 1.0) { phase -= 1.0; v[size_t(i)] = 1.0f; }
    }
    {
        // Zwei einpolige Tiefpässe bei 300 Hz: -12 dB je Oktave.
        const double a = std::exp(-2.0 * M_PI * 300.0 / kRate);
        for (int durchgang = 0; durchgang < 2; ++durchgang) {
            double z = 0.0;
            for (int i = 0; i < n; ++i) {
                z = (1.0 - a) * double(v[size_t(i)]) + a * z;
                v[size_t(i)] = float(z);
            }
        }
    }

    // Drei Formanten als Resonatoren.
    const double formant[3] = { 500.0, 1500.0, 2600.0 };
    const double guete[3]   = { 6.0, 8.0, 10.0 };
    std::vector<float> stimmhaft(size_t(n), 0.0f);
    for (int f = 0; f < 3; ++f) {
        const double w = 2.0 * M_PI * formant[f] / kRate;
        const double a = std::exp(-w / (2.0 * guete[f]));
        const double c1 = 2.0 * a * std::cos(w);
        const double c2 = -a * a;
        double z1 = 0.0;
        double z2 = 0.0;
        for (int i = 0; i < n; ++i) {
            const double y = double(v[size_t(i)]) + c1 * z1 + c2 * z2;
            stimmhaft[size_t(i)] += float(y * (1.0 / (f + 1)));
            z2 = z1; z1 = y;
        }
    }

    // Zischlaut: Rauschen durch einen schmalen Bandpass bei 6,5 kHz.
    std::vector<float> zischen(size_t(n), 0.0f);
    if (mitZischen) {
        std::mt19937 wuerfel(20261010u);
        std::normal_distribution<double> glocke(0.0, 1.0);
        const double w = 2.0 * M_PI * 6500.0 / kRate;
        const double a = std::exp(-w / (2.0 * 3.0));
        const double c1 = 2.0 * a * std::cos(w);
        const double c2 = -a * a;
        double z1 = 0.0;
        double z2 = 0.0;
        for (int i = 0; i < n; ++i) {
            const double y = glocke(wuerfel) + c1 * z1 + c2 * z2;
            zischen[size_t(i)] = float(y);
            z2 = z1; z1 = y;
        }
    }

    // Beide Teile auf Spitze 1 bringen, EHE sie gemischt werden. Nur
    // so ist das Mischungsverhältnis die Zahl, die im Quelltext
    // steht: der Abfall der Quelle hat den stimmhaften Teil um fast
    // 30 dB kleiner gemacht, und ein fester Faktor für das Zischen
    // wäre danach kein „neun Dezibel darunter" mehr, sondern alles.
    const auto aufEins = [](std::vector<float>& s) {
        double p = 0.0;
        for (float x : s) { p = std::max(p, double(std::fabs(x))); }
        if (p > 1e-12) {
            const float g = float(1.0 / p);
            for (float& x : s) { x *= g; }
        }
    };
    aufEins(stimmhaft);
    if (mitZischen) { aufEins(zischen); }

    // Silben: 0,0..0,55 stimmhaft mit Glocke, 0,45..0,62 das „ss"
    // neun Dezibel darunter, danach Pause. Ein Takt je Sekunde.
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / kRate;
        const double imTakt = std::fmod(t, 1.0);
        double hv = 0.0;
        if (imTakt < 0.55) {
            hv = std::sin(M_PI * imTakt / 0.55);
            hv *= hv;
        }
        double hz = 0.0;
        if (mitZischen && imTakt > 0.45 && imTakt < 0.62) {
            hz = std::sin(M_PI * (imTakt - 0.45) / 0.17);
        }
        v[size_t(i)] = float(double(stimmhaft[size_t(i)]) * hv
                             + double(zischen[size_t(i)]) * hz * 0.35);
    }

    // Grundgeräusch des Raumes.
    std::mt19937 wuerfel(4711u);
    std::normal_distribution<double> glocke(0.0, 1.0);
    const double rausch = std::pow(10.0, rauschDbFs / 20.0);
    for (int i = 0; i < n; ++i) {
        v[size_t(i)] += float(glocke(wuerfel) * rausch);
    }

    // Auf -12 dBFS Spitze. Jede Schwelle in der Kette steht in dBFS;
    // ein Prüfsignal mit falschem Pegel prüft eine andere Kette.
    double p = 0.0;
    for (float s : v) { p = std::max(p, double(std::fabs(s))); }
    const double ziel = std::pow(10.0, -12.0 / 20.0);
    if (p > 1e-9) {
        const float g = float(ziel / p);
        for (float& s : v) { s *= g; }
    }
    return v;
}

// ── Echte Sprache, weil ein Netz kein Nachbau akzeptiert ─────────────
//
// rnnoise ist auf gesprochenen Aufnahmen trainiert. Es hat die
// nachgebaute Stimme dieses Prüfstands mit einer Wahrscheinlichkeit
// von 0,13 für Sprache gehalten -- und darum entrauscht es sie nicht,
// es drückt sie weg. Jede Messreihe über „wieviel kostet der Anteil"
// an so einem Signal misst deshalb nicht den Entrauscher.
//
// Also wird hier wirkliche Sprache besorgt, mit dem, was auf jedem
// Mac vorhanden ist: der Sprachausgabe des Systems. Kein Download,
// keine Aufnahme vom Mikrofon des Betreibers, und jedes Mal
// dasselbe Ergebnis. Fehlt das Werkzeug, wird der Fall übersprungen
// und sagt das auch -- eine Messung, die ohne ihr Prüfsignal
// „bestanden" meldet, ist schlimmer als keine.
std::vector<float> wavLesen(const QString& pfad)
{
    QFile f(pfad);
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    const QByteArray b = f.readAll();
    if (b.size() < 44 || !b.startsWith("RIFF")) { return {}; }

    int kanaele = 0;
    int rate = 0;
    int bits = 0;
    qsizetype at = 12;
    qsizetype datenAb = -1;
    qsizetype datenLen = 0;
    const auto u32 = [&b](qsizetype i) {
        return quint32(quint8(b[i])) | (quint32(quint8(b[i + 1])) << 8)
             | (quint32(quint8(b[i + 2])) << 16)
             | (quint32(quint8(b[i + 3])) << 24);
    };
    const auto u16 = [&b](qsizetype i) {
        return quint16(quint8(b[i])) | (quint16(quint8(b[i + 1])) << 8);
    };
    while (at + 8 <= b.size()) {
        const QByteArray id = b.mid(at, 4);
        const qsizetype len = qsizetype(u32(at + 4));
        if (id == "fmt " && at + 24 <= b.size()) {
            kanaele = u16(at + 10);
            rate    = int(u32(at + 12));
            bits    = u16(at + 22);
        } else if (id == "data") {
            datenAb = at + 8;
            datenLen = std::min(len, b.size() - datenAb);
        }
        at += 8 + len + (len & 1);
    }
    if (datenAb < 0 || kanaele != 1 || bits != 16
        || std::abs(rate - int(kRate)) > 1) {
        return {};
    }
    std::vector<float> v(size_t(datenLen / 2), 0.0f);
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = float(qint16(u16(datenAb + qsizetype(i) * 2))) / 32768.0f;
    }
    return v;
}

std::vector<float> echteRede(QString* warum)
{
    static std::vector<float> zwischengespeichert;
    static bool versucht = false;
    static QString grund;
    if (versucht) {
        if (warum != nullptr) { *warum = grund; }
        return zwischengespeichert;
    }
    versucht = true;

    const QString say = QStandardPaths::findExecutable(QStringLiteral("say"));
    const QString konv = QStandardPaths::findExecutable(
        QStringLiteral("afconvert"));
    if (say.isEmpty() || konv.isEmpty()) {
        grund = QStringLiteral("say/afconvert nicht vorhanden -- echte "
                               "Sprache lässt sich hier nicht erzeugen");
        if (warum != nullptr) { *warum = grund; }
        return {};
    }

    static QTemporaryDir ordner;
    const QString aiff = ordner.filePath(QStringLiteral("rede.aiff"));
    const QString wav  = ordner.filePath(QStringLiteral("rede.wav"));

    QProcess p1;
    p1.start(say, { QStringLiteral("-v"), QStringLiteral("Anna"),
                    QStringLiteral("-o"), aiff,
                    QStringLiteral("Hallo, hier ist Oskar Echo Fünf Sierra "
                                   "Oskar Sierra. Die Sonne scheint seit "
                                   "sechs Stunden. Wie ist mein Signal bei "
                                   "dir? Sechsundsechzig sechzig.") });
    p1.waitForFinished(30000);
    QProcess p2;
    p2.start(konv, { QStringLiteral("-f"), QStringLiteral("WAVE"),
                     QStringLiteral("-d"), QStringLiteral("LEI16@48000"),
                     QStringLiteral("-c"), QStringLiteral("1"), aiff, wav });
    p2.waitForFinished(30000);

    zwischengespeichert = wavLesen(wav);
    if (zwischengespeichert.empty()) {
        grund = QStringLiteral("die Sprachausgabe hat keine brauchbare "
                               "Datei geliefert");
    }
    if (warum != nullptr) { *warum = grund; }
    return zwischengespeichert;
}

// Echte Sprache plus ein Grundgeräusch, auf -12 dBFS Spitze. Dazu
// eine Maske, die sagt, wo gesprochen wird -- hergeleitet aus der
// SAUBEREN Aufnahme, nicht aus dem verrauschten Gemisch, damit die
// Maske nicht von dem abhängt, was gemessen werden soll.
struct Rede {
    std::vector<float> signal;
    std::vector<char>  spricht;      // 1 = Sprache, 0 = Pause
    // ── Warum eine zweite, engere Maske ──────────────────────────
    //
    // Die Kette verzögert um 10 ms (das Entrauschen rechnet in
    // Blöcken von 480 Werten), und der Kompressor lässt nach einer
    // Silbe mit 150 ms los. Eine Pausenmaske, die bis an die Silbe
    // heranreicht, misst deshalb nicht den Raum, sondern den
    // verschobenen Sprachrest -- und das sieht aus wie ein
    // Entrauscher, der das Grundgeräusch um 16 dB ANHEBT. Genau
    // dieser Messfehler ist beim ersten Lauf passiert.
    //
    // Also: als Pause gilt nur, was 200 ms von jeder Silbe entfernt
    // ist.
    std::vector<char>  echtePause;
};

Rede redeMitRauschen(const std::vector<float>& roh, double rauschDbFs)
{
    // ── Stille anbauen ───────────────────────────────────────────
    //
    // Die Sprachausgabe des Systems redet durchgehend: in den 9,3
    // Sekunden gibt es keine einzige Stelle, die 200 ms von jeder
    // Silbe entfernt ist. Ohne Pause ist aber kein Störabstand
    // messbar, und ein Entrauscher, den man nur an der Sprache
    // misst, zeigt nur seinen Preis und nie seinen Nutzen.
    //
    // Also je 1,5 s Stille vor und hinter die Rede. Das Rauschen
    // kommt danach über alles -- dort steht dann der Raum allein.
    std::vector<float> sauber;
    const size_t stille = size_t(kRate * 1.5);
    sauber.assign(stille, 0.0f);
    sauber.insert(sauber.end(), roh.begin(), roh.end());
    sauber.insert(sauber.end(), stille, 0.0f);

    Rede r;
    const size_t n = sauber.size();
    r.signal.assign(n, 0.0f);
    r.spricht.assign(n, 0);

    // 20-ms-Effektivwert der sauberen Aufnahme.
    const size_t fenster = size_t(kRate * 0.02);
    double ges = 0.0;
    for (float x : sauber) { ges += double(x) * double(x); }
    const double gesRms = std::sqrt(ges / std::max<size_t>(1, n));
    const double schwelle = gesRms * 0.08;      // 22 dB unter dem Mittel

    for (size_t a = 0; a < n; a += fenster) {
        const size_t b = std::min(n, a + fenster);
        double s = 0.0;
        for (size_t i = a; i < b; ++i) { s += double(sauber[i]) * double(sauber[i]); }
        const bool laut = std::sqrt(s / double(b - a)) > schwelle;
        for (size_t i = a; i < b; ++i) { r.spricht[i] = laut ? 1 : 0; }
    }

    // Erodieren: 200 ms Sicherheitsabstand zu jeder Silbe.
    r.echtePause.assign(n, 0);
    {
        const size_t rand = size_t(kRate * 0.2);
        for (size_t i = 0; i < n; ++i) {
            if (r.spricht[i] != 0) { continue; }
            const size_t a = (i > rand) ? i - rand : 0;
            const size_t b = std::min(n, i + rand);
            bool frei = true;
            for (size_t k = a; k < b; ++k) {
                if (r.spricht[k] != 0) { frei = false; break; }
            }
            r.echtePause[i] = frei ? 1 : 0;
        }
    }

    std::mt19937 wuerfel(90210u);
    std::normal_distribution<double> glocke(0.0, 1.0);
    const double rausch = std::pow(10.0, rauschDbFs / 20.0);
    for (size_t i = 0; i < n; ++i) {
        r.signal[i] = float(double(sauber[i]) + glocke(wuerfel) * rausch);
    }
    double p = 0.0;
    for (float x : r.signal) { p = std::max(p, double(std::fabs(x))); }
    if (p > 1e-9) {
        const float g = float(std::pow(10.0, -12.0 / 20.0) / p);
        for (float& x : r.signal) { x *= g; }
    }
    return r;
}

// Effektivwert über die Sprachstellen und über die Pausen, getrennt.
void redePegel(const std::vector<float>& v, const Rede& r,
               double* spracheDb, double* pausenDb)
{
    double ss = 0.0;
    double sp = 0.0;
    size_t ns = 0;
    size_t np = 0;
    const size_t n = std::min(v.size(), r.spricht.size());
    for (size_t i = 0; i < n; ++i) {
        const double x = double(v[i]) * double(v[i]);
        if (r.spricht[i] != 0) { ss += x; ++ns; }
        if (r.echtePause[i] != 0) { sp += x; ++np; }
    }
    *spracheDb = dB(std::sqrt(ss / std::max<size_t>(1, ns)));
    *pausenDb  = np > 0 ? dB(std::sqrt(sp / double(np))) : -120.0;
}

std::vector<float> durch(StripChain& c, const std::vector<float>& in)
{
    std::vector<float> out = in;
    for (size_t at = 0; at + 240 <= out.size(); at += 240) {
        c.processMono(out.data() + at, 240);
    }
    return out;
}

// ── Ein IDEALER Kanal, zum Vergleich ────────────────────────────────
//
// Fensterfunktion mal sinc, 2047 Koeffizienten, Hann: senkrechte
// Flanken, kein Welligkeit, und vor allem LINEARE PHASE -- alle
// Frequenzen kommen gleich spaet heraus. So filtert ein Sender im
// Zwischenfrequenzweg. Der Preis steht im Namen der Zahl: 1023
// Abtastwerte, 21 ms feste Verzoegerung, die ein Mithoerer nicht
// bezahlen will.
//
// Das ist kein Vorschlag, sondern ein MASSSTAB: was unser IIR-Kanal
// gegen diesen verliert, ist der Teil des Unterschieds, der UNS
// gehoert. Was beide gleich machen, ist schlicht die Bandbreite.
std::vector<float> durchIdealenKanal(const std::vector<float>& in,
                                     double vonHz, double bisHz)
{
    constexpr int kN = 2047;
    std::vector<double> h(size_t(kN), 0.0);
    const double f1 = vonHz / kRate;
    const double f2 = bisHz / kRate;
    for (int n = 0; n < kN; ++n) {
        const int m = n - kN / 2;
        const double s2 = (m == 0) ? 2.0 * f2
                                   : std::sin(2.0 * M_PI * f2 * m) / (M_PI * m);
        const double s1 = (m == 0) ? 2.0 * f1
                                   : std::sin(2.0 * M_PI * f1 * m) / (M_PI * m);
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * n / (kN - 1));
        h[size_t(n)] = (s2 - s1) * w;
    }
    std::vector<float> out(in.size(), 0.0f);
    const int mitte = kN / 2;
    for (int i = 0; i < int(in.size()); ++i) {
        double acc = 0.0;
        const int von = std::max(0, i + mitte - (kN - 1));
        const int bis = std::min(int(in.size()) - 1, i + mitte);
        for (int j = von; j <= bis; ++j) {
            acc += double(in[size_t(j)]) * h[size_t(i + mitte - j)];
        }
        out[size_t(i)] = float(acc);   // schon um mitte zurueckgeschoben
    }
    return out;
}

std::vector<float> durchKanal(int kanal, const std::vector<float>& in)
{
    BandLimit b;
    b.prepare(kRate);
    b.setChannel(kanal);
    std::vector<float> out = in;
    for (size_t at = 0; at + 240 <= out.size(); at += 240) {
        b.process(out.data() + at, 240);
    }
    return out;
}

void empfehlung(StripChain& c)
{
    c.prepare(kRate);
    c.setEnabled(true);
    const bool ok = StripSettings::applyBuiltIn(
        QStringLiteral("Recommended — speech"), c);
    Q_ASSERT(ok);
    Q_UNUSED(ok);
}

// Spitze und Effektivwert der Sprachabschnitte, Pausen getrennt.
struct Messung {
    double spitzeDb{-120.0};
    double effektivDb{-120.0};
    double scheitelDb{0.0};
    double pausenDb{-120.0};
};

Messung messen(const std::vector<float>& v)
{
    Messung m;
    double p = 0.0;
    double s = 0.0;
    int n = 0;
    double sp = 0.0;
    int np = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        const double imTakt = std::fmod(double(i) / kRate, 1.0);
        const double x = double(v[i]);
        if (imTakt > 0.05 && imTakt < 0.60) {
            p = std::max(p, std::fabs(x));
            s += x * x; ++n;
        } else if (imTakt > 0.78 && imTakt < 0.96) {
            sp += x * x; ++np;
        }
    }
    m.spitzeDb   = dB(p);
    m.effektivDb = dB(std::sqrt(s / std::max(1, n)));
    m.scheitelDb = m.spitzeDb - m.effektivDb;
    m.pausenDb   = dB(std::sqrt(sp / std::max(1, np)));
    return m;
}

} // namespace

class TstBandbreitenGenau : public QObject {
    Q_OBJECT

private slots:
    // ── Zuerst das Messgerät selbst ─────────────────────────────────
    //
    // „roh" muss Verstärkung null sein, über den ganzen Bereich und
    // auf das Hundertstel. Ist das nicht so, misst alles danach den
    // Prüfstand und nicht den Kanal.
    void theBenchMeasuresZeroWhereThereIsNoFilter()
    {
        for (double hz : { 50.0, 110.0, 1000.0, 2500.0, 4050.0, 8000.0 }) {
            const double g = kanalDb(-1, hz);
            qInfo().noquote() << QStringLiteral("roh bei %1 Hz: %2 dB")
                                     .arg(hz, 0, 'f', 0).arg(g, 0, 'f', 4);
            QVERIFY2(std::fabs(g) < 0.01,
                     qPrintable(QStringLiteral("der Prüfstand selbst hat "
                                               "%1 dB Fehler").arg(g)));
        }
    }

    // ── Frage 1: sitzen die Ecken, wo es draufsteht? ────────────────
    void theCornersSitWhereTheLabelSaysTheyDo()
    {
        qInfo().noquote() << QStringLiteral(
            "Kanal                     unten         oben        Mitte");

        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            const auto& k = BandLimit::channels()[size_t(ch)];
            const double unten3 = ecke(ch, k.lowHz / 8.0, k.lowHz * 4.0,
                                       -3.0, true);
            const double oben3  = ecke(ch, k.highHz / 4.0, k.highHz * 4.0,
                                       -3.0, false);
            const double mitte  = std::sqrt(double(k.lowHz) * k.highHz);
            const double gMitte = kanalDb(ch, mitte);

            qInfo().noquote()
                << QStringLiteral("%1  %2 Hz (%3 %4 %)  %5 Hz (%6 %7 %)  "
                                  "%8 dB bei %9 Hz")
                       .arg(QString::fromUtf8(k.name), -24)
                       .arg(unten3, 7, 'f', 1)
                       .arg(unten3 >= k.lowHz ? QStringLiteral("+")
                                              : QStringLiteral("−"))
                       .arg(std::fabs(unten3 / k.lowHz - 1.0) * 100.0,
                            4, 'f', 1)
                       .arg(oben3, 7, 'f', 1)
                       .arg(oben3 >= k.highHz ? QStringLiteral("+")
                                              : QStringLiteral("−"))
                       .arg(std::fabs(oben3 / k.highHz - 1.0) * 100.0,
                            4, 'f', 1)
                       .arg(gMitte, 5, 'f', 2)
                       .arg(mitte, 0, 'f', 0);

            // Eine Ecke, die mehr als 3 % neben ihrer Aufschrift
            // sitzt, ist eine falsche Aufschrift. 3 % sind bei 4050 Hz
            // 120 Hz -- weit unter dem, was ein Ohr als „klingt wie
            // 2700" melden würde, und weit über jedem Rundungsrest.
            QVERIFY2(std::fabs(unten3 / k.lowHz - 1.0) < 0.03,
                     qPrintable(QStringLiteral("%1: untere Ecke bei %2 Hz "
                                               "statt %3")
                                    .arg(QString::fromUtf8(k.name))
                                    .arg(unten3, 0, 'f', 1).arg(k.lowHz)));
            QVERIFY2(std::fabs(oben3 / k.highHz - 1.0) < 0.03,
                     qPrintable(QStringLiteral("%1: obere Ecke bei %2 Hz "
                                               "statt %3")
                                    .arg(QString::fromUtf8(k.name))
                                    .arg(oben3, 0, 'f', 1).arg(k.highHz)));
            QVERIFY2(std::fabs(gMitte) < 0.2,
                     qPrintable(QStringLiteral("%1: in der Mitte %2 dB")
                                    .arg(QString::fromUtf8(k.name))
                                    .arg(gMitte)));
        }
    }

    // ── Dazwischen muss es flach sein ───────────────────────────────
    //
    // Ein Filter, das im Durchlass ein Dezibel wellt, klingt gefärbt,
    // auch wenn beide Ecken stimmen. Butterworth heisst gerade: keine
    // Welle.
    void thePassbandIsFlat()
    {
        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            const auto& k = BandLimit::channels()[size_t(ch)];
            double schlimmste = 0.0;
            double woHz = 0.0;
            // Ab wo „Durchlass" anfaengt, haengt an der Flanke. Eine
            // achte Ordnung ist beim 1,6-fachen der Ecke laengst
            // flach; eine zweite ist dort noch 0,6 dB darunter --
            // das ist keine Welligkeit, das ist die Flanke selbst.
            // Kanaele mit flacher unterer Flanke (Ecke unter
            // 150 Hz, siehe BandLimit::hochpassOrdnungFuer) werden
            // deshalb ab dem Dreifachen gemessen.
            const double von = k.lowHz * (k.lowHz < 150 ? 3.0 : 1.6);
            const double bis = k.highHz * 0.65;
            for (int i = 0; i <= 24; ++i) {
                const double hz = von * std::pow(bis / von, i / 24.0);
                const double g = kanalDb(ch, hz);
                if (std::fabs(g) > std::fabs(schlimmste)) {
                    schlimmste = g; woHz = hz;
                }
            }
            qInfo().noquote()
                << QStringLiteral("%1  Welligkeit %2 dB (bei %3 Hz, "
                                  "%4..%5 Hz gemessen)")
                       .arg(QString::fromUtf8(k.name), -24)
                       .arg(schlimmste, 5, 'f', 2)
                       .arg(woHz, 0, 'f', 0)
                       .arg(von, 0, 'f', 0).arg(bis, 0, 'f', 0);
            QVERIFY2(std::fabs(schlimmste) < 0.5,
                     qPrintable(QStringLiteral("%1 wellt %2 dB")
                                    .arg(QString::fromUtf8(k.name))
                                    .arg(schlimmste)));
        }
    }

    // ── Und die Flanken müssen achter Ordnung sein ──────────────────
    //
    // 48 dB je Oktave. Ein Filter zweiter Ordnung lässt eine Oktave
    // über der Ecke noch 12 dB stehen und klingt deshalb viel
    // breiter, als die Aufschrift sagt -- das wäre eine mögliche
    // Erklärung für „4 k klingt wie 2700", nur mit anderem Vorzeichen.
    void theSkirtsAreEighthOrder()
    {
        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            const auto& k = BandLimit::channels()[size_t(ch)];
            const double unterUnten = kanalDb(ch, k.lowHz / 2.0);
            const double ueberOben  = kanalDb(ch, k.highHz * 2.0);
            qInfo().noquote()
                << QStringLiteral("%1  eine Oktave unter %2 Hz: %3 dB · "
                                  "eine Oktave über %4 Hz: %5 dB")
                       .arg(QString::fromUtf8(k.name), -24)
                       .arg(k.lowHz).arg(unterUnten, 6, 'f', 1)
                       .arg(k.highHz).arg(ueberOben, 6, 'f', 1);

            // ── Unten 12, oben 48 ───────────────────────────────
            //
            // Hier stand einmal für beide Flanken dieselbe
            // Erwartung. Die untere ist seit dem 2026-10-10
            // absichtlich flacher: acht Pole bei 50 Hz kosteten
            // 16,8 ms Laufzeit und brachten dafür nur Dämpfung
            // unterhalb dessen, was eine Stimme enthält. Siehe die
            // Messreihe in BandLimit.h.
            //
            // 12 dB je Oktave heißt eine Oktave tiefer rund -12 dB;
            // geprüft wird, dass es überhaupt dämpft und nicht
            // heimlich wieder steil geworden ist.
            // Unter 150 Hz zweite Ordnung (rund -12 dB je Oktave),
            // ab 150 achte (rund -48). Siehe hochpassOrdnungFuer()
            // in BandLimit.h samt Messreihe.
            if (k.lowHz < 150) {
                QVERIFY2(unterUnten < -9.0 && unterUnten > -20.0,
                         qPrintable(QStringLiteral("%1: untere Flanke %2 dB "
                                                   "-- erwartet rund -12")
                                        .arg(QString::fromUtf8(k.name))
                                        .arg(unterUnten)));
            } else {
                QVERIFY2(unterUnten < -40.0,
                         qPrintable(QStringLiteral("%1: untere Flanke nur "
                                                   "%2 dB je Oktave")
                                        .arg(QString::fromUtf8(k.name))
                                        .arg(unterUnten)));
            }
            QVERIFY2(ueberOben < -40.0,
                     qPrintable(QStringLiteral("%1: obere Flanke nur %2 dB "
                                               "je Oktave")
                                    .arg(QString::fromUtf8(k.name))
                                    .arg(ueberOben)));
        }
    }

    // ── Frage 2: was kommt oben überhaupt an? ───────────────────────
    //
    // Dieselbe Stimme, einmal roh und einmal durch die Empfehlung,
    // beide durch denselben Kanal -- so, wie der Betreiber vergleicht.
    // Gemessen wird die ABSOLUTE Energie je Drittelband, mit der
    // eigenen Zerlegung des Prüfstands.
    void whatArrivesAtTheTopOfTheChannel()
    {
        const auto roh = stimme(6.0, true);
        StripChain c;
        empfehlung(c);
        const auto kette = durch(c, roh);

        struct Band { const char* name; double von; double bis; };
        const Band baender[] = {
            { "125 Hz", 112.0, 141.0 },
            { "250 Hz", 224.0, 282.0 },
            { "500 Hz", 447.0, 562.0 },
            { "1 kHz",  891.0, 1122.0 },
            { "2 kHz", 1778.0, 2239.0 },
            { "2,5 k", 2239.0, 2818.0 },
            { "3,2 k", 2818.0, 3548.0 },
            { "4 kHz", 3548.0, 4467.0 },
            { "6,3 k", 5623.0, 7079.0 },
        };

        const auto pRoh   = spektrum(roh);
        const auto pKette = spektrum(kette);

        qInfo().noquote() << QStringLiteral(
            "                   roh    Kette      Δ   (dB, absolut)");
        for (const Band& b : baender) {
            const double r = bandDb(pRoh, b.von, b.bis);
            const double k = bandDb(pKette, b.von, b.bis);
            qInfo().noquote()
                << QStringLiteral("  %1   %2  %3  %4")
                       .arg(QString::fromLatin1(b.name), -8)
                       .arg(r, 7, 'f', 1).arg(k, 7, 'f', 1)
                       .arg(k - r, 6, 'f', 1);
        }

        // Die Kippe: wieviel die Kette oben gegenüber der Mitte
        // verliert. Das ist die Zahl, die „stumpf" heisst.
        const double mitteR = bandDb(pRoh, 891.0, 1122.0);
        const double mitteK = bandDb(pKette, 891.0, 1122.0);
        const double obenR  = bandDb(pRoh, 2818.0, 4467.0);
        const double obenK  = bandDb(pKette, 2818.0, 4467.0);
        const double kippe  = (obenK - mitteK) - (obenR - mitteR);
        qInfo().noquote()
            << QStringLiteral("Kippe 2,8..4,5 kHz gegen 1 kHz: %1 dB "
                              "(negativ = die Kette macht stumpfer)")
                   .arg(kippe, 0, 'f', 2);

        // Mehr als 3 dB Kippe wäre genau der Befund, den das Ohr als
        // „4 k klingt wie 2700" melden würde -- und läge dann nicht am
        // Kanal, sondern an der Kette.
        QVERIFY2(kippe > -3.0,
                 qPrintable(QStringLiteral("die Kette nimmt oben %1 dB "
                                           "gegenüber der Mitte weg")
                                .arg(-kippe, 0, 'f', 1)));
    }

    // ── Wieviel von der Arbeit der Kette der Kanal wegwirft ─────────
    //
    // Die beiden oberen Griffe der einfachen Ansicht heissen PRÄSENZ
    // (2,5 k) und SCHÄRFE (3,8 k). Im DX-Kanal endet alles bei
    // 2500 Hz. Wer dort nach Gehör an SCHÄRFE dreht, stellt Energie
    // ein, die der Sender anschliessend wegwirft -- das ist der
    // ganze Grund, warum diese Auswahl existiert, und hier steht er
    // in Zahlen.
    void howMuchOfTheVoiceEachChannelThrowsAway()
    {
        const auto roh = stimme(6.0, true);
        StripChain c;
        empfehlung(c);
        const auto kette = durch(c, roh);
        const double ganz = bandDb(spektrum(kette), 20.0, 20000.0);

        qInfo().noquote() << QStringLiteral(
            "Kanal                     Energie   bei 2,5k   bei 3,8k");
        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            const auto& k = BandLimit::channels()[size_t(ch)];
            const auto p = spektrum(durchKanal(ch, kette));
            qInfo().noquote()
                << QStringLiteral("%1  %2 dB   %3 dB   %4 dB")
                       .arg(QString::fromUtf8(k.name), -24)
                       .arg(bandDb(p, 20.0, 20000.0) - ganz, 6, 'f', 1)
                       .arg(kanalDb(ch, 2500.0), 6, 'f', 1)
                       .arg(kanalDb(ch, 3800.0), 6, 'f', 1);
        }
    }

    // ── Der De-Esser, mit einem Zischlaut zum Messen ────────────────
    //
    // Offen geblieben aus dem Messlauf vom Vormittag: 0,0 dB überall,
    // und niemand wusste, ob das die Stufe oder das Prüfsignal war.
    // Jetzt hat das Signal ein „ss" bei 6,5 kHz.
    void theDeEsserFindsTheSibilant()
    {
        const auto mit  = stimme(6.0, true);
        const auto ohne = stimme(6.0, false);

        const double zischenRoh = bandDb(spektrum(mit), 5000.0, 8000.0)
                                  - bandDb(spektrum(ohne), 5000.0, 8000.0);
        qInfo().noquote()
            << QStringLiteral("Prüfsignal: der Zischlaut hebt 5..8 kHz um "
                              "%1 dB").arg(zischenRoh, 0, 'f', 1);
        QVERIFY2(zischenRoh > 6.0,
                 "das Prüfsignal hat keinen Zischlaut -- dann prüft der "
                 "Rest dieses Falls nichts");

        StripChain c;
        empfehlung(c);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        c.setStageEnabled(StripChain::Stage::DeEss, true);
        const auto nachher = durch(c, mit);

        const auto pV = spektrum(mit);
        const auto pN = spektrum(nachher);
        const double hoch = bandDb(pN, 5000.0, 8000.0)
                            - bandDb(pV, 5000.0, 8000.0);
        const double mittig = bandDb(pN, 500.0, 2000.0)
                              - bandDb(pV, 500.0, 2000.0);
        qInfo().noquote()
            << QStringLiteral("De-Esser (6 kHz, Schwelle -28): 5..8 kHz "
                              "%1 dB · 0,5..2 kHz %2 dB · Minderung laut "
                              "Anzeige %3 dB")
                   .arg(hoch, 0, 'f', 2).arg(mittig, 0, 'f', 2)
                   .arg(double(c.deEss().gainReductionDb()), 0, 'f', 2);
        // Die Anzeigen sind MOMENTANWERTE. Wer sie am Ende des
        // Signals abliest, liest die Pause -- derselbe Fehler, der
        // am Vormittag den Clipper dreimal mit 0,0 dB gemeldet hat.
        // Also ueber alle Bloecke mitlaufen.
        {
            StripChain d;
            empfehlung(d);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                d.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            d.setStageEnabled(StripChain::Stage::DeEss, true);
            std::vector<float> y = mit;
            double maxEin = -300.0;
            double maxSeite = -300.0;
            double maxMinder = 0.0;
            for (size_t at = 0; at + 240 <= y.size(); at += 240) {
                d.processMono(y.data() + at, 240);
                maxEin = std::max(maxEin, double(d.deEss().inputPeakDb()));
                maxSeite = std::max(maxSeite,
                                    double(d.deEss().sidechainPeakDb()));
                maxMinder = std::min(maxMinder,
                                     double(d.deEss().gainReductionDb()));
            }
            qInfo().noquote()
                << QStringLiteral("    waehrend des Laufs: Eingang bis "
                                  "%1 dBFS · Seitenweg bis %2 dBFS · "
                                  "Minderung bis %3 dB")
                       .arg(maxEin, 0, 'f', 1).arg(maxSeite, 0, 'f', 1)
                       .arg(maxMinder, 0, 'f', 2);
        }

        // Ab welcher Schwelle tut die Stufe ueberhaupt etwas? Das
        // trennt „falsch eingestellt" von „kaputt".
        for (double schwelle : { -20.0, -30.0, -40.0, -50.0, -55.0 }) {
            StripChain p;
            empfehlung(p);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                p.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            p.setStageEnabled(StripChain::Stage::DeEss, true);
            p.deEss().setThresholdDb(float(schwelle));
            const auto y = durch(p, mit);
            const auto pY = spektrum(y);
            qInfo().noquote()
                << QStringLiteral("    Schwelle %1 dB -> 5..8 kHz %2 dB · "
                                  "Seitenweg %3 dBFS · Minderung %4 dB")
                       .arg(schwelle, 5, 'f', 0)
                       .arg(bandDb(pY, 5000.0, 8000.0)
                            - bandDb(pV, 5000.0, 8000.0), 6, 'f', 2)
                       .arg(double(p.deEss().sidechainPeakDb()), 6, 'f', 1)
                       .arg(double(p.deEss().gainReductionDb()), 6, 'f', 2);
        }

        // Was ohne Zweifel falsch wäre: ein De-Esser, der die MITTE
        // anfasst. Dass er oben wenig tut, ist eine Einstellung;
        // dass er unten etwas tut, wäre ein Fehler.
        QVERIFY2(std::fabs(mittig) < 1.0,
                 qPrintable(QStringLiteral("der De-Esser greift in die "
                                           "Mitte: %1 dB").arg(mittig)));
    }

    // ── Wo der De-Esser stehen muss, damit er etwas bewacht ─────────
    //
    // Zwei Zahlen zugleich: Frequenz und Schwelle. Die Schwelle gilt
    // fuer den SEITENWEG, und der liegt bei Sprache mit -12 dBFS
    // Spitze rund 15 dB unter dem Vollbandpegel -- das ist der Grund,
    // aus dem -28 nie gegriffen hat.
    void whereTheDeEsserHasToStand()
    {
        const auto mit = stimme(6.0, true);
        const auto pV = spektrum(mit);

        qInfo().noquote() << QStringLiteral(
            "  Frequenz  Schwelle   5..8 kHz   2..4 kHz   Seitenweg   "
            "Minderung");
        for (double hz : { 4000.0, 6000.0 }) {
            for (double schwelle : { -28.0, -34.0, -38.0, -42.0 }) {
                StripChain d;
                empfehlung(d);
                for (int i = 0; i < StripChain::kStageCount; ++i) {
                    d.setStageEnabled(static_cast<StripChain::Stage>(i), false);
                }
                d.setStageEnabled(StripChain::Stage::DeEss, true);
                d.deEss().setFrequencyHz(float(hz));
                d.deEss().setThresholdDb(float(schwelle));

                std::vector<float> y = mit;
                double seite = -300.0;
                double minder = 0.0;
                for (size_t at = 0; at + 240 <= y.size(); at += 240) {
                    d.processMono(y.data() + at, 240);
                    seite = std::max(seite,
                                     double(d.deEss().sidechainPeakDb()));
                    minder = std::min(minder,
                                      double(d.deEss().gainReductionDb()));
                }
                const auto pY = spektrum(y);
                qInfo().noquote()
                    << QStringLiteral("  %1 Hz   %2 dB   %3 dB   %4 dB   "
                                      "%5 dBFS   %6 dB")
                           .arg(hz, 6, 'f', 0).arg(schwelle, 6, 'f', 0)
                           .arg(bandDb(pY, 5000.0, 8000.0)
                                - bandDb(pV, 5000.0, 8000.0), 6, 'f', 2)
                           .arg(bandDb(pY, 2000.0, 4000.0)
                                - bandDb(pV, 2000.0, 4000.0), 6, 'f', 2)
                           .arg(seite, 6, 'f', 1).arg(minder, 6, 'f', 2);
            }
        }
    }

    // ── Der Entrauscher, am realistischen Signal nachgemessen ───────
    //
    // Die Wahl 0,35 stammt vom Vormittag, gemessen an einem Signal
    // mit einem Pulszug als Quelle -- also bis 20 kHz gleich laut.
    // Eine echte Stimmlippe faellt mit 12 dB je Oktave, und ein
    // Entrauscher, der nach Sprache sucht, sieht dann etwas anderes.
    // Also noch einmal, mit der abfallenden Quelle.
    void theDenoiserAtEachAmountOnARealisticVoice()
    {
        const auto roh = stimme(6.0, true);
        const auto pR = spektrum(roh);
        const Messung r = messen(roh);

        qInfo().noquote() << QStringLiteral(
            "  Anteil   2 kHz   4 kHz   6 kHz   Pausen   Sprache");
        for (double anteil : { 0.20, 0.35, 0.50, 0.70, 1.00 }) {
            StripChain c;
            empfehlung(c);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.nr().setAmount(float(anteil));
            c.setStageEnabled(StripChain::Stage::Denoise, true);
            const auto y = durch(c, roh);
            const auto pY = spektrum(y);
            const Messung m = messen(y);
            qInfo().noquote()
                << QStringLiteral("   %1   %2   %3   %4   %5   %6")
                       .arg(anteil, 4, 'f', 2)
                       .arg(bandDb(pY, 1778.0, 2239.0)
                            - bandDb(pR, 1778.0, 2239.0), 5, 'f', 1)
                       .arg(bandDb(pY, 3548.0, 4467.0)
                            - bandDb(pR, 3548.0, 4467.0), 5, 'f', 1)
                       .arg(bandDb(pY, 5623.0, 7079.0)
                            - bandDb(pR, 5623.0, 7079.0), 5, 'f', 1)
                       .arg(m.pausenDb - r.pausenDb, 6, 'f', 1)
                       .arg(m.effektivDb - r.effektivDb, 6, 'f', 1);
        }
    }

    // ── Dasselbe noch einmal, mit wirklicher Sprache ────────────────
    //
    // Der entscheidende Fall. Alles, was heute Vormittag über den
    // Entrauscher gemessen wurde, hing an einem Signal, das rnnoise
    // nicht als Sprache erkannt hat -- die Zahlen waren echt, nur
    // über die falsche Sache. Hier steht die Reihe neu.
    void theDenoiserOnRealSpeech()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }

        const Rede r = redeMitRauschen(sauber, -48.0);
        qInfo().noquote()
            << QStringLiteral("Prüfsignal: %1 s echte Sprache, "
                              "Grundgeräusch -48 dBFS, Spitze auf -12 dBFS")
                   .arg(double(r.signal.size()) / kRate, 0, 'f', 1);

        double rohSprache = 0.0;
        double rohPausen = 0.0;
        redePegel(r.signal, r, &rohSprache, &rohPausen);
        const auto pR = spektrum(r.signal);
        qInfo().noquote()
            << QStringLiteral("  roh: Sprache %1 dB · Pausen %2 dB · "
                              "Störabstand %3 dB")
                   .arg(rohSprache, 0, 'f', 1).arg(rohPausen, 0, 'f', 1)
                   .arg(rohSprache - rohPausen, 0, 'f', 1);

        qInfo().noquote() << QStringLiteral(
            "  Anteil  Sprachw.   2 kHz   4 kHz   6 kHz   Sprache   "
            "Pausen   Störabstand");
        double besterGewinn = -100.0;
        double bestesAnteil = 0.0;
        for (double anteil : { 0.20, 0.35, 0.50, 0.70, 1.00 }) {
            StripChain c;
            empfehlung(c);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.nr().setAmount(float(anteil));
            c.setStageEnabled(StripChain::Stage::Denoise, true);

            std::vector<float> y = r.signal;
            double summeProb = 0.0;
            int nProb = 0;
            for (size_t at = 0; at + 240 <= y.size(); at += 240) {
                c.processMono(y.data() + at, 240);
                if (r.spricht[at] != 0) {
                    summeProb += double(c.nr().voiceProbability());
                    ++nProb;
                }
            }
            double sprache = 0.0;
            double pausen = 0.0;
            redePegel(y, r, &sprache, &pausen);
            const auto pY = spektrum(y);
            const double gewinn = (sprache - pausen)
                                  - (rohSprache - rohPausen);
            if (gewinn > besterGewinn) {
                besterGewinn = gewinn; bestesAnteil = anteil;
            }
            qInfo().noquote()
                << QStringLiteral("   %1    %2   %3   %4   %5   %6   %7   "
                                  "%8")
                       .arg(anteil, 4, 'f', 2)
                       .arg(summeProb / std::max(1, nProb), 5, 'f', 2)
                       .arg(bandDb(pY, 1778.0, 2239.0)
                            - bandDb(pR, 1778.0, 2239.0), 5, 'f', 1)
                       .arg(bandDb(pY, 3548.0, 4467.0)
                            - bandDb(pR, 3548.0, 4467.0), 5, 'f', 1)
                       .arg(bandDb(pY, 5623.0, 7079.0)
                            - bandDb(pR, 5623.0, 7079.0), 5, 'f', 1)
                       .arg(sprache - rohSprache, 7, 'f', 1)
                       .arg(pausen - rohPausen, 7, 'f', 1)
                       .arg(gewinn, 7, 'f', 1);
        }
        qInfo().noquote()
            << QStringLiteral("Bester Störabstandsgewinn: %1 dB bei Anteil "
                              "%2").arg(besterGewinn, 0, 'f', 1)
                   .arg(bestesAnteil, 0, 'f', 2);

        // Was ohne Zweifel falsch wäre: ein Entrauscher, der bei
        // keinem einzigen Anteil den Störabstand verbessert. Dann
        // wäre er eine Lautstärke mit Rechenaufwand.
        QVERIFY2(besterGewinn > 3.0,
                 qPrintable(QStringLiteral("der Entrauscher bringt "
                                           "höchstens %1 dB Störabstand")
                                .arg(besterGewinn)));
    }

    // ── Der De-Esser an echter Sprache ──────────────────────────────
    //
    // An der nachgebauten Stimme ergab sich: die Schwelle gilt fuer
    // den SEITENWEG, und der liegt rund 15 dB unter dem Vollband --
    // deshalb hat -28 nie gegriffen. Hier dieselbe Frage an wirklich
    // gesprochene Zischlaute.
    void theDeEsserOnRealSpeech()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede r = redeMitRauschen(sauber, -48.0);
        const auto pR = spektrum(r.signal);

        qInfo().noquote() << QStringLiteral(
            "  Frequenz  Schwelle   5..8 kHz   1..3 kHz   Seitenweg   "
            "Minderung");
        for (double hz : { 4000.0, 6000.0 }) {
            for (double schwelle : { -28.0, -34.0, -40.0, -46.0 }) {
                StripChain d;
                empfehlung(d);
                for (int i = 0; i < StripChain::kStageCount; ++i) {
                    d.setStageEnabled(static_cast<StripChain::Stage>(i),
                                      false);
                }
                d.setStageEnabled(StripChain::Stage::DeEss, true);
                d.deEss().setFrequencyHz(float(hz));
                d.deEss().setThresholdDb(float(schwelle));

                std::vector<float> y = r.signal;
                double seite = -300.0;
                double minder = 0.0;
                for (size_t at = 0; at + 240 <= y.size(); at += 240) {
                    d.processMono(y.data() + at, 240);
                    seite = std::max(seite,
                                     double(d.deEss().sidechainPeakDb()));
                    minder = std::min(minder,
                                      double(d.deEss().gainReductionDb()));
                }
                const auto pY = spektrum(y);
                qInfo().noquote()
                    << QStringLiteral("  %1 Hz   %2 dB   %3 dB   %4 dB   "
                                      "%5 dBFS   %6 dB")
                           .arg(hz, 6, 'f', 0).arg(schwelle, 6, 'f', 0)
                           .arg(bandDb(pY, 5000.0, 8000.0)
                                - bandDb(pR, 5000.0, 8000.0), 6, 'f', 2)
                           .arg(bandDb(pY, 1000.0, 3000.0)
                                - bandDb(pR, 1000.0, 3000.0), 6, 'f', 2)
                           .arg(seite, 6, 'f', 1).arg(minder, 6, 'f', 2);
            }
        }
    }

    // ── Liegen trocken und entrauscht auf derselben Zeit? ───────────
    //
    // Die Reihe oben hat eine Delle: bei Anteil 0,35 kostet die
    // Stufe 2,8 dB Sprache, bei Anteil 1,00 aber nur 0,1 dB. Das
    // kann eine Mischung aus zwei GLEICHEN Signalen nicht -- zwei
    // gleich laute, gleich liegende Signale ergeben gemischt wieder
    // dasselbe. Es geht nur, wenn sie einander stellenweise
    // auslöschen, und das heißt: sie liegen nicht auf derselben
    // Zeit.
    //
    // Der Quelltext behauptet das Gegenteil („Beide verlassen die
    // Stufe im selben Durchgang"). Also wird gemessen, statt
    // geglaubt: Anteil 0 ist der trockene Weg allein, Anteil 1 der
    // entrauschte allein, und die Kreuzkorrelation sagt, um wieviel
    // sie auseinanderliegen.
    void doTheDryAndTheDenoisedPathLineUp()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede r = redeMitRauschen(sauber, -48.0);

        const auto lauf = [&r](double anteil) {
            StripChain c;
            empfehlung(c);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.nr().setAmount(float(anteil));
            c.setStageEnabled(StripChain::Stage::Denoise, true);
            return durch(c, r.signal);
        };
        const auto trocken = lauf(0.0);
        const auto feucht  = lauf(1.0);

        // Nur über die Sprachstellen korrelieren: in der Stille ist
        // der entrauschte Weg leer, und zwei Stillen korrelieren
        // über jeden Versatz gleich gut.
        size_t von = 0;
        for (size_t i = 0; i < r.spricht.size(); ++i) {
            if (r.spricht[i] != 0) { von = i; break; }
        }
        von += size_t(kRate * 0.5);
        const size_t laenge = size_t(kRate * 3.0);

        int bestLag = 0;
        double bestes = -1.0;
        for (int lag = -1200; lag <= 1200; ++lag) {
            double zaehler = 0.0;
            double na = 0.0;
            double nb = 0.0;
            for (size_t i = 0; i < laenge; ++i) {
                const size_t ia = von + i;
                const qsizetype ib = qsizetype(von + i) + lag;
                if (ib < 0 || ib >= qsizetype(feucht.size())) { continue; }
                const double a = double(trocken[ia]);
                const double b = double(feucht[size_t(ib)]);
                zaehler += a * b;
                na += a * a;
                nb += b * b;
            }
            const double k = zaehler / std::sqrt(std::max(na * nb, 1e-20));
            if (k > bestes) { bestes = k; bestLag = lag; }
        }
        const double kBeiNull = [&]() {
            double zaehler = 0.0;
            double na = 0.0;
            double nb = 0.0;
            for (size_t i = 0; i < laenge; ++i) {
                const double a = double(trocken[von + i]);
                const double b = double(feucht[von + i]);
                zaehler += a * b; na += a * a; nb += b * b;
            }
            return zaehler / std::sqrt(std::max(na * nb, 1e-20));
        }();

        qInfo().noquote()
            << QStringLiteral("Versatz trocken gegen entrauscht: %1 "
                              "Abtastwerte (%2 ms) · Korrelation dort %3 · "
                              "bei Versatz 0 nur %4")
                   .arg(bestLag).arg(bestLag * 1000.0 / kRate, 0, 'f', 2)
                   .arg(bestes, 0, 'f', 3).arg(kBeiNull, 0, 'f', 3);
        qInfo().noquote()
            << QStringLiteral("   (ein Rahmen des Entrauschens sind %1 "
                              "Abtastwerte)").arg(480);

        // Was ohne Zweifel falsch ist: die beiden Wege, die in
        // DERSELBEN Zeile gemischt werden, liegen nicht auf
        // derselben Zeit. Ein halber Abtastwert Toleranz wäre
        // übertrieben; mehr als zwanzig ist ein Fehler, denn dann
        // löschen sich alle Frequenzen über 1,2 kHz stellenweise
        // aus.
        QVERIFY2(std::abs(bestLag) <= 20,
                 qPrintable(QStringLiteral("trocken und entrauscht liegen "
                                           "%1 Abtastwerte (%2 ms) "
                                           "auseinander -- die Mischung "
                                           "kämmt")
                                .arg(bestLag)
                                .arg(bestLag * 1000.0 / kRate, 0, 'f', 2)));
    }

    // ── Und die ganze Kette, an wirklicher Sprache ──────────────────
    void theWholeChainOnRealSpeech()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede r = redeMitRauschen(sauber, -48.0);

        StripChain c;
        empfehlung(c);
        const auto kette = durch(c, r.signal);

        const auto pR = spektrum(r.signal);
        const auto pK = spektrum(kette);

        struct Band { const char* name; double von; double bis; };
        const Band baender[] = {
            { "125 Hz", 112.0, 141.0 },  { "250 Hz", 224.0, 282.0 },
            { "500 Hz", 447.0, 562.0 },  { "1 kHz",  891.0, 1122.0 },
            { "2 kHz", 1778.0, 2239.0 }, { "3,2 k", 2818.0, 3548.0 },
            { "4 kHz", 3548.0, 4467.0 }, { "6,3 k", 5623.0, 7079.0 },
        };
        qInfo().noquote() << QStringLiteral(
            "               Δ gegen roh (dB, absolut)");
        for (const Band& b : baender) {
            qInfo().noquote()
                << QStringLiteral("  %1   %2")
                       .arg(QString::fromLatin1(b.name), -8)
                       .arg(bandDb(pK, b.von, b.bis)
                            - bandDb(pR, b.von, b.bis), 6, 'f', 1);
        }

        double rs = 0.0;
        double rp = 0.0;
        double ks = 0.0;
        double kp = 0.0;
        redePegel(r.signal, r, &rs, &rp);
        redePegel(kette, r, &ks, &kp);
        double spitzeR = 0.0;
        double spitzeK = 0.0;
        for (size_t i = 0; i < kette.size(); ++i) {
            if (r.spricht[i] == 0) { continue; }
            spitzeR = std::max(spitzeR, double(std::fabs(r.signal[i])));
            spitzeK = std::max(spitzeK, double(std::fabs(kette[i])));
        }
        qInfo().noquote()
            << QStringLiteral("  roh:    Spitze %1  Sprache %2  "
                              "Scheitel %3  Störabstand %4 dB")
                   .arg(dB(spitzeR), 6, 'f', 1).arg(rs, 6, 'f', 1)
                   .arg(dB(spitzeR) - rs, 5, 'f', 1).arg(rs - rp, 5, 'f', 1);
        qInfo().noquote()
            << QStringLiteral("  Kette:  Spitze %1  Sprache %2  "
                              "Scheitel %3  Störabstand %4 dB")
                   .arg(dB(spitzeK), 6, 'f', 1).arg(ks, 6, 'f', 1)
                   .arg(dB(spitzeK) - ks, 5, 'f', 1).arg(ks - kp, 5, 'f', 1);

        QVERIFY2(dB(spitzeK) < 0.0, "die Kette übersteuert");
        QVERIFY2((dB(spitzeK) - ks) < (dB(spitzeR) - rs),
                 "der Scheitelfaktor wird nicht kleiner");
    }

    // ── Hält rnnoise mein Prüfsignal überhaupt für eine Stimme? ─────
    //
    // Die Reihe oben sieht aus wie ein Mischfehler: jeder Anteil
    // nimmt genau (1 - Anteil) weg, also wie eine Lautstärke. Das
    // Mischen selbst ist aber nachweislich richtig gebaut
    // (NoiseReduction.cpp: dry und wet kommen aus DEMSELBEN Block).
    //
    // Dann bleibt nur eine Erklärung: das entrauschte Signal ist mit
    // dem trockenen nicht verwandt -- es ist weggedrückt. Und dafür
    // gibt es ein eingebautes Messgerät, das genau diese Frage
    // beantwortet: rnnoise meldet je Block, für wie wahrscheinlich
    // es eine Stimme hält.
    void doesTheNeuralNetHearAVoiceAtAll()
    {
        const auto roh = stimme(6.0, true);

        StripChain c;
        empfehlung(c);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        c.nr().setAmount(1.0f);
        c.setStageEnabled(StripChain::Stage::Denoise, true);

        std::vector<float> y = roh;
        double maxProb = 0.0;
        double summeProb = 0.0;
        int n = 0;
        double sprachProb = 0.0;
        int nSprache = 0;
        double minderung = 0.0;
        for (size_t at = 0; at + 240 <= y.size(); at += 240) {
            c.processMono(y.data() + at, 240);
            const double pr = double(c.nr().voiceProbability());
            maxProb = std::max(maxProb, pr);
            summeProb += pr; ++n;
            const double imTakt = std::fmod(double(at) / kRate, 1.0);
            if (imTakt > 0.1 && imTakt < 0.45) { sprachProb += pr; ++nSprache; }
            minderung = std::min(minderung, double(c.nr().reductionDb()));
        }
        qInfo().noquote()
            << QStringLiteral("rnnoise: Sprachwahrscheinlichkeit im Mittel "
                              "%1 · in den Silben %2 · höchstens %3 · "
                              "Minderung bis %4 dB")
                   .arg(summeProb / std::max(1, n), 0, 'f', 3)
                   .arg(sprachProb / std::max(1, nSprache), 0, 'f', 3)
                   .arg(maxProb, 0, 'f', 3)
                   .arg(minderung, 0, 'f', 1);

        // KEINE Prüfung mit Urteil: ob ein neuronales Netz ein
        // nachgebautes Signal für eine Stimme hält, ist keine
        // Eigenschaft von Longpath. Die Zahl gehört ins Protokoll,
        // weil sie sagt, ob die Reihe darüber überhaupt etwas über
        // den Entrauscher aussagt -- oder nur über mein Prüfsignal.
        qInfo().noquote() << QStringLiteral(
            "   (unter etwa 0,5 misst die Reihe oben nicht das "
            "Entrauschen, sondern wie stark rnnoise ein Signal "
            "wegdrückt, das es nicht als Sprache erkennt)");
    }

    // ── Wo der Seitenweg des De-Essers wirklich hinhört ─────────────
    //
    // Die Stufe tut erst ab Schwelle -40 etwas, obwohl der Zischlaut
    // bei -21 dBFS liegt. Also wird der Seitenweg gefragt, und zwar
    // mit reinen Tönen: einer je Frequenz, -20 dBFS, und sofort nach
    // dem Block abgelesen -- nicht am Ende des Signals, wo die
    // Anzeige in der Pause steht.
    void theSidechainListensWhereItSaysItDoes()
    {
        ClientDeEss d;
        d.prepare(kRate);
        d.setFrequencyHz(6000.0f);
        d.setEnabled(false);          // nur messen, nicht eingreifen

        qInfo().noquote() << QStringLiteral(
            "   Ton      Seitenweg (hinein: -20 dBFS)");
        double besteHz = 0.0;
        double bestes = -300.0;
        for (double hz : { 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0,
                           6500.0, 8000.0, 10000.0, 12000.0 }) {
            std::vector<float> x(size_t(kRate / 4), 0.0f);
            for (size_t i = 0; i < x.size(); ++i) {
                x[i] = float(0.1 * std::sin(2.0 * M_PI * hz
                                            * double(i) / kRate));
            }
            for (size_t at = 0; at + 240 <= x.size(); at += 240) {
                d.process(x.data() + at, 240, 1);
            }
            const double sw = double(d.sidechainPeakDb());
            qInfo().noquote()
                << QStringLiteral("  %1 Hz   %2 dBFS")
                       .arg(hz, 6, 'f', 0).arg(sw, 7, 'f', 1);
            if (sw > bestes) { bestes = sw; besteHz = hz; }
        }
        qInfo().noquote()
            << QStringLiteral("Lautester Punkt des Seitenwegs: %1 Hz "
                              "(eingestellt: 6000 Hz)").arg(besteHz, 0, 'f', 0);

        // Ein Seitenweg, dessen Maximum eine Oktave neben seiner
        // Einstellung liegt, bewacht die falsche Stelle -- und das
        // ist genau der Unterschied zwischen „greift nicht" und
        // „greift nie".
        QVERIFY2(besteHz > 4000.0 && besteHz < 8500.0,
                 qPrintable(QStringLiteral("der Seitenweg hört bei %1 Hz "
                                           "am lautesten, eingestellt sind "
                                           "6000 Hz").arg(besteHz)));
        QVERIFY2(bestes > -23.0,
                 qPrintable(QStringLiteral("der Seitenweg verliert %1 dB "
                                           "auf seiner eigenen Frequenz")
                                .arg(-20.0 - bestes)));
    }

    // ── Und was der Kanal vom De-Esser übrig lässt ──────────────────
    //
    // Ein De-Esser bei 6 kHz bewacht eine Stelle, die der DX-Kanal
    // 7,6 Oktaven unter seiner Ecke abschneidet. Die Zahl dazu gehört
    // ins Protokoll, weil sie eine Einstellung begründet und keine
    // Vermutung ist.
    void aDeEsserAboveTheChannelGuardsNothing()
    {
        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            const auto& k = BandLimit::channels()[size_t(ch)];
            qInfo().noquote()
                << QStringLiteral("%1  bei 6000 Hz: %2 dB")
                       .arg(QString::fromUtf8(k.name), -24)
                       .arg(kanalDb(ch, 6000.0), 7, 'f', 1);
        }
    }

    // ── Roh gegen Kette, durch denselben Kanal ──────────────────────
    //
    // Der Vergleich, den der Betreiber wirklich macht. Beide Seiten
    // durch denselben Kanal, damit nicht die Bandbreite den Unterschied
    // macht.
    void rawAgainstTheChainThroughTheSameChannel()
    {
        // ── Mit echter Sprache, und nur mit echter ──────────────
        //
        // Die erste Fassung dieses Falls nahm die nachgebaute
        // Stimme und meldete, die Kette VERSCHLECHTERE den
        // Scheitelfaktor von 14,3 auf 18,3 dB. Das war wahr und
        // bedeutungslos: der Entrauscher sieht in einem Signal, das
        // er nicht als Sprache erkennt, dauernd eine andere
        // Wahrscheinlichkeit und moduliert es deshalb -- genau so
        // entsteht ein hoeherer Scheitelfaktor. Mit wirklicher
        // Sprache gibt es das nicht.
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);
        const auto roh = rede.signal;

        StripChain c;
        empfehlung(c);
        const auto kette = durch(c, roh);

        // Spitze, Effektivwert und Stoerabstand ueber die Masken der
        // Rede -- die festen Zeitfenster der nachgebauten Stimme
        // passen hier nicht.
        const auto messenRede = [&rede](const std::vector<float>& v) {
            Messung m;
            double sprache = 0.0;
            double pausen = 0.0;
            redePegel(v, rede, &sprache, &pausen);
            double p = 0.0;
            for (size_t i = 0; i < v.size(); ++i) {
                if (rede.spricht[i] == 0) { continue; }
                p = std::max(p, double(std::fabs(v[i])));
            }
            m.spitzeDb = dB(p);
            m.effektivDb = sprache;
            m.scheitelDb = m.spitzeDb - sprache;
            m.pausenDb = pausen;
            return m;
        };

        qInfo().noquote() << QStringLiteral(
            "Kanal                      Spitze  effektiv  Scheitel  "
            "Störabstand");
        for (int ch : { -1, 0, 3 }) {
            const QString name = ch < 0
                ? QStringLiteral("roh (volle Bandbreite)")
                : QString::fromUtf8(
                      BandLimit::channels()[size_t(ch)].name);
            const Messung r = messenRede(durchKanal(ch, roh));
            const Messung k = messenRede(durchKanal(ch, kette));
            qInfo().noquote()
                << QStringLiteral("  %1\n    roh    %2  %3  %4  %5 dB"
                                  "\n    Kette  %6  %7  %8  %9 dB")
                       .arg(name, -24)
                       .arg(r.spitzeDb, 6, 'f', 1).arg(r.effektivDb, 8, 'f', 1)
                       .arg(r.scheitelDb, 8, 'f', 1)
                       .arg(r.effektivDb - r.pausenDb, 6, 'f', 1)
                       .arg(k.spitzeDb, 6, 'f', 1).arg(k.effektivDb, 8, 'f', 1)
                       .arg(k.scheitelDb, 8, 'f', 1)
                       .arg(k.effektivDb - k.pausenDb, 6, 'f', 1);

            // Durch jeden Kanal muss gelten, was die Kette verspricht:
            // kleinerer Scheitelfaktor und besserer Störabstand.
            QVERIFY2(k.scheitelDb < r.scheitelDb,
                     qPrintable(QStringLiteral("%1: Scheitelfaktor %2 -> %3")
                                    .arg(name).arg(r.scheitelDb)
                                    .arg(k.scheitelDb)));
            QVERIFY2((k.effektivDb - k.pausenDb)
                         > (r.effektivDb - r.pausenDb),
                     qPrintable(QStringLiteral("%1: Störabstand %2 -> %3")
                                    .arg(name)
                                    .arg(r.effektivDb - r.pausenDb)
                                    .arg(k.effektivDb - k.pausenDb)));
        }
    }

    // ── Was das Kanalfilter aus der Decke des Begrenzers macht ──────
    //
    // Aufgefallen beim A/B: die Kette haelt ihre Spitze brav bei
    // -1 dBFS, und NACH dem Kanalfilter steht +0,2 dBFS da. Das ist
    // kein Rechenfehler, sondern die Physik des Clippers: er
    // flacht die Spitzen ab und erzeugt dabei Energie weit
    // oberhalb des Kanals. Nimmt das Filter die wieder weg, fehlt
    // dem abgeflachten Verlauf gerade das, was ihn flach gehalten
    // hat -- und er schwingt ueber.
    //
    // Das heisst: eine Decke von -1 dB vor einem Bandfilter ist
    // KEINE Decke hinter ihm. Im Sender sitzt genau so ein Filter.
    void theChannelFilterOvershootsTheLimiter()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        StripChain c;
        empfehlung(c);
        const auto kette = durch(c, rede.signal);
        const auto spitze = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            return dB(p);
        };
        const double vor = spitze(kette);
        qInfo().noquote()
            << QStringLiteral("Kette vor dem Kanal: %1 dBFS "
                              "(Decke des Begrenzers -1,0)")
                   .arg(vor, 0, 'f', 2);

        double schlimmster = 0.0;
        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            const double nach = spitze(durchKanal(ch, kette));
            schlimmster = std::max(schlimmster, nach - vor);
            qInfo().noquote()
                << QStringLiteral("  %1  %2 dBFS  (Überschwinger %3 dB)")
                       .arg(QString::fromUtf8(
                                BandLimit::channels()[size_t(ch)].name), -24)
                       .arg(nach, 6, 'f', 2).arg(nach - vor, 5, 'f', 2);
        }
        qInfo().noquote()
            << QStringLiteral("Schlimmster Überschwinger: %1 dB -- so viel "
                              "Luft muss die Decke lassen")
                   .arg(schlimmster, 0, 'f', 2);

        // Was ohne Zweifel falsch ist: durch irgendeinen Kanal über
        // Vollaussteuerung zu kommen. Dort schneidet die Soundkarte
        // ab, und im Sender die Endstufe.
        //
        // Die Decke der Kette allein reicht dafuer NICHT -- siehe
        // clipThenFilterThenLimit(). Deshalb steht im Mithoerweg
        // hinter dem Kanalfilter ein zweiter Begrenzer, und hier
        // wird dieselbe Reihenfolge nachgestellt.
        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            auto nachKanal = durchKanal(ch, kette);
            ClientFinalLimiter lim;
            lim.prepare(kRate);
            lim.setEnabled(true);
            lim.setCeilingDb(-2.0f);
            for (size_t at = 0; at + 240 <= nachKanal.size(); at += 240) {
                lim.process(nachKanal.data() + at, 240, 1);
            }
            const double nach = spitze(nachKanal);
            QVERIFY2(nach < 0.0,
                     qPrintable(QStringLiteral("%1: %2 dBFS nach dem Kanal")
                                    .arg(QString::fromUtf8(
                                        BandLimit::channels()[size_t(ch)]
                                            .name))
                                    .arg(nach, 0, 'f', 2)));
        }
    }

    // ── Eine Bandgrenze vor dem Clipper hilft fast nichts ───────────
    //
    // Der naheliegende Gedanke, und er ist falsch -- hier steht er
    // samt Widerlegung, damit ihn niemand ein zweites Mal hat.
    //
    // Gedanke: der Ueberschwinger kommt daher, dass der Clipper
    // Energie weit oberhalb des Kanals erzeugt und das Filter sie
    // anschliessend wegnimmt. Der Rundfunk setzt die Bandgrenze
    // darum VOR das Clippen. Also muesste ein Tiefpass im
    // Entzerrer -- der steht vor dem Clipper -- das Gleiche tun.
    //
    // Gemessen:
    //
    //   wie jetzt              Ueberschwinger 1,89 dB
    //   mit Tiefpass 4050 Hz   Ueberschwinger 1,70 dB
    //
    // Nicht einmal zwei Zehntel. Der Grund ist offensichtlich,
    // sobald man ihn sieht: die Oberwellen entstehen IM Clipper,
    // also HINTER dem Tiefpass -- der kann sie unmoeglich
    // begrenzen. Ein bandbegrenzter Clipper muesste abwechselnd
    // clippen und filtern, mehrfach. Das waere eine neue Stufe und
    // keine Einstellung.
    //
    // Praktische Folge: der Ueberschwinger bleibt, und die Decke
    // des Begrenzers muss die 2 dB Luft lassen, die sie jetzt
    // laesst.
    void aBandLimitBeforeTheClipperBarelyHelps()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        const auto spitze = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            return dB(p);
        };

        qInfo().noquote() << QStringLiteral(
            "                           Kette   nach 50-4050   "
            "Überschwinger");
        for (int mit = 0; mit < 2; ++mit) {
            StripChain c;
            empfehlung(c);
            if (mit != 0) {
                // Band 6 ist in der Empfehlung ein Hochregal bei
                // 3 kHz mit 0 dB -- es tut nichts. Hier wird daraus
                // ein Tiefpass an der Oberkante des weitesten
                // Kanals.
                ClientEq::BandParams p;
                p.type = ClientEq::FilterType::LowPass;
                p.freqHz = 4050.0f;
                p.gainDb = 0.0f;
                p.q = 0.707f;
                p.enabled = true;
                p.slopeDbPerOct = 24;
                c.eq().setBand(6, p);
            }
            const auto kette = durch(c, rede.signal);
            const double vor = spitze(kette);
            const double nach = spitze(durchKanal(3, kette));
            qInfo().noquote()
                << QStringLiteral("  %1  %2 dBFS   %3 dBFS   %4 dB")
                       .arg(mit != 0 ? QStringLiteral("mit Tiefpass 4050 Hz")
                                     : QStringLiteral("wie jetzt"), -22)
                       .arg(vor, 6, 'f', 2).arg(nach, 6, 'f', 2)
                       .arg(nach - vor, 5, 'f', 2);
        }
    }

    // ── Wo der Druck verloren geht ──────────────────────────────────
    //
    // „zu wenig druck in der stimme."
    //
    // Druck ist keine Lautstärke. Druck ist LAUTHEIT BEI GLEICHER
    // SPITZE -- wie viel Stimme pro Dezibel Aussteuerung ankommt. Die
    // Zahl dafür ist die Lautheit nach BS.1770, gemessen NACHDEM
    // beide Fassungen auf dieselbe Spitze gebracht wurden. Alles
    // andere vergleicht Äpfel mit lauteren Äpfeln.
    //
    // Hier geht jede Stufe kumulativ dazu, in Kettenreihenfolge, und
    // es wird sichtbar, welche davon Druck macht und welche nur
    // lauter.
    void whereTheDensityIsLost()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        // Auf -2 dBFS Spitze normieren, DANN Lautheit messen. So
        // vergleicht man Dichte und nicht Verstärkung.
        const auto druck = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (p > 1e-9) {
                const float g = float(std::pow(10.0, -2.0 / 20.0) / p);
                for (float& x : n) { x *= g; }
            }
            return Loudness::measure(n.data(), int(n.size()), int(kRate));
        };

        const auto scheitel = [&rede](const std::vector<float>& v) {
            double p = 0.0;
            double s = 0.0;
            size_t n = 0;
            for (size_t i = 0; i < v.size(); ++i) {
                if (rede.spricht[i] == 0) { continue; }
                p = std::max(p, double(std::fabs(v[i])));
                s += double(v[i]) * double(v[i]); ++n;
            }
            return dB(p) - dB(std::sqrt(s / std::max<size_t>(1, n)));
        };

        const auto r0 = druck(rede.signal);
        qInfo().noquote()
            << QStringLiteral("ROH                 Lautheit %1 LUFS bei "
                              "-2 dBFS · Scheitelfaktor %2 dB · Umfang "
                              "%3 LU")
                   .arg(r0.integratedLufs, 6, 'f', 1)
                   .arg(scheitel(rede.signal), 5, 'f', 1)
                   .arg(r0.rangeLu, 4, 'f', 1);

        qInfo().noquote() << QStringLiteral(
            "                    Lautheit   Δ Druck   Scheitel   Umfang");
        double vorher = r0.integratedLufs;
        for (int bis = 0; bis < StripChain::kStageCount; ++bis) {
            StripChain c;
            empfehlung(c);
            // Alles ABSCHALTEN, was hinter `bis` steht; was davor
            // steht, bleibt so, wie die Empfehlung es gesetzt hat.
            for (int i = bis + 1; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            const auto y = durch(c, rede.signal);
            const auto m = druck(y);
            qInfo().noquote()
                << QStringLiteral("  + %1   %2   %3   %4   %5")
                       .arg(QString::fromLatin1(StripChain::stageName(
                                static_cast<StripChain::Stage>(bis))), -14)
                       .arg(m.integratedLufs, 7, 'f', 1)
                       .arg(m.integratedLufs - vorher, 7, 'f', 1)
                       .arg(scheitel(y), 7, 'f', 1)
                       .arg(m.rangeLu, 6, 'f', 1);
            vorher = m.integratedLufs;
        }

        // Was ohne Zweifel falsch wäre: eine Kette, die bei gleicher
        // Spitze NICHT lauter ist als das rohe Signal. Dann wäre
        // jede Stufe darin Zierde.
        StripChain ganz;
        empfehlung(ganz);
        const auto fertig = druck(durch(ganz, rede.signal));
        qInfo().noquote()
            << QStringLiteral("GANZE KETTE: %1 -> %2 LUFS, also %3 dB mehr "
                              "Druck bei gleicher Spitze")
                   .arg(r0.integratedLufs, 0, 'f', 1)
                   .arg(fertig.integratedLufs, 0, 'f', 1)
                   .arg(fertig.integratedLufs - r0.integratedLufs, 0, 'f', 1);
        QVERIFY2(fertig.integratedLufs > r0.integratedLufs + 3.0,
                 qPrintable(QStringLiteral("die Kette bringt nur %1 dB "
                                           "mehr Druck")
                                .arg(fertig.integratedLufs
                                     - r0.integratedLufs, 0, 'f', 1)));
    }

    // ── Wieviel Druck noch auf dem Tisch liegt ──────────────────────
    //
    // Dieselbe Kette, nur mit anderen Zahlen an den drei Hebeln, die
    // überhaupt Dichte machen: Kompressorschwelle, Clipper-Drive und
    // der Eingangspegel in den Begrenzer. Gemessen wird wieder
    // Lautheit bei -2 dBFS Spitze.
    void howMuchDensityIsStillOnTheTable()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        const auto druck = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (p > 1e-9) {
                const float g = float(std::pow(10.0, -2.0 / 20.0) / p);
                for (float& x : n) { x *= g; }
            }
            return Loudness::measure(n.data(), int(n.size()),
                                     int(kRate)).integratedLufs;
        };
        const auto scheitel = [&rede](const std::vector<float>& v) {
            double p = 0.0;
            double s = 0.0;
            size_t n = 0;
            for (size_t i = 0; i < v.size(); ++i) {
                if (rede.spricht[i] == 0) { continue; }
                p = std::max(p, double(std::fabs(v[i])));
                s += double(v[i]) * double(v[i]); ++n;
            }
            return dB(p) - dB(std::sqrt(s / std::max<size_t>(1, n)));
        };

        qInfo().noquote() << QStringLiteral(
            "  Schwelle  Verhältnis  Drive   Lautheit   Scheitel");
        struct Fall { double schwelle; double verh; double drive; };
        const Fall faelle[] = {
            { -22.0, 3.0, 12.0 },     // die Empfehlung von heute
            { -22.0, 3.0, 18.0 },
            { -28.0, 4.0, 12.0 },
            { -28.0, 4.0, 18.0 },
            { -34.0, 6.0, 18.0 },
            { -34.0, 6.0, 24.0 },
            { -40.0, 8.0, 24.0 },
        };
        for (const Fall& f : faelle) {
            StripChain c;
            empfehlung(c);
            c.comp().setThresholdDb(float(f.schwelle));
            c.comp().setRatio(float(f.verh));
            c.clipper().setDriveDb(float(f.drive));
            const auto y = durch(c, rede.signal);
            qInfo().noquote()
                << QStringLiteral("   %1 dB      %2:1    %3 dB   %4 LUFS   "
                                  "%5 dB")
                       .arg(f.schwelle, 5, 'f', 0).arg(f.verh, 3, 'f', 0)
                       .arg(f.drive, 4, 'f', 0)
                       .arg(druck(y), 7, 'f', 1)
                       .arg(scheitel(y), 6, 'f', 1);
        }
    }

    // ── Die Umkehrung: mehr Kompression, weniger Druck ──────────────
    //
    // Die Reihe oben zeigt etwas, das nicht sein darf: Schwelle -40
    // mit 8:1 ist DÜNNER als Schwelle -22 mit 3:1. Der Regler tut das
    // Gegenteil von dem, was draufsteht.
    //
    // Die Ursache steht in zwei Zeilen Quelltext:
    //   ClientComp:    makeupDb hat die Vorgabe 0,0 und wird nicht
    //                  nachgeführt -- Kompression macht also leiser.
    //   ClientClipper: `const float x = samples[i] * drive;` und die
    //                  Kante liegt bei VOLLAUSSTEUERUNG. Wieviel der
    //                  Clipper abschneidet, hängt also vollständig am
    //                  Pegel, der bei ihm ankommt.
    //
    // Zusammen: wer den Kompressor aufdreht, nimmt dem Clipper den
    // Pegel weg, und der Clipper ist die Stufe, die den Druck macht.
    //
    // Hier die Gegenprobe: dieselben Einstellungen, nur mit einem
    // Makeup, das die Rücknahme ausgleicht.
    void autoMakeupWouldStopTheInversion()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        const auto druck = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (p > 1e-9) {
                const float g = float(std::pow(10.0, -2.0 / 20.0) / p);
                for (float& x : n) { x *= g; }
            }
            return Loudness::measure(n.data(), int(n.size()),
                                     int(kRate)).integratedLufs;
        };

        qInfo().noquote() << QStringLiteral(
            "  Schwelle  Verh.   Makeup fest 3 dB   Makeup ausgeglichen");
        struct Fall { double schwelle; double verh; };
        const Fall faelle[] = {
            { -22.0, 3.0 }, { -28.0, 4.0 }, { -34.0, 6.0 }, { -40.0, 8.0 },
        };
        for (const Fall& f : faelle) {
            double werte[2] = { 0.0, 0.0 };
            for (int art = 0; art < 2; ++art) {
                StripChain c;
                empfehlung(c);
                c.comp().setThresholdDb(float(f.schwelle));
                c.comp().setRatio(float(f.verh));
                if (art == 1) {
                    // Der Ausgleich, den ein Auto-Makeup rechnen
                    // würde: was die Kennlinie einer Spitze von
                    // -12 dBFS wegnimmt, kommt wieder drauf.
                    const double ueber = -12.0 - f.schwelle;
                    const double weg = ueber * (1.0 - 1.0 / f.verh);
                    c.comp().setMakeupDb(float(std::max(0.0, weg)));
                }
                werte[art] = druck(durch(c, rede.signal));
            }
            qInfo().noquote()
                << QStringLiteral("   %1 dB   %2:1        %3 LUFS            "
                                  "%4 LUFS")
                       .arg(f.schwelle, 5, 'f', 0).arg(f.verh, 3, 'f', 0)
                       .arg(werte[0], 6, 'f', 1).arg(werte[1], 6, 'f', 1);
        }
    }

    // ── Wieviel Drive der Clipper verträgt ──────────────────────────
    //
    // Druck gegen Klirr. Der Clipper ist die Stufe, die Dichte macht;
    // die Frage ist nur, wo es anfängt weh zu tun.
    void howMuchDriveTheClipperTakes()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        const auto druck = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (p > 1e-9) {
                const float g = float(std::pow(10.0, -2.0 / 20.0) / p);
                for (float& x : n) { x *= g; }
            }
            return Loudness::measure(n.data(), int(n.size()),
                                     int(kRate)).integratedLufs;
        };
        const auto scheitel = [&rede](const std::vector<float>& v) {
            double p = 0.0;
            double s = 0.0;
            size_t n = 0;
            for (size_t i = 0; i < v.size(); ++i) {
                if (rede.spricht[i] == 0) { continue; }
                p = std::max(p, double(std::fabs(v[i])));
                s += double(v[i]) * double(v[i]); ++n;
            }
            return dB(p) - dB(std::sqrt(s / std::max<size_t>(1, n)));
        };

        qInfo().noquote() << QStringLiteral(
            "  Drive   Lautheit   Scheitel   abgeschnitten   Klirr bei 1 kHz");
        for (double drive : { 0.0, 6.0, 12.0, 15.0, 18.0, 21.0, 24.0 }) {
            StripChain c;
            empfehlung(c);
            c.clipper().setDriveDb(float(drive));

            std::vector<float> y = rede.signal;
            double schnitt = 0.0;
            for (size_t at = 0; at + 240 <= y.size(); at += 240) {
                c.processMono(y.data() + at, 240);
                schnitt = std::max(schnitt,
                                   double(c.clipper().clippedDb()));
            }

            // Klirr: ein Ton auf dem Pegel, auf dem die Sprache
            // steht, durch dieselbe Kette.
            StripChain t;
            empfehlung(t);
            t.clipper().setDriveDb(float(drive));
            const int n = int(kRate);
            std::vector<float> ton(size_t(n), 0.0f);
            for (int i = 0; i < n; ++i) {
                ton[size_t(i)] = float(0.25 * std::sin(2.0 * M_PI * 1000.0
                                                       * i / kRate));
            }
            const auto ty = durch(t, ton);
            const auto pT = spektrum(ty);
            const double grund = bandDb(pT, 950.0, 1050.0);
            double rest = 0.0;
            for (int h = 2; h <= 8; ++h) {
                const double f = 1000.0 * h;
                rest += std::pow(10.0, bandDb(pT, f - 60.0, f + 60.0) / 10.0);
            }
            const double klirr = 10.0 * std::log10(std::max(rest, 1e-20))
                                 - grund;

            qInfo().noquote()
                << QStringLiteral("  %1 dB   %2 LUFS   %3 dB   %4 dB   %5 dB")
                       .arg(drive, 5, 'f', 0)
                       .arg(druck(y), 7, 'f', 1)
                       .arg(scheitel(y), 7, 'f', 1)
                       .arg(schnitt, 10, 'f', 1)
                       .arg(klirr, 12, 'f', 1);
        }
    }

    // ── Clippen, filtern, DANN begrenzen ────────────────────────────
    //
    // Der Ueberschwinger ist mit der Dichte mitgewachsen: 1,89 dB bei
    // Scheitelfaktor 12,9, aber 5,35 dB bei 8,2. Das ist kein Fehler,
    // sondern die Folge: je haerter geclippt wird, desto mehr Energie
    // entsteht ausserhalb des Kanals, und desto mehr nimmt das Filter
    // des Senders anschliessend wieder weg -- und genau diese Energie
    // hat die Spitzen flach gehalten.
    //
    // Der Rundfunk macht es deshalb in dieser Reihenfolge: clippen,
    // bandbegrenzen, DANN begrenzen. Der Begrenzer sieht dann die
    // Wellenform, die wirklich gesendet wird, und kann sie halten.
    //
    // Hier wird nur gemessen, ob das traegt. Gebaut wird es nicht von
    // selbst: eine Bandgrenze in der Kette macht das Mithoeren
    // dunkler, und das ist eine Entscheidung des Betreibers.
    void clipThenFilterThenLimit()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        const auto spitze = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            return dB(p);
        };
        const auto druck = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (p > 1e-9) {
                const float g = float(std::pow(10.0, -2.0 / 20.0) / p);
                for (float& x : n) { x *= g; }
            }
            return Loudness::measure(n.data(), int(n.size()),
                                     int(kRate)).integratedLufs;
        };

        // A: wie jetzt -- Begrenzer in der Kette, Kanal danach.
        StripChain a;
        empfehlung(a);
        const auto wieJetzt = durchKanal(3, durch(a, rede.signal));

        // B: Kette OHNE Begrenzer -> Kanal -> Begrenzer.
        StripChain b;
        empfehlung(b);
        b.setStageEnabled(StripChain::Stage::Limiter, false);
        auto roh = durchKanal(3, durch(b, rede.signal));
        {
            ClientFinalLimiter lim;
            lim.prepare(kRate);
            lim.setEnabled(true);
            lim.setCeilingDb(-2.0f);
            for (size_t at = 0; at + 240 <= roh.size(); at += 240) {
                lim.process(roh.data() + at, 240, 1);
            }
        }

        qInfo().noquote()
            << QStringLiteral("A  Kette -> Kanal:            Spitze %1 dBFS "
                              "· Druck %2 LUFS")
                   .arg(spitze(wieJetzt), 6, 'f', 2)
                   .arg(druck(wieJetzt), 6, 'f', 1);
        qInfo().noquote()
            << QStringLiteral("B  Kette ohne Begrenzer -> Kanal -> "
                              "Begrenzer: Spitze %1 dBFS · Druck %2 LUFS")
                   .arg(spitze(roh), 6, 'f', 2)
                   .arg(druck(roh), 6, 'f', 1);

        // Keine Pruefung mit Urteil -- das ist eine Messung fuer eine
        // Entscheidung, nicht eine Zusicherung.
        qInfo().noquote() << QStringLiteral(
            "   (B ist die Reihenfolge, die der Rundfunk benutzt; A ist "
            "die, die wir haben)");
    }

    // ── Der Eingangspegel, von selbst ───────────────────────────────
    //
    // „mir geht die kraft als vorverstärker ab."
    //
    // Am Gerät des Betreibers stand: Eingang -39,6 dBFS · Ziel
    // -12 dBFS · 28 dB fehlen · Regler auf 0,0 dB. Jede Schwelle der
    // Kette ist auf -12 gerechnet; bei -39,6 liegt die Gate-Schwelle
    // ÜBER der Stimme. Die Kette konnte gar nicht arbeiten.
    //
    // Hier wird geprüft, dass sie den Punkt allein findet -- und,
    // genauso wichtig, dass sie in der Stille stillsteht.
    void theInputFindsItsOwnWorkingPoint()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }

        // Die Rede 28 dB zu leise, so wie sie bei ihm ankommt.
        Rede rede = redeMitRauschen(sauber, -48.0);
        const float zuLeise = float(std::pow(10.0, -27.6 / 20.0));
        for (float& x : rede.signal) { x *= zuLeise; }

        StripChain c;
        c.prepare(kRate);
        c.setEnabled(true);
        c.setInputAuto(true);

        // Mehrfach durchlaufen lassen: 12,3 s Rede sind sechs
        // Zeitfenster, und der Schritt ist ein Drittel des Fehlers.
        std::vector<double> verlauf;
        for (int runde = 0; runde < 4; ++runde) {
            std::vector<float> y = rede.signal;
            for (size_t at = 0; at + 240 <= y.size(); at += 240) {
                c.processMono(y.data() + at, 240);
            }
            verlauf.push_back(double(c.effectiveInputGainDb()));
        }
        qInfo().noquote()
            << QStringLiteral("Gefundene Verstärkung je Durchlauf: "
                              "%1 · %2 · %3 · %4 dB (gebraucht: 27,6)")
                   .arg(verlauf[0], 0, 'f', 1).arg(verlauf[1], 0, 'f', 1)
                   .arg(verlauf[2], 0, 'f', 1).arg(verlauf[3], 0, 'f', 1);

        QVERIFY2(std::fabs(verlauf.back() - 27.6) < 3.0,
                 qPrintable(QStringLiteral("landet bei %1 dB statt bei "
                                           "27,6").arg(verlauf.back())));

        // Und jetzt Stille. Die gefundene Zahl darf sich nicht mehr
        // rühren -- sonst zieht sie das Grundgeräusch hoch, bis die
        // Kette es für eine Stimme hält.
        const double vorStille = double(c.effectiveInputGainDb());
        std::vector<float> stille(size_t(kRate * 20), 0.0f);
        std::mt19937 w(31337u);
        std::normal_distribution<double> g(0.0, 1.0);
        const double rausch = std::pow(10.0, -75.0 / 20.0);
        for (float& x : stille) { x = float(g(w) * rausch); }
        for (size_t at = 0; at + 240 <= stille.size(); at += 240) {
            c.processMono(stille.data() + at, 240);
        }
        const double nachStille = double(c.effectiveInputGainDb());
        qInfo().noquote()
            << QStringLiteral("Nach 20 s Stille: %1 -> %2 dB")
                   .arg(vorStille, 0, 'f', 2).arg(nachStille, 0, 'f', 2);
        QVERIFY2(std::fabs(nachStille - vorStille) < 0.01,
                 "die Automatik läuft in der Stille weiter");
    }

    // ── Und was sie an der ganzen Kette ändert ──────────────────────
    void theQuietMicrophoneThroughTheWholeChain()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        Rede rede = redeMitRauschen(sauber, -48.0);
        const float zuLeise = float(std::pow(10.0, -27.6 / 20.0));
        for (float& x : rede.signal) { x *= zuLeise; }

        const auto druck = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (p > 1e-9) {
                const float gg = float(std::pow(10.0, -2.0 / 20.0) / p);
                for (float& x : n) { x *= gg; }
            }
            return Loudness::measure(n.data(), int(n.size()),
                                     int(kRate)).integratedLufs;
        };
        const auto spitze = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            return dB(p);
        };

        for (int autoAn = 0; autoAn < 2; ++autoAn) {
            StripChain c;
            empfehlung(c);
            c.setInputAuto(autoAn != 0);
            // Zweimal durch, damit die Automatik ihren Punkt hat.
            std::vector<float> y;
            for (int runde = 0; runde < 3; ++runde) { y = durch(c, rede.signal); }
            qInfo().noquote()
                << QStringLiteral("Mikrofon 28 dB zu leise, Automatik %1: "
                                  "Spitze %2 dBFS · Druck %3 LUFS · "
                                  "Verstärkung %4 dB")
                       .arg(autoAn != 0 ? QStringLiteral("AN ")
                                        : QStringLiteral("AUS"))
                       .arg(spitze(y), 6, 'f', 1).arg(druck(y), 6, 'f', 1)
                       .arg(double(c.effectiveInputGainDb()), 5, 'f', 1);
        }
    }

    // ── Was die Kette WEGNIMMT, Band für Band ───────────────────────
    //
    // „also roh, volle bandbreite — auf dem signal sollte man
    //  aufbauen … aber die abstriche sind leider viel zu groß."
    //
    // Bis hierher wurde gemessen, was die Kette BRINGT: Dichte,
    // Störabstand, Scheitelfaktor. Diese Reihe fragt das Gegenteil
    // und misst es am selben Maßstab: bei gleicher Lautheit, welche
    // Bänder fehlen gegenüber dem rohen Signal?
    //
    // Gleiche Lautheit, nicht gleiche Spitze: wer vergleichen will,
    // was eine Kette WEGNIMMT, darf nicht gegen ihre Verstärkung
    // messen.
    void whatTheChainTakesAway()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        // Auf gleiche Lautheit bringen, dann Spektrum vergleichen.
        const auto aufLautheit = [](const std::vector<float>& v,
                                    double zielLufs) {
            const double ist = Loudness::measure(v.data(), int(v.size()),
                                                 int(kRate)).integratedLufs;
            std::vector<float> n = v;
            const float g = float(std::pow(10.0, (zielLufs - ist) / 20.0));
            for (float& x : n) { x *= g; }
            return n;
        };
        const double zielLufs =
            Loudness::measure(rede.signal.data(), int(rede.signal.size()),
                              int(kRate)).integratedLufs;

        struct Band { const char* name; double von; double bis; };
        const Band baender[] = {
            {  "63 Hz",  56.0,   71.0 },
            {  "80 Hz",  71.0,   89.0 },
            { "100 Hz",  89.0,  112.0 },
            { "125 Hz", 112.0,  141.0 },
            { "160 Hz", 141.0,  178.0 },
            { "200 Hz", 178.0,  224.0 },
            { "250 Hz", 224.0,  282.0 },
            { "315 Hz", 282.0,  355.0 },
            { "400 Hz", 355.0,  447.0 },
            { "630 Hz", 562.0,  708.0 },
            {  "1 kHz", 891.0, 1122.0 },
            {  "2 kHz",1778.0, 2239.0 },
            {  "4 kHz",3548.0, 4467.0 },
            {"6,3 kHz",5623.0, 7079.0 },
        };

        const auto pRoh = spektrum(rede.signal);

        StripChain ganz;
        empfehlung(ganz);
        const auto pKette = spektrum(aufLautheit(durch(ganz, rede.signal),
                                                 zielLufs));

        // Und dieselbe Kette OHNE den Entzerrer, damit man sieht,
        // was davon der Entzerrer ist und was der Rest.
        StripChain ohneEq;
        empfehlung(ohneEq);
        ohneEq.setStageEnabled(StripChain::Stage::Eq, false);
        const auto pOhneEq = spektrum(aufLautheit(durch(ohneEq, rede.signal),
                                                  zielLufs));

        qInfo().noquote() << QStringLiteral(
            "   Band     Kette   ohne Entzerrer   (dB gegen roh, bei "
            "gleicher Lautheit)");
        double schlimmste = 0.0;
        QString woBand;
        for (const Band& b : baender) {
            const double r = bandDb(pRoh, b.von, b.bis);
            const double k = bandDb(pKette, b.von, b.bis) - r;
            const double o = bandDb(pOhneEq, b.von, b.bis) - r;
            if (k < schlimmste) { schlimmste = k; woBand = QString::fromLatin1(b.name); }
            qInfo().noquote()
                << QStringLiteral("  %1   %2   %3")
                       .arg(QString::fromLatin1(b.name), -8)
                       .arg(k, 7, 'f', 1).arg(o, 10, 'f', 1);
        }
        qInfo().noquote()
            << QStringLiteral("Größter Abstrich: %1 dB bei %2")
                   .arg(schlimmste, 0, 'f', 1).arg(woBand);
    }

    // ── Roh und Kette müssen gleich lange brauchen ──────────────────
    //
    // Der Vergleich, mit dem der Betreiber drei Tage lang geurteilt
    // hat, war nicht fair: beim Umschalten auf „roh" fiel die
    // Laufzeit der Kette weg. 30 ms sind beim Mithören der eigenen
    // Stimme kein Nichts -- man hört sich selbst mit Nachschlag, und
    // das klingt IMMER schlechter, unabhängig davon, was die Kette
    // mit dem Klang macht.
    //
    // Geprüft wird deshalb die Laufzeit beider Wege, nicht die
    // Anwesenheit einer Verzögerungsleitung.
    void theBypassedPathWaitsJustAsLong()
    {
        StripLiveMonitor m;
        StripChain* c = m.chain();
        QVERIFY(c != nullptr);
        c->prepare(kRate);
        c->setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c->setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        // Nur das Entrauschen, und auf 0 gemischt: dann ist der
        // Ausgang der Eingang, nur später -- der saubere Maßstab.
        c->nr().setAmount(0.0f);
        c->setStageEnabled(StripChain::Stage::Denoise, true);
        if (c->nr().latencySamples() <= 0) { QSKIP("kein rnnoise-Modell"); }

        const auto laufzeit = [&m](bool roh) {
            m.setStripBypassed(roh);
            const int n = int(kRate / 2);
            std::vector<float> x(size_t(n), 0.0f);
            for (int i = 0; i < 480; ++i) {
                x[size_t(4800 + i)] =
                    float(0.5 * std::sin(2.0 * M_PI * 1000.0 * i / kRate));
            }
            for (size_t at = 0; at + 960 <= x.size(); at += 960) {
                m.applyStrip(x.data() + at, 960);
            }
            size_t wo = 0;
            double p = 0.0;
            for (size_t i = 0; i < x.size(); ++i) {
                if (std::fabs(double(x[i])) > p) {
                    p = std::fabs(double(x[i])); wo = i;
                }
            }
            return double(wo) - 4800.0;
        };

        const double durchKette = laufzeit(false);
        const double rohWeg     = laufzeit(true);
        qInfo().noquote()
            << QStringLiteral("Laufzeit: durch die Kette %1 Abtastwerte "
                              "(%2 ms) · roh %3 (%4 ms)")
                   .arg(durchKette).arg(durchKette * 1000.0 / kRate, 0, 'f', 1)
                   .arg(rohWeg).arg(rohWeg * 1000.0 / kRate, 0, 'f', 1);

        QVERIFY2(std::fabs(durchKette - rohWeg) < 48.0,
                 qPrintable(QStringLiteral("der Vergleich ist unfair: "
                                           "Kette %1, roh %2 Abtastwerte")
                                .arg(durchKette).arg(rohWeg)));
    }

    // ── Der versteckte Dichtehebel: Drive im Kompressor ─────────────
    //
    // AetherSDR hat genau einen Dichtesteller, und es ist nicht der
    // Clipper: Drive im Kompressor, 0..18 dB, nach dem Optimod-Modell
    // -- hinein verstärken, am Ausgang gleich viel zurück. Netto also
    // keine Lautstärke, sondern mehr Material in der Kennlinie.
    // Longpaths Kompressor kann das Stück für Stück identisch
    // (ClientComp.cpp: driveLin vor der Schleife, derselbe Faktor
    // wieder in gainLin), aber es gibt keinen Knopf dafür und kein
    // Profil setzt den Wert.
    //
    // Bevor einer gebaut wird: was bringt er, und was kostet er?
    void theCompressorsHiddenDrive()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -48.0);

        const auto druck = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (p > 1e-9) {
                const float gg = float(std::pow(10.0, -2.0 / 20.0) / p);
                for (float& x : n) { x *= gg; }
            }
            return Loudness::measure(n.data(), int(n.size()),
                                     int(kRate)).integratedLufs;
        };
        const auto scheitel = [&rede](const std::vector<float>& v) {
            double p = 0.0;
            double s = 0.0;
            size_t n = 0;
            for (size_t i = 0; i < v.size(); ++i) {
                if (rede.spricht[i] == 0) { continue; }
                p = std::max(p, double(std::fabs(v[i])));
                s += double(v[i]) * double(v[i]); ++n;
            }
            return dB(p) - dB(std::sqrt(s / std::max<size_t>(1, n)));
        };

        // Klirr an einem Ton auf Sprachpegel, durch dieselbe Kette --
        // derselbe Maßstab wie beim Clipper, damit die beiden
        // Dichtewege vergleichbar sind.
        const auto klirr = [](StripChain& c) {
            const int n = int(kRate);
            std::vector<float> ton(size_t(n), 0.0f);
            for (int i = 0; i < n; ++i) {
                ton[size_t(i)] = float(0.25 * std::sin(2.0 * M_PI * 1000.0
                                                       * i / kRate));
            }
            const auto p = spektrum(durch(c, ton));
            const double grund = bandDb(p, 950.0, 1050.0);
            double rest = 0.0;
            for (int h = 2; h <= 8; ++h) {
                const double f = 1000.0 * h;
                rest += std::pow(10.0, bandDb(p, f - 60.0, f + 60.0) / 10.0);
            }
            return 10.0 * std::log10(std::max(rest, 1e-20)) - grund;
        };

        qInfo().noquote() << QStringLiteral(
            "  Drive   Lautheit   Scheitel   Rücknahme   verst. Begrenzer   "
            "Klirr");
        for (double drive : { 0.0, 3.0, 6.0, 9.0, 12.0, 18.0 }) {
            StripChain c;
            empfehlung(c);
            c.comp().setDriveDb(float(drive));
            std::vector<float> y = rede.signal;
            double gr = 0.0;
            double limGr = 0.0;
            for (size_t at = 0; at + 240 <= y.size(); at += 240) {
                c.processMono(y.data() + at, 240);
                gr = std::min(gr, double(c.comp().gainReductionDb()));
                limGr = std::min(limGr,
                                 double(c.comp().limiterGrDb()));
            }
            StripChain t;
            empfehlung(t);
            t.comp().setDriveDb(float(drive));
            qInfo().noquote()
                << QStringLiteral("  %1 dB   %2 LUFS   %3 dB   %4 dB   "
                                  "%5 dB   %6 dB")
                       .arg(drive, 5, 'f', 0)
                       .arg(druck(y), 7, 'f', 1)
                       .arg(scheitel(y), 7, 'f', 1)
                       .arg(gr, 8, 'f', 1).arg(limGr, 13, 'f', 1)
                       .arg(klirr(t), 7, 'f', 1);
        }
        qInfo().noquote() << QStringLiteral(
            "   (die letzte Spalte ist das versteckte Brickwall IM "
            "Kompressor, Decke -1 dBFS, ohne Knopf)");
    }

    // ── Nach dem Zurücksetzen darf nichts nachlaufen ────────────────
    //
    // Das Entrauschen hält eine Verzögerungsleitung über drei Rahmen,
    // also 1440 Abtastwerte. StripChain::reset() hat es bis zum
    // 2026-10-10 nicht angefasst -- die nächste Durchsage begann
    // deshalb mit 30 ms der vorigen. Das hört man nicht als Fehler,
    // sondern als Knacken, das niemand zuordnet.
    void nothingRunsOnAfterAReset()
    {
        StripChain c;
        c.prepare(kRate);
        c.setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        c.nr().setAmount(0.0f);
        c.setStageEnabled(StripChain::Stage::Denoise, true);
        if (c.nr().latencySamples() <= 0) { QSKIP("kein rnnoise-Modell"); }

        std::vector<float> laut(size_t(kRate / 4), 0.0f);
        for (size_t i = 0; i < laut.size(); ++i) {
            laut[i] = float(0.5 * std::sin(2.0 * M_PI * 1000.0
                                           * double(i) / kRate));
        }
        for (size_t at = 0; at + 240 <= laut.size(); at += 240) {
            c.processMono(laut.data() + at, 240);
        }

        c.reset();

        std::vector<float> stille(size_t(kRate / 4), 0.0f);
        for (size_t at = 0; at + 240 <= stille.size(); at += 240) {
            c.processMono(stille.data() + at, 240);
        }
        double p = 0.0;
        for (float v : stille) { p = std::max(p, double(std::fabs(v))); }
        qInfo().noquote()
            << QStringLiteral("Nach dem Zurücksetzen läuft noch %1 dBFS "
                              "nach").arg(dB(p), 0, 'f', 1);
        QVERIFY2(p < 1e-6,
                 qPrintable(QStringLiteral("%1 dBFS laufen nach -- eine "
                                           "Stufe hält ihren Zustand")
                                .arg(dB(p), 0, 'f', 1)));
    }

    // ── Ein Begrenzer auf einem Signal, das keiner bestellt hat ─────
    //
    // „volle bandbreite läuft perfekt über apollo und longpath,
    //  sobald 4000k eingeschalten ist wird die qualität massiv
    //  anders und schlechter."
    //
    // Der Mithörweg legt seit heute einen Begrenzer HINTER das
    // Kanalfilter -- richtig gemessen und richtig begründet, solange
    // die Kette läuft. Die Bedingung dafür lautete aber nur „ein
    // Kanal ist gewählt" und nicht „die Kette ist eingeschaltet".
    //
    // Folge: wer den Kanalzug ausschaltet, um sein unbearbeitetes
    // Signal zu hören, und dabei einen Kanal wählt, bekommt einen
    // Brickwall bei -2 dBFS mit harter Kappung auf ein Signal, das
    // schon aus einem Studiovorverstärker kommt und entsprechend
    // laut ist. Bei „roh (volle Bandbreite)" läuft er nicht -- genau
    // der Unterschied, den der Betreiber beschreibt.
    void aBypassedStripMustNotAddALimiter()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        Rede rede = redeMitRauschen(sauber, -60.0);

        // So laut, wie es aus einem Apollo mit LA-2 und 610 kommt:
        // Spitzen knapp unter Vollaussteuerung.
        double p0 = 0.0;
        for (float x : rede.signal) { p0 = std::max(p0, double(std::fabs(x))); }
        const float heiss = float(std::pow(10.0, -1.5 / 20.0) / p0);
        for (float& x : rede.signal) { x *= heiss; }

        const auto klirrVon = [](const std::vector<float>& v,
                                 const std::vector<float>& ref) {
            // Wieviel Energie ist dazugekommen, die im Original nicht
            // war -- der Rest nach Abzug des besten Pegelabgleichs.
            double zaehler = 0.0;
            double nenner = 0.0;
            for (size_t i = 0; i < v.size(); ++i) {
                zaehler += double(v[i]) * double(ref[i]);
                nenner  += double(ref[i]) * double(ref[i]);
            }
            const double a = zaehler / std::max(nenner, 1e-20);
            double rest = 0.0;
            double sig = 0.0;
            for (size_t i = 0; i < v.size(); ++i) {
                const double d = double(v[i]) - a * double(ref[i]);
                rest += d * d;
                sig  += double(v[i]) * double(v[i]);
            }
            return 10.0 * std::log10(std::max(rest, 1e-20)
                                     / std::max(sig, 1e-20));
        };

        // A: nur das Kanalfilter -- so, wie es sein soll.
        const auto nurKanal = durchKanal(3, rede.signal);

        // B: Kanalfilter plus der Begrenzer, wie er heute läuft.
        auto mitBegrenzer = nurKanal;
        {
            ClientFinalLimiter lim;
            lim.prepare(kRate);
            lim.setEnabled(true);
            lim.setCeilingDb(-2.0f);
            for (size_t at = 0; at + 240 <= mitBegrenzer.size(); at += 240) {
                lim.process(mitBegrenzer.data() + at, 240, 1);
            }
        }

        const auto spitze = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float x : v) { p = std::max(p, double(std::fabs(x))); }
            return dB(p);
        };
        const double schaden = klirrVon(mitBegrenzer, nurKanal);
        qInfo().noquote()
            << QStringLiteral("Apollo-Signal, Spitze %1 dBFS · nur Kanal: "
                              "%2 dBFS · mit Begrenzer: %3 dBFS · "
                              "Fremdanteil %4 dB")
                   .arg(spitze(rede.signal), 0, 'f', 1)
                   .arg(spitze(nurKanal), 0, 'f', 1)
                   .arg(spitze(mitBegrenzer), 0, 'f', 1)
                   .arg(schaden, 0, 'f', 1);

        // Gemessen wurde hier, WAS der Begrenzer auf einem solchen
        // Signal anrichtet -- 19,7 dB Fremdanteil, also rund zehn
        // Prozent. Die Behebung ist nicht, ihn zu zaehmen, sondern
        // ihn bei ueberbrueckter Kette gar nicht erst laufen zu
        // lassen (StripLiveMonitor::pump). Diese Zahl bleibt als
        // Begruendung stehen, und die Pruefung dazu steht in
        // theBypassedPathAddsNothingAtAll().
        qInfo().noquote() << QStringLiteral(
            "   (deshalb laeuft er bei ueberbrueckter Kette nicht mehr)");
        QVERIFY2(schaden < -10.0,
                 qPrintable(QStringLiteral("der Begrenzer legt %1 dB "
                                           "Fremdanteil auf ein Signal, "
                                           "das niemand bearbeiten wollte")
                                .arg(schaden, 0, 'f', 1)));
    }

    // ── Was das Kanalfilter mit der ZEIT macht ──────────────────────
    //
    // „4k sollte ähnlich sein wie volle bandbreite, ist es aber
    //  nicht." -- und das schon mit dem alten USB-Mikrofon, also
    //  lange vor allem, was heute gebaut wurde.
    //
    // Der Pegelgang ist nachgemessen und tadellos: Ecken auf ein
    // Zehntelprozent, Durchlass flach auf 0,02 dB. Ein Filter hat
    // aber zwei Seiten, und die zweite wurde nie angesehen: die
    // Gruppenlaufzeit. Acht Pole bei 50 Hz stehen sehr dicht am
    // Einheitskreis, und was dort an Phase passiert, hoert man als
    // Verschmieren -- nicht als Klangfarbe, sondern als „anders".
    //
    // Gemessen wird sie direkt: ein kurzer Ton je Frequenz, und der
    // Schwerpunkt seiner Huellkurve sagt, wann er herauskommt.
    // ── „bei 4000 klingt das nicht besonders, roh ist eine ganz
    //    andere Liga" (2026-10-13) ─────────────────────────────────
    //
    // Die Frage dahinter ist nicht Geschmack, sondern Zustaendigkeit:
    // liegt der Unterschied an der BANDBREITE (dann gehoert er dem
    // Sender und niemand kann ihn wegnehmen) oder an UNSEREM FILTER
    // (dann gehoert er uns)?
    //
    // Gemessen an echter Sprache, drei Fassungen desselben Signals:
    //   roh          -- nichts getan
    //   unser Kanal  -- BandLimit, IIR, 50-4050
    //   idealer Kanal-- linearphasige Faltung, dieselben Ecken
    //
    // Der Abstand zwischen den letzten beiden ist unser Anteil.
    void whatOurChannelCostsAgainstAPerfectOne()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }

        const auto& k = BandLimit::channels()[3];       // 50 - 4050, ESSB
        const auto unser = durchKanal(3, sauber);
        const auto ideal = durchIdealenKanal(sauber, double(k.lowHz),
                                             double(k.highHz));

        const auto pRoh   = spektrum(sauber);
        const auto pUnser = spektrum(unser);
        const auto pIdeal = spektrum(ideal);

        struct B { const char* name; double von; double bis; };
        static const B kB[] = {
            { "63 Hz",   56,   71   }, { "125 Hz",  112,  141  },
            { "250 Hz",  224,  282  }, { "500 Hz",  447,  562  },
            { "1 kHz",   891,  1122 }, { "2 kHz",   1778, 2239 },
            { "3,15 k",  2818, 3548 }, { "4 kHz",   3548, 4467 },
            { "5 kHz",   4467, 5623 }, { "8 kHz",   7079, 8913 },
        };
        qInfo().noquote() << QStringLiteral(
            "   Band      roh      unser Kanal   idealer Kanal   "
            "wir gegen ideal");
        double groessteAbweichung = 0.0;
        for (const B& b : kB) {
            const double r = bandDb(pRoh,   b.von, b.bis);
            const double u = bandDb(pUnser, b.von, b.bis);
            const double i = bandDb(pIdeal, b.von, b.bis);
            if (b.bis <= double(k.highHz) && b.von >= double(k.lowHz)) {
                groessteAbweichung = std::max(groessteAbweichung,
                                              std::fabs(u - i));
            }
            qInfo().noquote()
                << QStringLiteral("  %1  %2  %3  %4  %5")
                       .arg(QString::fromUtf8(b.name), -8)
                       .arg(r, 8, 'f', 1).arg(u, 12, 'f', 1)
                       .arg(i, 14, 'f', 1).arg(u - i, 14, 'f', 2);
        }

        // Und der Rest, der nach dem Abziehen des idealen Kanals
        // uebrig bleibt -- Phasenfehler zeigen sich hier, auch wenn
        // die Pegel oben stimmen. Der ideale Kanal ist um nichts
        // verzoegert (die Faltung schiebt schon zurueck), unserer um
        // weniger als eine halbe Millisekunde: der Rest ist also
        // ehrlich vergleichbar.
        double eU = 0.0;
        double eD = 0.0;
        const size_t n = std::min(unser.size(), ideal.size());
        for (size_t i = 0; i < n; ++i) {
            const double d = double(unser[i]) - double(ideal[i]);
            eU += double(ideal[i]) * double(ideal[i]);
            eD += d * d;
        }
        const double restDb = 10.0 * std::log10(std::max(eD, 1e-30)
                                                / std::max(eU, 1e-30));
        qInfo().noquote() << QStringLiteral(
            "   grösste Abweichung IM Band: %1 dB · Rest nach Abzug "
            "des idealen Kanals: %2 dB")
            .arg(groessteAbweichung, 0, 'f', 2).arg(restDb, 0, 'f', 1);
        qInfo().noquote() << QStringLiteral(
            "   (Liegt der Rest tief und die Abweichung bei null, "
            "gehört der ganze Unterschied zu „roh\" der Bandbreite "
            "— nicht dem Filter.)");

        QVERIFY2(groessteAbweichung < 1.5,
                 qPrintable(QStringLiteral("unser Kanal weicht im "
                                           "Durchlassbereich um %1 dB vom "
                                           "idealen ab")
                                .arg(groessteAbweichung)));
    }

    // ── Wieviel von SEINEM Zischdämpfer überlebt den Kanal? ─────────
    //
    // Sein Aether-Profil fasst die Zischlaute bei 3836 Hz mit Güte 5
    // an und senkt dort um 15,1 dB. Der ESSB-Kanal macht bei 4050 Hz
    // Schluss, und die Flanke beginnt davor. Die Frage ist dieselbe
    // wie beim Kuhschwanz bei 4982 Hz: stellt er etwas ein, das die
    // Gegenseite gar nicht mehr hört?
    //
    // Gemessen wird an echter Sprache, Terz für Terz, mit und ohne
    // Zischdämpfer -- jeweils DURCH den Kanal.
    void howMuchOfHisDeEsserSurvivesTheChannel()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }

        const Rede r = redeMitRauschen(sauber, -60.0);

        const auto durchDeEss = [&](bool an) {
            ClientDeEss d;
            d.prepare(kRate);
            d.setEnabled(an);
            // Seine Werte, aus SSB_30.07.2026.
            d.setFrequencyHz(3836.18f);
            d.setQ(5.0f);
            d.setThresholdDb(-34.5f);
            d.setAmountDb(-15.12f);
            d.setAttackMs(1.0f);
            d.setReleaseMs(65.03f);
            std::vector<float> x = r.signal;
            for (size_t at = 0; at + 240 <= x.size(); at += 240) {
                d.process(x.data() + at, 240, 1);
            }
            return x;
        };

        const auto ohne = durchDeEss(false);
        const auto mit  = durchDeEss(true);

        // ── Gegenprobe am Werkzeug ──────────────────────────────────
        //
        // Misst die Reihe unten lauter Nullen, kann das zweierlei
        // heissen: der Daempfer ruehrt sich nicht, oder der Prueftand
        // ruft ihn falsch. Also einmal mit einer Schwelle, bei der er
        // sich ruehren MUSS.
        {
            ClientDeEss d;
            d.prepare(kRate);
            d.setEnabled(true);
            d.setFrequencyHz(3836.18f);
            d.setQ(5.0f);
            d.setThresholdDb(-70.0f);
            d.setAmountDb(-15.12f);
            std::vector<float> x = r.signal;
            for (size_t at = 0; at + 240 <= x.size(); at += 240) {
                d.process(x.data() + at, 240, 1);
            }
            // Schmal messen, nicht in Terzen: eine Güte von 5 greift
            // 767 Hz breit, eine Terz ist fast 1000 Hz breit -- der
            // Schnitt verschwindet im Mittelwert.
            const double o = bandDb(spektrum(r.signal), 3700, 4000);
            const double m = bandDb(spektrum(x), 3700, 4000);
            qInfo().noquote() << QStringLiteral(
                "   Gegenprobe (Schwelle -70 dB): 3,7–4,0 k %1 → %2 dB (%3)")
                .arg(o, 0, 'f', 1).arg(m, 0, 'f', 1).arg(o - m, 0, 'f', 2);
            QVERIFY2(o - m > 0.5,
                     "der Prüfstand ruft den Dämpfer falsch -- er rührt "
                     "sich auch bei -70 dB Schwelle nicht");
        }

        struct B { const char* name; double von; double bis; };
        static const B kB[] = {
            { "2 kHz",      1778, 2239 }, { "3,15 k",     2818, 3548 },
            { "3,7–4,0 k",  3700, 4000 },   // dort arbeitet SEIN Dämpfer
            { "4,0–4,4 k",  4000, 4400 }, { "5 kHz",      4467, 5623 },
            { "6,3 k",      5623, 7079 },
        };

        qInfo().noquote() << QStringLiteral(
            "   Band     ohne Kanal: ohne/mit Dämpfer    "
            "durch 50–4050: ohne/mit");
        double groessteRoh = 0.0;
        double groessteImKanal = 0.0;
        for (const B& b : kB) {
            const double o = bandDb(spektrum(ohne), b.von, b.bis);
            const double m = bandDb(spektrum(mit),  b.von, b.bis);
            const double ok = bandDb(spektrum(durchKanal(3, ohne)), b.von, b.bis);
            const double mk = bandDb(spektrum(durchKanal(3, mit)),  b.von, b.bis);
            groessteRoh = std::max(groessteRoh, o - m);
            groessteImKanal = std::max(groessteImKanal, ok - mk);
            qInfo().noquote()
                << QStringLiteral("  %1  %2 → %3 (%4 dB)   %5 → %6 (%7 dB)")
                       .arg(QString::fromUtf8(b.name), -7)
                       .arg(o, 6, 'f', 1).arg(m, 6, 'f', 1).arg(o - m, 5, 'f', 2)
                       .arg(ok, 6, 'f', 1).arg(mk, 6, 'f', 1)
                       .arg(ok - mk, 5, 'f', 2);
        }
        qInfo().noquote() << QStringLiteral(
            "   stärkste Absenkung: roh %1 dB · durch den Kanal %2 dB")
            .arg(groessteRoh, 0, 'f', 2).arg(groessteImKanal, 0, 'f', 2);
        qInfo().noquote() << QStringLiteral(
            "   (Je weiter die zweite Zahl unter der ersten liegt, desto "
            "mehr stellt er etwas ein, das die Gegenseite nicht hört.)");
    }

    // ── „wie groß der unterschied zwischen 4k und volle bandbreite
    //    ist. hast du vorschläge um das zu verbessern" ──────────────
    //
    // Das Filter ist es nicht (siehe
    // whatOurChannelCostsAgainstAPerfectOne: im Band nicht von einem
    // perfekten zu unterscheiden). Was bleibt, ist die Frage, wie man
    // die 4 kHz, die man HAT, besser nutzt.
    //
    // Drei Griffe aus seinem eigenen Profil, die heute über der
    // Kanalgrenze arbeiten und dort nichts mehr ausrichten:
    //
    //   Kuhschwanz 4982 Hz (+2,63 dB) -- die Anhebung liegt fast ganz
    //     über 4050 Hz.
    //   PUDUs „doo" bei 3972 Hz -- der Anreicherer erzeugt Oberwellen
    //     ÜBER seiner Einstellfrequenz, also durchweg draußen.
    //   Zischdämpfer bei 3836 Hz -- arbeitet an der Flanke.
    //
    // Gemessen wird an echter Sprache durch den ESSB-Kanal: Glanz
    // (Energie 2-4 kHz gegen 0,3-1 kHz) und Dichte (LUFS bei gleicher
    // Spitze). Die Reihe sagt, welcher Griff wieviel bringt -- und ob
    // überhaupt.
    void whatHelpsInsideTheFourKilohertz()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const Rede rede = redeMitRauschen(sauber, -60.0);

        const QString profil = QString::fromLatin1(LONGPATH_TEST_DATA_DIR)
                               + QStringLiteral("/aether-ssb-30-07-2026.json");
        if (!QFile::exists(profil)) { QSKIP("Martins Profil fehlt"); }

        // Eine Kette mit seinem Profil, mit genau EINEM Griff anders.
        // bassAb: die Anhebung unter 150 Hz zurücknehmen. Sein Profil
        // hebt bei 69 Hz um 5,71 dB an und setzt den Hochpass mit Güte
        // 2,53 auf 62,7 Hz -- also noch eine Spitze obendrauf. Auf
        // voller Bandbreite trägt das; in einem 4-kHz-Kanal frisst es
        // Aussteuerung, die oben fehlt.
        const auto fahre = [&](int kuhschwanzHz, int dooHz, int zischHz,
                               bool bassAb = false) {
            StripChain c;
            c.prepare(kRate);
            const auto e = AetherPreset::applyFile(profil, c);
            if (!e.ok) { return std::vector<float>{}; }
            c.setEnabled(true);
            if (kuhschwanzHz > 0) {
                for (int i = 0; i < c.eq().activeBandCount(); ++i) {
                    ClientEq::BandParams b = c.eq().band(i);
                    if (b.type == ClientEq::FilterType::HighShelf
                        && b.enabled && b.freqHz > 4050.0f) {
                        b.freqHz = float(kuhschwanzHz);
                        c.eq().setBand(i, b);
                        break;
                    }
                }
            }
            if (bassAb) {
                for (int i = 0; i < c.eq().activeBandCount(); ++i) {
                    ClientEq::BandParams b = c.eq().band(i);
                    if (b.type == ClientEq::FilterType::Peak && b.enabled
                        && b.freqHz < 150.0f && b.gainDb > 0.5f) {
                        b.gainDb = 0.0f;
                        c.eq().setBand(i, b);
                    }
                    if (b.type == ClientEq::FilterType::HighPass && b.enabled
                        && b.q > 1.0f) {
                        b.q = 0.707f;          // keine Spitze an der Ecke
                        c.eq().setBand(i, b);
                    }
                }
            }
            if (dooHz > 0)   { c.pudu().setDooTuneHz(float(dooHz)); }
            if (zischHz > 0) { c.deEss().setFrequencyHz(float(zischHz)); }

            std::vector<float> x = rede.signal;
            constexpr int kBlock = 64;
            for (size_t at = 0; at + kBlock <= x.size(); at += kBlock) {
                c.processMono(x.data() + at, kBlock);
            }
            return durchKanal(3, x);      // 50 - 4050, sein Kanal
        };

        const auto glanz = [](const std::vector<float>& v) {
            const auto p = spektrum(v);
            return bandDb(p, 2000.0, 4000.0) - bandDb(p, 300.0, 1000.0);
        };
        const auto dichte = [](const std::vector<float>& v) {
            double pk = 0.0;
            for (float x : v) { pk = std::max(pk, double(std::fabs(x))); }
            std::vector<float> n = v;
            if (pk > 1e-9) {
                const float g = float(std::pow(10.0, -2.0 / 20.0) / pk);
                for (float& x : n) { x *= g; }
            }
            return Loudness::measure(n.data(), int(n.size()), int(kRate)).integratedLufs;
        };

        struct V { const char* name; int shelf; int doo; int zisch; bool bass; };
        static const V kV[] = {
            { "wie im Profil",          0,    0,    0, false },
            { "Kuhschwanz 3400",     3400,    0,    0, false },
            { "PUDU doo 2800",          0, 2800,    0, false },
            { "Zischen 3000",           0,    0, 3000, false },
            { "Bass unter 150 flach",   0,    0,    0, true  },
            { "Bass flach + doo 2800",  0, 2800,    0, true  },
            { "alles zusammen",      3400, 2800,    0, true  },
        };

        qInfo().noquote() << QStringLiteral(
            "   durch 50–4050 Hz          Glanz (2–4k gegen 0,3–1k)   "
            "Dichte bei gleicher Spitze");
        double basisGlanz = 0.0;
        double basisDichte = 0.0;
        for (const V& v : kV) {
            const auto y = fahre(v.shelf, v.doo, v.zisch, v.bass);
            if (y.empty()) { QFAIL("das Profil ließ sich nicht anwenden"); }
            const double g = glanz(y);
            const double d = dichte(y);
            if (basisGlanz == 0.0) { basisGlanz = g; basisDichte = d; }
            qInfo().noquote()
                << QStringLiteral("  %1  %2 dB (%3)   %4 LUFS (%5)")
                       .arg(QString::fromUtf8(v.name), -22)
                       .arg(g, 7, 'f', 2)
                       .arg(g - basisGlanz, 5, 'f', 2)
                       .arg(d, 7, 'f', 1)
                       .arg(d - basisDichte, 5, 'f', 2);
        }
        qInfo().noquote() << QStringLiteral(
            "   (Glanz: wieviel Energie oben gegen unten steht. "
            "Dichte: Lautheit bei gleicher Spitze.)");
    }

    // ── „alle profile sehr laut mit starkem brummen" (2026-10-14) ───
    //
    // Brummen kann die Kette nicht erfinden — aber sie kann es
    // ANHEBEN, und sein Profil tut dazu zweierlei an derselben Stelle:
    //
    //   Hochpass 62,7 Hz mit Güte 2,531. Eine Güte über 0,707 ist bei
    //     einem Hochpass kein Schnitt mehr, sondern eine Spitze AUF
    //     der Ecke.
    //   dazu eine Glocke bei 68,95 Hz mit +5,71 dB.
    //
    // Beides sitzt genau dort, wo die zweite Harmonische des Netzes
    // liegt (100 Hz) und wo 50 Hz noch nicht weit weg ist. Auf einem
    // Kopfhörer am ruhigen Rechner ist das Wärme; mit Netzbrummen auf
    // der Leitung ist es ein Verstärker dafür.
    //
    // Gemessen mit echter Sprache PLUS 50 Hz Brumm bei -40 dBFS.
    void howMuchHisProfileLiftsMainsHum()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }
        const QString profil = QString::fromLatin1(LONGPATH_TEST_DATA_DIR)
                               + QStringLiteral("/aether-ssb-30-07-2026.json");
        if (!QFile::exists(profil)) { QSKIP("Martins Profil fehlt"); }

        // Sprache auf -12 dBFS Spitze, Brumm 28 dB darunter: 50 Hz und
        // die 100-Hz-Oberwelle, wie ein Netzteil sie macht.
        std::vector<float> x = sauber;
        double spitze = 0.0;
        for (float v : x) { spitze = std::max(spitze, double(std::fabs(v))); }
        const double g = std::pow(10.0, -12.0 / 20.0) / std::max(spitze, 1e-9);
        for (float& v : x) { v = float(v * g); }
        const double brumm = std::pow(10.0, -40.0 / 20.0);
        for (size_t i = 0; i < x.size(); ++i) {
            const double t = double(i) / kRate;
            x[i] += float(brumm * (std::sin(2.0 * M_PI * 50.0 * t)
                                   + 0.5 * std::sin(2.0 * M_PI * 100.0 * t)));
        }

        const auto durch = [&](bool zahm) {
            StripChain c;
            c.prepare(kRate);
            const auto e = AetherPreset::applyFile(profil, c);
            if (!e.ok) { return std::vector<float>{}; }
            c.setEnabled(true);
            if (zahm) {
                for (int i = 0; i < c.eq().activeBandCount(); ++i) {
                    ClientEq::BandParams b = c.eq().band(i);
                    if (b.type == ClientEq::FilterType::HighPass && b.enabled) {
                        b.q = 0.707f;          // keine Spitze auf der Ecke
                        b.freqHz = 100.0f;     // über die 50 und die 100
                        c.eq().setBand(i, b);
                    } else if (b.type == ClientEq::FilterType::Peak && b.enabled
                               && b.freqHz < 150.0f && b.gainDb > 0.5f) {
                        b.gainDb = 0.0f;       // die Anhebung im Brummband
                        c.eq().setBand(i, b);
                    }
                }
            }
            std::vector<float> y = x;
            constexpr int kBlock = 64;
            for (size_t at = 0; at + kBlock <= y.size(); at += kBlock) {
                c.processMono(y.data() + at, kBlock);
            }
            return y;
        };

        const auto wie = durch(false);
        const auto zahm = durch(true);
        QVERIFY(!wie.empty() && !zahm.empty());

        const auto pEin  = spektrum(x);
        const auto pWie  = spektrum(wie);
        const auto pZahm = spektrum(zahm);

        struct B { const char* name; double von; double bis; };
        static const B kB[] = {
            { "50 Hz",  47,  53 }, { "100 Hz",  95, 105 },
            { "69 Hz",  65,  73 }, { "200 Hz", 190, 210 },
            { "1 kHz", 950, 1050 },
        };
        qInfo().noquote() << QStringLiteral(
            "   Band      Eingang   sein Profil   gezähmt   "
            "(Profil minus Eingang)");
        for (const B& b : kB) {
            const double e = bandDb(pEin,  b.von, b.bis);
            const double w = bandDb(pWie,  b.von, b.bis);
            const double z = bandDb(pZahm, b.von, b.bis);
            qInfo().noquote()
                << QStringLiteral("  %1  %2  %3  %4   %5 dB")
                       .arg(QString::fromUtf8(b.name), -8)
                       .arg(e, 8, 'f', 1).arg(w, 12, 'f', 1)
                       .arg(z, 9, 'f', 1).arg(w - e, 7, 'f', 1);
        }

        const auto spitzeDb = [](const std::vector<float>& v) {
            double p = 0.0;
            for (float s : v) { p = std::max(p, double(std::fabs(s))); }
            return 20.0 * std::log10(std::max(p, 1e-9));
        };
        qInfo().noquote() << QStringLiteral(
            "   Spitze: Eingang %1 dBFS · sein Profil %2 · gezähmt %3")
            .arg(spitzeDb(x), 0, 'f', 1).arg(spitzeDb(wie), 0, 'f', 1)
            .arg(spitzeDb(zahm), 0, 'f', 1);
    }

    // ── Die Eingangsautomatik darf Brummen NICHT auf Sprachpegel
    //    heben (2026-10-14) ───────────────────────────────────────
    //
    // Am Gerät des Betreibers hing ein Headset an der Klinkenbuchse,
    // dessen Störteppich bei etwa −48 dBFS lag. Die Automatik hob ihn
    // um 35 dB auf das Ziel, die Kette presste ihn an die Decke des
    // Begrenzers, und im Kopfhörer stand Dauerpegel. „ich traue mir
    // nicht mehr die kopfhörer aufzusetzen."
    //
    // Geprüft werden beide Richtungen — eine Schranke, die Brummen
    // aussperrt, aber auch Sprache, wäre nur ein anderer Fehler.
    void theInputAutomaticIgnoresHumButFindsSpeech()
    {
        QString warum;
        const auto sauber = echteRede(&warum);
        if (sauber.empty()) { QSKIP(qPrintable(warum)); }

        const auto fahre = [](const std::vector<float>& x) {
            StripChain c;
            c.prepare(kRate);
            c.setEnabled(true);
            c.setInputAuto(true);
            std::vector<float> y = x;
            constexpr int kBlock = 64;
            for (size_t at = 0; at + kBlock <= y.size(); at += kBlock) {
                c.processMono(y.data() + at, kBlock);
            }
            return c.effectiveInputGainDb();
        };

        // 1. Nur Brummen, so wie es an seiner Buchse steht: 50 und
        //    100 Hz, Spitze −40 dBFS, zehn Sekunden.
        std::vector<float> brumm(size_t(kRate * 10), 0.0f);
        for (size_t i = 0; i < brumm.size(); ++i) {
            const double t = double(i) / kRate;
            brumm[i] = float(0.01 * (std::sin(2.0 * M_PI * 50.0 * t)
                                     + 0.5 * std::sin(2.0 * M_PI * 100.0 * t)));
        }
        const float beiBrumm = fahre(brumm);

        // 2. Dieselbe Länge echte Sprache, ebenfalls leise (Spitze
        //    −40 dBFS) — die MUSS gefunden werden.
        std::vector<float> rede = sauber;
        double spitze = 0.0;
        for (float v : rede) { spitze = std::max(spitze, double(std::fabs(v))); }
        const double g = std::pow(10.0, -40.0 / 20.0) / std::max(spitze, 1e-9);
        for (float& v : rede) { v = float(v * g); }
        const float beiSprache = fahre(rede);

        qInfo().noquote() << QStringLiteral(
            "   Verstärkung der Automatik: bei Brummen %1 dB · bei "
            "leiser Sprache %2 dB")
            .arg(double(beiBrumm), 0, 'f', 1).arg(double(beiSprache), 0, 'f', 1);

        QVERIFY2(beiBrumm < 1.0f,
                 qPrintable(QStringLiteral("die Automatik hebt reines "
                                           "Brummen um %1 dB")
                                .arg(double(beiBrumm))));
        QVERIFY2(beiSprache > 10.0f,
                 qPrintable(QStringLiteral("die Automatik findet leise "
                                           "Sprache nicht mehr (%1 dB)")
                                .arg(double(beiSprache))));
    }

    void theGroupDelayOfEachChannel()
    {
        const auto laufzeitMs = [](int ch, double hz) {
            // Ein Wellenpaket: Ton unter einer Hann-Glocke.
            const int n = int(kRate * 1.2);
            const int laenge = int(kRate * 0.06);
            const int start = int(kRate * 0.3);
            std::vector<float> x(size_t(n), 0.0f);
            for (int i = 0; i < laenge; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i
                                                      / (laenge - 1));
                x[size_t(start + i)] =
                    float(w * std::sin(2.0 * M_PI * hz * i / kRate));
            }
            const auto schwerpunkt = [](const std::vector<float>& v) {
                double s = 0.0;
                double sw = 0.0;
                for (size_t i = 0; i < v.size(); ++i) {
                    const double e = double(v[i]) * double(v[i]);
                    s += e; sw += e * double(i);
                }
                return s > 1e-20 ? sw / s : 0.0;
            };
            const double vorher = schwerpunkt(x);
            auto y = durchKanal(ch, x);
            const double nachher = schwerpunkt(y);
            return (nachher - vorher) * 1000.0 / kRate;
        };

        qInfo().noquote() << QStringLiteral(
            "   Kanal                      60 Hz   100 Hz   200 Hz   "
            "1 kHz   3 kHz   4 kHz");
        for (int ch = 0; ch < BandLimit::kChannelCount; ++ch) {
            const auto& k = BandLimit::channels()[size_t(ch)];
            qInfo().noquote()
                << QStringLiteral("  %1  %2  %3  %4  %5  %6  %7")
                       .arg(QString::fromUtf8(k.name), -24)
                       .arg(laufzeitMs(ch, 60.0), 6, 'f', 1)
                       .arg(laufzeitMs(ch, 100.0), 7, 'f', 1)
                       .arg(laufzeitMs(ch, 200.0), 7, 'f', 1)
                       .arg(laufzeitMs(ch, 1000.0), 6, 'f', 1)
                       .arg(laufzeitMs(ch, 3000.0), 6, 'f', 1)
                       .arg(laufzeitMs(ch, 4000.0), 6, 'f', 1);
        }
        qInfo().noquote() << QStringLiteral(
            "   (Millisekunden. Was zwischen zwei Frequenzen stark "
            "auseinanderläuft, hört man als Verschmieren.)");
    }

    // ── Steilheit gegen Laufzeit, abgewogen ─────────────────────────
    //
    // Die Flanke von 48 dB je Oktave kostet 16,8 ms bei 60 Hz. Die
    // Frage ist, was sie dafür bringt -- und ob eine flachere Flanke
    // dasselbe leistet, ohne die Stimme zu verschmieren.
    //
    // Gemessen wird beides am selben Kanal (50-4050 ESSB) und mit
    // denselben Werkzeugen: Laufzeit über ein Wellenpaket,
    // Dämpfung über reine Töne.
    void steepnessAgainstSmear()
    {
        const auto laufzeitMs = [](BandLimit& b, double hz) {
            const int n = int(kRate * 1.2);
            const int laenge = int(kRate * 0.06);
            const int start = int(kRate * 0.3);
            std::vector<float> x(size_t(n), 0.0f);
            for (int i = 0; i < laenge; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i
                                                      / (laenge - 1));
                x[size_t(start + i)] =
                    float(w * std::sin(2.0 * M_PI * hz * i / kRate));
            }
            const auto schwerpunkt = [](const std::vector<float>& v) {
                double s = 0.0;
                double sw = 0.0;
                for (size_t i = 0; i < v.size(); ++i) {
                    const double e = double(v[i]) * double(v[i]);
                    s += e; sw += e * double(i);
                }
                return s > 1e-20 ? sw / s : 0.0;
            };
            const double vorher = schwerpunkt(x);
            b.reset();
            for (size_t at = 0; at + 240 <= x.size(); at += 240) {
                b.process(x.data() + at, 240);
            }
            return (schwerpunkt(x) - vorher) * 1000.0 / kRate;
        };
        const auto pegelDb = [](BandLimit& b, double hz) {
            const int perioden = std::max(8, int(0.40 * hz));
            const int mess = int(std::lround(perioden * kRate / hz));
            const int ruhe = int(kRate * 0.8);
            std::vector<float> x(size_t(ruhe + mess));
            for (size_t i = 0; i < x.size(); ++i) {
                x[i] = float(std::sin(2.0 * M_PI * hz * double(i) / kRate));
            }
            b.reset();
            for (size_t at = 0; at + 240 <= x.size(); at += 240) {
                b.process(x.data() + at, 240);
            }
            double s = 0.0;
            for (int i = 0; i < mess; ++i) {
                const double v = double(x[size_t(ruhe + i)]);
                s += v * v;
            }
            return 10.0 * std::log10(std::max(s / mess, 1e-14) / 0.5);
        };

        qInfo().noquote() << QStringLiteral(
            "  Hochpass   Laufzeit 60 Hz   100 Hz   200 Hz   "
            "Pegel 50 Hz   30 Hz   20 Hz");
        for (int ordnung : { 2, 4, 6, 8 }) {
            BandLimit b;
            b.prepare(kRate);
            b.setOrders(ordnung, 8);
            b.setChannel(3);                    // 50 - 4050 Hz
            qInfo().noquote()
                << QStringLiteral("  %1 dB/Okt   %2 ms   %3 ms   %4 ms   "
                                  "%5 dB   %6 dB   %7 dB")
                       .arg(ordnung * 6, 2)
                       .arg(laufzeitMs(b, 60.0), 10, 'f', 1)
                       .arg(laufzeitMs(b, 100.0), 6, 'f', 1)
                       .arg(laufzeitMs(b, 200.0), 6, 'f', 1)
                       .arg(pegelDb(b, 50.0), 8, 'f', 1)
                       .arg(pegelDb(b, 30.0), 6, 'f', 1)
                       .arg(pegelDb(b, 20.0), 6, 'f', 1);
        }
        qInfo().noquote() << QStringLiteral(
            "   (ein Sender braucht unten keine 48 dB je Oktave -- er "
            "braucht Ruhe unterhalb der Stimme)");
    }

    // ── Die eigene Verzögerung ──────────────────────────────────────
    //
    // Das Entrauschen rechnet in Blöcken von 480 Werten, also 10 ms,
    // und schiebt das Trockene durch denselben Ring. Wer mithört,
    // hört sich selbst mit dieser Verzögerung; wer sie nicht kennt,
    // sucht sie in der Soundkarte.
    void theChainsOwnDelay()
    {
        const int n = int(kRate);
        std::vector<float> stoss(size_t(n), 0.0f);
        stoss[size_t(kRate * 0.1)] = 0.5f;

        StripChain c;
        empfehlung(c);
        // Nur Entrauschen: die anderen Stufen haben keine
        // Verzögerungsleitung, und das Gate würde einen einzelnen
        // Stoss wegräumen.
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        c.setStageEnabled(StripChain::Stage::Denoise, true);
        const auto y = durch(c, stoss);

        size_t wo = 0;
        double p = 0.0;
        for (size_t i = 0; i < y.size(); ++i) {
            if (std::fabs(double(y[i])) > p) {
                p = std::fabs(double(y[i])); wo = i;
            }
        }
        // Und derselbe Stoss durch den ENTRAUSCHTEN Weg allein. Die
        // Zahl, auf die es ankommt: der trockene Weg ist um einen
        // Rahmen verzoegert, der entrauschte um mehr -- und die
        // Differenz ist der Versatz, mit dem gemischt wird.
        {
            StripChain f;
            empfehlung(f);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                f.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            f.nr().setAmount(1.0f);
            f.setStageEnabled(StripChain::Stage::Denoise, true);
            // Ein einzelner Stoss ist fuer ein Sprachnetz kein
            // Signal; es drueckt ihn weg. Also ein kurzer Ton, den
            // man wiederfinden kann.
            std::vector<float> ton(size_t(n), 0.0f);
            for (int i = 0; i < int(kRate * 0.02); ++i) {
                const int at = int(kRate * 0.1) + i;
                ton[size_t(at)] = float(0.3 * std::sin(2.0 * M_PI * 1000.0
                                                       * i / kRate));
            }
            const auto fy = durch(f, ton);
            size_t fwo = 0;
            double fp = 0.0;
            for (size_t i = 0; i < fy.size(); ++i) {
                if (std::fabs(double(fy[i])) > fp) {
                    fp = std::fabs(double(fy[i])); fwo = i;
                }
            }
            qInfo().noquote()
                << QStringLiteral("Entrauschter Weg allein (Anteil 1,0): "
                                  "Tonanfang %1 Abtastwerte spaeter "
                                  "(%2 ms)")
                       .arg(qint64(fwo) - qint64(kRate * 0.1))
                       .arg((double(fwo) - kRate * 0.1) * 1000.0 / kRate,
                            0, 'f', 2);
        }

        const double msec = (double(wo) - kRate * 0.1) * 1000.0 / kRate;
        qInfo().noquote()
            << QStringLiteral("Entrauschen: Stoss kommt %1 ms später heraus "
                              "(gemeldet: %2 ms)")
                   .arg(msec, 0, 'f', 2)
                   .arg(c.nr().latencySamples() * 1000.0 / kRate, 0, 'f', 2);

        // Was die Stufe meldet, muss stimmen -- sonst rechnet das
        // Mithörfenster mit einer falschen Zahl.
        const double gemeldet = c.nr().latencySamples() * 1000.0 / kRate;
        QVERIFY2(std::fabs(msec - gemeldet) < 1.0,
                 qPrintable(QStringLiteral("gemessen %1 ms, gemeldet %2 ms")
                                .arg(msec).arg(gemeldet)));
    }
};

QTEST_MAIN(TstBandbreitenGenau)
#include "tst_bandbreiten_genau.moc"
