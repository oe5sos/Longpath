// SPDX-License-Identifier: GPL-3.0-or-later
//
// Vom Ton zum Text — der ganze Weg, mit einem gestellten Erkenner.
//
// 2026-08-23. Der Betreiber hat sich fuer den oertlichen Dienst
// entschieden ("1"), also Whisper auf seinem eigenen Rechner. Der Weg
// dorthin fuehrt durch vier Stationen:
//
//   Abgriff -> umtasten 48 kHz Stereo auf 16 kHz Mono
//           -> Zerleger (Sprechabschnitte finden)
//           -> Erkenner auf eigenem Faden
//           -> Text
//
// Jede einzeln zu pruefen genuegt nicht: die Uebergaenge sind die
// gefaehrlichen Stellen. Einer davon ist die Warteschlange zum
// Arbeiterfaden — Qt schickt einen Typ nur darueber, wenn er
// angemeldet ist, und schlaegt sonst STILL fehl. Kein
// Uebersetzungsfehler, keine Meldung, nur kein Text.
//
// Der Erkenner ist hier gestellt: er braucht keinen Dienst, keine
// Modelldatei und kein Netz. Geprueft wird der WEG, nicht Whisper.

#include <QtTest>
#include <QSignalSpy>

#include "asr/AsrService.h"
#include "asr/IAsrBackend.h"

#include <cmath>

using namespace Longpath;

namespace {

// Ein Erkenner, der zaehlt, was bei ihm ankommt, und eine feste
// Antwort gibt.
class FakeBackend : public IAsrBackend {
public:
    bool load(const QString&, QString*) override { m_loaded = true; return true; }
    bool isLoaded() const override { return m_loaded; }
    void unload() override { m_loaded = false; }

    AsrTranscript transcribe(const std::vector<float>& pcm, QString*) override
    {
        ++calls;
        lastSamples = int(pcm.size());
        return {QStringLiteral("CQ CQ DE OE5SOS"), 0.87f};
    }

    std::atomic<int> calls{0};
    std::atomic<int> lastSamples{0};
private:
    bool m_loaded = false;
};

// Ein Sprechabschnitt: Rauschen mit Sprechpegel, dann Stille.
// Verschraenktes Stereo bei 48 kHz, wie es aus der Mischung kommt.
QVector<float> speechThenSilence(int speechMs, int silenceMs, int rate = 48000)
{
    const int sFrames = rate * speechMs / 1000;
    const int qFrames = rate * silenceMs / 1000;
    QVector<float> out((sFrames + qFrames) * 2, 0.0f);
    for (int i = 0; i < sFrames; ++i) {
        // 0,2 Effektivwert liegt deutlich ueber der Vorgabeschwelle
        // von 0,010 — ein Sprechabschnitt, kein Rauschteppich.
        const float v = 0.28f * std::sin(float(i) * 0.05f);
        out[i * 2]     = v;
        out[i * 2 + 1] = v;
    }
    return out;
}

} // namespace

class TstAsrEndToEnd : public QObject
{
    Q_OBJECT

private slots:
    void tonWirdZuText()
    {
        AsrService svc;
        auto fake = std::make_unique<FakeBackend>();
        FakeBackend* raw = fake.get();
        svc.setBackend(std::move(fake));
        svc.start();
        QVERIFY(svc.isRunning());

        QSignalSpy got(&svc, &AsrService::transcript);

        const QVector<float> audio = speechThenSilence(800, 600);
        svc.feedForTest(audio.constData(), audio.size() / 2, 48000);

        // Der Erkenner sitzt auf einem eigenen Faden; das Ergebnis
        // kommt ueber die Warteschlange zurueck.
        QVERIFY2(got.wait(5000),
                 "Kein Text angekommen — der Weg ist irgendwo unterbrochen");
        QCOMPARE(got.count(), 1);
        QCOMPARE(got.at(0).at(0).toString(), QStringLiteral("CQ CQ DE OE5SOS"));
        qInfo().noquote() << "Text:" << got.at(0).at(0).toString()
                          << " Zuversicht:" << got.at(0).at(1).toFloat();

        // ── Die Umtastung ist wirklich passiert ─────────────────────
        //
        // 800 ms Sprache PLUS 300 ms Nachlauf, den der Zerleger
        // absichtlich mitnimmt (hangoverMs), sind 1100 ms. Bei 16 kHz
        // also 17 600 Werte. Kaeme der Ton ungetastet an, waeren es
        // dreimal so viele — und Whisper bekaeme 48 kHz vorgesetzt, wo
        // es 16 erwartet, und lieferte etwas, das plausibel klingt und
        // nicht stimmt.
        //
        // ── Was diese Zeile schon gefunden hat ──────────────────────
        //
        // Beim ersten Lauf standen hier 11 040 Werte, beim naechsten
        // 17 600 — dieselbe Eingabe, verschiedene Ergebnisse. Die
        // Ursache war ein echter Fehler: AsrService gab dem Wandler
        // bis zu 67 200 Rahmen auf einmal, obwohl er fuer 16 384
        // gebaut war. Es kam Ton heraus, nur die falsche Menge, und
        // ohne diese Zaehlung waere das nie aufgefallen.
        //
        // Die erste Fassung dieser Zeile erwartete 12 800 und war
        // damit selbst falsch — der Nachlauf war mir entgangen.
        const int n = raw->lastSamples.load();
        qInfo() << "Werte an den Erkenner:" << n << "(erwartet 17600)";
        QVERIFY2(n > 15000 && n < 20000,
                 qPrintable(QStringLiteral("%1 Werte — die Umtastung "
                                           "stimmt nicht").arg(n)));
        svc.stop();
    }

    void stilleErzeugtKeinenAufruf()
    {
        // Whisper auf Rauschen laufen zu lassen kostet Rechenzeit und
        // liefert erfundene Woerter. Der Zerleger muss das abfangen.
        AsrService svc;
        auto fake = std::make_unique<FakeBackend>();
        FakeBackend* raw = fake.get();
        svc.setBackend(std::move(fake));
        svc.start();

        QVector<float> quiet(48000 * 2, 0.0f);
        svc.feedForTest(quiet.constData(), 48000, 48000);
        QTest::qWait(300);
        qInfo() << "Aufrufe bei Stille:" << raw->calls.load();
        QCOMPARE(raw->calls.load(), 0);
        svc.stop();
    }

    void anhaltenBeendetDenFaden()
    {
        // Der Arbeiter haelt einen nackten Zeiger auf den Erkenner.
        // Wird der freigegeben, waehrend der Faden noch laeuft, ist das
        // ein Absturz mit Ansage.
        AsrService svc;
        svc.setBackend(std::make_unique<FakeBackend>());
        svc.start();
        QVERIFY(svc.isRunning());
        svc.stop();
        QVERIFY(!svc.isRunning());
        // Und ein zweites Mal darf nichts tun.
        svc.stop();
        qInfo() << "Anhalten ist wiederholbar";
    }
};

QTEST_MAIN(TstAsrEndToEnd)
#include "tst_asr_end_to_end.moc"
