// Die Anbindung der DCS-Sperre an Kanal und Oberflaeche.
//
// `tst_dcs_detector` prueft den Detektor. Hier geht es darum, was
// darum herum passiert -- und vor allem um die eine Stelle, an der
// CTCSS und DCS sich in die Quere kommen koennen: sie teilen sich
// denselben Basisband-Abgriff und denselben Weg zum Audiopanel, also
// darf immer nur eine laufen.

#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QtTest>

#include <cmath>
#include <vector>

#include "core/DcsCode.h"
#include "gui/StyleConstants.h"
#include "core/RxChannel.h"
#include "gui/widgets/VfoModeContainers.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

constexpr int    kKanal  = 0;
constexpr int    kPuffer = 1024;
constexpr double kRate   = 48000.0;

/// Ein DCS-Sender, stereo-verschraenkt wie der WDSP-Abgriff liefert.
struct Sender {
    uint32_t wort;
    double   phase{0.0};
    int      bitIndex{0};
    double   amp{0.15};

    Sender(int oktal, DcsPolarity pol) : wort(dcsExpectedWord(oktal, pol)) {}

    std::vector<double> block(double sekunden)
    {
        const int n = static_cast<int>(kRate * sekunden);
        std::vector<double> out(static_cast<size_t>(n) * 2);
        const double step = kDcsBitRateHz / kRate;
        for (int i = 0; i < n; ++i) {
            const double v = ((wort >> bitIndex) & 1u) ? amp : -amp;
            out[static_cast<size_t>(2 * i)]     = v;
            out[static_cast<size_t>(2 * i) + 1] = v;
            phase += step;
            if (phase >= 1.0) { phase -= 1.0; bitIndex = (bitIndex + 1) % kDcsWordBits; }
        }
        return out;
    }
};

double fuettern(RxChannel& ch, Sender& sender, double maxSekunden)
{
    double t = 0.0;
    while (t < maxSekunden && !ch.ctcssTonePresent()) {
        const std::vector<double> buf = sender.block(0.05);
        ch.feedDcsBasebandForTest(buf.data(), static_cast<int>(buf.size() / 2));
        t += 0.05;
    }
    return t;
}

template<typename T>
T* findNamed(QWidget* parent, const char* name)
{
    return parent->findChild<T*>(QString::fromLatin1(name));
}

} // namespace

class TstDcsSquelchAnbindung : public QObject
{
    Q_OBJECT

private slots:
    void beimEinschaltenIstSieZu();
    void dereigeneCodeOeffnetSie();
    void einFremderCodeOeffnetSieNicht();
    void dieBeidenSperrenSchliessenEinanderAus();
    void derWaehlerZeigtBeiDcsDieCodes();
    void derWaehlerSchreibtCodeUndPolaritaet();
    void dieLeuchteGiltAuchFuerDcs();
};

void TstDcsSquelchAnbindung::beimEinschaltenIstSieZu()
{
    RxChannel ch(kKanal, kPuffer, kRate);
    QSignalSpy spion(&ch, &RxChannel::ctcssTonePresenceChanged);
    ch.setDcsSquelch(true, 23, false);
    QVERIFY2(ch.dcsSquelchEnabled(), "Die DCS-Sperre ging nicht an");
    QVERIFY2(!ch.ctcssTonePresent(), "Die DCS-Sperre war beim Einschalten offen");
    QCOMPARE(spion.count(), 1);
}

void TstDcsSquelchAnbindung::dereigeneCodeOeffnetSie()
{
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setDcsSquelch(true, 131, false);
    Sender sender(131, DcsPolarity::Normal);
    const double t = fuettern(ch, sender, 2.0);
    QVERIFY2(ch.ctcssTonePresent(),
             qPrintable(QStringLiteral("Code 131 oeffnete nicht (%1 von 23)")
                            .arg(ch.dcsLastAgreement())));
    // Zwei Woerter sind 342 ms; mit Anlauf unter einer Sekunde.
    QVERIFY2(t <= 1.0, qPrintable(QStringLiteral("brauchte %1 s").arg(t)));
}

void TstDcsSquelchAnbindung::einFremderCodeOeffnetSieNicht()
{
    RxChannel ch(kKanal, kPuffer, kRate);
    ch.setDcsSquelch(true, 131, false);
    Sender sender(243, DcsPolarity::Normal);
    for (int k = 0; k < 40; ++k) {
        const std::vector<double> buf = sender.block(0.05);
        ch.feedDcsBasebandForTest(buf.data(), static_cast<int>(buf.size() / 2));
    }
    QVERIFY2(!ch.ctcssTonePresent(),
             qPrintable(QStringLiteral("Der fremde Code 243 oeffnete die auf 131 "
                                       "gestellte Sperre (%1 von 23)")
                            .arg(ch.dcsLastAgreement())));
}

void TstDcsSquelchAnbindung::dieBeidenSperrenSchliessenEinanderAus()
{
    // Beide teilen sich den Basisband-Abgriff und den Weg zum
    // Audiopanel. Liefen sie gleichzeitig, wuerde die eine die
    // Entscheidung der anderen ueberschreiben -- und zwar je nach
    // Blockgrenze mal so, mal so.
    RxChannel ch(kKanal, kPuffer, kRate);

    ch.setCtcssSquelch(true, 100.0);
    QVERIFY(ch.ctcssSquelchEnabled());
    QVERIFY2(!ch.dcsSquelchEnabled(), "DCS lief ungefragt mit");

    ch.setDcsSquelch(true, 131, false);
    QVERIFY2(ch.dcsSquelchEnabled(), "DCS ging nicht an");
    QVERIFY2(!ch.ctcssSquelchEnabled(), "CTCSS lief neben DCS weiter");

    ch.setCtcssSquelch(true, 100.0);
    QVERIFY2(ch.ctcssSquelchEnabled(), "CTCSS ging nicht wieder an");
    QVERIFY2(!ch.dcsSquelchEnabled(), "DCS lief neben CTCSS weiter");

    // Und beide aus lassen den Kanal hoerbar.
    ch.setCtcssSquelch(false, 0.0);
    QVERIFY2(ch.ctcssTonePresent(), "Nach dem Abschalten haengt der Kanal stumm");
}

void TstDcsSquelchAnbindung::derWaehlerZeigtBeiDcsDieCodes()
{
    SliceModel s;
    FmOptContainer c;
    c.setSlice(&s);

    s.setFmCtcssMode(2);                 // CTCSS Decode
    c.syncFromSlice();
    auto* werte = findNamed<QComboBox>(&c, "toneValueCmb");
    QVERIFY(werte != nullptr);
    QVERIFY2(werte->findText(QStringLiteral("100.0")) >= 0,
             "Bei CTCSS fehlt der Ton 100,0 im Waehler");

    s.setFmCtcssMode(4);                 // DCS Decode
    c.syncFromSlice();
    QVERIFY2(werte->findText(QStringLiteral("023 N")) >= 0,
             "Bei DCS fehlt der Code 023 N im Waehler");
    QVERIFY2(werte->findText(QStringLiteral("023 I")) >= 0,
             "Bei DCS fehlt der Code 023 I im Waehler");
    QVERIFY2(werte->findText(QStringLiteral("100.0")) < 0,
             "Bei DCS stehen noch CTCSS-Toene im Waehler");
    QCOMPARE(werte->count(), dcsStandardCodes().size() * 2);

    // Und zurueck.
    s.setFmCtcssMode(2);
    c.syncFromSlice();
    QVERIFY2(werte->findText(QStringLiteral("100.0")) >= 0,
             "Nach der Rueckkehr zu CTCSS fehlen die Toene");
}

void TstDcsSquelchAnbindung::derWaehlerSchreibtCodeUndPolaritaet()
{
    SliceModel s;
    FmOptContainer c;
    c.setSlice(&s);
    s.setFmCtcssMode(4);
    c.syncFromSlice();

    auto* werte = findNamed<QComboBox>(&c, "toneValueCmb");
    QVERIFY(werte != nullptr);

    const int idxN = werte->findText(QStringLiteral("131 N"));
    QVERIFY(idxN >= 0);
    werte->setCurrentIndex(idxN);
    QCOMPARE(s.fmDcsCode(), 131);
    QCOMPARE(s.fmDcsPolarity(), 0);

    const int idxI = werte->findText(QStringLiteral("131 I"));
    QVERIFY(idxI >= 0);
    werte->setCurrentIndex(idxI);
    QCOMPARE(s.fmDcsCode(), 131);
    QCOMPARE(s.fmDcsPolarity(), 1);
}

void TstDcsSquelchAnbindung::dieLeuchteGiltAuchFuerDcs()
{
    SliceModel s;
    s.setFmCtcssMode(4);
    s.setFmDcsCode(131);
    FmOptContainer c;
    c.setSlice(&s);
    c.syncFromSlice();

    auto* lamp = findNamed<QLabel>(&c, "toneLamp");
    QVERIFY(lamp != nullptr);
    QVERIFY2(lamp->styleSheet().contains(QLatin1String(Longpath::Style::kBorderMuted)),
             "Bei DCS ohne Code ist die Leuchte nicht dunkel");
    QVERIFY2(lamp->toolTip().contains(QStringLiteral("131")),
             qPrintable(QStringLiteral("Der Hinweis nennt den DCS-Code nicht: %1")
                            .arg(lamp->toolTip())));

    s.setFmCtcssToneDetected(true);
    QVERIFY2(lamp->styleSheet().contains(QLatin1String(Longpath::Style::kLiveGreen)),
             "Die Leuchte folgte dem erkannten DCS-Code nicht");
}

QTEST_MAIN(TstDcsSquelchAnbindung)
#include "tst_dcs_squelch_anbindung.moc"
