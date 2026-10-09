// DCS: die Codewoerter und der Detektor.
//
// Weder Thetis noch WDSP kennen DCS, in keiner Richtung -- hier ist
// nichts portiert. Gebaut nach der oeffentlichen Beschreibung des
// Digital Coded Squelch: 23-Bit-Golay-Wort, 134,4 bit/s, keine
// Synchronbits.
//
// Vier Faelle dieses Pruefstands sind gegen Fehler gebaut, die beim
// Bauen wirklich passiert sind, und jeder hat den Entwurf veraendert:
//
//   1. `dieFestenBitsSitzenRichtig` / `alleCodesSindUnterscheidbar`:
//      Die drei festen Bits standen zuerst verkehrt herum (0b001 statt
//      0b100). Das faellt nicht weiter auf -- der Code kodiert und
//      dekodiert damit genauso sauber -- aber es macht 47 der 104 Codes
//      zu zyklischen Verschiebungen voneinander und damit im endlosen
//      Datenstrom ununterscheidbar. Mit der richtigen Lage sind alle
//      104 unterscheidbar. Gefunden nur durch die Gegenprobe gegen eine
//      veroeffentlichte Angabe (siehe `diePolaritaetspaareStimmen`).
//
//   2. `nurRauschenOeffnetNie`: der schwerste Fehler. Der Detektor lief
//      zuerst ueber alle 23 Lagen und oeffnete beim ersten Treffer --
//      reines Rauschen riss die Sperre damit in 10 von 15 Laeufen auf.
//      Bei 23 Zufallsbits trifft eine Lage mit 1 zu 4096, und bei
//      134,4 Bit je Sekunde kommt diese Gelegenheit in vier Sekunden
//      ueber fuenfhundert Mal.
//
//   3. `derZyklusIstDieFalle`: belegt, warum der Detektor korreliert
//      statt zu dekodieren.
//
//   4. `diePolaritaetspaareStimmen`: die einzige Stelle, an der sich
//      die ganze Kodierung gegen eine fremde, veroeffentlichte Angabe
//      pruefen laesst.
//
// Alle Signale sind synthetisch -- kein Funkgeraet noetig.

#include <QtTest>

#include <cmath>
#include <map>
#include <random>
#include <set>
#include <vector>

#include "core/DcsCode.h"
#include "core/DcsDetector.h"

using namespace Longpath;

namespace {

constexpr double kRate = 48000.0;

int gewicht(uint32_t w)
{
    int n = 0;
    while (w) { w &= (w - 1); ++n; }
    return n;
}

/// Ein DCS-Sender: dasselbe 23-Bit-Wort endlos, als NRZ bei `bitrate`.
struct Sender {
    uint32_t wort;
    double   phase{0.0};
    int      bitIndex{0};
    double   amp;
    double   bitrate;

    Sender(int oktal, DcsPolarity pol, double a = 0.15, double br = kDcsBitRateHz)
        : wort(dcsExpectedWord(oktal, pol)), amp(a), bitrate(br) {}

    std::vector<float> block(double sekunden)
    {
        const int n = static_cast<int>(kRate * sekunden);
        std::vector<float> out(static_cast<size_t>(n));
        const double step = bitrate / kRate;
        for (int i = 0; i < n; ++i) {
            out[static_cast<size_t>(i)] =
                static_cast<float>(((wort >> bitIndex) & 1u) ? amp : -amp);
            phase += step;
            if (phase >= 1.0) { phase -= 1.0; bitIndex = (bitIndex + 1) % kDcsWordBits; }
        }
        return out;
    }
};

/// TIEFfrequentes Rauschen. Breitbandiges waere kein Pruefstein: der
/// Tiefpass bei 300 Hz schneidet es fast vollstaendig weg, und man
/// misst dann die Guete des Filters statt die des Detektors.
void tiefesRauschen(std::vector<float>& buf, double amp, unsigned seed)
{
    std::mt19937 gen(seed);
    std::normal_distribution<double> d(0.0, amp);
    double z = 0.0;
    const double a = std::exp(-2.0 * M_PI * 150.0 / kRate);
    for (float& x : buf) {
        z = a * z + (1.0 - a) * d(gen);
        x = static_cast<float>(x + z * 6.0);
    }
}

/// Wie lange bis zur Erkennung? -1, wenn sie ausbleibt.
double erkennzeit(DcsDetector& det, Sender& sender, double maxSekunden,
                  double rauschAmp = 0.0)
{
    double t = 0.0;
    while (t < maxSekunden && !det.detected()) {
        std::vector<float> buf = sender.block(0.05);
        if (rauschAmp > 0.0) {
            tiefesRauschen(buf, rauschAmp, static_cast<unsigned>(t * 1000) + 1);
        }
        det.process(buf.data(), static_cast<int>(buf.size()));
        t += 0.05;
    }
    return det.detected() ? t : -1.0;
}

} // namespace

class TstDcsDetector : public QObject
{
    Q_OBJECT

private slots:
    // --- die Codewoerter ---
    void derGolayGeneratorStimmt();
    void jederStandardcodeLaeuftRund();
    void dreiBitfehlerWerdenKorrigiertVierNicht();
    void derZyklusIstDieFalle();
    void dieFestenBitsSitzenRichtig();
    void alleCodesSindUnterscheidbar();
    void diePolaritaetspaareStimmen();
    void derAbstandZuFremdenCodesTraegt();
    // --- der Detektor ---
    void dereigeneCodeWirdErkannt();
    void fremdeCodesOeffnenNie();
    void nurRauschenOeffnetNie();
    void dieFalschePolaritaetOeffnetNicht();
    void einVerstimmterSenderWirdNochErkannt();
    void nachDemEndeSchliesstSieWieder();
};

void TstDcsDetector::derGolayGeneratorStimmt()
{
    // Der Beleg, dass das Generatorpolynom das richtige ist: ein
    // Golay(23,12) hat Mindestabstand 7. Stimmte der Generator nicht,
    // waere er kleiner -- und die ganze Fehlerkorrektur daneben.
    std::vector<uint32_t> woerter;
    woerter.reserve(4096);
    for (uint32_t d = 0; d < 4096; ++d) {
        woerter.push_back((d & 0xFFF) | (dcsGolayParity(d) << 12));
    }
    int minAbstand = 99;
    for (size_t i = 0; i < woerter.size(); ++i) {
        for (size_t j = i + 1; j < woerter.size(); ++j) {
            minAbstand = std::min(minAbstand, gewicht(woerter[i] ^ woerter[j]));
        }
    }
    QCOMPARE(minAbstand, 7);
}

void TstDcsDetector::jederStandardcodeLaeuftRund()
{
    for (int oktal : dcsStandardCodes()) {
        const uint32_t code = dcsOctalToCode(oktal);
        int korrigiert = -1;
        const int zurueck = dcsDecode(dcsEncode(code), &korrigiert);
        QVERIFY2(zurueck == static_cast<int>(code),
                 qPrintable(QStringLiteral("Code %1 kam als %2 zurueck").arg(oktal).arg(zurueck)));
        QCOMPARE(korrigiert, 0);
        QCOMPARE(dcsCodeToOctal(code), oktal);
    }
}

void TstDcsDetector::dreiBitfehlerWerdenKorrigiertVierNicht()
{
    // Golay(23,12) korrigiert bis zu drei Fehler. Vier duerfen ruhig
    // scheitern -- sie duerfen nur nicht FALSCH erkannt werden.
    for (int anzahl = 1; anzahl <= 4; ++anzahl) {
        int richtig = 0, falsch = 0, versuche = 0;
        for (int oktal : dcsStandardCodes()) {
            const uint32_t code = dcsOctalToCode(oktal);
            const uint32_t wort = dcsEncode(code);
            for (int start = 0; start + anzahl <= kDcsWordBits; start += 7) {
                uint32_t kaputt = wort;
                for (int k = 0; k < anzahl; ++k) { kaputt ^= 1u << (start + k); }
                const int zurueck = dcsDecode(kaputt);
                ++versuche;
                if (zurueck == static_cast<int>(code)) { ++richtig; }
                else if (zurueck >= 0) { ++falsch; }
            }
        }
        QVERIFY2(falsch == 0,
                 qPrintable(QStringLiteral("%1 Bitfehler: %2 Woerter FALSCH erkannt")
                                .arg(anzahl).arg(falsch)));
        if (anzahl <= 3) {
            QCOMPARE(richtig, versuche);
        } else {
            QCOMPARE(richtig, 0);
        }
    }
}

void TstDcsDetector::derZyklusIstDieFalle()
{
    // Der Grund, warum der Detektor korreliert statt zu dekodieren:
    // Golay ist zyklisch, JEDE ringfoermige Verschiebung eines gueltigen
    // Codewortes hat wieder eine gueltige Pruefsumme. Die Pruefsumme
    // taugt also nicht zum Synchronisieren.
    const uint32_t wort = dcsEncode(dcsOctalToCode(23));
    int mitGueltigerPruefsumme = 0;
    for (int i = 1; i < kDcsWordBits; ++i) {
        const uint32_t gedreht = dcsRotate(wort, i);
        // Pruefsumme von Hand, ohne die festen Bits zu betrachten.
        uint32_t rest = gedreht;
        for (int b = 22; b >= 11; --b) {
            if (rest & (1u << b)) { rest ^= 0xC75u << (b - 11); }
        }
        if ((rest & 0x7FF) == 0) { ++mitGueltigerPruefsumme; }
    }
    QCOMPARE(mitGueltigerPruefsumme, kDcsWordBits - 1);
}

void TstDcsDetector::dieFestenBitsSitzenRichtig()
{
    // Das Wort lautet geschrieben P11..P1 - 100 - C9..C1. Die festen
    // Bits liegen also auf den Stellen 11, 10, 9 und heissen dort
    // 1, 0, 0. Standen sie verkehrt herum, kodierte und dekodierte alles
    // genauso sauber -- nur waeren 47 der Codes ununterscheidbar
    // geworden (siehe den naechsten Fall).
    QCOMPARE(kDcsFixedBits, 0b100u);
    for (int oktal : dcsStandardCodes()) {
        const uint32_t wort = dcsEncode(dcsOctalToCode(oktal));
        QCOMPARE((wort >> kDcsCodeBits) & 0b111u, 0b100u);
    }
}

void TstDcsDetector::alleCodesSindUnterscheidbar()
{
    // Zwei Codes, deren Woerter zyklische Verschiebungen voneinander
    // sind, waeren im endlosen Datenstrom nicht auseinanderzuhalten --
    // es gibt keine Synchronbits, die die Lage festlegen.
    std::map<uint32_t, std::vector<int>> klassen;
    for (int oktal : dcsStandardCodes()) {
        const uint32_t w = dcsEncode(dcsOctalToCode(oktal));
        uint32_t kanonisch = w;
        for (int p = 1; p < kDcsWordBits; ++p) {
            kanonisch = std::min(kanonisch, dcsRotate(w, p));
        }
        klassen[kanonisch].push_back(oktal);
    }
    for (const auto& [kanonisch, liste] : klassen) {
        Q_UNUSED(kanonisch);
        if (liste.size() > 1) {
            QString namen;
            for (int o : liste) { namen += QStringLiteral("%1 ").arg(o, 3, 10, QLatin1Char('0')); }
            QFAIL(qPrintable(QStringLiteral("Diese Codes sind ununterscheidbar: %1").arg(namen)));
        }
    }
    QCOMPARE(static_cast<int>(klassen.size()), dcsStandardCodes().size());
}

void TstDcsDetector::diePolaritaetspaareStimmen()
{
    // Die einzige Stelle, an der sich die ganze Kodierung gegen eine
    // FREMDE, veroeffentlichte Angabe pruefen laesst. Die Beschreibung
    // des Verfahrens nennt als Beispiel: "+023 erzeugt dasselbe Muster
    // wie -047". Wenn unsere Bitanordnung, die Lage der festen Bits und
    // das Generatorpolynom alle stimmen, muss genau das herauskommen --
    // und nichts daran ist frei gewaehlt.
    QCOMPARE(dcsInversePartner(47), 23);
    QCOMPARE(dcsInversePartner(23), 47);

    // Und jeder Code hat genau einen solchen Partner.
    for (int oktal : dcsStandardCodes()) {
        const int partner = dcsInversePartner(oktal);
        QVERIFY2(partner > 0,
                 qPrintable(QStringLiteral("Code %1 hat keinen Partner").arg(oktal)));
        QCOMPARE(dcsInversePartner(partner), oktal);
    }
}

void TstDcsDetector::derAbstandZuFremdenCodesTraegt()
{
    // Die Zahl, auf der die Schwelle von 20 von 23 beruht: ueber alle
    // Paarungen und alle Phasen darf ein FREMDER Code nie nahe genug
    // herankommen.
    int schlimmste = 0;
    for (int soll : dcsStandardCodes()) {
        const uint32_t sollWort = dcsExpectedWord(soll, DcsPolarity::Normal);
        for (int fremd : dcsStandardCodes()) {
            if (fremd == soll) { continue; }
            const uint32_t fremdWort = dcsExpectedWord(fremd, DcsPolarity::Normal);
            for (int p = 0; p < kDcsWordBits; ++p) {
                schlimmste = std::max(schlimmste,
                                      dcsAgreement(dcsRotate(fremdWort, p), sollWort));
            }
        }
    }
    QVERIFY2(schlimmste <= 16,
             qPrintable(QStringLiteral("Ein fremder Code kommt auf %1 von 23 -- die "
                                       "Schwelle 20 haette dann keine Reserve mehr")
                            .arg(schlimmste)));
}

void TstDcsDetector::dereigeneCodeWirdErkannt()
{
    for (int oktal : {23, 47, 131, 445, 754}) {
        DcsDetector det(kRate, oktal);
        Sender sender(oktal, DcsPolarity::Normal);
        const double t = erkennzeit(det, sender, 2.0);
        QVERIFY2(t > 0.0,
                 qPrintable(QStringLiteral("Code %1 nicht erkannt (%2 von 23)")
                                .arg(oktal).arg(det.lastAgreement())));
        // Zwei Woerter sind 342 ms; mit Anlauf muss es unter einer
        // Sekunde durch sein.
        QVERIFY2(t <= 1.0,
                 qPrintable(QStringLiteral("Code %1 brauchte %2 s").arg(oktal).arg(t)));
    }
}

void TstDcsDetector::fremdeCodesOeffnenNie()
{
    int geprueft = 0;
    for (int soll : {23, 47, 131, 754}) {
        for (int sende : {25, 26, 51, 125, 243, 331, 445, 606, 732}) {
            if (soll == sende) { continue; }
            DcsDetector det(kRate, soll);
            Sender sender(sende, DcsPolarity::Normal);
            for (int k = 0; k < 60; ++k) {
                const std::vector<float> buf = sender.block(0.05);
                det.process(buf.data(), static_cast<int>(buf.size()));
            }
            ++geprueft;
            QVERIFY2(!det.detected(),
                     qPrintable(QStringLiteral("Soll %1 oeffnete bei Sender %2 (%3 von 23)")
                                    .arg(soll).arg(sende).arg(det.lastAgreement())));
        }
    }
    QVERIFY(geprueft >= 30);
}

void TstDcsDetector::nurRauschenOeffnetNie()
{
    // Der Fall, der den schwersten Fehler gefangen hat. Mit einer
    // Schleife ueber die 23 Lagen und einem einzelnen Treffer als
    // Bedingung riss reines Rauschen die Sperre in 10 von 15 Laeufen
    // auf -- bei 23 Zufallsbits trifft eine Lage mit 1 zu 4096, und bei
    // 134,4 Bit je Sekunde kommt diese Gelegenheit oft genug.
    for (double amp : {0.05, 0.2, 0.5, 1.0, 2.0}) {
        for (int soll : {23, 131, 754}) {
            DcsDetector det(kRate, soll);
            bool jemalsOffen = false;
            for (int k = 0; k < 80; ++k) {       // 4 Sekunden
                std::vector<float> buf(static_cast<size_t>(kRate * 0.05), 0.0f);
                tiefesRauschen(buf, amp, static_cast<unsigned>(k + soll));
                det.process(buf.data(), static_cast<int>(buf.size()));
                if (det.detected()) { jemalsOffen = true; }
            }
            QVERIFY2(!jemalsOffen,
                     qPrintable(QStringLiteral("Rauschen %1, Soll %2: hat geoeffnet")
                                    .arg(amp).arg(soll)));
        }
    }
    // Und ein Dauerpegel ohne jeden Datenstrom auch nicht.
    DcsDetector det(kRate, 131);
    for (int k = 0; k < 60; ++k) {
        const std::vector<float> buf(static_cast<size_t>(kRate * 0.05), 0.2f);
        det.process(buf.data(), static_cast<int>(buf.size()));
    }
    QVERIFY2(!det.detected(), "Ein Dauerpegel hat die Sperre geoeffnet");
}

void TstDcsDetector::dieFalschePolaritaetOeffnetNicht()
{
    // Geraete fuehren jeden Code als N und I. Wer auf N steht, darf von
    // einem I-Sender nicht geoeffnet werden.
    DcsDetector det(kRate, 23, DcsPolarity::Normal);
    Sender sender(23, DcsPolarity::Inverted);
    for (int k = 0; k < 60; ++k) {
        const std::vector<float> buf = sender.block(0.05);
        det.process(buf.data(), static_cast<int>(buf.size()));
    }
    QVERIFY2(!det.detected(),
             qPrintable(QStringLiteral("023N oeffnete bei einem 023I-Sender (%1 von 23)")
                            .arg(det.lastAgreement())));

    // Umgekehrt muss es gehen.
    DcsDetector det2(kRate, 23, DcsPolarity::Inverted);
    Sender sender2(23, DcsPolarity::Inverted);
    QVERIFY2(erkennzeit(det2, sender2, 2.0) > 0.0, "023I erkannte seinen eigenen Sender nicht");
}

void TstDcsDetector::einVerstimmterSenderWirdNochErkannt()
{
    // Sender und Empfaenger haben nie exakt dieselbe Bitrate. Quarze
    // halten 0,1 % muehelos; geprueft wird bis 2 %, also mit dem
    // Zwanzigfachen an Reserve.
    for (double abweichung : {-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0}) {
        DcsDetector det(kRate, 131);
        Sender sender(131, DcsPolarity::Normal, 0.15,
                      kDcsBitRateHz * (1.0 + abweichung / 100.0));
        const double t = erkennzeit(det, sender, 3.0);
        QVERIFY2(t > 0.0,
                 qPrintable(QStringLiteral("Sender %1 %% daneben wurde nicht erkannt")
                                .arg(abweichung)));
    }
}

void TstDcsDetector::nachDemEndeSchliesstSieWieder()
{
    DcsDetector det(kRate, 131);
    Sender sender(131, DcsPolarity::Normal);
    QVERIFY2(erkennzeit(det, sender, 2.0) > 0.0, "Die Sperre ging nicht auf");

    // Sender weg, nur noch Rauschen.
    double t = 0.0;
    while (t < 3.0 && det.detected()) {
        std::vector<float> buf(static_cast<size_t>(kRate * 0.05), 0.0f);
        tiefesRauschen(buf, 0.05, static_cast<unsigned>(t * 1000) + 7);
        det.process(buf.data(), static_cast<int>(buf.size()));
        t += 0.05;
    }
    QVERIFY2(!det.detected(), "Die Sperre blieb nach dem Sendeende offen");
    // Die Haltezeit sind zwei Wortlaengen, also rund 350 ms; unter 1,5 s
    // muss es durch sein.
    QVERIFY2(t <= 1.5, qPrintable(QStringLiteral("brauchte %1 s zum Schliessen").arg(t)));
}

QTEST_MAIN(TstDcsDetector)
#include "tst_dcs_detector.moc"
