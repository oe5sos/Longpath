// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_rtty_senden.cpp  (Longpath)
// =================================================================
// RTTY senden — und der Rundlauf durch den eigenen Dekoder.
//
// Der staerkste Pruefstand fuer einen Sender ist der Empfaenger, den
// es schon gibt: was RttyTxSource auf Toene legt, liest RttyDecoder
// wieder ein. Stimmen Baudot-Tafel, Bitreihenfolge, Start- und
// Stoppbits, Tonlage oder Takt nicht, kommt kein lesbarer Text
// heraus — und zwar ohne dass dieser Lauf wissen muesste, WO der
// Fehler steckt.
//
// Dazu die Regel allein: nachBaudot() ist statisch und ohne Zustand,
// also ohne Tonerzeugung zu pruefen.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include "core/RttyTx.h"
#include "core/RttyDecoder.h"

using namespace Longpath;

class TstRttySenden : public QObject
{
    Q_OBJECT

private slots:
    // ── Die Regel: ASCII zu Baudot ──────────────────────────────────

    // Jede Sendung faengt mit LTRS an: sonst steht der Empfaenger
    // womoeglich noch in der Ziffernbelegung vom vorigen Durchgang
    // und druckt Zahlen statt Buchstaben.
    void jedeSendungFaengtMitLtrsAn()
    {
        const QVector<int> b = RttyTxSource::nachBaudot(QStringLiteral("E"));
        QVERIFY(!b.isEmpty());
        QCOMPARE(b.first(), RttyTxSource::kLtrs);
        QCOMPARE(b.size(), 2);
        QCOMPARE(b.at(1), 1);          // 'E' steht auf Platz 1
    }

    // Vor einer Ziffer wird umgeschaltet, danach zurueck.
    void ziffernSchaltenUmUndZurueck()
    {
        // "A1A": Buchstabe, Ziffer, Buchstabe
        const QVector<int> b = RttyTxSource::nachBaudot(QStringLiteral("A1A"));
        QVector<int> erwartet;
        erwartet << RttyTxSource::kLtrs
                 << 3                           // 'A'
                 << RttyTxSource::kFigs
                 << 23                          // '1'
                 << RttyTxSource::kLtrs
                 << 3;                          // 'A'
        QCOMPARE(b, erwartet);
    }

    // Leerzeichen stehen in BEIDEN Tafeln auf demselben Platz. Fuer
    // sie wird nicht umgeschaltet — sonst stuende nach jedem Wort eine
    // Umschaltung mehr auf dem Band, und bei 45 Baud kostet jede 22 ms.
    void dasLeerzeichenSchaltetNichtUm()
    {
        const QVector<int> b = RttyTxSource::nachBaudot(QStringLiteral("1 2"));
        QVector<int> erwartet;
        erwartet << RttyTxSource::kLtrs
                 << RttyTxSource::kFigs
                 << 23                          // '1'
                 << 4                           // ' ' — kein Umschalten
                 << 19;                         // '2', immer noch FIGS
        QCOMPARE(b, erwartet);
    }

    // Zeichen, die Baudot nicht kennt, fallen weg. Ein Ersatzzeichen
    // waere schlimmer, weil es den Text verfaelscht.
    void unbekannteZeichenFallenWeg()
    {
        const QVector<int> b = RttyTxSource::nachBaudot(QStringLiteral("A~A"));
        QVector<int> erwartet;
        erwartet << RttyTxSource::kLtrs << 3 << 3;
        QCOMPARE(b, erwartet);
    }

    void kleinbuchstabenWerdenGross()
    {
        QCOMPARE(RttyTxSource::nachBaudot(QStringLiteral("a")),
                 RttyTxSource::nachBaudot(QStringLiteral("A")));
    }

    // ── Die Toene ───────────────────────────────────────────────────

    void unsinnigeZahlenWerdenAbgelehnt()
    {
        RttyTxSource q(48000);
        QVERIFY(!q.vorbereiten(QString()));                    // leer
        QVERIFY(!q.vorbereiten(QStringLiteral("CQ"), 0));      // kein Mark
        QVERIFY(!q.vorbereiten(QStringLiteral("CQ"), 2295, 0));// kein Shift
        QVERIFY(!q.vorbereiten(QStringLiteral("CQ"), 2295, 170, 0.0f));
        // Shift groesser als Mark: Space laege unter null Hertz.
        QVERIFY(!q.vorbereiten(QStringLiteral("CQ"), 170, 2295));
        QVERIFY(!q.laeuft());
        QCOMPARE(q.proben(), 0);
    }

    // Die Laenge stimmt mit der Rechnung: 7,5 Bitzeiten je Zeichen.
    void dieLaengeStimmt()
    {
        RttyTxSource q(48000);
        QVERIFY(q.vorbereiten(QStringLiteral("E")));   // LTRS + E = 2 Zeichen
        const double probenJeBit = 48000.0 / 45.45;
        const int erwartet = static_cast<int>(std::llround(probenJeBit * 7.5 * 2));
        QCOMPARE(q.proben(), erwartet);
    }

    // Ohne starten() kommt Stille, nicht der Rest von vorhin.
    void ohneStartenKommtStille()
    {
        RttyTxSource q(48000);
        QVERIFY(q.vorbereiten(QStringLiteral("CQ")));
        QVector<float> puffer(512, 1.0f);
        QCOMPARE(q.pullSamples(puffer.data(), 512), 512);
        for (float v : puffer) { QCOMPARE(v, 0.0f); }
    }

    // Und am Ende hoert sie von selbst auf.
    void amEndeHoertSieAuf()
    {
        RttyTxSource q(48000);
        QVERIFY(q.vorbereiten(QStringLiteral("E")));
        q.starten();
        QVERIFY(q.laeuft());
        QVector<float> puffer(q.proben() + 1024);
        q.pullSamples(puffer.data(), puffer.size());
        QVERIFY2(!q.laeuft(), "nach dem letzten Zeichen muss sie stehen");
    }

    // ── Der Rundlauf ────────────────────────────────────────────────
    //
    // Das eigentliche Stueck: der eigene Dekoder liest, was der
    // Sender schreibt.
    void derEigeneDekoderLiestEsWiederEin()
    {
        const QString text = QStringLiteral("CQ CQ DE OE5SOS");

        RttyTxSource q(48000);
        QVERIFY(q.vorbereiten(text));
        q.starten();

        QVector<float> mono(q.proben());
        QCOMPARE(q.pullSamples(mono.data(), mono.size()), mono.size());

        RttyDecoder dek;
        QString gelesen;
        connect(&dek, &RttyDecoder::textDecoded, this,
                [&gelesen](const QString& t, float) { gelesen += t; });
        dek.start();

        // Der Dekoder will Stereo, verschachtelt. Dazu etwas Vorlauf
        // in Mark-Ruhe, damit seine Huellkurven eingeschwungen sind,
        // bevor das erste Startbit kommt — ohne den frisst er das
        // erste Zeichen.
        const int vorlauf = 48000 / 4;
        QVector<float> stereo;
        stereo.reserve((vorlauf + mono.size() + vorlauf) * 2);
        double phase = 0.0;
        for (int i = 0; i < vorlauf; ++i) {
            phase += 2.0 * M_PI * 2295.0 / 48000.0;
            const float v = 0.5f * static_cast<float>(std::sin(phase));
            stereo << v << v;
        }
        for (float v : mono) { stereo << v << v; }
        for (int i = 0; i < vorlauf; ++i) {
            phase += 2.0 * M_PI * 2295.0 / 48000.0;
            const float v = 0.5f * static_cast<float>(std::sin(phase));
            stereo << v << v;
        }

        // In Haeppchen, wie im Betrieb.
        const int haeppchen = 480;
        for (int i = 0; i + haeppchen <= stereo.size() / 2; i += haeppchen) {
            dek.feedAudio(stereo.constData() + i * 2, haeppchen);
        }
        QTRY_VERIFY_WITH_TIMEOUT(gelesen.size() >= text.size(), 15000);
        dek.stop();

        const QString sauber = gelesen.trimmed();
        QVERIFY2(sauber.contains(text),
                 qPrintable(QStringLiteral("gesendet: %1 / gelesen: %2")
                                .arg(text, sauber)));
    }
};

QTEST_MAIN(TstRttySenden)
#include "tst_rtty_senden.moc"
