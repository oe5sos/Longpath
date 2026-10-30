// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_swr_riegel.cpp  (Longpath)
// =================================================================
//
// „Sollte SWR ueber 2,5 sein, bitte nicht senden!" — Betreiber,
// 2026-10-30. Hier steht, was das genau heiszt, einschliesslich der
// drei Faelle, in denen eine Messung NICHTS heiszt.
//
// Die Zeit kommt von Hand herein; eine Haltbarkeit von zwei Minuten
// laeuft darum in einer Millisekunde durch.
// =================================================================

#include <QtTest/QtTest>

#include "core/safety/SwrRiegel.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"

using namespace Longpath::safety;

class TstSwrRiegel : public QObject
{
    Q_OBJECT

private slots:
    void dieVorgabeIstDieZahlDesBetreibers()
    {
        QCOMPARE(SwrRiegel::kVorgabeGrenze, 2.5f);
        SwrRiegel r;
        QCOMPARE(r.grenze(), 2.5f);
    }

    // ── Der Fall, um den gebeten wurde ────────────────────────────
    void ueberDerGrenzeWirdGebrochenUndDannNichtMehrGetastet()
    {
        SwrRiegel r;
        const qint64 t = 1000000;

        // Eine saubere Sendung: nichts passiert.
        QVERIFY(!r.messung(1.6f, 32.0f, t));
        QVERIFY(r.darfTasten(t));

        // Jetzt reiszt die Antenne ab.
        QVERIFY2(r.messung(3.1f, 30.0f, t + 1000),
                 "die Sendung haette gebrochen werden muessen");
        // Und der naechste Versuch geht gar nicht erst los.
        QVERIFY(!r.darfTasten(t + 2000));
        QVERIFY(r.grund(t + 2000).contains(QStringLiteral("3.10")));

        // Dieselbe Ueberschreitung meldet NICHT noch einmal: der
        // Sender klingt aus, es kommen weitere Messungen herein.
        QVERIFY(!r.messung(3.4f, 20.0f, t + 1100));
        QVERIFY(!r.messung(3.2f, 10.0f, t + 1200));
    }

    // Genau auf der Grenze wird gesendet. 2,5 ist erlaubt, „ueber 2,5"
    // ist es nicht — so stand es in der Bitte.
    void genauAufDerGrenzeWirdNochGesendet()
    {
        SwrRiegel r;
        QVERIFY(!r.messung(2.5f, 20.0f, 0));
        QVERIFY(r.darfTasten(0));
        QVERIFY(r.messung(2.51f, 20.0f, 100));
        QVERIFY(!r.darfTasten(100));
    }

    // ── Wenn eine Messung nichts heiszt ───────────────────────────
    //
    // 1. Ohne Leistung ist die Stehwellenmessung Rauschen.
    void ohneLeistungZaehltKeineMessung()
    {
        SwrRiegel r;
        QVERIFY(!r.messung(9.9f, 0.2f, 0));
        QVERIFY2(r.darfTasten(0),
                 "eine Rauschmessung beim Hochfahren haette jede "
                 "Sendung gesperrt");
        // Sie wird auch nicht gemerkt.
        QCOMPARE(r.letzteSwr(), 0.0f);
    }

    // 2. Wurde nie gemessen, wird nicht gesperrt.
    void ohneJedeMessungWirdGetastet()
    {
        SwrRiegel r;
        QVERIFY(r.darfTasten(0));
        QVERIFY(r.darfTasten(99999999));
        QVERIFY(r.grund(0).isEmpty());
    }

    // 3. Eine Messung altert. Wer die Antenne wechselt, soll nicht
    //    eine Stunde gesperrt bleiben.
    void eineAlteMessungSperrtNichtMehr()
    {
        SwrRiegel r;
        r.messung(4.0f, 20.0f, 0);
        QVERIFY(!r.darfTasten(SwrRiegel::kHaltbarkeitMs - 1));
        QVERIFY(r.darfTasten(SwrRiegel::kHaltbarkeitMs));
    }

    void ausgeschaltetSperrtNie()
    {
        SwrRiegel r;
        r.setzeAn(false);
        QVERIFY(!r.messung(9.0f, 50.0f, 0));
        QVERIFY(r.darfTasten(0));
    }

    void dieGrenzeLaesstSichSetzenAberNichtInsSinnlose()
    {
        SwrRiegel r;
        r.setzeGrenze(2.0f);
        QCOMPARE(r.grenze(), 2.0f);
        // Unter 1,0 gibt es keine Stehwelle -- das waere eine
        // Dauersperre.
        r.setzeGrenze(0.5f);
        QCOMPARE(r.grenze(), 1.0f);
        r.setzeGrenze(99.0f);
        QCOMPARE(r.grenze(), 10.0f);
    }

    // Nach dem Loesen faengt die naechste Sendung ohne alten Bruch an
    // -- aber die Messung bleibt stehen, sie soll ja gerade das
    // Tasten verhindern.
    void zuruecksetzenLoeschtDenBruchNichtDieMessung()
    {
        SwrRiegel r;
        r.messung(3.0f, 20.0f, 0);
        r.zuruecksetzen();
        QVERIFY2(!r.darfTasten(1000),
                 "die Messung darf das Zuruecksetzen ueberleben");
        // Wird es besser, geht es wieder.
        QVERIFY(!r.messung(1.4f, 20.0f, 2000));
        QVERIFY(r.darfTasten(2000));
    }

    // ── Und er ist ANGESCHLOSSEN ──────────────────────────────────
    //
    // Nicht „gibt es", sondern „die Zahl kommt an". Eine Einheit, die
    // geprueft ist, aber die Einstellung nie sieht, steht auf der
    // Vorgabe und tut stillschweigend etwas anderes als gewuenscht.
    void dieGrenzeAusDenEinstellungenKommtImModellAn()
    {
        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrGrenze"),
                                         QStringLiteral("2.0"));
        Longpath::RadioModel m;
        QCOMPARE(m.flexSwrRiegel().grenze(), 2.0f);
        QVERIFY(m.flexSwrRiegel().an());

        // Und abschalten laesst er sich auch.
        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrRiegelAn"),
                                         QStringLiteral("False"));
        Longpath::RadioModel m2;
        QVERIFY(!m2.flexSwrRiegel().an());

        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrRiegelAn"),
                                         QStringLiteral("True"));
        Longpath::AppSettings::instance().setValue(
            QStringLiteral("FlexSwrGrenze"),
            QString::number(SwrRiegel::kVorgabeGrenze));
    }

    // Unsinn in der Einstellungsdatei faellt auf die Vorgabe zurueck,
    // statt den Riegel auf eine Zufallszahl zu stellen.
    void unsinnInDerDateiGibtDieVorgabe()
    {
        Longpath::AppSettings::instance().setValue(QStringLiteral("FlexSwrGrenze"),
                                         QStringLiteral("spaeter"));
        Longpath::RadioModel m;
        QCOMPARE(m.flexSwrRiegel().grenze(), SwrRiegel::kVorgabeGrenze);
        Longpath::AppSettings::instance().setValue(
            QStringLiteral("FlexSwrGrenze"),
            QString::number(SwrRiegel::kVorgabeGrenze));
    }
};

QTEST_MAIN(TstSwrRiegel)
#include "tst_swr_riegel.moc"
