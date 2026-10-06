// =================================================================
// tests/tst_qso_recorder_applet.cpp  (Longpath)
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
// Modification history (Longpath):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-09-02 — Deckel-Anzeige entfernt (zeitlich unbegrenzt, siehe
//                 QsoRecorder.h), von Martin Fischer, KI-gestuetzt
//                 ueber Anthropic Claude (Cowork).
//   2026-10-06 — Prueffall fuer einen abgelehnten Start, von Martin
//                 Fischer, KI-gestuetzt ueber Anthropic Claude
//                 (Cowork). Begruendung am Prueffall.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <vector>

#include "core/audio/QsoRecorderController.h"
#include "gui/applets/QsoRecorderApplet.h"
#include "models/RadioModel.h"

using namespace Longpath;

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

    // Der Deckel ist weg (2026-09-02, Ansage des Betreibers: die
    // Aufnahme soll zeitlich unbegrenzt laufen) — daneben darf keine
    // Zielmarke "of XX:00" mehr auftauchen. Der Schutz gegen eine
    // vergessen laufende Aufnahme ist jetzt eine Speicherplatz-Wache im
    // Hintergrund, keine Uhr zum Anschauen.
    void theClockHasNoCeilingAnymore()
    {
        RadioModel model;
        QsoRecorderApplet a(&model);

        static const QRegularExpression capPattern(
            QStringLiteral("^of \\d+:00$"));
        for (QLabel* l : a.findChildren<QLabel*>()) {
            QVERIFY2(!capPattern.match(l->text()).hasMatch(),
                     "kein Deckel mehr neben der Uhr");
        }
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

    // ── Der Grund fuer einen fehlgeschlagenen Start muss im
    //    Prueflauf SICHTBAR werden, nicht als Timeout enden.
    //
    // Am 2026-10-05 lief dieser Prueffall nicht, aber der darueber:
    // Martins Platte hatte 9 % frei, der Boden ist 10 — die
    // Platz-Wache in onRecordClicked() oeffnete ein modales
    // QMessageBox::warning, und QDialog::exec() wartete auf einen
    // Klick, den im Prueflauf niemand macht. Ergebnis: 120-s-Timeout
    // statt einer Meldung. Dieselbe Klasse wie der CI-Hänger in
    // docs/architecture/2026-09-09-ci-discovery-hang-investigation.md.
    //
    // Geprueft wird deshalb beides: dass der Klick ZURUECKKOMMT (haengt
    // er, laeuft dieser Prueffall in den Timeout, und genau das ist
    // der Fang), und dass der Grund danach an der Flaeche steht.
    void aRefusedStartSaysSoInsteadOfBlocking()
    {
        RadioModel model;
        QsoRecorderApplet a(&model);

        QsoRecorderController::setHasEnoughDiskSpaceForTests(false);
        // Auch wenn hier etwas schiefgeht: der naechste Prueffall darf
        // die Platte nicht voll vorfinden.
        struct Restore {
            ~Restore()
            { QsoRecorderController::setHasEnoughDiskSpaceForTests(true); }
        } restore;

        QPushButton* b = recordButtonOf(a);
        QVERIFY(b);

        b->click();

        QVERIFY2(!model.qsoRecorder().isRecording(),
                 "bei zu wenig Platz wird nicht aufgenommen");
        QVERIFY2(b->text().contains(QStringLiteral("Record")),
                 "und der Knopf bietet es weiter an, statt \"Stop\" "
                 "zu behaupten");

        // Der Grund gehoert an die Flaeche — sonst steht der Bediener
        // vor einem Knopf, der nichts tut.
        // isVisibleTo(&a), nicht isVisible(): das Applet wird hier nie
        // gezeigt, und ohne gezeigtes Fenster ist isVisible() immer
        // falsch — auch fuer eine Zeile, die im Betrieb dasteht.
        // Gefragt ist, ob sie ausgeblendet ist, nicht ob ein Schirm da
        // ist.
        bool said = false;
        for (QLabel* l : a.findChildren<QLabel*>()) {
            if (l->isVisibleTo(&a)
                && l->text().contains(QStringLiteral("disk space"),
                                      Qt::CaseInsensitive)) {
                said = true;
            }
        }
        QVERIFY2(said, "der Grund muss dastehen, nicht nur im Protokoll");
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
