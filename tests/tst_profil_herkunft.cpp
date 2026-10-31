// =================================================================
// tests/tst_profil_herkunft.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Longpath hatte am 2026-10-31 zwei Profilimporteure an zwei Orten
// mit zwei Zielen — und keiner von beiden konnte die Datei des
// anderen auch nur benennen. Wer seine Thetis-Ausfuhr in den
// Kanalzug zog, bekam „Das ist kein AetherSDR-Kanalzugprofil (Feld
// „schema" lautet „—")": richtig, und doch die falsche Auskunft, denn
// Longpath KANN diese Datei lesen, nur zwei Fenster weiter.
//
// Dieser Stand haelt den Erkenner fest, der vor beiden Lesern sitzt.
// Die wichtigste Zusage steht in `keineEndungsraterei`: erkannt wird
// am INHALT. Das ist Zeus' Regel („recognizes the Thetis file on its
// own, even if it has been renamed") und bei uns kein Luxus, sondern
// die Voraussetzung dafuer, dass ein Betreiber seine Datei nicht
// umbenennen muss, um sie loszuwerden.
//
// Und `rateNichtsZusammen`: eine fremde Datei bekommt KEINE Herkunft
// angedichtet. Was hier als erkannt gilt, hat einen Leser — sonst
// waere die Auskunft eine Zusage, die niemand einloest.
// =================================================================

#include <QtTest/QtTest>

#include "core/ProfilHerkunft.h"

using namespace Longpath;
using Longpath::ProfilHerkunft::Art;

namespace {

// Ein Aether-Kanalzugprofil, auf das Noetigste gekuerzt: das
// `schema`-Feld ist, woran es haengt (AetherPreset.h, kSchema).
QByteArray aetherProfil()
{
    return QByteArrayLiteral(
        "{\n"
        "  \"schema\": \"AetherSDR ChannelStripPreset v1\",\n"
        "  \"name\": \"Teufel ESSB\",\n"
        "  \"chain\": [\"Tube\",\"Gate\",\"Eq\",\"Comp\"]\n"
        "}\n");
}

// Thetis schreibt beide Dateiarten mit `WriteXml(..., WriteSchema)`:
// Wurzel `<Data>`, eingebettetes XSD, je Profil ein `<TXProfile>`.
// Der Unterschied ist, wie viele TABELLEN darin stehen.
QByteArray thetisAusfuhr()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" standalone=\"yes\"?>\n"
        "<Data>\n"
        "  <xs:schema id=\"Data\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
        "    <xs:element name=\"Data\"/>\n"
        "  </xs:schema>\n"
        "  <TXProfile>\n"
        "    <Name>ESSB 3k1</Name>\n"
        "    <TXEQPreamp>-10</TXEQPreamp>\n"
        "  </TXProfile>\n"
        "</Data>\n");
}

QByteArray thetisDatenbank()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" standalone=\"yes\"?>\n"
        "<Data>\n"
        "  <xs:schema id=\"Data\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
        "    <xs:element name=\"Data\"/>\n"
        "  </xs:schema>\n"
        "  <TXProfile>\n"
        "    <Name>ESSB 3k1</Name>\n"
        "  </TXProfile>\n"
        "  <BandStack>\n"
        "    <BandName>40M</BandName>\n"
        "  </BandStack>\n"
        "  <Options>\n"
        "    <Key>SampleRate</Key>\n"
        "  </Options>\n"
        "</Data>\n");
}

} // namespace

class TstProfilHerkunft : public QObject
{
    Q_OBJECT

private slots:

    void aetherKanalzugWirdErkannt()
    {
        const auto b = ProfilHerkunft::erkenne(aetherProfil());
        QCOMPARE(b.art, Art::AetherKanalzug);
        QVERIFY(!b.was.isEmpty());
        QVERIFY2(b.wohin.contains(QStringLiteral("Kanalzug")),
                 "der Befund muss sagen, WOHIN die Datei gehoert");
        QVERIFY(b.warumNicht.isEmpty());
    }

    void thetisAusfuhrUndDatenbankSindZweiArten()
    {
        const auto a = ProfilHerkunft::erkenne(thetisAusfuhr());
        QCOMPARE(a.art, Art::ThetisProfilausfuhr);
        QVERIFY(a.wohin.contains(QStringLiteral("TX-Profile")));

        const auto d = ProfilHerkunft::erkenne(thetisDatenbank());
        QCOMPARE(d.art, Art::ThetisDatenbank);
        QVERIFY2(d.was != a.was,
                 "eine ganze Datenbank ist etwas anderes als eine "
                 "Einzelausfuhr, und der Betreiber soll das lesen");
    }

    // ── Die Zusage, auf die es ankommt ───────────────────────────────
    //
    // Erkannt wird am Inhalt. Eine Datei, die „voice.txt" heiszt, ist
    // dieselbe Datei — der Erkenner sieht die Endung gar nicht, er
    // bekommt nur Bytes.
    void keineEndungsraterei()
    {
        QCOMPARE(ProfilHerkunft::erkenne(aetherProfil()).art,
                 Art::AetherKanalzug);
        QCOMPARE(ProfilHerkunft::erkenne(thetisAusfuhr()).art,
                 Art::ThetisProfilausfuhr);
        // Und der Dateidialog sperrt nichts aus, was der Erkenner
        // lesen koennte.
        QVERIFY2(ProfilHerkunft::dateiFilter().contains(QStringLiteral("(*)")),
                 "der Filter muss „alle Dateien\" anbieten, sonst ist "
                 "die Erkennung am Inhalt umsonst");
    }

    // ── Und was sie NICHT verspricht ─────────────────────────────────
    void rateNichtsZusammen()
    {
        const QByteArray fremd = QByteArrayLiteral(
            "{ \"schema\": \"SomeOtherRadio Preset v3\", \"gain\": 4 }");
        const auto b = ProfilHerkunft::erkenne(fremd);
        QCOMPARE(b.art, Art::Unbekannt);
        QVERIFY2(b.warumNicht.contains(QStringLiteral("SomeOtherRadio Preset v3")),
                 "die Absage muss das Feld nennen, an dem es haengt — "
                 "sonst kann der Betreiber nicht nachsehen");

        // Ein XML, das kein Thetis ist.
        const auto x = ProfilHerkunft::erkenne(
            QByteArrayLiteral("<?xml version=\"1.0\"?><Rig><Name>x</Name></Rig>"));
        QCOMPARE(x.art, Art::Unbekannt);
        QVERIFY(!x.warumNicht.isEmpty());

        // Und etwas, das gar nichts ist.
        const auto n = ProfilHerkunft::erkenne(
            QByteArrayLiteral("\x89PNG\r\n\x1a\n"));
        QCOMPARE(n.art, Art::Unbekannt);
    }

    void leereDateiIstKeinProfil()
    {
        const auto b = ProfilHerkunft::erkenne(QByteArray());
        QCOMPARE(b.art, Art::Unbekannt);
        QVERIFY(!b.warumNicht.isEmpty());
    }

    // ── Ein `<Data>` ohne TXProfile ist keine Profildatei ────────────
    //
    // Sonst nimmt der Erkenner jede .NET-DataSet-Ausfuhr an und
    // schickt sie an einen Leser, der nichts darin findet.
    void dataOhneProfileZaehltNicht()
    {
        const auto b = ProfilHerkunft::erkenne(QByteArrayLiteral(
            "<?xml version=\"1.0\"?><Data><Options><Key>x</Key></Options></Data>"));
        QCOMPARE(b.art, Art::Unbekannt);
    }
};

QTEST_MAIN(TstProfilHerkunft)
#include "tst_profil_herkunft.moc"
