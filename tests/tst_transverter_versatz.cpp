// SPDX-License-Identifier: GPL-3.0-or-later
//
// Was das ZF-Geraet zeigt, ist nicht, was auf der Antenne liegt.
//
// Der Betreiber baut eine 2-m-Conteststation: ein klassisches
// Funkgeraet als ZF hinter einem Transverter (144-148 MHz auf
// 28-30 MHz), Longpath haengt mit einem SDR am ZF-Ausgang. Ohne
// Umrechnung traegt jedes Logbuch 28,3 MHz, wo 144,3 MHz war.
//
// Der Fall, auf den es hier wirklich ankommt, ist der dritte: eine
// Frequenz AUSSERHALB des ZF-Fensters muss unberuehrt bleiben. Wer
// mit eingeschaltetem Transverter auf 20 m hoert, will 14 MHz sehen
// und nicht 130 MHz. Eine Rechnung, die immer addiert, waere
// schlimmer als keine.

#include <QtTest>

#include "core/TransverterSetup.h"
#include "core/AppSettings.h"

using namespace Longpath;

namespace {

constexpr qint64 k28_300 = 28300000;
constexpr qint64 k144_300 = 144300000;

TransverterSetup zweiMeter()
{
    TransverterSetup t;
    t.enabled  = true;
    t.ifLowHz  = 28000000;
    t.ifHighHz = 30000000;
    t.offsetHz = 116000000;
    return t;
}

} // namespace

class TstTransverterVersatz : public QObject
{
    Q_OBJECT

private slots:
    void ausgeschaltetRechnetNicht()
    {
        TransverterSetup t = zweiMeter();
        t.enabled = false;

        QVERIFY(!t.active());
        QCOMPARE(t.rfFrequencyHz(k28_300), k28_300);
        QCOMPARE(t.ifFrequencyHz(k144_300), k144_300);
    }

    void imFensterWirdGerechnet()
    {
        const TransverterSetup t = zweiMeter();

        QVERIFY(t.active());
        QCOMPARE(t.rfFrequencyHz(k28_300), k144_300);
        QCOMPARE(t.ifFrequencyHz(k144_300), k28_300);
    }

    void hinUndZurueckIstDasselbe()
    {
        const TransverterSetup t = zweiMeter();

        for (qint64 hz = 28000000; hz <= 30000000; hz += 250000) {
            QCOMPARE(t.ifFrequencyHz(t.rfFrequencyHz(hz)), hz);
        }
    }

    // Der eigentliche Grund fuer das Fenster.
    void ausserhalbBleibtUnberuehrt()
    {
        const TransverterSetup t = zweiMeter();

        QCOMPARE(t.rfFrequencyHz(14200000), 14200000);   // 20 m
        QCOMPARE(t.rfFrequencyHz(7100000),   7100000);   // 40 m
        QCOMPARE(t.rfFrequencyHz(50150000), 50150000);   // 6 m
        QCOMPARE(t.ifFrequencyHz(14200000), 14200000);
    }

    void dieGrenzenGehoerenDazu()
    {
        const TransverterSetup t = zweiMeter();

        QCOMPARE(t.rfFrequencyHz(28000000), 144000000);
        QCOMPARE(t.rfFrequencyHz(30000000), 146000000);
        QCOMPARE(t.rfFrequencyHz(27999999),  27999999);
        QCOMPARE(t.rfFrequencyHz(30000001),  30000001);

        QCOMPARE(t.ifFrequencyHz(144000000), 28000000);
        QCOMPARE(t.ifFrequencyHz(146000000), 30000000);
        QCOMPARE(t.ifFrequencyHz(143999999), 143999999);
        QCOMPARE(t.ifFrequencyHz(146000001), 146000001);
    }

    void unbrauchbareEinstellungRechnetNicht()
    {
        TransverterSetup t = zweiMeter();

        t.offsetHz = 0;
        QVERIFY(!t.configured());
        QVERIFY(!t.active());
        QCOMPARE(t.rfFrequencyHz(k28_300), k28_300);

        t = zweiMeter();
        t.ifHighHz = t.ifLowHz;          // Fenster ohne Breite
        QVERIFY(!t.configured());

        t = zweiMeter();
        t.ifHighHz = 20000000;           // Obergrenze unter der Untergrenze
        QVERIFY(!t.configured());

        t = zweiMeter();
        t.ifLowHz = 0;
        QVERIFY(!t.configured());
    }

    // Abwaertsmischung: nicht jeder Transverter addiert.
    void negativerVersatzGehtAuch()
    {
        TransverterSetup t;
        t.enabled  = true;
        t.ifLowHz  = 144000000;
        t.ifHighHz = 146000000;
        t.offsetHz = -116000000;

        QVERIFY(t.active());
        QCOMPARE(t.rfFrequencyHz(k144_300), k28_300);
        QCOMPARE(t.ifFrequencyHz(k28_300), k144_300);
        QCOMPARE(t.rfFrequencyHz(14200000), 14200000);
    }

    void beschreibungNenntBeideBereiche()
    {
        const TransverterSetup t = zweiMeter();
        const QString s = t.describe();

        QVERIFY(s.contains(QStringLiteral("28.000")));
        QVERIFY(s.contains(QStringLiteral("30.000")));
        QVERIFY(s.contains(QStringLiteral("144.000")));
        QVERIFY(s.contains(QStringLiteral("146.000")));
        QVERIFY(s.contains(QStringLiteral("+116.000")));

        TransverterSetup leer;
        leer.offsetHz = 0;
        QVERIFY(leer.describe().isEmpty());
    }

    void beschreibungZeigtAuchDasMinus()
    {
        TransverterSetup t;
        t.enabled  = true;
        t.ifLowHz  = 144000000;
        t.ifHighHz = 146000000;
        t.offsetHz = -116000000;

        const QString s = t.describe();
        QVERIFY(s.contains(QStringLiteral("-116.000")));
        QVERIFY(!s.contains(QStringLiteral("+")));
    }

    // Eine Vorgabe, die nicht rechnet, waere eine Falle: sie sieht
    // richtig aus und liefert eine falsche Frequenz.
    void vorgabenSindInSichSchluessig()
    {
        const QVector<TransverterSetup::Preset> p = TransverterSetup::presets();
        QVERIFY(!p.isEmpty());

        for (const TransverterSetup::Preset& v : p) {
            TransverterSetup t;
            t.enabled  = true;
            t.ifLowHz  = v.ifLowHz;
            t.ifHighHz = v.ifHighHz;
            t.offsetHz = v.offsetHz;

            QVERIFY2(t.configured(), v.name);
            QVERIFY2(t.ifLowHz + t.offsetHz > 0, v.name);
            // Die Fensterbreite bleibt beim Mischen erhalten.
            QCOMPARE((t.ifHighHz + t.offsetHz) - (t.ifLowHz + t.offsetHz),
                     t.ifHighHz - t.ifLowHz);
        }
    }

    void zweiMeterVorgabeTrifftDasBand()
    {
        const QVector<TransverterSetup::Preset> p = TransverterSetup::presets();

        bool gefunden = false;
        for (const TransverterSetup::Preset& v : p) {
            if (v.ifLowHz != 28000000 || v.offsetHz != 116000000) { continue; }
            gefunden = true;

            TransverterSetup t;
            t.enabled  = true;
            t.ifLowHz  = v.ifLowHz;
            t.ifHighHz = v.ifHighHz;
            t.offsetHz = v.offsetHz;
            QCOMPARE(t.rfFrequencyHz(k28_300), k144_300);
        }
        QVERIFY2(gefunden, "keine 144/28-Vorgabe in der Liste");
    }

    void einstellungUeberlebtDenRundlauf()
    {
        // TestSandboxInit haengt in jedem Pruefbinaer und legt
        // QStandardPaths in den Pruefmodus -- die Ablage des
        // Betreibers wird dabei nicht angefasst.
        TransverterSetup t;
        t.enabled  = true;
        t.ifLowHz  = 144000000;
        t.ifHighHz = 146000000;
        t.offsetHz = 1152000000;
        t.save();

        const TransverterSetup g = TransverterSetup::load();
        QCOMPARE(g.enabled,  true);
        QCOMPARE(g.ifLowHz,  144000000);
        QCOMPARE(g.ifHighHz, 146000000);
        QCOMPARE(g.offsetHz, 1152000000);

        // Und der Wahrheitswert liegt als "True" in der Ablage, nicht
        // als "true" -- sonst liest ihn kein anderer Leser.
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Transverter/Enabled")).toString(),
                 QStringLiteral("True"));

        t.enabled = false;
        t.save();
        QCOMPARE(TransverterSetup::load().enabled, false);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Transverter/Enabled")).toString(),
                 QStringLiteral("False"));
    }
};

QTEST_MAIN(TstTransverterVersatz)
#include "tst_transverter_versatz.moc"
