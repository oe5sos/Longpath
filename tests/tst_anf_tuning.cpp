// The auto-notch's four values, and the scaling between what a slider
// says and what WDSP gets.
//
// The scaling is the part that goes wrong. NR1 already carries the same
// two factors — gain is slider x 1e-6, leakage is slider x 1e-3 — and a
// factor typed wrong is not a crash: it is a notch that is a thousand
// times too eager or does nothing at all, which reads as "ANF is
// useless on this radio" rather than as a bug.
// no-port-check: NereusSDR-original; the WDSP call signatures and the
// defaults are attributed to Thetis in RxChannel.h.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "models/SliceModel.h"

using namespace NereusSDR;

// The two conversions the setup page performs. Kept here as the
// specification: if the page changes, this is what it has to keep
// agreeing with.
static double gainFromSlider(int ui)    { return ui * 1e-6; }
static int    sliderFromGain(double v)  { return std::min(999, static_cast<int>(v * 1e6)); }
static double leakFromSlider(int ui)    { return ui * 1e-3; }
static int    sliderFromLeak(double v)  { return static_cast<int>(v * 1e3); }

class TstAnfTuning : public QObject {
    Q_OBJECT
private slots:
    void defaults_match_thetis();
    void anf_defaults_differ_from_nr1_on_purpose();
    void setters_change_the_value_and_signal();
    void setting_the_same_value_twice_is_silent();
    void gain_scaling_round_trips();
    void leakage_scaling_round_trips();
    void the_thetis_gain_default_survives_the_slider();
    void position_defaults_to_post_agc();
};

void TstAnfTuning::defaults_match_thetis()
{
    // Thetis radio.cs:722-729 [@852bf0e]: anf_taps 64, anf_delay 16,
    // anf_gain 10e-4, anf_leak 1e-7.
    SliceModel s;
    QCOMPARE(s.anfTaps(), 64);
    QCOMPARE(s.anfDelay(), 16);
    QVERIFY(qFuzzyCompare(s.anfGain(), 10e-4));
    QVERIFY(qFuzzyCompare(s.anfLeakage(), 1e-7));
}

void TstAnfTuning::anf_defaults_differ_from_nr1_on_purpose()
{
    // Same algorithm, different job. A notch has to settle on a steady
    // carrier without chewing at speech, so it adapts more slowly than
    // the denoiser: NR1 is 16e-4 / 10e-7, ANF is 10e-4 / 1e-7. Copying
    // NR1's numbers across would look like tidying and would change the
    // behaviour.
    SliceModel s;
    QVERIFY(s.anfGain() < s.nr1Gain());
    QVERIFY(s.anfLeakage() < s.nr1Leakage());
}

void TstAnfTuning::setters_change_the_value_and_signal()
{
    SliceModel s;
    QSignalSpy taps(&s, &SliceModel::anfTapsChanged);
    QSignalSpy delay(&s, &SliceModel::anfDelayChanged);
    QSignalSpy gain(&s, &SliceModel::anfGainChanged);
    QSignalSpy leak(&s, &SliceModel::anfLeakageChanged);
    QSignalSpy pos(&s, &SliceModel::anfPositionChanged);

    s.setAnfTaps(256);
    s.setAnfDelay(32);
    s.setAnfGain(5e-4);
    s.setAnfLeakage(2e-7);
    s.setAnfPosition(NrPosition::PreAgc);

    QCOMPARE(s.anfTaps(), 256);
    QCOMPARE(s.anfDelay(), 32);
    QVERIFY(qFuzzyCompare(s.anfGain(), 5e-4));
    QVERIFY(qFuzzyCompare(s.anfLeakage(), 2e-7));
    QCOMPARE(s.anfPosition(), NrPosition::PreAgc);

    QCOMPARE(taps.count(), 1);
    QCOMPARE(delay.count(), 1);
    QCOMPARE(gain.count(), 1);
    QCOMPARE(leak.count(), 1);
    QCOMPARE(pos.count(), 1);
}

void TstAnfTuning::setting_the_same_value_twice_is_silent()
{
    // Each of these signals reaches WDSP and schedules a settings save.
    // A setter that fires on every assignment turns dragging a slider
    // into a stream of identical writes.
    SliceModel s;
    s.setAnfTaps(128);
    QSignalSpy taps(&s, &SliceModel::anfTapsChanged);
    s.setAnfTaps(128);
    QCOMPARE(taps.count(), 0);
}

void TstAnfTuning::gain_scaling_round_trips()
{
    // Slider to WDSP and back has to land on the same slider position,
    // or the control jumps as soon as the model echoes the change back.
    for (int ui : {0, 1, 100, 500, 999}) {
        QCOMPARE(sliderFromGain(gainFromSlider(ui)), ui);
    }
}

void TstAnfTuning::leakage_scaling_round_trips()
{
    for (int ui : {0, 1, 100, 500, 999}) {
        QCOMPARE(sliderFromLeak(leakFromSlider(ui)), ui);
    }
}

void TstAnfTuning::the_thetis_gain_default_survives_the_slider()
{
    // 10e-4 in WDSP terms is 1000 on a slider that stops at 999, so the
    // default has to clamp rather than wrap to something tiny. NR1 has
    // the same problem with 16e-4 and solves it the same way; this is
    // here so that the clamp is a decision on record rather than an
    // accident of std::min.
    SliceModel s;
    QCOMPARE(sliderFromGain(s.anfGain()), 999);
    QVERIFY(sliderFromGain(s.anfGain()) > 0);
}

void TstAnfTuning::position_defaults_to_post_agc()
{
    // Notching before the AGC lets the carrier drive the AGC anyway,
    // which is the complaint the notch was meant to fix.
    SliceModel s;
    QCOMPARE(s.anfPosition(), NrPosition::PostAgc);
    // And NR4 now has one at all, which it did not.
    QCOMPARE(s.nr4Position(), NrPosition::PostAgc);
}

QTEST_MAIN(TstAnfTuning)
#include "tst_anf_tuning.moc"
