// Die Anbindung des Tonsquelch an den Empfangskanal.
//
// `tst_ctcss_detector` prueft den Detektor selbst -- findet er den Ton,
// weist er den Nachbarn ab, flattert er nicht. Dieser Pruefstand prueft,
// was darum herum passiert: haengt der Detektor am richtigen Signal, und
// schaltet sein Urteil den Audioweg richtig?
//
// Zwei Dinge sind hier leicht falsch zu machen, und beide haben einen
// eigenen Fall:
//
//   1. WDSP liefert das Basisband STEREO-VERSCHRAENKT (`fmd.c`:
//      `audio[2*i+1] = audio[2*i+0]`). Wer `audio[i]` statt `audio[2*i]`
//      liest, bekommt bei gleichen Kanaelen trotzdem ein brauchbares
//      Signal -- nur eben das halbe Fenster, also die doppelte Frequenz.
//      Der Fehler faellt im Betrieb erst auf, wenn ein Ton nicht erkannt
//      wird. `derAbgriffLiestDenLinkenKanal` legt darum absichtlich ein
//      Gegensignal in den rechten Kanal.
//
//   2. Die Tonsperre darf Martins Stummschalter nicht mitbenutzen. Wer
//      dafuer `setMuted` nimmt, ueberschreibt die Bedienung und laesst
//      `muted()` falsch anzeigen. `dieTonsperreFasstDenStummschalter
//      NichtAn` haelt beide Wege auseinander.

#include <QSignalSpy>
#include <QtTest>

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include "core/RxChannel.h"

using namespace Longpath;

namespace {

constexpr int    kKanal    = 0;
constexpr int    kPuffer   = 1024;
constexpr double kRate     = 48000.0;

/// Ein Block Basisband, wie WDSP ihn liefert: stereo-verschraenkt, beide
/// Kanaele mit demselben Wert. `rechtsAnders` legt stattdessen das
/// Gegensignal in den rechten Kanal -- damit faellt auf, wenn der Abgriff
/// die Verschraenkung uebersieht.
std::vector<double> basisband(double hz, double sekunden, double amp = 0.5,
                              double* phase = nullptr, bool rechtsAnders = false)
{
    const int n = static_cast<int>(kRate * sekunden);
    std::vector<double> out(static_cast<size_t>(n) * 2);
    double ph = phase ? *phase : 0.0;
    const double step = 2.0 * M_PI * hz / kRate;
    for (int i = 0; i < n; ++i) {
        const double v = amp * std::sin(ph);
        out[static_cast<size_t>(2 * i)]     = v;
        out[static_cast<size_t>(2 * i) + 1] = rechtsAnders ? -v : v;
        ph += step;
    }
    if (phase) { *phase = ph; }
    return out;
}

/// Den Kanal so lange mit `hz` fuettern, bis die Sperre offen ist oder
/// `maxSekunden` verstrichen sind. Liefert die verstrichene Zeit.
double fuettern(RxChannel& ch, double hz, double maxSekunden, double amp = 0.5,
                bool rechtsAnders = false)
{
    double ph = 0.0;
    double t  = 0.0;
    while (t < maxSekunden) {
        const std::vector<double> buf = basisband(hz, 0.05, amp, &ph, rechtsAnders);
        ch.feedCtcssBasebandForTest(buf.data(), static_cast<int>(buf.size() / 2));
        t += 0.05;
        if (ch.ctcssTonePresent()) { break; }
    }
    return t;
}

} // namespace

class TstCtcssSquelchAnbindung : public QObject
{
    Q_OBJECT

private slots:
    void ohneTonsperreIstNichtsGesperrt();
    void beimEinschaltenIstSieZu();
    void derEigeneTonOeffnetSie();
    void derNachbartonOeffnetSieNicht();
    void nachDemTonendeSchliesstSieWieder();
    void derAbgriffLiestDenLinkenKanal();
    void dieTonsperreFasstDenStummschalterNichtAn();
    void dasAbschaltenHinterlaesstKeinenStummenKanal();
    void einTonwechselSetztDenDetektorZurueck();
    void umschaltenWaehrendDerAbgriffLaeuft();
};

void TstCtcssSquelchAnbindung::ohneTonsperreIstNichtsGesperrt()
{
    RxChannel ch(kKanal, kPuffer, kRate);
    QVERIFY2(!ch.ctcssSquelchEnabled(), "Die Tonsperre war ungefragt an");
    // Ohne Tonsperre muss der Ton als "liegt an" gelten -- sonst waere
    // jeder Kanal von Anfang an stumm.
    QVERIFY2(ch.ctcssTonePresent(), "Ohne Tonsperre galt der Ton als fehlend");
}

void TstCtcssSquelchAnbindung::beimEinschaltenIstSieZu()
{
    // Die sichere Richtung: der Detektor braucht sein erstes Fenster
    // (250 ms). Bis dahin muss die Sperre ZU sein, sonst laesst sie eine
    // fremde Station eine Viertelsekunde durch.
    RxChannel ch(kKanal, kPuffer, kRate);
    QSignalSpy spion(&ch, &RxChannel::ctcssTonePresenceChanged);
    ch.setCtcssSquelch(true, 100.0);
    QVERIFY2(ch.ctcssSquelchEnabled(), "Die Tonsperre ging nicht an");
    QVERIFY2(!ch.ctcssTonePresent(), "Die Tonsperre war beim Einschalten offen");
    QCOMPARE(spion.count(), 1);
    QCOMPARE(spion.at(0).at(0).toBool(), false);
}

void TstCtcssSquelchAnbindung::derEigeneTonOeffnetSie()
{
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setCtcssSquelch(true, 100.0);
    QSignalSpy spion(&ch, &RxChannel::ctcssTonePresenceChanged);

    const double gebraucht = fuettern(ch, 100.0, 1.0);
    QVERIFY2(ch.ctcssTonePresent(),
             qPrintable(QStringLiteral("Der eigene Ton oeffnete die Sperre nicht "
                                       "(Verhaeltnis %1)").arg(ch.ctcssLastRatio())));
    QVERIFY2(gebraucht <= 0.5,
             qPrintable(QStringLiteral("brauchte %1 s, die Norm verlangt unter 0,5")
                            .arg(gebraucht)));
    QCOMPARE(spion.count(), 1);
    QCOMPARE(spion.at(0).at(0).toBool(), true);
}

void TstCtcssSquelchAnbindung::derNachbartonOeffnetSieNicht()
{
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setCtcssSquelch(true, 100.0);
    fuettern(ch, 103.5, 1.0);
    QVERIFY2(!ch.ctcssTonePresent(),
             qPrintable(QStringLiteral("Der Nachbarton 103,5 Hz oeffnete die Sperre "
                                       "(Verhaeltnis %1)").arg(ch.ctcssLastRatio())));
}

void TstCtcssSquelchAnbindung::nachDemTonendeSchliesstSieWieder()
{
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setCtcssSquelch(true, 100.0);
    fuettern(ch, 100.0, 1.0);
    QVERIFY2(ch.ctcssTonePresent(), "Die Sperre war vor dem Tonende nicht offen");

    // Ton aus, nur noch Stille mit etwas Rauschen.
    double t = 0.0;
    while (t < 2.0 && ch.ctcssTonePresent()) {
        std::vector<double> buf(static_cast<size_t>(kRate * 0.05) * 2, 0.0);
        for (size_t i = 0; i < buf.size(); i += 2) {
            const double r = 0.05 * std::sin(static_cast<double>(i) * 0.37);
            buf[i] = r;
            buf[i + 1] = r;
        }
        ch.feedCtcssBasebandForTest(buf.data(), static_cast<int>(buf.size() / 2));
        t += 0.05;
    }
    QVERIFY2(!ch.ctcssTonePresent(), "Die Sperre blieb nach dem Tonende offen");
}

void TstCtcssSquelchAnbindung::derAbgriffLiestDenLinkenKanal()
{
    // Der rechte Kanal traegt hier das GEGENSIGNAL. Wer die
    // Verschraenkung uebersieht und `audio[i]` liest, sieht damit einen
    // Ton der doppelten Frequenz und darf die Sperre nicht oeffnen.
    // Richtig gelesen ist es derselbe saubere 100-Hz-Ton wie sonst.
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setCtcssSquelch(true, 100.0);
    fuettern(ch, 100.0, 1.0, 0.5, /*rechtsAnders=*/true);
    QVERIFY2(ch.ctcssTonePresent(),
             qPrintable(QStringLiteral("Mit Gegensignal im rechten Kanal wurde der Ton "
                                       "nicht erkannt (Verhaeltnis %1) -- liest der "
                                       "Abgriff audio[i] statt audio[2*i]?")
                            .arg(ch.ctcssLastRatio())));
}

void TstCtcssSquelchAnbindung::dieTonsperreFasstDenStummschalterNichtAn()
{
    RxChannel ch(kKanal, kPuffer, kRate);

    // Martin schaltet stumm. Die Tonsperre darf das nicht sehen.
    ch.setMuted(true);
    ch.setCtcssSquelch(true, 100.0);
    QVERIFY2(ch.muted(), "Die Tonsperre hat den Stummschalter geloescht");

    fuettern(ch, 100.0, 1.0);
    QVERIFY2(ch.ctcssTonePresent(), "Der Ton wurde bei stummem Kanal nicht erkannt");
    QVERIFY2(ch.muted(), "Der erkannte Ton hat den Stummschalter aufgehoben");

    // Und zurueck: Stummschalter aus laesst die Tonsperre unberuehrt.
    ch.setMuted(false);
    QVERIFY2(!ch.muted(), "Der Stummschalter ging nicht aus");
    QVERIFY2(ch.ctcssTonePresent(), "Der Stummschalter hat die Tonsperre geschlossen");
}

void TstCtcssSquelchAnbindung::dasAbschaltenHinterlaesstKeinenStummenKanal()
{
    // Der Fall, der im Betrieb aergerlich waere: Tonsperre an, kein Ton,
    // Kanal stumm -- und dann schaltet man die Tonsperre ab. Danach muss
    // der Kanal hoerbar sein, nicht stumm haengen.
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setCtcssSquelch(true, 100.0);
    QVERIFY2(!ch.ctcssTonePresent(), "Die Sperre war beim Einschalten offen");

    QSignalSpy spion(&ch, &RxChannel::ctcssTonePresenceChanged);
    ch.setCtcssSquelch(false, 0.0);
    QVERIFY2(!ch.ctcssSquelchEnabled(), "Die Tonsperre ging nicht aus");
    QVERIFY2(ch.ctcssTonePresent(),
             "Nach dem Abschalten galt der Ton weiter als fehlend -- der Kanal "
             "haengt stumm");
    QCOMPARE(spion.count(), 1);
    QCOMPARE(spion.at(0).at(0).toBool(), true);
}

void TstCtcssSquelchAnbindung::einTonwechselSetztDenDetektorZurueck()
{
    // Auf 100,0 offen, dann auf 123,0 umgestellt: der alte Ton darf die
    // Sperre nicht offen halten.
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setCtcssSquelch(true, 100.0);
    fuettern(ch, 100.0, 1.0);
    QVERIFY2(ch.ctcssTonePresent(), "Die Sperre war vor dem Tonwechsel nicht offen");

    ch.setCtcssSquelch(true, 123.0);
    QVERIFY2(!ch.ctcssTonePresent(), "Der Tonwechsel liess die Sperre offen");

    // Der alte Ton oeffnet sie jetzt nicht mehr.
    fuettern(ch, 100.0, 0.8);
    QVERIFY2(!ch.ctcssTonePresent(),
             qPrintable(QStringLiteral("Der alte Ton 100 Hz oeffnete die auf 123 Hz "
                                       "gestellte Sperre (Verhaeltnis %1)")
                            .arg(ch.ctcssLastRatio())));
    // Der neue schon.
    fuettern(ch, 123.0, 1.0);
    QVERIFY2(ch.ctcssTonePresent(), "Der neue Ton 123 Hz oeffnete die Sperre nicht");
}

void TstCtcssSquelchAnbindung::umschaltenWaehrendDerAbgriffLaeuft()
{
    // Der Fall, der im Betrieb wirklich auftritt und am teuersten ist:
    // der DSP-Thread fuettert gerade, waehrend der Benutzer den Subton
    // umstellt oder die Sperre abschaltet. Dabei verschwindet der
    // Detektor unter dem fuetternden Faden -- genau dagegen nimmt
    // `onCtcssBaseband` ein `try_lock` und bricht ab, wenn es das
    // Schloss nicht bekommt.
    //
    // Ohne dieses Zusammenspiel gibt es zwei Arten Fehler: ein Zugriff
    // auf einen freigegebenen Detektor (Absturz, von ASAN gefangen) oder
    // ein wartender DSP-Faden (Aussetzer im Audioweg). Dieser Fall ist
    // darum vor allem unter ASAN/TSAN etwas wert.
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setCtcssSquelch(true, 100.0);

    std::atomic<bool> schluss{false};
    std::atomic<int>  bloecke{0};

    std::thread dsp([&] {
        double ph = 0.0;
        while (!schluss.load(std::memory_order_acquire)) {
            const std::vector<double> buf = basisband(100.0, 0.02, 0.5, &ph);
            ch.feedCtcssBasebandForTest(buf.data(), static_cast<int>(buf.size() / 2));
            bloecke.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Erst anlaufen lassen -- ohne das ist die Umschaltschleife fertig,
    // bevor der Faden ueberhaupt gestartet ist, und es verschraenkt sich
    // nichts (beim ersten Versuch kam der Faden auf 0 Bloecke).
    while (bloecke.load(std::memory_order_relaxed) < 5) {
        std::this_thread::yield();
    }

    // Jetzt umstellen, abschalten, wieder an -- waehrend gefuettert wird.
    for (int runde = 0; runde < 200; ++runde) {
        ch.setCtcssSquelch(true, (runde % 2) ? 123.0 : 100.0);
        ch.setCtcssSquelch(false, 0.0);
        ch.setCtcssSquelch(true, 100.0);
        if ((runde % 20) == 0) {
            // Dem Faden Luft geben, damit er zwischen den Umschaltungen
            // wirklich hereinkommt.
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    schluss.store(true, std::memory_order_release);
    dsp.join();

    const int durchgekommen = bloecke.load();
    qInfo() << "Bloecke durch den fuetternden Faden:" << durchgekommen;
    QVERIFY2(durchgekommen > 100,
             qPrintable(QStringLiteral("Nur %1 Bloecke -- es hat sich nichts "
                                       "verschraenkt").arg(durchgekommen)));
    // Nach dem letzten Einschalten muss der Zustand stimmig sein: Sperre
    // an, und der Detektor lebt (lastRatio() greift auf ihn zu).
    QVERIFY2(ch.ctcssSquelchEnabled(), "Die Tonsperre blieb am Ende aus");
    const double verh = ch.ctcssLastRatio();
    QVERIFY2(verh >= 0.0 && verh <= 1.0,
             qPrintable(QStringLiteral("Verhaeltnis ausserhalb 0..1: %1").arg(verh)));
}

QTEST_MAIN(TstCtcssSquelchAnbindung)
#include "tst_ctcss_squelch_anbindung.moc"
