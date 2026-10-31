// =================================================================
// tests/tst_zeus_kanalzug.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Hier steht etwas, was bei den anderen Profil-Pruefstaenden fehlt:
// eine ECHTE Antwort. `tests/daten/zeus-tx-audio-profiles.json` ist
// am 2026-10-31 von Martins laufender Zeus-Maschine geholt worden
// (`GET http://127.0.0.1:55111/api/tx-audio-profiles`, HTTP 200,
// 22 466 Bytes, zwei der vier Profile behalten).
//
// Das ist kein Beiwerk. Der Thetis-Importeur wurde am 2026-10-13 ohne
// eine einzige echte Datei gebaut — das Format kam aus dem Quelltext
// — und beim Zeus-Leser war es bis heute Mittag genauso. Was eine
// echte Antwort sofort umgeworfen hat:
//
//   Zeus speichert JEDEN Wert als ZEICHENKETTE, auch Zahlen
//   ("param:threshold-db": "-45"). Wer `toDouble()` auf dem
//   JSON-Wert ruft, bekommt ueberall null und eine Kette, die
//   gleichmaessig falsch eingestellt ist, ohne dass etwas warnt.
//
//   Die Schluessel der Audio-Suite sind NICHT unlesbar. Genau das
//   stand bis heute Mittag in unserem eigenen Kopfkommentar, und es
//   war falsch.
//
// `dieWerteKommenAlsZeichenketten` haelt das erste fest.
// =================================================================

#include <QtTest/QtTest>

#include "core/strip/ZeusKanalzug.h"
#include "core/strip/StripChain.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Longpath;

namespace {

QJsonObject profilMitId(const QString& id, bool* gefunden)
{
    *gefunden = false;
    const QString wurzel =
        QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR"));
    if (wurzel.isEmpty()) { return {}; }

    QFile f(wurzel + QStringLiteral("/tests/daten/zeus-tx-audio-profiles.json"));
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    const QJsonArray ps = d.object().value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue& v : ps) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() == id) {
            *gefunden = true;
            return o;
        }
    }
    return {};
}

} // namespace

class TstZeusKanalzug : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR")).isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt — ueber ctest fahren");
        }
    }

    // ── Die Falle, die eine echte Antwort aufgedeckt hat ─────────────
    void dieWerteKommenAlsZeichenketten()
    {
        bool da = false;
        const QJsonObject p = profilMitId(QStringLiteral("essb-broadcast"), &da);
        QVERIFY2(da, "die Vorlage fehlt");

        const QJsonObject tor = p.value(QStringLiteral("nativePluginStates"))
                                    .toObject()
                                    .value(QStringLiteral("noise-gate"))
                                    .toObject();
        const QJsonValue schwelle =
            tor.value(QStringLiteral("param:threshold-db"));
        QVERIFY2(schwelle.isString(),
                 "Zeus schickt Zahlen als Zeichenketten — faellt das "
                 "weg, liest der Importeur ueberall null");
        QVERIFY2(qFuzzyIsNull(schwelle.toDouble()),
                 "und toDouble() auf dem JSON-Wert gibt genau deshalb "
                 "null; der Leser muss ueber toString() gehen");
    }

    // ── Die Stufen kommen wirklich an ────────────────────────────────
    void dieStufenWerdenGesetzt()
    {
        bool da = false;
        const QJsonObject p = profilMitId(QStringLiteral("essb-broadcast"), &da);
        QVERIFY(da);

        StripChain k;
        k.prepare(48000.0);
        const auto e = ZeusKanalzug::anwenden(p, k);

        QVERIFY2(e.ok, qPrintable(e.fehler));
        QCOMPARE(e.name, QStringLiteral("ESSB Broadcast"));
        QVERIFY2(e.stufen >= 8,
                 qPrintable(QStringLiteral("nur %1 Stufen gesetzt")
                                .arg(e.stufen)));

        // Die gemessenen Werte aus der echten Antwort, Zahl fuer Zahl.
        QCOMPARE(k.gate().thresholdDb(), -45.0f);
        QCOMPARE(k.gate().attackMs(), 2.0f);
        QCOMPARE(k.gate().holdMs(), 100.0f);
        QCOMPARE(k.gate().releaseMs(), 250.0f);
        QCOMPARE(k.gate().floorDb(), -15.0f);

        QCOMPARE(k.deEss().frequencyHz(), 4000.0f);
        QCOMPARE(k.deEss().thresholdDb(), -30.0f);

        QCOMPARE(k.comp().thresholdDb(), -18.0f);
        QCOMPARE(k.comp().ratio(), 3.0f);
        QCOMPARE(k.comp().kneeDb(), 6.0f);

        QCOMPARE(k.limiter().ceilingDb(), -1.0f);
    }

    // ── Die Schalter folgen `requestedEnabled` ───────────────────────
    //
    // „rag-chew-natural" ist die Gegenprobe zu „essb-broadcast": dort
    // laeuft der Kompressor und der De-Esser NICHT, hier umgekehrt.
    // Zwei Profile, verschiedene Schalter — ein Importeur, der alles
    // einschaltet, faellt hier durch.
    void dieSchalterFolgenDerVorlage()
    {
        bool da = false;
        const QJsonObject breit =
            profilMitId(QStringLiteral("essb-broadcast"), &da);
        QVERIFY(da);
        const QJsonObject plausch =
            profilMitId(QStringLiteral("rag-chew-natural"), &da);
        QVERIFY(da);

        StripChain a;
        a.prepare(48000.0);
        ZeusKanalzug::anwenden(breit, a);
        QVERIFY2(a.stageEnabled(StripChain::Stage::DeEss),
                 "essb-broadcast hat den De-Esser AN");
        QVERIFY2(!a.stageEnabled(StripChain::Stage::Comp),
                 "und den Kompressor AUS");

        StripChain b;
        b.prepare(48000.0);
        ZeusKanalzug::anwenden(plausch, b);
        QVERIFY2(!b.stageEnabled(StripChain::Stage::DeEss),
                 "rag-chew-natural hat den De-Esser AUS");
        QVERIFY2(b.stageEnabled(StripChain::Stage::Comp),
                 "und den Kompressor AN");
    }

    // ── Der Entzerrer: zehn Baender, ab eins gezaehlt ────────────────
    void derEntzerrerZaehltUm()
    {
        bool da = false;
        const QJsonObject p = profilMitId(QStringLiteral("essb-broadcast"), &da);
        QVERIFY(da);

        StripChain k;
        k.prepare(48000.0);
        ZeusKanalzug::anwenden(p, k);

        // Zeus' band-1 ist unser Band 0.
        const auto b0 = k.eq().band(0);
        QCOMPARE(b0.freqHz, 100.0f);
        QCOMPARE(b0.gainDb, -2.0f);
        // Art 1 = tiefe Flanke, auf beiden Seiten dieselbe Zahl.
        QCOMPARE(b0.type, ClientEq::FilterType::LowShelf);

        const auto b9 = k.eq().band(9);
        QCOMPARE(b9.freqHz, 3600.0f);
        QCOMPARE(b9.type, ClientEq::FilterType::HighShelf);
    }

    // ── Was Zeus hat und wir nicht, steht im Bericht ─────────────────
    void fehlendeStufenWerdenGemeldetMitWerten()
    {
        bool da = false;
        const QJsonObject p = profilMitId(QStringLiteral("essb-broadcast"), &da);
        QVERIFY(da);

        StripChain k;
        k.prepare(48000.0);
        const auto e = ZeusKanalzug::anwenden(p, k);

        const QString alles = e.hinweise.join(QLatin1Char('\n'));
        QVERIFY2(alles.contains(QStringLiteral("Mehrband")),
                 qPrintable(QStringLiteral("kein Hinweis zum "
                                           "Mehrband-Kompressor:\n%1")
                                .arg(alles)));
        QVERIFY2(alles.contains(QStringLiteral("180")),
                 "der Hinweis muss die WERTE nennen, sonst kann man "
                 "nichts von Hand nachstellen");
        QVERIFY(alles.contains(QStringLiteral("Anschlag")));
        QVERIFY(alles.contains(QStringLiteral("Sendeautomatik")));
    }

    // ── Was Zeus nicht hat, laeuft nicht mit ─────────────────────────
    void unsereEigenenStufenBleibenAus()
    {
        bool da = false;
        const QJsonObject p = profilMitId(QStringLiteral("essb-broadcast"), &da);
        QVERIFY(da);

        StripChain k;
        k.prepare(48000.0);
        k.setStageEnabled(StripChain::Stage::Clip, true);   // vorher an
        ZeusKanalzug::anwenden(p, k);

        QVERIFY2(!k.stageEnabled(StripChain::Stage::Clip),
                 "den Clipper kennt Zeus nicht — ein eingelesenes "
                 "Profil darf nichts mitlaufen lassen, das in der "
                 "Vorlage nie zu hoeren war");
    }

    // ── Der Hauptschalter bleibt unberuehrt ──────────────────────────
    void derHauptschalterWirdNichtAngefasst()
    {
        bool da = false;
        const QJsonObject p = profilMitId(QStringLiteral("essb-broadcast"), &da);
        QVERIFY(da);

        StripChain aus;
        aus.prepare(48000.0);
        QVERIFY(!aus.isEnabled());
        ZeusKanalzug::anwenden(p, aus);
        QVERIFY2(!aus.isEnabled(),
                 "ein Profil entscheidet, WIE die Kette klingt, nicht "
                 "OB sie im Sendeweg steht");
    }

    void einProfilOhneSuiteWirdAbgelehnt()
    {
        QJsonObject leer;
        leer.insert(QStringLiteral("name"), QStringLiteral("nichts"));
        QVERIFY(!ZeusKanalzug::traegtKanalzug(leer));

        StripChain k;
        k.prepare(48000.0);
        const auto e = ZeusKanalzug::anwenden(leer, k);
        QVERIFY(!e.ok);
        QVERIFY(!e.fehler.isEmpty());
    }
};

QTEST_MAIN(TstZeusKanalzug)
#include "tst_zeus_kanalzug.moc"
