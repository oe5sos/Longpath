// tests/tst_tci_spektrum_quelle.cpp  (Longpath)
// no-port-check: Longpath-eigener Prüfstand. Der Spektrumstrom hat kein
// Thetis-Vorbild.
//
// ── Worum es geht ────────────────────────────────────────────────────────────
//
// Am 2026-11-02 am echten Gerät (Flex, 40 m, lebendes Bild in Longpath):
// `spectrum_start:0,256,10;` wird bestätigt, das Protokoll meldet
// "Spektrum-Abgriff an FFTEngine::fftReady" und "Spektrum abonniert" — und
// es kommen 0 Binärrahmen in 8 s. Der Abgriff hing an der FFTEngine von
// Strom 0, und die rechnet bei der Flex nichts: die Maschine liefert das
// Bild fertig (RadioModel::flexSpectrumReady). Verbunden war nicht erreicht.
//
// Seit 2026-11-03 hängt der Server an RadioModel::spektrumBereit, einem
// geräteunabhängigen Weg, den jede Quelle füttert. Dieser Prüfstand fährt
// genau den Weg: ein Modell, ein Server, ein echter QWebSocket-Client, ein
// Abonnement — und dann Bilder über das MODELL, nicht über eine Engine.
//
// Gegenprobe gegen die alte Fassung (TciServer von origin/main, 2026-11-03):
// `modell_meldung_erreicht_den_client` fällt dort mit 0 Rahmen um, genau
// wie am Gerät. Mit der Behebung: Rahmen kommen, der Träger steht am
// richtigen Punkt, die Spanne wird aus der Meldung genommen.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>
#include <QVector>

#include "core/TciServer.h"
#include "core/TciBinaryFrame.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TestTciSpektrumQuelle : public QObject {
    Q_OBJECT

private:
    static quint32 feld(const QByteArray& f, int index) {
        const auto* p = reinterpret_cast<const quint8*>(f.constData());
        const int o = index * 4;
        return quint32(p[o]) | (quint32(p[o+1]) << 8)
             | (quint32(p[o+2]) << 16) | (quint32(p[o+3]) << 24);
    }
    static quint8 wert(const QByteArray& f, int i) {
        return static_cast<quint8>(f.at(64 + i));
    }

    // 4096 Bins Rauschen bei -120 dBm, ein Träger (-60 dBm) in der Mitte.
    static QVector<float> bildMitTraeger() {
        QVector<float> bins(4096, -120.0f);
        bins[2048] = -60.0f;
        return bins;
    }

    // Verbindet einen Client und bestellt rx 0 mit `punkte` Punkten, 10 B/s
    // und (wenn > 0) einer gewünschten Spanne. Wartet auf die Bestätigung
    // des Servers — erst dann steht das Abonnement wirklich.
    static bool abonniere(QWebSocket& client, TciServer& server,
                          int punkte, int spanneHz, QSignalSpy& textSpy) {
        QSignalSpy verbunden(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        if (!verbunden.wait(2000)) { return false; }

        const QString befehl = (spanneHz > 0)
            ? QStringLiteral("spectrum_start:0,%1,10,%2;").arg(punkte).arg(spanneHz)
            : QStringLiteral("spectrum_start:0,%1,10;").arg(punkte);
        client.sendTextMessage(befehl);

        // Die Bestätigung trägt die TATSÄCHLICH gültigen Werte.
        const QString erwartet = QStringLiteral("spectrum_start:0,%1,10;").arg(punkte);
        QElapsedTimer t; t.start();
        while (t.elapsed() < 5000) {
            for (int i = 0; i < textSpy.count(); ++i) {
                if (textSpy.at(i).at(0).toString().contains(erwartet)) { return true; }
            }
            QTest::qWait(20);
        }
        return false;
    }

    // Füttert das Modell wie eine Quelle es täte: alle 20 ms ein Bild,
    // bis `anzahl` Rahmen beim Client sind oder die Frist abläuft.
    static void fuettere(RadioModel& modell, int rx, int spanneHz,
                         QSignalSpy& binSpy, int anzahl, int fristMs) {
        const QVector<float> bild = bildMitTraeger();
        QElapsedTimer t; t.start();
        while (t.elapsed() < fristMs && binSpy.count() < anzahl) {
            modell.meldeSpektrum(rx, bild, spanneHz);
            QTest::qWait(20);
        }
    }

private slots:

    void initTestCase() {
        AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"),
                                         QStringLiteral("False"));
    }

    // ── Der Weg, der am Gerät tot war ────────────────────────────────────
    void modell_meldung_erreicht_den_client() {
        RadioModel modell;
        TciServer server(&modell);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy textSpy(&client, &QWebSocket::textMessageReceived);
        QSignalSpy binSpy(&client, &QWebSocket::binaryMessageReceived);
        QVERIFY2(abonniere(client, server, 256, 0, textSpy),
                 "Der Server muss spectrum_start bestaetigen");

        // Zehn Bilder sind verlangt, bei 10 B/s also rund eine Sekunde.
        // Alte Fassung: hier bleiben es 0 — genau der Befund vom Gerät.
        fuettere(modell, 0, 48000, binSpy, 5, 4000);
        QVERIFY2(binSpy.count() >= 5,
                 qPrintable(QStringLiteral("Erwartet >= 5 Spektrumrahmen, bekommen %1")
                                .arg(binSpy.count())));

        // Kopf: Stromtyp 100, 256 Punkte, rx 0.
        const QByteArray f = binSpy.at(0).at(0).toByteArray();
        QCOMPARE(f.size(), 64 + 256);
        QCOMPARE(feld(f, 0), 0u);
        QCOMPARE(feld(f, 5), 256u);
        QCOMPARE(feld(f, 6), static_cast<quint32>(TciStreamType::SpectrumStream));

        // Der Träger bei Bin 2048 von 4096 landet auf Punkt 128 von 256,
        // und -60 dBm sind Byte 140. Das Rauschen (-120 dBm) ist Byte 80.
        QCOMPARE(wert(f, 128), quint8(140));
        QCOMPARE(wert(f, 0),   quint8(80));
        QCOMPARE(wert(f, 255), quint8(80));

        // Und die Spanne wurde gemeldet: 48 kHz, so wie die Quelle sagte —
        // ohne dass je ein iq_samplerate vom Gerät kam.
        bool spanneGemeldet = false;
        for (int i = 0; i < textSpy.count(); ++i) {
            if (textSpy.at(i).at(0).toString().contains(
                    QStringLiteral("spectrum_span:0,48000;"))) {
                spanneGemeldet = true;
            }
        }
        QVERIFY2(spanneGemeldet, "Die Spanne muss aus der Meldung der Quelle kommen");

        client.close();
        server.stop();
    }

    // ── Der Flex-Weg, ausdrücklich ────────────────────────────────────────
    //
    // Am Gerät lief genau dieser Weg: die Maschine liefert 4096 Bins über
    // FlexVitaStream::spectrumReady → RadioModel::meldeFlexSpektrum. Im
    // Protokoll vom 2026-11-02 kommt "FFTEngine" genau einmal vor — in der
    // Abgriff-Zeile; der Flex-Treiber führt kein rawIqData, die Engine 0
    // rechnet dort nie. Der Prüfstand füttert den Flex-Einstieg selbst, mit
    // dem 200-kHz-Ausschnitt der Maschine, und erwartet Rahmen beim Client.
    // Alte Fassung: 0 Rahmen.
    void flex_meldung_erreicht_den_client() {
        RadioModel modell;
        modell.setFlexAusschnittForTest(6985340.0, 7185340.0);   // wie im Gerätelog
        TciServer server(&modell);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy textSpy(&client, &QWebSocket::textMessageReceived);
        QSignalSpy binSpy(&client, &QWebSocket::binaryMessageReceived);
        QVERIFY(abonniere(client, server, 718, 0, textSpy));   // 718 wie das Logprogramm

        // Das Fenster muss das Bild weiterhin bekommen …
        QSignalSpy fensterSpy(&modell, &RadioModel::flexSpectrumReady);

        const QVector<float> bild = bildMitTraeger();
        QElapsedTimer t; t.start();
        while (t.elapsed() < 4000 && binSpy.count() < 5) {
            modell.meldeFlexSpektrum(0x40000000u, bild);
            QTest::qWait(20);
        }
        QVERIFY(fensterSpy.count() >= 5);
        QVERIFY2(binSpy.count() >= 5,
                 qPrintable(QStringLiteral("Flex-Weg: erwartet >= 5 Rahmen, bekommen %1")
                                .arg(binSpy.count())));

        const QByteArray f = binSpy.at(0).at(0).toByteArray();
        QCOMPARE(f.size(), 64 + 718);
        QCOMPARE(feld(f, 6), static_cast<quint32>(TciStreamType::SpectrumStream));
        QCOMPARE(feld(f, 2), static_cast<quint32>(TciSampleType::UInt8Dbm));
        // Träger bei Bin 2048/4096 → Punkt 359 von 718.
        QCOMPARE(wert(f, 359), quint8(140));

        // Und die Spanne ist der Ausschnitt der Maschine: 200 000 Hz.
        bool gemeldet = false;
        for (int i = 0; i < textSpy.count(); ++i) {
            if (textSpy.at(i).at(0).toString().contains(
                    QStringLiteral("spectrum_span:0,200000;"))) {
                gemeldet = true;
            }
        }
        QVERIFY2(gemeldet, "Die Flex-Spanne (Panadapter-Ausschnitt) muss gemeldet werden");

        // Und die Bildmitte (2026-11-03, zweiter Schritt): an der Flex ist
        // `dds:` das VFO, nicht die Mitte — der Client braucht sie fuer die
        // Skala. (6 985 340 + 7 185 340) / 2 = 7 085 340 Hz.
        bool mitteGemeldet = false;
        for (int i = 0; i < textSpy.count(); ++i) {
            if (textSpy.at(i).at(0).toString().contains(
                    QStringLiteral("spectrum_centre:0,7085340;"))) {
                mitteGemeldet = true;
            }
        }
        QVERIFY2(mitteGemeldet, "Die Bildmitte muss als spectrum_centre gemeldet werden");

        client.close();
        server.stop();
    }

    // ── Die Spanne kommt von der Quelle, nicht vom I/Q-Strom ─────────────
    //
    // Die Flex meldet ihren Panadapter-Ausschnitt (hier 200 kHz), nie eine
    // I/Q-Abtastrate. Verlangt der Client 50 kHz, muss der Server mittig
    // auf ein Viertel beschneiden und GENAU diese 50 kHz zurückmelden. Ohne
    // die Spanne aus der Meldung wüsste er die Breite nicht, bliebe bei
    // allem und meldete 0.
    void spanne_der_quelle_bestimmt_den_ausschnitt() {
        RadioModel modell;
        TciServer server(&modell);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy textSpy(&client, &QWebSocket::textMessageReceived);
        QSignalSpy binSpy(&client, &QWebSocket::binaryMessageReceived);
        QVERIFY(abonniere(client, server, 256, 50000, textSpy));

        fuettere(modell, 0, 200000, binSpy, 2, 4000);
        QVERIFY2(binSpy.count() >= 1, "Es muss mindestens ein Rahmen ankommen");

        bool gemeldet = false;
        for (int i = 0; i < textSpy.count(); ++i) {
            if (textSpy.at(i).at(0).toString().contains(
                    QStringLiteral("spectrum_span:0,50000;"))) {
                gemeldet = true;
            }
        }
        QVERIFY2(gemeldet, "Ein Viertel von 200 kHz sind 50 kHz, und die sind zu melden");

        // Mittig beschnitten: der Träger in der Bildmitte bleibt in der
        // Bildmitte, und die Ränder sind Rauschen.
        const QByteArray f = binSpy.at(0).at(0).toByteArray();
        QCOMPARE(wert(f, 128), quint8(140));
        QCOMPARE(wert(f, 0),   quint8(80));

        client.close();
        server.stop();
    }

    // ── Nur Empfänger 0 und 1 ─────────────────────────────────────────────
    //
    // Die SunSDR-Engine trägt die Pseudo-Stromnummer 100000. Würde die als
    // Empfängernummer durchgereicht, verwürfe der Server sie — darum
    // hängt MainWindow_SunSdr.cpp sie ausdrücklich als Empfänger 0 an.
    // Hier wird festgehalten, dass das Verwerfen tatsächlich greift.
    void fremde_empfaengernummer_wird_verworfen() {
        RadioModel modell;
        TciServer server(&modell);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy textSpy(&client, &QWebSocket::textMessageReceived);
        QSignalSpy binSpy(&client, &QWebSocket::binaryMessageReceived);
        QVERIFY(abonniere(client, server, 256, 0, textSpy));

        fuettere(modell, 100000, 48000, binSpy, 1, 600);
        QCOMPARE(binSpy.count(), 0);

        client.close();
        server.stop();
    }

    // ── Ohne Abonnement kein Rahmen ───────────────────────────────────────
    void ohne_abonnement_kein_rahmen() {
        RadioModel modell;
        TciServer server(&modell);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy verbunden(&client, &QWebSocket::connected);
        QSignalSpy binSpy(&client, &QWebSocket::binaryMessageReceived);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(verbunden.wait(2000));
        QTest::qWait(100);

        fuettere(modell, 0, 48000, binSpy, 1, 600);
        QCOMPARE(binSpy.count(), 0);

        client.close();
        server.stop();
    }
};

QTEST_GUILESS_MAIN(TestTciSpektrumQuelle)
#include "tst_tci_spektrum_quelle.moc"

#else  // !HAVE_WEBSOCKETS

int main() { return 0; }

#endif // HAVE_WEBSOCKETS
