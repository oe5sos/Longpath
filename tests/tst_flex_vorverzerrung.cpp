// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_vorverzerrung.cpp  (Longpath)
// =================================================================
//
// Betreiber 2026-11-01, zu einem Video, das sagt, die Flex koenne
// kein PureSignal: „baue einmal ohne endstufe".
//
// Die Flex rechnet ihre Vorverzerrung selbst. Was hier geprueft wird,
// ist darum nicht eine Korrektur, sondern der Umgang mit dem, was die
// Maschine MELDET — und die beiden Fallen darin.
//
// Alle Zeilen in diesem Prueflauf sind am 2026-11-01 an der FLEX-8400
// des Betreibers mitgeschrieben (Firmware SmartSDR-MB 4.2.20.41343),
// nicht erfunden. Die abgewiesene Schreibweise `apd equalizer_reset`
// (Code 0x50001000) ebenso.
// =================================================================

#include <QtTest/QtTest>

#include "core/FlexApdState.h"
#include "core/FlexRadioConnection.h"

using namespace Longpath;

namespace {

/// Zerlegt eine echte Statuszeile so, wie `parseLine` es tut: die
/// Objektgrenze liegt vor dem ersten '='. So prueft der Prueflauf
/// denselben Weg, den die Maschine nimmt.
QPair<QString, QMap<QString, QString>> zerlege(const QString& rumpf)
{
    const int firstEq = rumpf.indexOf(QLatin1Char('='));
    const int kvStart = rumpf.lastIndexOf(QLatin1Char(' '), firstEq);
    const QString object = rumpf.left(kvStart).trimmed();
    QMap<QString, QString> kvs;
    const QStringList paare =
        rumpf.mid(kvStart + 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& paar : paare) {
        const int gleich = paar.indexOf(QLatin1Char('='));
        if (gleich > 0) {
            kvs.insert(paar.left(gleich), paar.mid(gleich + 1));
        }
    }
    return { object, kvs };
}

void fuettere(FlexApdState& zustand, const QString& rumpf)
{
    const auto [object, kvs] = zerlege(rumpf);
    zustand.apply(object, kvs);
}

}  // namespace

class TstFlexVorverzerrung : public QObject
{
    Q_OBJECT

private slots:
    // ── Die Befehle ──────────────────────────────────────────────────

    // Die Wortlaute sind gemessen, nicht abgeleitet.
    void dieBefehleTragenDieNamenDerMaschine()
    {
        QCOMPARE(FlexCommandWire::apdEnableCommand(true),
                 QStringLiteral("apd enable=1"));
        QCOMPARE(FlexCommandWire::apdEnableCommand(false),
                 QStringLiteral("apd enable=0"));
        QCOMPARE(FlexCommandWire::apdSamplerCommand(QStringLiteral("ANT1"),
                                                    QStringLiteral("XVTA")),
                 QStringLiteral("apd sampler tx_ant=ANT1 sample_port=XVTA"));

        // `apd equalizer_reset` weist die Maschine ab (0x50001000); der
        // Befehl heisst `apd reset`.
        QCOMPARE(FlexCommandWire::apdResetCommand(), QStringLiteral("apd reset"));
    }

    // Ein halber Befehl ist schlimmer als keiner: die Maschine
    // quittiert ihn mit Code 0 und tut nichts.
    void ohneAngabeKeinBefehl()
    {
        QVERIFY(FlexCommandWire::apdSamplerCommand(QString(),
                                                   QStringLiteral("XVTA")).isEmpty());
        QVERIFY(FlexCommandWire::apdSamplerCommand(QStringLiteral("ANT1"),
                                                   QString()).isEmpty());
        QVERIFY(FlexCommandWire::apdSamplerCommand(QStringLiteral("  "),
                                                   QStringLiteral("  ")).isEmpty());
    }

    // ── Der Zustand ──────────────────────────────────────────────────

    void ohneMeldungIstNichtsBekannt()
    {
        FlexApdState zustand;
        QVERIFY(!zustand.bekannt());
        QVERIFY(!zustand.eingeschaltet());
        QVERIFY(!zustand.einstellbar());
        QVERIFY(zustand.sendeantennen().isEmpty());

        // Eine fremde Zeile aendert daran nichts.
        fuettere(zustand, QStringLiteral("transmit freq=7.133162 rfpower=0"));
        QVERIFY(!zustand.bekannt());
    }

    void derSchalterWirdGelesen()
    {
        FlexApdState zustand;
        fuettere(zustand, QStringLiteral("apd enable=0 configurable=1"));
        QVERIFY(zustand.bekannt());
        QVERIFY(!zustand.eingeschaltet());
        QVERIFY(zustand.einstellbar());

        fuettere(zustand, QStringLiteral("apd enable=1 configurable=1"));
        QVERIFY(zustand.eingeschaltet());
    }

    void dieMesspunkteKommenJeSendeantenne()
    {
        FlexApdState zustand;
        fuettere(zustand, QStringLiteral(
            "apd sampler tx_ant=ANT1 valid_samplers=XVTA,RX_A selected_sampler=INVALID"));
        fuettere(zustand, QStringLiteral(
            "apd sampler tx_ant=ANT2 valid_samplers=XVTA,RX_A selected_sampler=INVALID"));
        fuettere(zustand, QStringLiteral(
            "apd sampler tx_ant=XVTA valid_samplers=RX_A selected_sampler=INVALID"));

        QCOMPARE(zustand.sendeantennen().size(), 3);
        QCOMPARE(zustand.moeglicheMesspunkte(QStringLiteral("ANT1")),
                 QStringList({ QStringLiteral("XVTA"), QStringLiteral("RX_A") }));

        // Ueber XVTA gesendet bleibt nur RX_A — die Maschine kann nicht
        // auf derselben Buchse senden und messen.
        QCOMPARE(zustand.moeglicheMesspunkte(QStringLiteral("XVTA")),
                 QStringList({ QStringLiteral("RX_A") }));

        // „INVALID" ist kein Messpunkt, sondern das Wort der Maschine
        // fuer „keiner gewaehlt".
        QVERIFY(!zustand.hatMesspunkt(QStringLiteral("ANT1")));

        fuettere(zustand, QStringLiteral(
            "apd sampler tx_ant=ANT1 valid_samplers=XVTA,RX_A selected_sampler=XVTA"));
        QVERIFY(zustand.hatMesspunkt(QStringLiteral("ANT1")));
        QCOMPARE(zustand.gewaehlterMesspunkt(QStringLiteral("ANT1")),
                 QStringLiteral("XVTA"));
    }

    // ── Falle 1: ZWEI verschiedene Zeilen heiszen „apd" ──────────────
    //
    // Die Scheibenzeile traegt kein `enable`. Wer den Zustand je ZEILE
    // uebernimmt statt je SCHLUESSEL, schaltet die Anzeige mit jeder
    // Scheibenmeldung aus, waehrend die Vorverzerrung laeuft.
    void dieScheibenzeileLoeschtDenSchalterNicht()
    {
        FlexApdState zustand;
        fuettere(zustand, QStringLiteral("apd enable=1 configurable=1"));
        QVERIFY(zustand.eingeschaltet());

        fuettere(zustand, QStringLiteral(
            "apd slice=0 mmx=0 client_handle=0x0A0F163D ant=ANT1 freq=14.100000 "
            "rfpower=5 rx_error_mHz=0.000000 equalizer_active=0 configurable=1"));

        QVERIFY(zustand.eingeschaltet());          // <- die Falle
        QCOMPARE(zustand.antenne(), QStringLiteral("ANT1"));
        QCOMPARE(zustand.frequenzMhz(), 14.1);
        QCOMPARE(zustand.leistungProzent(), 5);
        QVERIFY(!zustand.kurveTraegt());
    }

    // ── Falle 2: die Maschine schickt Teilmeldungen ──────────────────
    //
    // Nach einem `apd sampler`-Befehl kam
    // `apd sampler tx_ant=ANT1 selected_sampler=INVALID` — ohne
    // `valid_samplers`. Die Liste darf dadurch nicht verschwinden,
    // sonst steht im Auswahlfeld nichts mehr zur Wahl.
    void dieTeilmeldungLoeschtDieListeNicht()
    {
        FlexApdState zustand;
        fuettere(zustand, QStringLiteral(
            "apd sampler tx_ant=ANT1 valid_samplers=XVTA,RX_A selected_sampler=XVTA"));
        fuettere(zustand, QStringLiteral(
            "apd sampler tx_ant=ANT1 selected_sampler=INVALID"));

        QCOMPARE(zustand.moeglicheMesspunkte(QStringLiteral("ANT1")),
                 QStringList({ QStringLiteral("XVTA"), QStringLiteral("RX_A") }));
        QVERIFY(!zustand.hatMesspunkt(QStringLiteral("ANT1")));
    }

    // Nur echte Aenderungen melden: die Maschine schiebt denselben
    // Zustand mehrfach, und eine Anzeige, die bei jeder Wiederholung
    // neu baut, flackert.
    void gleicherZustandMeldetNichts()
    {
        FlexApdState zustand;
        const auto [object, kvs] =
            zerlege(QStringLiteral("apd enable=1 configurable=1"));

        QVERIFY(zustand.apply(object, kvs));    // erstes Mal: neu
        QVERIFY(!zustand.apply(object, kvs));   // zweites Mal: nichts
    }

    void trennenVergisstAlles()
    {
        FlexApdState zustand;
        fuettere(zustand, QStringLiteral("apd enable=1 configurable=1"));
        fuettere(zustand, QStringLiteral(
            "apd sampler tx_ant=ANT1 valid_samplers=XVTA,RX_A selected_sampler=XVTA"));
        QVERIFY(zustand.bekannt());

        zustand.clear();
        QVERIFY(!zustand.bekannt());
        QVERIFY(!zustand.eingeschaltet());
        QVERIFY(zustand.sendeantennen().isEmpty());
    }

    // Eine unlesbare Zahl ist keine Null: eine 0,000 MHz im
    // Zustandsfeld saehe aus wie eine Messung.
    void unlesbareZahlenBleibenDraussen()
    {
        FlexApdState zustand;
        fuettere(zustand, QStringLiteral(
            "apd slice=0 ant=ANT1 freq=14.100000 rfpower=5 equalizer_active=1"));
        QCOMPARE(zustand.frequenzMhz(), 14.1);

        fuettere(zustand, QStringLiteral(
            "apd slice=0 ant=ANT1 freq=--- rfpower=x equalizer_active=1"));
        QCOMPARE(zustand.frequenzMhz(), 14.1);
        QCOMPARE(zustand.leistungProzent(), 5);
    }
};

QTEST_MAIN(TstFlexVorverzerrung)
#include "tst_flex_vorverzerrung.moc"
