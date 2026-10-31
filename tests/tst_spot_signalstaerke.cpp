// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_spot_signalstaerke.cpp  (Longpath)
// =================================================================
//
// Wie stark kommt er an -- und kommt die Zahl bis zum Panadapter?
//
// ── Warum es diese Pruefung gibt ────────────────────────────────────
//
// Betreiber am 2026-10-31, am Geraet: „ich will sehen, wie gut sein
// signal bei mir ankommt. so kann ich entscheiden, ob ich es probiere
// oder ob es schierig wird."
//
// Der eigene FT8-Dekoder MISST den Stoerabstand -- in der Fundliste
// steht er in der zweiten Spalte. Auf dem Weg zum Panadapter ging er
// verloren: `kvsFromSpot` traegt Rufzeichen, Frequenz, Quelle und
// Bemerkung weiter und `snr` nicht, und `SpotData` hatte ueberhaupt
// kein Feld dafuer. Ueber dem Wasserfall stand darum ein Rufzeichen
// ohne jede Auskunft darueber, ob der Versuch sich lohnt -- genau die
// Entscheidung, um die es ihm geht.
//
// Drei Stellen, drei Faelle: das Feld traegt die Zahl, der eigene
// Dekoder gibt sie mit, und die Beschriftung zeigt sie -- aber nur,
// wo sie wirklich gemessen wurde.
//
// Modification history (Longpath):
//   2026-10-31 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QFile>

#include "models/SpotModel.h"

using namespace Longpath;

namespace {

QString quelle(const QString& relativ)
{
    const QString wurzel = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
    if (wurzel.isEmpty()) { return {}; }
    return wurzel + QLatin1Char('/') + relativ;
}

QString liesGanz(const QString& relativ)
{
    QFile f(quelle(relativ));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }
    return QString::fromUtf8(f.readAll());
}

/// Der Rumpf einer Funktion, von ihrem Namen bis zur schliessenden
/// Klammer auf Spaltenanfang. Ohne das wuerde ein Fund irgendwo
/// sonst in der Datei als Treffer durchgehen.
QString rumpfVon(const QString& text, const QString& kopf)
{
    const int a = text.indexOf(kopf);
    if (a < 0) { return {}; }
    const int b = text.indexOf(QStringLiteral("\n}\n"), a);
    return b < 0 ? text.mid(a) : text.mid(a, b - a);
}

}  // namespace

class TstSpotSignalstaerke : public QObject
{
    Q_OBJECT

private slots:
    // 1. Das Feld traegt die gemessene Zahl -- und ohne Angabe bleibt
    //    es auf dem Sonderwert stehen, nicht auf null. Null ist bei
    //    FT8 ein ganz gewoehnlicher Pegel; waere sie der Vorgabewert,
    //    bekaeme jeder Cluster-Spot eine Zahl, die niemand gemessen
    //    hat.
    void dieGemesseneZahlStehtImFund()
    {
        SpotModel m;

        QMap<QString, QString> mitZahl;
        mitZahl[QStringLiteral("callsign")] = QStringLiteral("R5AV");
        mitZahl[QStringLiteral("rx_freq")]  = QStringLiteral("7.0740");
        mitZahl[QStringLiteral("snr")]      = QStringLiteral("-13");
        m.applySpotStatus(1, mitZahl);

        QMap<QString, QString> ohneZahl;
        ohneZahl[QStringLiteral("callsign")] = QStringLiteral("DL1ABC");
        ohneZahl[QStringLiteral("rx_freq")]  = QStringLiteral("7.0750");
        m.applySpotStatus(2, ohneZahl);

        const auto& funde = m.spots();
        QVERIFY(funde.contains(1));
        QVERIFY(funde.contains(2));
        QCOMPARE(funde.value(1).snrDb, -13);
        QCOMPARE(funde.value(2).snrDb, kSpotKeinSnr);
    }

    // Auch die Null muss durchkommen: sie ist eine Messung, keine
    // fehlende Angabe. Mit dem alten Vorgabewert 0 waeren beide Faelle
    // nicht zu unterscheiden gewesen.
    void nullDbIstEineMessungUndKeineLuecke()
    {
        SpotModel m;
        QMap<QString, QString> kvs;
        kvs[QStringLiteral("callsign")] = QStringLiteral("OH1RF");
        kvs[QStringLiteral("rx_freq")]  = QStringLiteral("7.0740");
        kvs[QStringLiteral("snr")]      = QStringLiteral("0");
        m.applySpotStatus(7, kvs);

        QCOMPARE(m.spots().value(7).snrDb, 0);
        QVERIFY(m.spots().value(7).snrDb != kSpotKeinSnr);
    }

    // 2. Der eigene Dekoder gibt die Zahl mit. Ohne diese Zeile war
    //    sie ab `meldeEigenenFund` weg -- und genau das war der
    //    Befund.
    void dereigeneDekoderGibtDieZahlWeiter()
    {
        const QString text = liesGanz(
            QStringLiteral("src/models/RadioModel.cpp"));
        if (text.isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt");
        }
        const QString rumpf = rumpfVon(
            text, QStringLiteral("void RadioModel::meldeEigenenFund("));
        QVERIFY2(!rumpf.isEmpty(), "meldeEigenenFund nicht gefunden");
        QVERIFY2(rumpf.contains(QStringLiteral("QStringLiteral(\"snr\")")),
                 "der eigene Fund geht ohne den gemessenen Stoerabstand "
                 "zum Panadapter");
        QVERIFY2(rumpf.contains(QStringLiteral("spot.snr")),
                 "die Zahl wird gesetzt, aber nicht aus dem Fund gelesen");
    }

    // 3. Und die Beschriftung zeigt sie -- aber nur, wo gemessen
    //    wurde. Ein Cluster-Spot bekommt keine erfundene Null.
    void dieBeschriftungZeigtSieNurWennGemessen()
    {
        const QString text = liesGanz(
            QStringLiteral("src/gui/SpectrumWidget.cpp"));
        if (text.isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt");
        }
        QVERIFY2(text.contains(QStringLiteral("const QString label = spot.snrDb")),
                 "die Beschriftung des Panadapters kennt den "
                 "Stoerabstand nicht");
        QVERIFY2(text.contains(QStringLiteral("SpotMarker::kKeineAngabe")),
                 "ohne die Abfrage auf \"keine Angabe\" bekaeme jeder "
                 "Cluster-Spot eine Zahl, die niemand gemessen hat");
        // Aendert sich die Zahl, muss sich das Bild erneuern -- sonst
        // bliebe die erste Beschriftung stehen.
        QVERIFY2(text.contains(QStringLiteral("a.snrDb != b.snrDb")),
                 "der Vergleich uebersieht den Stoerabstand, das Bild "
                 "wird nicht neu gemalt");
    }
};

QTEST_MAIN(TstSpotSignalstaerke)
#include "tst_spot_signalstaerke.moc"
