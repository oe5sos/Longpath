// =================================================================
// tests/tst_flex_wasserfall_kachel.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Die FLEX-8400 fuehrt ZWEI Bildstroeme, und bis zum 2026-10-23 zog
// Longpath nur den ersten: der Wasserfall wurde aus den FFT-Bins
// abgeleitet, mit eigenem Zeitgeber und eigenen Schwellen. Der
// Betreiber am 2026-10-22: „das rot ist eigenartig gewesen, auch die
// zeitliche laenge, bis der wasserfall anfaengt".
//
// Der Weg steht in docs/protocols/flex-wasserfall-aethersdr.md,
// nachgelesen bei AetherSDR @d58e2b8a. Dieser Stand prueft die drei
// Dinge, an denen es scheitern kann:
//
//   1. Die Kachelwerte sind KEINE dBm. Sie sind eine Intensitaet
//      (qint16/128, am Geraet +88 bis +120). In den dBm-Weg gegeben
//      stuenden sie sofort am oberen Anschlag — genau das Rot, das
//      behoben werden soll. (Erst gebaut, dann herausgenommen,
//      2026-10-22.)
//   2. Ein Traeger muss an SEINER Frequenz landen, auch wenn der
//      Ausschnitt der Kachel und der des Fensters nicht
//      uebereinstimmen (gemessen 230,6 kHz gegen 200 kHz).
//   3. Solange Kacheln kommen, darf der abgeleitete Weg nicht
//      mitschreiben — zwei Schreiber aus zwei Takten geben ein
//      Flackern.
// =================================================================

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"
#include "core/FlexVitaReader.h"
#include "core/FlexRadioConnection.h"

#include <algorithm>
#include <cmath>

using namespace Longpath;

namespace {

// Eine Kachel, wie die Maschine sie schickt: Rauschteppich um 96,
// ein Traeger bei 115 an bekannter Stelle. Zahlen aus AetherSDRs
// eigenem Kommentar („typical noise floor ~96-106, signal peaks
// ~110-115").
QVector<float> kachel(int bins, int traegerBin, float traeger = 115.0f)
{
    QVector<float> v(bins);
    for (int i = 0; i < bins; ++i) {
        v[i] = 96.0f + 1.5f * std::sin(0.37f * i);   // Teppich mit Textur
    }
    if (traegerBin >= 0 && traegerBin < bins) { v[traegerBin] = traeger; }
    return v;
}

int hoechsteStufe(const QVector<quint8>& stufen, int* wo)
{
    int best = -1, idx = -1;
    for (int i = 0; i < stufen.size(); ++i) {
        if (stufen[i] > best) { best = stufen[i]; idx = i; }
    }
    if (wo) { *wo = idx; }
    return best;
}

} // namespace

class TstFlexWasserfallKachel : public QObject {
    Q_OBJECT

private:
    // Ein Fenster, das wirklich ein Wasserfallbild hat: ohne Groesze
    // ist `m_waterfall` leer und jede Zeile kehrt sofort um.
    void richteEin(SpectrumWidget& sw, double mitteHz, double breiteHz)
    {
        sw.setKiwiDisplaySource(true);
        sw.setFrequencyRange(mitteHz, breiteHz);
        sw.resize(1200, 600);
        sw.show();
        QTest::qWait(300);
    }

private slots:

    // ── 1. Der Teppich darf NICHT am Anschlag stehen ─────────────────
    void derTeppichStehtNichtAmAnschlag()
    {
        SpectrumWidget sw;
        richteEin(sw, 14.100e6, 200.0e3);

        sw.zeigeFremdeWasserfallzeile(kachel(2048, -1), 14.000e6, 14.200e6);
        const auto stufen = sw.wfZeilenstufenForTest();
        QVERIFY(!stufen.isEmpty());

        int wo = -1;
        const int hoch = hoechsteStufe(stufen, &wo);
        double summe = 0.0;
        for (quint8 v : stufen) { summe += v; }
        const double mittel = summe / stufen.size();
        qInfo().nospace() << "nur Teppich: hoechste Stufe " << hoch
                          << ", Mittel " << mittel;

        // Waere die Intensitaet als dBm gelesen worden, stuende JEDER
        // Punkt auf 255 — das ist das einfarbige Rot.
        // Und nicht am unteren Anschlag: der Teppich soll seine
        // Struktur zeigen, nicht schwarz sein („der teppich sollte
        // nicht so dunkel sein", 2026-10-23).
        QVERIFY2(mittel > 15.0,
                 qPrintable(QStringLiteral("Der Rauschteppich steht im "
                     "Mittel auf %1 von 255 — zu dunkel, seine Struktur "
                     "ist nicht zu sehen.").arg(mittel, 0, 'f', 1)));
        QVERIFY2(mittel < 200.0,
                 qPrintable(QStringLiteral("Der Rauschteppich steht im "
                     "Mittel auf %1 von 255 — das Bild ist einfarbig.")
                     .arg(mittel, 0, 'f', 1)));
    }

    // ── 2. Der Traeger landet an seiner Frequenz ─────────────────────
    void derTraegerLandetAnSeinerFrequenz()
    {
        SpectrumWidget sw;
        // Fenster 200 kHz um 14.100 — die Kachel ist BREITER (230,6
        // kHz), genau wie am Geraet gemessen.
        richteEin(sw, 14.100e6, 200.0e3);

        const int bins = 2048;
        const double kachelTief = 13.9846e6, kachelHoch = 14.2152e6;
        // Traeger auf 14.125 MHz setzen.
        const double zielHz = 14.125e6;
        const int traegerBin = static_cast<int>(
            (zielHz - kachelTief) / ((kachelHoch - kachelTief) / bins));

        sw.zeigeFremdeWasserfallzeile(kachel(bins, traegerBin),
                                      kachelTief, kachelHoch);
        const auto stufen = sw.wfZeilenstufenForTest();
        QVERIFY(!stufen.isEmpty());

        int wo = -1;
        const int hoch = hoechsteStufe(stufen, &wo);
        // Erwartete Stelle im Fenster.
        const double fensterTief = 14.100e6 - 100.0e3;
        const int erwartet = static_cast<int>(
            (zielHz - fensterTief) / (200.0e3 / stufen.size()));
        qInfo().nospace() << "Traeger bei 14.125 MHz: hoechste Stufe "
                          << hoch << " an Punkt " << wo
                          << ", erwartet " << erwartet;

        QVERIFY2(hoch > 0, "der Traeger ist gar nicht angekommen");
        QVERIFY2(std::abs(wo - erwartet) <= 3,
                 qPrintable(QStringLiteral("Der Traeger steht an Punkt %1 "
                     "statt %2 — das sind %3 Bildpunkte daneben.")
                     .arg(wo).arg(erwartet).arg(std::abs(wo - erwartet))));
    }

    // ── 3. Nur EIN Schreiber ─────────────────────────────────────────
    void dieKachelUebernimmtDasBild()
    {
        SpectrumWidget sw;
        richteEin(sw, 14.100e6, 200.0e3);
        QVERIFY(!sw.fremdeWasserfallquelleForTest());

        sw.zeigeFremdeWasserfallzeile(kachel(2048, 500), 14.000e6, 14.200e6);
        QVERIFY2(sw.fremdeWasserfallquelleForTest(),
                 "nach einer Kachel muss der abgeleitete Weg schweigen");
    }

    // ── Der Wasserfall ist ein EIGENER Strom ────────────────────────
    //
    // Der Panadapter-Zustand nennt ihn (`waterfall=0x42000000` neben
    // `panadapter=0x40000000`), und er hat eigene Einstellungen.
    // Longpath hat sie nie angefasst: am Geraet stand in ALLEN fuenf
    // Statusmeldungen `auto_black=1`, also rechnete die Maschine ihren
    // eigenen Schwarzpunkt in die Kachel — auf SmartSDRs Darstellung
    // gezielt, waehrend wir obendrauf noch einmal schaetzen.
    //
    // From AetherSDR RadioModel::configureWaterfall [@d58e2b8a].
    void wasserfallstromWirdEingerichtet()
    {
        using FlexCommandWire::setWaterfallSetupCommand;

        QCOMPARE(setWaterfallSetupCommand(QStringLiteral("0x42000000"), false),
                 QStringLiteral("display panafall set 0x42000000 "
                                "auto_black=0 black_level=0 color_gain=50 "
                                "line_duration=100"));
        // Rueckfall fuer Fassungen ohne Panafall-Adressierung.
        QCOMPARE(setWaterfallSetupCommand(QStringLiteral("0x42000000"), true),
                 QStringLiteral("display waterfall set 0x42000000 "
                                "auto_black=0 black_level=0 color_gain=50 "
                                "line_duration=100"));
        QVERIFY(setWaterfallSetupCommand(QString(), false).isEmpty());
    }

    // ── Die Umrechnung selbst, als Beleg fuer die Einheit ────────────
    void kachelwerteSindIntensitaetKeineDbm()
    {
        // Echte Werte vom Geraet liegen bei ~11300 -> ~88.
        QCOMPARE(FlexVita::tileColourValue(11438), 11438.0f / 128.0f);
        QVERIFY(FlexVita::tileColourValue(11438) > 85.0f);
        QVERIFY(FlexVita::tileColourValue(11438) < 95.0f);
        // POSITIV. Waeren es dBm, muessten sie negativ sein — und in
        // Longpaths dBm-Wasserfall stuende alles am oberen Anschlag.
        QVERIFY(FlexVita::tileColourValue(11438) > 0.0f);
    }

    // ── 4. Die zeitliche Mittelung (2026-10-23) ──────────────────────
    //
    // Der Betreiber im Vergleich mit der Anvelina: „oben schaut es nach
    // strichen aus, flex nach punkten", und nach dem Wechsel von
    // Maximum auf Mittel JE BILDPUNKT: „unten ist es nach wie vor sehr
    // pixelig". Es lag an der ZEIT: der abgeleitete Weg mittelt ueber
    // die Frames, die Kachel ging ungemittelt aufs Bild.
    //
    // Gemessen wird das, was man sieht: die Streuung der Stufen von
    // Zeile zu Zeile an derselben Stelle. Rauschen allein muss ruhig
    // werden.
    void dieMittelungBeruhigtDasRauschen()
    {
        // Ein Teppich mit WECHSELNDEM Rauschen, zweimal durchgefahren:
        // einmal ohne Mittelung, einmal mit. Dieselben Zahlen, damit
        // nur die Mittelung den Unterschied macht.
        auto streuung = [this](SpectrumAveraging art) -> double {
            SpectrumWidget sw;
            sw.setWaterfallAveraging(art);
            sw.setWaterfallAverageTimeMs(400);
            richteEin(sw, 14.100e6, 200.0e3);

            QVector<double> vorher;
            double summeAbstand = 0.0; int zahl = 0;
            quint32 saat = 12345;
            for (int zeileNr = 0; zeileNr < 24; ++zeileNr) {
                QVector<float> k(2048);
                for (int i = 0; i < 2048; ++i) {
                    // Eigener Zufall, damit der Lauf wiederholbar ist.
                    saat = saat * 1103515245u + 12345u;
                    const float r = float((saat >> 16) & 0x7fff) / 32767.0f;
                    k[i] = 96.0f + 6.0f * (r - 0.5f);
                }
                sw.zeigeFremdeWasserfallzeile(k, 14.000e6, 14.200e6);
                const auto stufen = sw.wfZeilenstufenForTest();
                if (stufen.isEmpty()) { return -1.0; }
                QVector<double> jetztStufen;
                for (int x = 0; x < stufen.size(); x += 37) {
                    jetztStufen.append(stufen[x]);
                }
                if (!vorher.isEmpty() && zeileNr > 6) {
                    for (int i = 0; i < jetztStufen.size(); ++i) {
                        summeAbstand += std::abs(jetztStufen[i] - vorher[i]);
                        ++zahl;
                    }
                }
                vorher = jetztStufen;
                QTest::qWait(25);   // Kachel-Takt, damit dt echt ist
            }
            return zahl > 0 ? summeAbstand / zahl : 0.0;
        };

        const double ohne = streuung(SpectrumAveraging::None);
        const double mit  = streuung(SpectrumAveraging::LogRecursive);
        QVERIFY2(ohne >= 0.0 && mit >= 0.0,
                 "keine Zeilenstufen — das Fenster hat kein Wasserfallbild");
        qInfo().nospace() << "Zeilenabstand ohne Mittelung " << ohne
                          << ", mit " << mit;

        // Die Gegenprobe steht im selben Lauf: ohne Mittelung MUSS es
        // zappeln, sonst messe ich nicht die Mittelung, sondern ein
        // flaches Bild.
        QVERIFY2(ohne > 3.0, qPrintable(QStringLiteral(
            "ohne Mittelung zappelt es nicht (%1) — der Pruefstand "
            "misst dann nichts").arg(ohne)));
        QVERIFY2(mit < ohne * 0.6, qPrintable(QStringLiteral(
            "Mittelung beruhigt zu wenig: %1 gegen %2").arg(mit).arg(ohne)));
    }

    // Und der Traeger muss die Mittelung UEBERLEBEN: sie darf das
    // Bild beruhigen, nicht einebnen.
    void derTraegerUeberlebtDieMittelung()
    {
        SpectrumWidget sw;
        sw.setWaterfallAveraging(SpectrumAveraging::LogRecursive);
        sw.setWaterfallAverageTimeMs(400);
        richteEin(sw, 14.100e6, 200.0e3);

        sw.zeigeFremdeWasserfallzeile(kachel(2048, 1024), 14.000e6,
                                      14.200e6);
        int woErste = -1;
        const int ersteZeile = hoechsteStufe(sw.wfZeilenstufenForTest(),
                                             &woErste);
        for (int i = 0; i < 20; ++i) {
            QTest::qWait(25);
            sw.zeigeFremdeWasserfallzeile(kachel(2048, 1024), 14.000e6,
                                          14.200e6);
        }
        const auto stufen = sw.wfZeilenstufenForTest();
        int wo = -1;
        const int hoch = hoechsteStufe(stufen, &wo);
        // Kachelmitte -> 14,100 MHz -> Fenstermitte.
        const double soll = stufen.size() * 0.5;
        qInfo().nospace() << "Traeger: erste Zeile Stufe " << ersteZeile
                          << ", nach 20 gemittelten Stufe " << hoch
                          << " bei " << wo << " (Soll " << soll << ")";
        QVERIFY(std::abs(wo - soll) < stufen.size() * 0.02);

        // Der Vergleich gegen die UNGEMITTELTE erste Zeile, nicht gegen
        // eine Wunschzahl: ein stehender Traeger darf durch die
        // Mittelung nichts verlieren. (Seine absolute Hoehe haengt am
        // Mittel JE BILDPUNKT — ein Traeger in einem Bin von knapp
        // zwei wird dort verduennt, das ist der bewusste Preis vom
        // selben Tag und nicht die Mittelung hier.)
        QVERIFY2(hoch >= ersteZeile - 2, qPrintable(QStringLiteral(
            "Traeger eingeebnet: %1 nach Mittelung gegen %2 davor")
            .arg(hoch).arg(ersteZeile)));

        // Und er muss sich weiterhin vom Teppich abheben.
        double teppich = 0.0; int zahl = 0;
        for (int x = 0; x < stufen.size(); ++x) {
            if (std::abs(x - wo) > 20) { teppich += stufen[x]; ++zahl; }
        }
        teppich = zahl > 0 ? teppich / zahl : 0.0;
        qInfo().nospace() << "Teppich um ihn herum: " << teppich;
        QVERIFY2(hoch > teppich + 30.0, qPrintable(QStringLiteral(
            "Traeger hebt sich nicht ab: %1 gegen Teppich %2")
            .arg(hoch).arg(teppich)));
    }

    // Ein Bandwechsel darf das alte Band nicht mitziehen. Die
    // Kachelbreite bleibt dabei gleich, die Groessenpruefung allein
    // faengt es also NICHT.
    void einAusschnittswechselSetztDieMittelungZurueck()
    {
        SpectrumWidget sw;
        sw.setWaterfallAveraging(SpectrumAveraging::LogRecursive);
        sw.setWaterfallAverageTimeMs(400);
        richteEin(sw, 14.100e6, 200.0e3);

        for (int i = 0; i < 10; ++i) {
            sw.zeigeFremdeWasserfallzeile(kachel(2048, 1024), 14.000e6,
                                          14.200e6);
            QTest::qWait(25);
        }
        // Gleiche Breite, anderer Ausschnitt, Traeger an anderer Stelle.
        QVector<float> neu = kachel(2048, 512);
        sw.zeigeFremdeWasserfallzeile(neu, 3.600e6, 3.800e6);
        const auto& mittel = sw.fremdeWfMittelForTest();
        QCOMPARE(mittel.size(), neu.size());
        for (int i = 0; i < neu.size(); i += 101) {
            QCOMPARE(mittel[i], neu[i]);   // zurueckgesetzt, nicht gemischt
        }
    }

    // ── 5. Der Melder ist der des Betreibers ────────────────────────
    //
    // Der Kachelweg hat `DisplayWaterfallDetector` bis zum 2026-10-23
    // ignoriert und selbst entschieden (erst Maximum, dann Mittel).
    // Beim Betreiber steht Sample. „Wie bei der Anvelina" heisst nur
    // dann dasselbe, wenn beide Wege dieselbe Einstellung lesen.
    void derMelderDesBetreibersGiltAuchFuerDieKachel()
    {
        // Ein Zahnmuster: jeder zweite Bin hoch. Knapp zwei Bins
        // fallen auf einen Bildpunkt, also enthaelt fast jedes Fenster
        // einen hohen und einen tiefen Bin. Peak nimmt dann IMMER den
        // hohen, Sample den mittleren (mal hoch, mal tief), das Mittel
        // liegt dazwischen. Gemessen wird das Mittel der ganzen Zeile,
        // nicht ihr Hoechstwert: der ist bei Peak und Sample gleich,
        // sobald Sample einen hohen Bin ERWISCHT.
        auto zeilenmittel = [this](SpectrumDetector art) -> double {
            SpectrumWidget sw;
            sw.setWaterfallDetector(art);
            sw.setWaterfallAveraging(SpectrumAveraging::None);
            richteEin(sw, 14.100e6, 200.0e3);
            QVector<float> k(2048);
            for (int i = 0; i < k.size(); ++i) {
                k[i] = (i & 1) ? 112.0f : 96.0f;
            }
            sw.zeigeFremdeWasserfallzeile(k, 14.000e6, 14.200e6);
            const auto stufen = sw.wfZeilenstufenForTest();
            if (stufen.isEmpty()) { return -1.0; }
            double summe = 0.0;
            for (quint8 v : stufen) { summe += v; }
            return summe / stufen.size();
        };

        const double mitPeak   = zeilenmittel(SpectrumDetector::Peak);
        const double mitSample = zeilenmittel(SpectrumDetector::Sample);
        const double mitMittel = zeilenmittel(SpectrumDetector::Average);
        qInfo().nospace() << "Zeilenmittel — Peak " << mitPeak
                          << ", Sample " << mitSample
                          << ", Mittel " << mitMittel;
        QVERIFY(mitPeak >= 0.0 && mitSample >= 0.0 && mitMittel >= 0.0);

        // Peak nimmt die Zaehne, das Mittel glaettet sie.
        QVERIFY2(mitPeak > mitMittel + 5.0, qPrintable(QStringLiteral(
            "Peak (%1) hebt sich nicht vom Mittel (%2) ab — die "
            "Einstellung wird nicht gelesen").arg(mitPeak).arg(mitMittel)));
        // Und Sample liest EINEN Bin, nimmt also nicht durchgehend den
        // hohen: es muss unter Peak liegen.
        QVERIFY2(mitSample < mitPeak - 2.0, qPrintable(QStringLiteral(
            "Sample (%1) verhaelt sich wie Peak (%2)")
            .arg(mitSample).arg(mitPeak)));
    }

    // ── 6. Die Kachel sagt, wo die Mitte steht (2026-10-23) ─────────
    //
    // Betreiber: „ich glaube, es ist hier wieder versetzt". Die
    // Kreuzkorrelation im Protokoll gab ihm recht: 0 kHz nach dem
    // Verbinden, +7,4 kHz nach seinem Abstimmen, stabil ueber zwei
    // Berichte.
    //
    // Ursache: die beiden Stroeme standen nicht mehr konzentrisch.
    // Die Kachel traegt ihren Bereich in JEDEM Paket, der Panadapter
    // traegt gar keinen (sein Unterfeld hat nur Bin-Nummern) — sein
    // Ausschnitt kommt aus dem Zustandsschub der Maschine, und der
    // hing fest.
    void dieKachelZiehtDenPanadapterNach()
    {
        // Die gemessenen Zahlen vom Geraet, 15 m, 14:16:12:
        // Kachel 21,1304–21,361 (Mitte 21,2457),
        // Panadapter 21,1636–21,3636 (Mitte 21,2636).
        const auto a = FlexVita::aufKachelZentriert(
            21.1636e6, 21.3636e6, 21.1304e6, 21.361e6);
        QVERIFY(a.geaendert);
        // Die BREITE bleibt die des Zustands — 200 kHz, nicht die
        // 230,6 der Kachel. Genau darum geht der Ausschnitt ueberhaupt
        // ueber den Zustand und nicht ueber die Kachel.
        QVERIFY(qAbs((a.hochHz - a.tiefHz) - 200.0e3) < 1.0);
        // Und die Mitte ist jetzt die der Kachel.
        QVERIFY(qAbs((a.tiefHz + a.hochHz) * 0.5 - 21.2457e6) < 100.0);
    }

    void konzentrischBleibtUnangetastet()
    {
        // Die Zahlen vom selben Geraet um 14:15:12, als der Versatz
        // 0 war: Kachel symmetrisch um das Fenster, je 15 kHz
        // Ueberstand. Da darf nichts nachgezogen werden.
        const auto a = FlexVita::aufKachelZentriert(
            20.9548e6, 21.1548e6, 20.9394e6, 21.17e6);
        QVERIFY2(!a.geaendert, "konzentrisch — nichts nachzuziehen");
        QCOMPARE(a.tiefHz, 20.9548e6);
        QCOMPARE(a.hochHz, 21.1548e6);
    }

    void unterDerSchwelle_wirdNichtGezappelt()
    {
        // 500 Hz Unterschied: darunter bleibt alles stehen, sonst
        // zoege jedes Paket das Bild ein Stueck.
        const auto a = FlexVita::aufKachelZentriert(
            21.100e6, 21.300e6, 21.1005e6, 21.3005e6);
        QVERIFY(!a.geaendert);
        // 2 kHz: darueber wird nachgezogen.
        const auto b = FlexVita::aufKachelZentriert(
            21.100e6, 21.300e6, 21.102e6, 21.302e6);
        QVERIFY(b.geaendert);
    }

    void unbrauchbareBereiche_aendernNichts()
    {
        QVERIFY(!FlexVita::aufKachelZentriert(0.0, 0.0, 21.1e6, 21.3e6)
                     .geaendert);
        QVERIFY(!FlexVita::aufKachelZentriert(21.1e6, 21.3e6, 0.0, 0.0)
                     .geaendert);
    }
};

QTEST_MAIN(TstFlexWasserfallKachel)
#include "tst_flex_wasserfall_kachel.moc"
