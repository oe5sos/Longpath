// =================================================================
// tests/tst_qso_recorder_applet.cpp  (NereusSDR)
// =================================================================
//
// Die Oberflaeche der QSO-Aufnahme.
//
// Geprueft wird ohne Funkgeraet und ohne Audiogeraet. Der Test
// schreibt selbst in die Zwischenspeicher — genau das, was der
// Audio-Faden tut — und sieht nach, ob die Anzeige es zeigt.
//
// DER FALL, DEN ICH HEUTE FUENFMAL GEFUNDEN HABE, ist ein anderer:
// gebaut, aber an keiner Flaeche. Der letzte Testfall prueft deshalb
// nicht Verhalten, sondern Erreichbarkeit — beide Spuren muessen da
// sein, sonst ist das Fenster genau die Anzeige, die man nicht bauen
// wollte.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <vector>

#include "gui/applets/QsoRecorderApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

QPushButton* recordButtonOf(QsoRecorderApplet& a)
{
    for (QPushButton* b : a.findChildren<QPushButton*>()) {
        if (b->text().contains(QStringLiteral("Record"))
            || b->text().contains(QStringLiteral("Stop"))) {
            return b;
        }
    }
    return nullptr;
}

// Die Uhr ist die einzige Beschriftung im Format mm:ss.
QLabel* clockOf(QsoRecorderApplet& a)
{
    static const QRegularExpression re(QStringLiteral("^\\d\\d:\\d\\d$"));
    for (QLabel* l : a.findChildren<QLabel*>()) {
        if (re.match(l->text()).hasMatch()) { return l; }
    }
    return nullptr;
}

} // namespace

class TestQsoRecorderApplet : public QObject
{
    Q_OBJECT

private slots:

    // BEIDE Spuren muessen da sein. Eine gemeinsame Anzeige laesst
    // Stille nicht von „Mikrofon aus" unterscheiden — das ist der ganze
    // Grund fuer die Aufteilung, und ohne Test faellt sie beim naechsten
    // Aufraeumen weg.
    void bothTracksAreOnScreen()
    {
        RadioModel model;
        QsoRecorderApplet a(&model);

        bool other = false, own = false;
        for (QLabel* l : a.findChildren<QLabel*>()) {
            if (l->text().contains(QStringLiteral("OTHER STATION"))) { other = true; }
            if (l->text().contains(QStringLiteral("YOUR VOICE")))    { own = true; }
        }
        QVERIFY2(other, "die Spur der Gegenstation fehlt");
        QVERIFY2(own,   "die eigene Spur fehlt — dann sieht man nicht, "
                        "ob das Mikrofon ankommt");
    }

    void theClockShowsItsCeiling()
    {
        RadioModel model;
        QsoRecorderApplet a(&model);

        bool sawCap = false;
        for (QLabel* l : a.findChildren<QLabel*>()) {
            if (l->text().contains(QStringLiteral("of %1:00")
                                       .arg(QsoRecorder::kMaxMinutes))) {
                sawCap = true;
            }
        }
        QVERIFY2(sawCap,
                 "der Deckel muss dastehen, bevor er zuschlaegt");
    }

    void theButtonStartsAndStopsTheRecording()
    {
        RadioModel model;
        QsoRecorderApplet a(&model);

        QPushButton* b = recordButtonOf(a);
        QVERIFY(b);
        QVERIFY(!model.qsoRecorder().isRecording());

        b->click();
        QVERIFY2(model.qsoRecorder().isRecording(),
                 "ein Klick nimmt auf");
        QVERIFY2(b->text().contains(QStringLiteral("Stop")),
                 "und der Knopf sagt, was der naechste Klick tut");

        b->click();
        QVERIFY(!model.qsoRecorder().isRecording());
    }

    // Die Uhr muss dem folgen, was wirklich in der Aufnahme liegt.
    void theClockFollowsWhatWasCollected()
    {
        RadioModel model;
        QsoRecorderApplet a(&model);

        QsoRecorderController& rec = model.qsoRecorder();
        rec.setSampleRate(1000);

        QPushButton* b = recordButtonOf(a);
        QVERIFY(b);
        b->click();

        // In Haeppchen, mit Abholen dazwischen. Der Zwischenspeicher
        // fasst zwei Sekunden — fuenf am Stueck hineinzuschreiben waere
        // im Betrieb ein Ueberlauf und im Test ein falsches Ergebnis,
        // das nach einem Fehler der Anzeige aussieht.
        const std::vector<float> oneSecond(2 * 1000, 0.4f);
        for (int i = 0; i < 5; ++i) {
            rec.rxRing().write(oneSecond.data(),
                               static_cast<int>(oneSecond.size()));
            rec.drainNow();
        }
        QCOMPARE(rec.droppedSamples(), 0LL);

        QLabel* clock = clockOf(a);
        QVERIFY(clock);
        QCOMPARE(clock->text(), QStringLiteral("00:05"));

        b->click();
    }
};

QTEST_MAIN(TestQsoRecorderApplet)
#include "tst_qso_recorder_applet.moc"
