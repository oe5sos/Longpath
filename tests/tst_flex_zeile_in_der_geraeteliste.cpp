// =================================================================
// tests/tst_flex_zeile_in_der_geraeteliste.cpp  (Longpath)
// =================================================================
//
// Flex-Treiber Stufe 1: steht eine FlexRadio-Maschine richtig
// beschriftet in der Geraeteliste?
//
// Der Anlass ist ein offener Punkt aus dem Entwurf: am 2026-10-18 war
// die Zeile NICHT live zu sehen — Longpath lief gerade (eine zweite
// Fassung daneben haette den Betrieb gestoert), und danach war die
// 8400 nicht mehr erreichbar. Ein Blick, den man nicht tun kann, ist
// kein Beleg; ein Pruefstand ist einer.
//
// Geprueft wird genau das, was beim SunSDR einmal falsch war und erst
// live auffiel (`ConnectionPanel.cpp:1742` beschreibt es: die
// Protokollspalte zeigte „SunSDR", das Feld darunter trotzdem „P1",
// weil ein binaeres Fragezeichen alles ausser Protocol2 auf P1 schob).
// Dieselbe Falle gilt fuer Flex an drei Stellen — Platine, Spalte,
// Einzelheiten.
//
// Longpath-original: kein Thetis- oder AetherSDR-Urtext.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include <QHostAddress>
#include <QTableWidget>

#include "core/RadioDiscovery.h"
#include "gui/ConnectionPanel.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

RadioInfo flexZeile()
{
    // Die Werte sind die der echten FLEX-8400, am 2026-10-18 vom Draht
    // gelesen (siehe tst_flex_stufe1.cpp).
    RadioInfo info;
    info.protocol     = ProtocolVersion::Flex;
    info.boardType    = HPSDRHW::Flex8400;
    info.name         = QStringLiteral("FLEX-8400 (Martin)");
    info.macAddress   = QStringLiteral("00:1C:2D:05:34:28");
    info.address      = QHostAddress(QStringLiteral("172.30.30.12"));
    info.port         = 4992;
    info.maxReceivers = 2;
    info.adcCount     = 1;
    // Die Flex meldet ihre Fassung als Punktfolge, nicht als Zahl —
    // `firmwareVersion` bleibt deshalb 0. Genau das war die Falle.
    info.firmwareText = QStringLiteral("4.3.16.42034");
    return info;
}

QTableWidget* tabelle(ConnectionPanel& panel)
{
    return panel.findChild<QTableWidget*>();
}

int zeileMit(QTableWidget* t, const QString& name)
{
    for (int r = 0; r < t->rowCount(); ++r) {
        if (QTableWidgetItem* it = t->item(r, 1)) {   // Spalte 1 = Name
            if (it->text().contains(name)) {
                return r;
            }
        }
    }
    return -1;
}

}  // namespace

class TstFlexZeileInDerGeraeteliste : public QObject
{
    Q_OBJECT

private slots:
    void flexStehtMitPlatineUndProtokollInDerListe()
    {
        RadioModel model;
        ConnectionPanel panel(&model);

        QTableWidget* t = tabelle(panel);
        QVERIFY(t != nullptr);

        panel.onRadioDiscovered(flexZeile());

        const int r = zeileMit(t, QStringLiteral("FLEX-8400"));
        QVERIFY2(r >= 0, "keine Zeile fuer die FLEX-8400 in der Liste");

        // Spalte 2 = Board. Ohne den eigenen Fall stuende hier
        // „Unknown" (der default-Zweig).
        QCOMPARE(t->item(r, 2)->text(), QStringLiteral("FLEX-8400"));

        // Spalte 3 = Protocol. Ohne den eigenen Fall stuende hier „P1" —
        // genau der SunSDR-Fehler von 2026-08-26.
        QCOMPARE(t->item(r, 3)->text(), QStringLiteral("Flex"));

        // Spalte 4 = IP, Spalte 7 = In-Use.
        QCOMPARE(t->item(r, 4)->text(), QStringLiteral("172.30.30.12"));
        QCOMPARE(t->item(r, 7)->text(), QStringLiteral("free"));
    }

    // PR #286 hat drei Leser auf `firmwareText` gebracht und diesen
    // Dialog uebersehen. Bis zum 2026-10-21 stand deshalb in der
    // Spalte „Firmware" und in der Detailzeile eine 0, obwohl der Ruf
    // der Maschine „4.3.16.42034" mitbrachte — der Betreiber sah es
    // bei jedem Verbinden, einen Monat lang.
    void dieFassungAusDemRufStehtInDerListe()
    {
        RadioModel model;
        ConnectionPanel panel(&model);
        QTableWidget* t = tabelle(panel);
        QVERIFY(t != nullptr);

        panel.onRadioDiscovered(flexZeile());
        const int r = zeileMit(t, QStringLiteral("FLEX-8400"));
        QVERIFY(r >= 0);

        // Spalte 6 = Firmware.
        QCOMPARE(t->item(r, 6)->text(), QStringLiteral("4.3.16.42034"));
    }

    // Eine Platine ohne Zahl ist ein Strich, keine Null (HAUSSTIL
    // Regel 7). Eine 0 liest sich wie eine gemessene Fassung.
    void ohneFassungStehtEinStrich()
    {
        RadioModel model;
        ConnectionPanel panel(&model);
        QTableWidget* t = tabelle(panel);
        QVERIFY(t != nullptr);

        RadioInfo ohne = flexZeile();
        ohne.firmwareText.clear();
        ohne.firmwareVersion = 0;
        panel.onRadioDiscovered(ohne);

        const int r = zeileMit(t, QStringLiteral("FLEX-8400"));
        QVERIFY(r >= 0);
        QCOMPARE(t->item(r, 6)->text(), QStringLiteral("—"));
    }

    void einZweiterRufFuehrtDieZeileNachStattSieZuVerdoppeln()
    {
        RadioModel model;
        ConnectionPanel panel(&model);
        QTableWidget* t = tabelle(panel);
        QVERIFY(t != nullptr);

        panel.onRadioDiscovered(flexZeile());
        const int nachEinem = t->rowCount();

        RadioInfo belegt = flexZeile();
        belegt.inUse = true;
        panel.onRadioUpdated(belegt);

        QCOMPARE(t->rowCount(), nachEinem);
        const int r = zeileMit(t, QStringLiteral("FLEX-8400"));
        QVERIFY(r >= 0);
        QCOMPARE(t->item(r, 7)->text(), QStringLiteral("in use"));
    }

    void einFlexUndEinOpenHpsdrStehenNebeneinander()
    {
        // Die Geraeteliste braucht keine zweite Zeilenart — das ist der
        // Grund, warum FlexRadioDiscovery in Longpaths eigenes RadioInfo
        // uebersetzt statt eine eigene Struktur zu melden.
        RadioModel model;
        ConnectionPanel panel(&model);
        QTableWidget* t = tabelle(panel);
        QVERIFY(t != nullptr);

        RadioInfo anan;
        anan.protocol   = ProtocolVersion::Protocol2;
        anan.boardType  = HPSDRHW::OrionMKII;
        anan.name       = QStringLiteral("Anvelina PRO III");
        anan.macAddress = QStringLiteral("00:1C:2D:00:AA:BB");
        anan.address    = QHostAddress(QStringLiteral("172.30.30.26"));

        panel.onRadioDiscovered(anan);
        panel.onRadioDiscovered(flexZeile());

        const int rFlex = zeileMit(t, QStringLiteral("FLEX-8400"));
        const int rAnan = zeileMit(t, QStringLiteral("Anvelina"));
        QVERIFY(rFlex >= 0);
        QVERIFY(rAnan >= 0);
        QVERIFY(rFlex != rAnan);

        QCOMPARE(t->item(rFlex, 3)->text(), QStringLiteral("Flex"));
        QCOMPARE(t->item(rAnan, 3)->text(), QStringLiteral("P2"));
        QCOMPARE(t->item(rAnan, 2)->text(), QStringLiteral("Orion MkII"));
    }
};

QTEST_MAIN(TstFlexZeileInDerGeraeteliste)
#include "tst_flex_zeile_in_der_geraeteliste.moc"
