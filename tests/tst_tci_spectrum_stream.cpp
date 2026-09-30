// no-port-check: Longpath-eigener Prüfstand. Der Spektrumstrom hat kein
// Thetis-Vorbild — Thetis kennt nur die fünf TCI-Ströme (I/Q, RX-Ton, TX-Ton,
// TX_CHRONO, Lineout) und lässt jeden Client, der ein Bild will, den rohen
// I/Q-Strom ziehen und selbst eine FFT rechnen.
//
// Warum es ihn trotzdem gibt, in einer Zahl: mit der eigenen Handfunke
// gemessen kostet roher I/Q 404 kB/s bei 48 kHz, hochgerechnet rund 1,5 MB/s
// bei den 192 kHz, die am ANAN anliegen. Das trägt im WLAN und bricht
// unterwegs. Der Server hat die fertigen dBm-Bins ohnehin (FFTEngine::
// fftReady); sie auf die Bildpunktzahl des Clients zu verdichten und als ein
// Byte je Punkt zu schicken kostet rund 27 kB/s.
//
// Geprüft wird hier der Rahmen selbst (Kopf, Kodierung, Randfälle) und die
// Verdichtung. Der Weg über den Draht ist in tst_tci_origin_and_tx_time_cap
// und live gegen die laufende Fassung belegt.

#include <QtTest/QtTest>

#include "core/TciBinaryFrame.h"

using namespace Longpath;

class TestTciSpectrumStream : public QObject {
    Q_OBJECT

private:
    // Der Kopf ist 16 x uint32 little-endian — genau wie bei den anderen
    // Strömen, damit ein Client nur einen Zerleger braucht.
    static quint32 feld(const QByteArray& f, int index) {
        const auto* p = reinterpret_cast<const quint8*>(f.constData());
        const int o = index * 4;
        return quint32(p[o]) | (quint32(p[o+1]) << 8)
             | (quint32(p[o+2]) << 16) | (quint32(p[o+3]) << 24);
    }
    static quint8 wert(const QByteArray& f, int i) {
        return static_cast<quint8>(f.at(64 + i));
    }

private slots:

    void kopf_traegt_die_vereinbarten_felder() {
        const QVector<float> bins{-100.0f, -90.0f, -80.0f, -70.0f};
        const QByteArray f = TciBinaryFrame::buildSpectrumPayload(
            /*receiver*/ 1, /*fps*/ 10, bins.size(), bins.constData());

        QCOMPARE(f.size(), 64 + 4);
        QCOMPARE(feld(f, 0), 1u);     // receiver
        QCOMPARE(feld(f, 1), 10u);    // hier: Bildrate, nicht Abtastrate
        QCOMPARE(feld(f, 2), static_cast<quint32>(TciSampleType::UInt8Dbm));
        QCOMPARE(feld(f, 5), 4u);     // Anzahl Werte
        QCOMPARE(feld(f, 6), static_cast<quint32>(TciStreamType::SpectrumStream));
        QCOMPARE(feld(f, 7), 1u);     // ein "Kanal"
    }

    void kodierung_ist_dbm_plus_200() {
        // Das Maß stammt von piHPSDR (src/server_thread.c): ein Byte je Wert,
        // dBm + 200, also -200..+55 dBm bei 1 dB Auflösung. Es deckt genau den
        // Bereich ab, den ein Empfänger zeigt.
        const QVector<float> bins{-200.0f, -130.0f, -73.0f, 0.0f, 55.0f};
        const QByteArray f = TciBinaryFrame::buildSpectrumPayload(0, 10, bins.size(),
                                                                 bins.constData());
        QCOMPARE(wert(f, 0), quint8(0));     // -200 dBm -> 0
        QCOMPARE(wert(f, 1), quint8(70));    // -130 dBm -> 70
        QCOMPARE(wert(f, 2), quint8(127));   //  -73 dBm (S9) -> 127
        QCOMPARE(wert(f, 3), quint8(200));   //    0 dBm -> 200
        QCOMPARE(wert(f, 4), quint8(255));   //  +55 dBm -> 255
    }

    void werte_ausserhalb_werden_geklemmt_statt_umzulaufen() {
        // Ohne Klemmung liefe ein Wert unter -200 dBm auf einen GROSSEN Byte
        // wert über — im Wasserfall ein greller Strich genau dort, wo gar
        // nichts ist.
        const QVector<float> bins{-500.0f, 999.0f};
        const QByteArray f = TciBinaryFrame::buildSpectrumPayload(0, 10, bins.size(),
                                                                 bins.constData());
        QCOMPARE(wert(f, 0), quint8(0));
        QCOMPARE(wert(f, 1), quint8(255));
    }

    void nan_faellt_auf_den_boden() {
        // Ein stummer Kanal liefert gelegentlich NaN oder -inf. Beides darf
        // nicht als Zufallsbyte durchrutschen.
        const QVector<float> bins{std::numeric_limits<float>::quiet_NaN(),
                                  -std::numeric_limits<float>::infinity()};
        const QByteArray f = TciBinaryFrame::buildSpectrumPayload(0, 10, bins.size(),
                                                                 bins.constData());
        QCOMPARE(wert(f, 0), quint8(0));
        QCOMPARE(wert(f, 1), quint8(0));
    }

    void leerer_rahmen_wird_nicht_gebaut() {
        QVERIFY(TciBinaryFrame::buildSpectrumPayload(0, 10, 0, nullptr).isEmpty());
        const float eins = -100.0f;
        QVERIFY(TciBinaryFrame::buildSpectrumPayload(0, 10, -5, &eins).isEmpty());
        QVERIFY(TciBinaryFrame::buildSpectrumPayload(0, 10, 4, nullptr).isEmpty());
    }

    void ein_byte_je_punkt_ist_der_ganze_gewinn() {
        // Die Rechnung, die den Strom rechtfertigt, als Prüfpunkt festgehalten:
        // 256 Punkte kosten 64 + 256 = 320 Byte. Bei 10 Bildern je Sekunde sind
        // das 3,2 kB/s — gegen 384 kB/s für rohes I/Q bei 48 kHz float32
        // stereo, und rund 1,5 MB/s bei 192 kHz.
        QVector<float> bins(256, -120.0f);
        const QByteArray f = TciBinaryFrame::buildSpectrumPayload(0, 10, bins.size(),
                                                                 bins.constData());
        QCOMPARE(f.size(), 320);

        const double spektrumProSekunde = f.size() * 10.0;
        const double iqProSekunde192k   = 192000.0 * 2 /*I+Q*/ * 4 /*float32*/;
        QVERIFY2(iqProSekunde192k / spektrumProSekunde > 400.0,
                 "Der Spektrumstrom muss um Größenordnungen billiger sein als I/Q");
    }

    // ── Verdichtung ──────────────────────────────────────────────────────────
    //
    // Die Verdichtung selbst sitzt in TciServer::onFftBinsReady (sie braucht
    // die Sitzung). Hier wird die Regel geprüft, auf die es ankommt, an einer
    // gleichlautenden Rechnung: über den SPITZENWERT, nicht den Mittelwert.

    void spitzenwert_erhaelt_einen_schmalen_traeger() {
        // 1024 Bins Rauschen, ein einziger Träger. Über den Mittelwert
        // verdichtet verschwindet er; über den Spitzenwert bleibt er stehen.
        const int n = 1024, punkte = 64;
        QVector<float> bins(n, -130.0f);
        bins[500] = -60.0f;                       // ein Träger, ein Bin breit

        QVector<float> spitze(punkte), mittel(punkte);
        for (int i = 0; i < punkte; ++i) {
            const int von = static_cast<int>(static_cast<qint64>(i) * n / punkte);
            const int bis = std::max(von + 1,
                static_cast<int>(static_cast<qint64>(i + 1) * n / punkte));
            float s = -200.0f, summe = 0.0f;
            int zahl = 0;
            for (int b = von; b < bis && b < n; ++b) {
                s = std::max(s, bins[b]); summe += bins[b]; ++zahl;
            }
            spitze[i] = s;
            mittel[i] = summe / std::max(1, zahl);
        }

        const int topf = 500 * punkte / n;
        QCOMPARE(spitze[topf], -60.0f);
        QVERIFY2(mittel[topf] < -120.0f,
                 "Der Mittelwert begräbt den Träger — genau deshalb Spitzenwert");
    }

    void verdichtung_deckt_jeden_bin_ab() {
        // Kein Bin darf durch die Ritzen fallen: die Töpfe müssen lückenlos
        // aneinanderstoßen, sonst verschwindet ein Träger je nach Frequenz.
        for (const int punkte : {64, 100, 256, 373, 1024}) {
            const int n = 4096;
            int letzterBis = 0;
            for (int i = 0; i < punkte; ++i) {
                const int von = static_cast<int>(static_cast<qint64>(i) * n / punkte);
                const int bis = std::max(von + 1,
                    static_cast<int>(static_cast<qint64>(i + 1) * n / punkte));
                QCOMPARE(von, letzterBis);   // stößt am vorigen Topf an
                QVERIFY(bis > von);          // kein leerer Topf
                letzterBis = bis;
            }
            QCOMPARE(letzterBis, n);         // und deckt bis zum letzten Bin
        }
    }
};

QTEST_MAIN(TestTciSpectrumStream)
#include "tst_tci_spectrum_stream.moc"
