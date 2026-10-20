// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_kanalzug_messlauf.cpp  (Longpath)
// =================================================================
// „bitte teste nun unseren kanalzug ganz genau. mit den ergebnissen
//  bin ich nicht wirklich zufrieden, das rohe audiosignal ist besser
//  als alles andere."
//
// Zweimal dieselbe Aussage an einem Tag ist ein Befund, kein
// Geschmacksurteil. Dieser Prüfstand rät nicht, welche Stufe schuld
// ist — er misst jede EINZELN gegen dasselbe Signal und schreibt
// auf, was sie tut:
//
//   * wieviel Pegel sie nimmt oder gibt,
//   * was sie mit dem Scheitelfaktor macht,
//   * wie sie das Spektrum verbiegt (Sechstel-Oktaven),
//   * wieviel Klirr sie auf einem reinen Ton erzeugt,
//   * ob sie während des Sprechens zumacht.
//
// Das Prüfsignal ist eine nachgebaute Stimme MIT Grundgeräusch, auf
// dem Pegel, den die Kette erwartet (−12 dBFS Spitze). Denn ein
// Prüfsignal, das die Schwellen nicht erreicht, prüft die Stufe
// nicht — das war heute schon einmal die Lehre.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/LtasSpectrum.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = 48000;

// ── Eine nachgebaute Stimme ─────────────────────────────────────────
//
// Stimmhafte Sprache ist ein Pulszug durch einen Resonator: die
// Stimmlippen schlagen, der Mund- und Rachenraum formt daraus
// Formanten. Nachgebaut mit einem Pulszug bei 115 Hz und drei
// Resonanzen (500 / 1500 / 2600 Hz) -- das ist kein „aaa", aber es
// hat die Hüllkurve, den Scheitelfaktor und die spektrale Neigung
// einer Stimme, und darum geht es hier.
//
// Dazu ein gleichmäßiges Grundgeräusch wie von einem Lüfter, und
// Pausen: ohne Pausen lässt sich über ein Gate nichts aussagen.
std::vector<float> stimme(double sekunden, double rauschDbFs = -48.0)
{
    const int n = int(sekunden * kRate);
    std::vector<float> v(static_cast<size_t>(n), 0.0f);

    // Pulszug.
    const int periode = int(kRate / 115.0);
    for (int i = 0; i < n; i += periode) {
        v[size_t(i)] = 1.0f;
    }
    // Drei Resonanzen.
    const double formant[3] = { 500.0, 1500.0, 2600.0 };
    const double guete[3]   = { 6.0, 8.0, 10.0 };
    for (int f = 0; f < 3; ++f) {
        const double w = 2.0 * M_PI * formant[f] / kRate;
        const double a = std::exp(-w / (2.0 * guete[f]));
        const double c1 = 2.0 * a * std::cos(w);
        const double c2 = -a * a;
        double z1 = 0.0;
        double z2 = 0.0;
        std::vector<float> out(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            const double y = double(v[size_t(i)]) + c1 * z1 + c2 * z2;
            z2 = z1; z1 = y;
            out[size_t(i)] = float(y);
        }
        if (f == 0) { v = out; }
        else {
            for (int i = 0; i < n; ++i) { v[size_t(i)] += out[size_t(i)] * 0.6f; }
        }
    }

    // Silben: 0,6 s Sprache, 0,4 s Pause, mit weichen Flanken.
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / kRate;
        const double imTakt = std::fmod(t, 1.0);
        double h = 0.0;
        if (imTakt < 0.6) {
            h = 1.0;
            if (imTakt < 0.03)        { h = imTakt / 0.03; }
            else if (imTakt > 0.57)   { h = (0.6 - imTakt) / 0.03; }
        }
        v[size_t(i)] = float(double(v[size_t(i)]) * h);
    }

    // Auf −12 dBFS Spitze bringen -- der Pegel, für den die Kette
    // geschrieben ist.
    double p = 0.0;
    for (float x : v) { p = std::max(p, std::fabs(double(x))); }
    const double ziel = std::pow(10.0, -12.0 / 20.0);
    if (p > 1e-9) {
        for (float& x : v) { x = float(double(x) * ziel / p); }
    }

    // Und das Grundgeräusch darunter.
    std::mt19937 rng(20261010);
    std::normal_distribution<float> nd(
        0.0f, float(std::pow(10.0, rauschDbFs / 20.0)));
    for (float& x : v) { x += nd(rng); }
    return v;
}

struct Messung {
    double spitzeDb{-120.0};
    double effektivDb{-120.0};
    double scheitelDb{0.0};
    double pausenDb{-120.0};      // Grundgeräusch in den Pausen
    std::vector<double> ltas;
};

double dB(double v) { return 20.0 * std::log10(std::max(v, 1e-12)); }

Messung messen(const std::vector<float>& v)
{
    Messung m;
    double p = 0.0;
    double s = 0.0;
    int n = 0;
    // Sprache: die erste Hälfte jeder Sekunde. Pause: 0,75..0,95.
    double sp = 0.0;
    int np = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        const double t = double(i) / kRate;
        const double imTakt = std::fmod(t, 1.0);
        const double x = double(v[i]);
        if (imTakt > 0.1 && imTakt < 0.55) {
            p = std::max(p, std::fabs(x));
            s += x * x; ++n;
        } else if (imTakt > 0.75 && imTakt < 0.95) {
            sp += x * x; ++np;
        }
    }
    const double rms = std::sqrt(s / std::max(1, n));
    m.spitzeDb   = dB(p);
    m.effektivDb = dB(rms);
    m.scheitelDb = m.spitzeDb - m.effektivDb;
    m.pausenDb   = dB(std::sqrt(sp / std::max(1, np)));
    m.ltas = Ltas::analyse(v.data(), int(v.size()), kRate);
    return m;
}

std::vector<float> durch(StripChain& c, const std::vector<float>& in)
{
    std::vector<float> out = in;
    for (size_t at = 0; at + 240 <= out.size(); at += 240) {
        c.processMono(out.data() + at, 240);
    }
    return out;
}

// Klirr: ein reiner Ton hinein, alles ausser dem Grundton heraus.
double klirrDb(StripChain& c, double hz, double amp)
{
    const int n = kRate;
    std::vector<float> x(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        x[size_t(i)] = float(amp * std::sin(2.0 * M_PI * hz * i / kRate));
    }
    const auto y = durch(c, x);

    // Goertzel auf dem Grundton, Rest als Differenz der Leistungen.
    const int von = n / 2;
    const int m = n - von;
    const double w = 2.0 * M_PI * hz / kRate;
    const double cw = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    double ges = 0.0;
    for (int i = 0; i < m; ++i) {
        const double s = double(y[size_t(von + i)]) + cw * s1 - s2;
        s2 = s1; s1 = s;
        ges += double(y[size_t(von + i)]) * double(y[size_t(von + i)]);
    }
    const double grund = (s1 * s1 + s2 * s2 - cw * s1 * s2) * 2.0 / (m * m);
    const double rest = std::max(ges / m - grund, 0.0);
    return 10.0 * std::log10(std::max(rest, 1e-15)
                             / std::max(grund, 1e-15));
}

void baueKette(StripChain& c)
{
    c.prepare(kRate);
    c.setEnabled(true);
}

} // namespace

class TstKanalzugMesslauf : public QObject {
    Q_OBJECT

private slots:
    // ── Das Prüfsignal selbst ───────────────────────────────────────
    //
    // Zuerst: taugt es überhaupt? Ein Prüfstand, der mit einem
    // falschen Signal misst, misst sich selbst.
    void theTestSignalLooksLikeAVoice()
    {
        const auto v = stimme(6.0);
        const Messung m = messen(v);
        qInfo() << "Prüfsignal: Spitze" << m.spitzeDb << "dB · effektiv"
                << m.effektivDb << "dB · Scheitelfaktor" << m.scheitelDb
                << "dB · Pausen" << m.pausenDb << "dB";

        QVERIFY2(std::fabs(m.spitzeDb + 12.0) < 1.0, "nicht auf -12 dBFS");
        QVERIFY2(m.scheitelDb > 8.0 && m.scheitelDb < 22.0,
                 qPrintable(QStringLiteral("Scheitelfaktor %1 dB ist keine "
                                           "Stimme").arg(m.scheitelDb)));
        QVERIFY2(m.pausenDb < m.effektivDb - 20.0,
                 "die Pausen sind nicht leiser als die Sprache");
    }

    // ── Jede Stufe einzeln ──────────────────────────────────────────
    //
    // Dieselbe Stimme, immer nur eine Stufe an, mit den Werten aus
    // der Empfehlung. Das Protokoll ist das Ergebnis; die Prüfungen
    // fangen nur, was ohne Zweifel falsch wäre.
    void eachStageOnItsOwn()
    {
        const auto roh = stimme(6.0);
        const Messung r = messen(roh);
        qInfo() << "ROH            Spitze" << r.spitzeDb << "effektiv"
                << r.effektivDb << "Scheitel" << r.scheitelDb
                << "Pausen" << r.pausenDb;

        struct Fall { const char* name; StripChain::Stage st; };
        const Fall faelle[] = {
            {"Entrauschen", StripChain::Stage::Denoise},
            {"Gate       ", StripChain::Stage::Gate},
            {"Entzerrer  ", StripChain::Stage::Eq},
            {"De-Esser   ", StripChain::Stage::DeEss},
            {"Kompressor ", StripChain::Stage::Comp},
            {"Clipper    ", StripChain::Stage::Clip},
            {"Begrenzer  ", StripChain::Stage::Limiter},
        };

        for (const Fall& f : faelle) {
            StripChain c;
            baueKette(c);
            QVERIFY(StripSettings::applyBuiltIn(
                QStringLiteral("Recommended — speech"), c));
            // Alles aus ausser dieser einen.
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.setStageEnabled(f.st, true);

            const Messung m = messen(durch(c, roh));
            qInfo().noquote()
                << QStringLiteral("%1   Spitze %2  effektiv %3  Scheitel %4  "
                                  "Pausen %5  (Δ eff %6)")
                       .arg(QString::fromLatin1(f.name))
                       .arg(m.spitzeDb, 7, 'f', 1)
                       .arg(m.effektivDb, 7, 'f', 1)
                       .arg(m.scheitelDb, 6, 'f', 1)
                       .arg(m.pausenDb, 7, 'f', 1)
                       .arg(m.effektivDb - r.effektivDb, 6, 'f', 1);

            // Was ohne Zweifel falsch waere: eine Stufe, die die
            // Sprache mehr als 6 dB leiser macht, ohne dass jemand
            // das bestellt hat.
            QVERIFY2(m.effektivDb > r.effektivDb - 6.0,
                     qPrintable(QStringLiteral("%1 nimmt %2 dB Sprache weg")
                                    .arg(QString::fromLatin1(f.name))
                                    .arg(r.effektivDb - m.effektivDb,
                                         0, 'f', 1)));
            // Und eine, die gar nichts tut, waere auch ein Befund --
            // aber kein Fehler, also nur ins Protokoll.
        }
    }

    // ── Was jede Stufe am Spektrum tut ──────────────────────────────
    void whatEachStageDoesToTheSpectrum()
    {
        const auto roh = stimme(6.0);
        const Messung r = messen(roh);
        const auto& mitten = Ltas::centresHz();

        const auto bei = [&](const std::vector<double>& l, double hz) {
            for (size_t i = 0; i < mitten.size(); ++i) {
                if (mitten[i] >= hz) { return l[i]; }
            }
            return l.back();
        };

        struct Fall { const char* name; StripChain::Stage st; };
        const Fall faelle[] = {
            {"Entrauschen", StripChain::Stage::Denoise},
            {"Entzerrer  ", StripChain::Stage::Eq},
            {"De-Esser   ", StripChain::Stage::DeEss},
            {"Clipper    ", StripChain::Stage::Clip},
        };
        for (const Fall& f : faelle) {
            StripChain c;
            baueKette(c);
            QVERIFY(StripSettings::applyBuiltIn(
                QStringLiteral("Recommended — speech"), c));
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.setStageEnabled(f.st, true);
            const Messung m = messen(durch(c, roh));
            if (m.ltas.empty() || r.ltas.empty()) { continue; }

            qInfo().noquote()
                << QStringLiteral("%1   125 Hz %2  500 Hz %3  1k %4  "
                                  "2k %5  4k %6  6k %7  (dB gegen roh)")
                       .arg(QString::fromLatin1(f.name))
                       .arg(bei(m.ltas,125) - bei(r.ltas,125), 6, 'f', 1)
                       .arg(bei(m.ltas,500) - bei(r.ltas,500), 6, 'f', 1)
                       .arg(bei(m.ltas,1000)- bei(r.ltas,1000),6, 'f', 1)
                       .arg(bei(m.ltas,2000)- bei(r.ltas,2000),6, 'f', 1)
                       .arg(bei(m.ltas,4000)- bei(r.ltas,4000),6, 'f', 1)
                       .arg(bei(m.ltas,6000)- bei(r.ltas,6000),6, 'f', 1);
        }
    }

    // ── Klirr je Stufe ──────────────────────────────────────────────
    //
    // Ein reiner Ton bei 1 kHz, −12 dBFS. Was nicht Grundton ist,
    // hat die Stufe erfunden.
    void howMuchEachStageDistorts()
    {
        struct Fall { const char* name; StripChain::Stage st; };
        const Fall faelle[] = {
            {"Entrauschen", StripChain::Stage::Denoise},
            {"Entzerrer  ", StripChain::Stage::Eq},
            {"Kompressor ", StripChain::Stage::Comp},
            {"Clipper    ", StripChain::Stage::Clip},
            {"Begrenzer  ", StripChain::Stage::Limiter},
        };
        for (const Fall& f : faelle) {
            StripChain c;
            baueKette(c);
            QVERIFY(StripSettings::applyBuiltIn(
                QStringLiteral("Recommended — speech"), c));
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.setStageEnabled(f.st, true);
            const double k = klirrDb(c, 1000.0, std::pow(10.0, -12.0/20.0));
            qInfo().noquote()
                << QStringLiteral("%1   Klirr %2 dB")
                       .arg(QString::fromLatin1(f.name)).arg(k, 7, 'f', 1);
        }
    }

    // ── Was kostet das Entrauschen, und was bringt der Clipper ──────
    //
    // Der Messlauf oben zeigte zwei Dinge: das Entrauschen nimmt bei
    // 4 und 6 kHz sieben Dezibel weg, und der Clipper schneidet bei
    // 3 dB Drive gar nichts ab -- er macht nur lauter. Hier wird
    // beides durchgereicht, damit die Zahl entscheidet und nicht ich.
    void howTheTwoSuspectsBehaveAcrossTheirRange()
    {
        const auto roh = stimme(6.0);
        const Messung r = messen(roh);
        const auto& mitten = Ltas::centresHz();
        const auto bei = [&](const std::vector<double>& l, double hz) {
            for (size_t i = 0; i < mitten.size(); ++i) {
                if (mitten[i] >= hz) { return l[i]; }
            }
            return l.back();
        };

        for (const float anteil : {0.2f, 0.35f, 0.5f, 0.7f, 1.0f}) {
            StripChain c;
            baueKette(c);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.nr().setAmount(anteil);
            c.setStageEnabled(StripChain::Stage::Denoise, true);
            const Messung m = messen(durch(c, roh));
            qInfo().noquote()
                << QStringLiteral("Entrauschen %1   4k %2 dB  6k %3 dB  "
                                  "Pausen %4 dB  Sprache %5 dB")
                       .arg(double(anteil), 4, 'f', 2)
                       .arg(bei(m.ltas,4000)-bei(r.ltas,4000), 6, 'f', 1)
                       .arg(bei(m.ltas,6000)-bei(r.ltas,6000), 6, 'f', 1)
                       .arg(m.pausenDb - r.pausenDb, 6, 'f', 1)
                       .arg(m.effektivDb - r.effektivDb, 6, 'f', 1);
        }

        for (const float drive : {3.0f, 6.0f, 9.0f, 12.0f, 15.0f}) {
            StripChain c;
            baueKette(c);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.clipper().setDriveDb(drive);
            c.clipper().setKnee(0.2f);
            c.clipper().setLevelMatch(false);
            c.setStageEnabled(StripChain::Stage::Clip, true);
            // Den Zaehler WAEHREND des Laufs ablesen, nicht danach:
            // er haelt den Hoechstwert und laesst ihn fallen, und am
            // Ende stehen vierhundert Millisekunden Pause. Wer erst
            // dann hinsieht, liest die Null des Zerfalls und nicht
            // die Arbeit der Stufe. (Genau darauf bin ich beim
            // Bauen zweimal hereingefallen.)
            std::vector<float> ausgabe = roh;
            float hoechst = 0.0f;
            for (size_t at = 0; at + 240 <= ausgabe.size(); at += 240) {
                c.processMono(ausgabe.data() + at, 240);
                hoechst = std::max(hoechst, c.clipper().clippedDb());
            }
            const Messung m = messen(ausgabe);
            qInfo().noquote()
                << QStringLiteral("Clipper %1 dB   Spitze %2  effektiv %3  "
                                  "Scheitel %4 → %5  abgeschnitten %6 dB")
                       .arg(double(drive), 4, 'f', 0)
                       .arg(m.spitzeDb, 6, 'f', 1)
                       .arg(m.effektivDb, 6, 'f', 1)
                       .arg(r.scheitelDb, 5, 'f', 1)
                       .arg(m.scheitelDb, 5, 'f', 1)
                       .arg(double(hoechst), 5, 'f', 1);
        }
    }

    // ── Tut das Gate überhaupt etwas? ───────────────────────────────
    //
    // Im Messlauf oben stand hinter „Gate" zweimal eine Null: kein
    // Pegel weg, kein Rauschen weg. Eine Stufe, die nichts tut, ist
    // entweder richtig eingestellt oder kaputt -- und das muss man
    // auseinanderhalten können.
    void doesTheGateDoAnythingAtAll()
    {
        const auto roh = stimme(6.0);
        const Messung r = messen(roh);
        qInfo().noquote()
            << QStringLiteral("roh: Sprache %1 dB, Pausen %2 dB")
                   .arg(r.effektivDb, 0, 'f', 1).arg(r.pausenDb, 0, 'f', 1);

        for (const float schwelle : {-50.0f, -42.0f, -38.0f, -34.0f, -28.0f}) {
            StripChain c;
            baueKette(c);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.gate().setThresholdDb(schwelle);
            c.gate().setFloorDb(-18.0f);
            c.gate().setAttackMs(3.0f);
            c.gate().setHoldMs(40.0f);
            c.gate().setReleaseMs(180.0f);
            c.gate().setReturnDb(5.0f);
            c.setStageEnabled(StripChain::Stage::Gate, true);
            const Messung m = messen(durch(c, roh));
            qInfo().noquote()
                << QStringLiteral("Gate Schwelle %1 dB   Sprache %2  "
                                  "Pausen %3  (Δ Pausen %4)")
                       .arg(double(schwelle), 5, 'f', 0)
                       .arg(m.effektivDb, 7, 'f', 1)
                       .arg(m.pausenDb, 7, 'f', 1)
                       .arg(m.pausenDb - r.pausenDb, 6, 'f', 1);
        }
    }

    // ── Und die ganze Kette ─────────────────────────────────────────
    void theWholeChainAgainstRaw()
    {
        const auto roh = stimme(6.0);
        const Messung r = messen(roh);

        StripChain c;
        baueKette(c);
        QVERIFY(StripSettings::applyBuiltIn(
            QStringLiteral("Recommended — speech"), c));
        const Messung m = messen(durch(c, roh));

        qInfo().noquote()
            << QStringLiteral("GANZE KETTE  Spitze %1 → %2 · effektiv %3 → "
                              "%4 · Scheitel %5 → %6 · Pausen %7 → %8")
                   .arg(r.spitzeDb, 0, 'f', 1).arg(m.spitzeDb, 0, 'f', 1)
                   .arg(r.effektivDb, 0, 'f', 1).arg(m.effektivDb, 0, 'f', 1)
                   .arg(r.scheitelDb, 0, 'f', 1).arg(m.scheitelDb, 0, 'f', 1)
                   .arg(r.pausenDb, 0, 'f', 1).arg(m.pausenDb, 0, 'f', 1);

        // Die Kette muss zwei Dinge liefern, sonst ist sie ihr Geld
        // nicht wert: weniger Scheitelfaktor, und ein leiseres
        // Grundgeräusch zwischen den Wörtern.
        QVERIFY2(m.scheitelDb < r.scheitelDb,
                 qPrintable(QStringLiteral("Scheitelfaktor %1 → %2 dB")
                                .arg(r.scheitelDb, 0, 'f', 1)
                                .arg(m.scheitelDb, 0, 'f', 1)));
        // Nicht der absolute Pausenpegel -- die Kette hebt den
        // GANZEN Pegel an, und der Raum geht mit hoch. Was zählt,
        // ist der Abstand zwischen Sprache und Raum. Die erste
        // Fassung dieser Prüfung verglich die absoluten Werte und
        // schlug an, obwohl die Kette den Abstand verbessert hatte:
        // sie maß das Falsche und klang dabei streng.
        const double snrRoh  = r.effektivDb - r.pausenDb;
        const double snrKette = m.effektivDb - m.pausenDb;
        qInfo().noquote()
            << QStringLiteral("Störabstand %1 → %2 dB")
                   .arg(snrRoh, 0, 'f', 1).arg(snrKette, 0, 'f', 1);
        QVERIFY2(snrKette > snrRoh,
                 qPrintable(QStringLiteral("Störabstand %1 → %2 dB -- die "
                                           "Kette macht es schlechter")
                                .arg(snrRoh, 0, 'f', 1)
                                .arg(snrKette, 0, 'f', 1)));
        // Und sie darf die Spitze nicht über Vollaussteuerung lassen.
        QVERIFY2(m.spitzeDb < 0.0,
                 qPrintable(QStringLiteral("Spitze bei %1 dBFS")
                                .arg(m.spitzeDb, 0, 'f', 1)));
    }
};

QTEST_MAIN(TstKanalzugMesslauf)
#include "tst_kanalzug_messlauf.moc"
