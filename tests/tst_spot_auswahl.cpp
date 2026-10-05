// Welche Spots gehören ins Bild — und welche auf keinen Fall.
//
// Hintergrund (2026-10-05): Longpath sammelt Cluster, RBN, POTA und
// SpotCollector in EINEM SpotModel. Die Filter des Spot-Hub-Dialogs
// (Band, Land, Quelle) sitzen im DIALOG, nicht im Modell — das Modell
// enthält alles. RBN allein liefert in einer guten Stunde Hunderte
// Meldungen.
//
// Wer die ungefiltert über die TCI-Steuerleitung an ein Telefon schiebt,
// blockiert damit jede Bedienung dahinter: dieselbe Leitung trägt die
// Frequenz, die Betriebsart und den Abbruch eines Sendewunsches. Das ist
// kein Schönheitsfehler, sondern eine Fernbedienung, die hängt, weil
// jemand ein Pile-up gespottet hat.
//
// Gefiltert wird darum nach dem einzigen Kriterium, das auf einem Telefon
// zählt: passt der Spot ins Bild? Was außerhalb liegt, kann die Seite
// ohnehin nicht zeichnen.
//
// Die drei Zusagen, die hier festgehalten sind:
//   * Außerhalb des Ausschnitts kommt NICHTS mit — sonst wäre der
//     Flutschutz keiner.
//   * Abgelaufene Spots fallen weg. Der Kehrbesen im SpotModel läuft nur
//     alle 30 s; ohne diese Prüfung stünde ein toter Spot auf dem Telefon
//     länger als am Pult — und ein Spot, den es nicht mehr gibt, kostet
//     einen Anruf ins Leere.
//   * Muss gekürzt werden, bleiben die NAHE DER MITTE. Dort hat der
//     Bediener gerade hingedreht; die Ränder sind das, was man opfert.

#include "core/SpotAuswahl.h"
#include "models/SpotModel.h"

#include <QtTest>

using namespace Longpath;
using namespace Longpath::SpotAuswahl;

namespace {

constexpr qint64 kJetzt = 1'800'000'000'000LL;   // feste Uhr, kein QDateTime

SpotData spot(const QString& ruf, double mhz, const QString& quelle = QStringLiteral("CLUSTER"),
              int alterSek = 0, int lebenSek = 1800)
{
    SpotData s;
    s.callsign        = ruf;
    s.rxFreqMhz       = mhz;
    s.mode            = QStringLiteral("CW");
    s.source          = quelle;
    s.lifetimeSeconds = lebenSek;
    s.addedMs         = kJetzt - qint64(alterSek) * 1000;
    s.lastSeenMs      = s.addedMs;
    return s;
}

QMap<int, SpotData> karte(const QVector<SpotData>& v)
{
    QMap<int, SpotData> m;
    for (int i = 0; i < v.size(); ++i) { m.insert(i, v.at(i)); }
    return m;
}

QStringList rufe(const QVector<Zeile>& z)
{
    QStringList l;
    for (const Zeile& x : z) { l << x.ruf; }
    return l;
}

}  // namespace

class TstSpotAuswahl : public QObject
{
    Q_OBJECT

private slots:
    void nurWasInsBildPasst();
    void dieRaenderGehoerenDazu();
    void abgelaufeneFallenWeg();
    void ohneLebenszeitBleibtErStehen();
    void gekuerztWirdVonAussenNachInnen();
    void ausgabeStehtNachFrequenz();
    void unbekannteSpanneIstKeineLeereAntwort();
    void spotOhneFrequenzOderRufzeichenZaehltNicht();
    void sendefrequenzSpringtEinWennEmpfangFehlt();
    void dasAlterWirdMitgegeben();
};

void TstSpotAuswahl::nurWasInsBildPasst()
{
    // Bild: 14,100 MHz ± 24 kHz.
    const auto m = karte({
        spot(QStringLiteral("DRIN1"), 14.090),
        spot(QStringLiteral("DRIN2"), 14.110),
        spot(QStringLiteral("WEIT1"),  7.030),    // anderes Band
        spot(QStringLiteral("WEIT2"), 14.200),    // gleiches Band, weit weg
    });
    const QVector<Zeile> z = sichtbare(m, 14'100'000, 48000, kJetzt);
    QCOMPARE(rufe(z), QStringList({QStringLiteral("DRIN1"), QStringLiteral("DRIN2")}));
}

void TstSpotAuswahl::dieRaenderGehoerenDazu()
{
    // Genau auf der Kante: beide gehören hinein. Ein Spot, der am Bildrand
    // verschwindet, sieht aus wie ein Spot, den es nicht gibt.
    const auto m = karte({
        spot(QStringLiteral("LINKS"),  14.076),   // -24 kHz
        spot(QStringLiteral("RECHTS"), 14.124),   // +24 kHz
        spot(QStringLiteral("KNAPP"),  14.1241),  // ein Haar darüber
    });
    QCOMPARE(rufe(sichtbare(m, 14'100'000, 48000, kJetzt)),
             QStringList({QStringLiteral("LINKS"), QStringLiteral("RECHTS")}));
}

void TstSpotAuswahl::abgelaufeneFallenWeg()
{
    // Lebenszeit 600 s, zuletzt vor 900 s gesehen -> tot. Der Kehrbesen im
    // SpotModel hätte ihn vielleicht noch nicht erwischt.
    const auto m = karte({
        spot(QStringLiteral("FRISCH"), 14.100, QStringLiteral("RBN"), 60,  600),
        spot(QStringLiteral("TOT"),    14.101, QStringLiteral("RBN"), 900, 600),
    });
    QCOMPARE(rufe(sichtbare(m, 14'100'000, 48000, kJetzt)),
             QStringList({QStringLiteral("FRISCH")}));
}

void TstSpotAuswahl::ohneLebenszeitBleibtErStehen()
{
    // lifetimeSeconds == 0 heißt "keine Angabe", nicht "sofort tot". Eine
    // fehlende Zahl als Ablauf zu lesen, löschte jeden Spot einer Quelle,
    // die das Feld nicht füllt.
    auto s = spot(QStringLiteral("OHNE"), 14.100, QStringLiteral("POTA"), 99999, 0);
    QCOMPARE(sichtbare(karte({s}), 14'100'000, 48000, kJetzt).size(), 1);
}

void TstSpotAuswahl::gekuerztWirdVonAussenNachInnen()
{
    // Zehn Spots, Platz für drei. Bleiben müssen die drei NÄCHSTEN zur
    // Mitte — dort ist gerade abgestimmt.
    QVector<SpotData> v;
    for (int i = 0; i < 10; ++i) {
        // 14.100 ± i kHz, abwechselnd nach links und rechts
        const double versatz = ((i % 2) ? 1.0 : -1.0) * (i + 1) * 0.001;
        v << spot(QStringLiteral("S%1").arg(i), 14.100 + versatz);
    }
    const QVector<Zeile> z = sichtbare(karte(v), 14'100'000, 48000, kJetzt, 3);
    QCOMPARE(z.size(), 3);
    // S0 (-1 kHz), S1 (+2 kHz), S2 (-3 kHz) sind die drei nächsten.
    QCOMPARE(rufe(z), QStringList({QStringLiteral("S2"), QStringLiteral("S0"),
                                   QStringLiteral("S1")}));

    // Gegenprobe: ohne Kürzung kommen alle zehn, und zwar nach Frequenz.
    QCOMPARE(sichtbare(karte(v), 14'100'000, 48000, kJetzt, 40).size(), 10);
    QCOMPARE(sichtbare(karte(v), 14'100'000, 48000, kJetzt, 0).size(), 0);
}

void TstSpotAuswahl::ausgabeStehtNachFrequenz()
{
    // Die Reihenfolge im SpotModel ist die der Indizes, also die des
    // Eintreffens. Ausgegeben wird nach Frequenz, damit die Liste dieselbe
    // Reihenfolge hat wie das Bild.
    const auto m = karte({
        spot(QStringLiteral("DRITT"), 14.110),
        spot(QStringLiteral("ERST"),  14.085),
        spot(QStringLiteral("ZWEIT"), 14.100),
    });
    QCOMPARE(rufe(sichtbare(m, 14'100'000, 48000, kJetzt)),
             QStringList({QStringLiteral("ERST"), QStringLiteral("ZWEIT"),
                          QStringLiteral("DRITT")}));
}

void TstSpotAuswahl::unbekannteSpanneIstKeineLeereAntwort()
{
    // Hat der Client noch kein Spektrum abonniert, kennt der Server die
    // Spanne nicht. Dann die Vorgabe nehmen — eine leere Liste sähe aus wie
    // "keine Spots da" und wäre die schlechtere Auskunft.
    const auto m = karte({spot(QStringLiteral("DA"), 14.100)});
    QCOMPARE(sichtbare(m, 14'100'000, 0,  kJetzt).size(), 1);
    QCOMPARE(sichtbare(m, 14'100'000, -1, kJetzt).size(), 1);
}

void TstSpotAuswahl::spotOhneFrequenzOderRufzeichenZaehltNicht()
{
    QVector<SpotData> v;
    SpotData ohneRuf = spot(QStringLiteral("  "), 14.100);
    SpotData ohneHz  = spot(QStringLiteral("NIX"), 0.0);
    ohneHz.txFreqMhz = 0.0;
    v << ohneRuf << ohneHz;
    QCOMPARE(sichtbare(karte(v), 14'100'000, 48000, kJetzt).size(), 0);
}

void TstSpotAuswahl::sendefrequenzSpringtEinWennEmpfangFehlt()
{
    // Manche Quellen füllen nur die Sendefrequenz. Bei Split ist das nicht
    // dasselbe — aber ein Spot ohne jede Frequenz ist gar keiner.
    SpotData s = spot(QStringLiteral("SPLIT"), 0.0);
    s.txFreqMhz = 14.100;
    const QVector<Zeile> z = sichtbare(karte({s}), 14'100'000, 48000, kJetzt);
    QCOMPARE(z.size(), 1);
    QCOMPARE(z.first().hz, 14'100'000LL);
}

void TstSpotAuswahl::dasAlterWirdMitgegeben()
{
    // Die Seite lässt alte Spots verblassen. Dafür braucht sie das Alter,
    // nicht den Zeitstempel: eine Uhr auf dem Telefon, die zwei Minuten
    // falsch geht, verschöbe sonst jedes Alter um zwei Minuten.
    const auto m = karte({spot(QStringLiteral("ALT"), 14.100,
                               QStringLiteral("RBN"), 300)});
    const QVector<Zeile> z = sichtbare(m, 14'100'000, 48000, kJetzt);
    QCOMPARE(z.size(), 1);
    QCOMPARE(z.first().alterSek, 300);
    QCOMPARE(z.first().quelle, QStringLiteral("RBN"));
    QCOMPARE(z.first().mode, QStringLiteral("CW"));
}

QTEST_MAIN(TstSpotAuswahl)
#include "tst_spot_auswahl.moc"
