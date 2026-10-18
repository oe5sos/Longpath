// SPDX-License-Identifier: GPL-3.0-or-later
//
// gui/applets/RttyDecoderApplet: the slice binding itself, not the DSP
// (that's tst_rtty_decoder.cpp). Regression for a real bug a code review
// found 2026-09-06: the applet never unbound when its bound slice was
// removed, leaving AudioEngine's tap routing whatever slice
// RadioModel::addSlice()'s lowest-free-index reuse handed the same index
// to next -- silently decoding the wrong slice's audio under this
// applet's title.

#include <QtTest>
#include <QLabel>

#include <cmath>

#include "gui/applets/RttyDecoderApplet.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"
#include "models/RxDecodeModel.h"
#include "core/DxSpot.h"

using namespace Longpath;

namespace {
QLabel* markShiftLabelOf(RttyDecoderApplet& a)
{
    for (QLabel* l : a.findChildren<QLabel*>()) {
        if (l->text().contains(QStringLiteral("Mark"))) { return l; }
    }
    return nullptr;
}
} // namespace

class TstRttyDecoderApplet : public QObject
{
    Q_OBJECT

private slots:
    // ── Aus dem Zeichenstrom ein Rufzeichen (2026-10-17) ───────────
    //
    // Reine Funktion, darum ohne Fenster und ohne Dekoder. Die Faelle
    // sind die, die im Betrieb vorkommen — und die, an denen eine
    // naive Suche Unsinn meldet.
    void rufzeichenNachDe_data()
    {
        QTest::addColumn<QString>("strom");
        QTest::addColumn<QString>("erwartet");

        QTest::newRow("CQ-Anruf")
            << QStringLiteral("CQ CQ CQ DE DL1ABC DL1ABC DL1ABC K ")
            << QStringLiteral("DL1ABC");
        QTest::newRow("Antwort an uns")
            << QStringLiteral("OE5SOS DE DL1ABC DL1ABC KN ")
            << QStringLiteral("DL1ABC");
        QTest::newRow("mit Schraegstrich")
            << QStringLiteral("CQ DE DL1ABC/P DL1ABC/P K ")
            << QStringLiteral("DL1ABC/P");
        QTest::newRow("kleingeschrieben")
            << QStringLiteral("cq de dl1abc dl1abc k ")
            << QStringLiteral("DL1ABC");

        // Das LETZTE DE gilt: im Strom stehen mehrere Anrufe, und der
        // juengste ist der, der gerade sendet.
        QTest::newRow("zwei Anrufe")
            << QStringLiteral("CQ DE DL1ABC K  CQ DE OK2XYZ OK2XYZ K ")
            << QStringLiteral("OK2XYZ");

        // Noch nicht abgeschlossen: kein Trennzeichen hinter dem Feld.
        // Ohne diese Regel meldet der Lauf „DL1AB", sobald das B da
        // ist — ein zweiter, falscher Spot fuer dieselbe Station.
        QTest::newRow("noch im Tippen")
            << QStringLiteral("CQ CQ DE DL1AB")
            << QString();

        // Kein DE: nichts. Auch wenn rufzeichenaehnliche Felder da
        // stehen — ein Rapport ist keine Station.
        QTest::newRow("nur Rapport")
            << QStringLiteral("599 599 OE5SOS OE5SOS ")
            << QString();
        QTest::newRow("leer")       << QString()                  << QString();
        QTest::newRow("nur DE")     << QStringLiteral("DE ")      << QString();
        // DE mit Unsinn dahinter ist ein Treffer auf eine
        // verstuemmelte Stelle — und kein Grund, ein aelteres DE zu
        // raten.
        QTest::newRow("DE mit Muell")
            << QStringLiteral("CQ DE DL1ABC K  DE 599 ")
            << QString();
        // Feldkenner und Zahlen fallen heraus (letzte Stelle Ziffer).
        QTest::newRow("DE Feldkenner")
            << QStringLiteral("DE JN78 K ") << QString();

        // Gerade abgeschlossen: das Trennzeichen ist da, also gilt
        // das Rufzeichen sofort — es muss nicht erst ein weiteres
        // Feld hinterherkommen.
        QTest::newRow("gerade abgeschlossen")
            << QStringLiteral("CQ DE DL1ABC ") << QStringLiteral("DL1ABC");
    }

    void rufzeichenNachDe()
    {
        QFETCH(QString, strom);
        QFETCH(QString, erwartet);
        QCOMPARE(RttyDecoderApplet::rufzeichenNachDe(strom), erwartet);
    }

    // ── Der Anruf erreicht Spotliste und Sammler (2026-10-17) ──────
    //
    // Bis heute endete ein RTTY-Anruf im Textfeld und nirgends sonst.
    // Der eigene FT8-Dekoder reicht seine Funde weiter (Spotliste und
    // damit Panadapter, Sammler „was mein Geraet gehoert hat", PSK
    // Reporter) — RTTY nicht, obwohl derselbe Weg offen lag.
    //
    // Geprueft wird der Weg, nicht die Darstellung: kommt der Anruf
    // in beiden Modellen an, mit Rufzeichen, Betriebsart und der
    // Frequenz aus Wahlfrequenz plus Mark-Tonlage?
    void einAnrufErreichtSpotlisteUndSammler()
    {
        RadioModel radio;
        QVERIFY(radio.spotModel());
        QVERIFY(radio.rxDecodeModel());

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14080000);        // 14,080 MHz, RTTY-Bereich
        slice->setDspMode(DSPMode::DIGU);     // oberes Seitenband
        slice->setRttyMarkHz(2125);

        RttyDecoderApplet applet(&radio);
        applet.setSlice(slice);

        const int vorher = radio.rxDecodeModel()->decodes().size();
        applet.speiseTextFuerTest(
            QStringLiteral("CQ CQ CQ DE DL1ABC DL1ABC DL1ABC K "));

        // 1. Im Sammler.
        const auto funde = radio.rxDecodeModel()->decodes();
        QVERIFY2(funde.size() == vorher + 1,
                 qPrintable(QStringLiteral("%1 Funde statt einem neuen")
                                .arg(funde.size() - vorher)));
        const RxDecode& d = funde.last();
        QCOMPARE(d.callsign, QStringLiteral("DL1ABC"));
        QCOMPARE(d.mode, QStringLiteral("RTTY"));
        QCOMPARE(d.source, QStringLiteral("Longpath"));
        // Wahlfrequenz plus Mark: 14,080000 + 0,002125 MHz. Bei
        // unterem Seitenband waere es minus — darum steht die
        // Betriebsart oben ausdruecklich auf DIGU.
        QVERIFY2(std::abs(d.freqMhz - 14.082125) < 1e-6,
                 qPrintable(QStringLiteral("%1 MHz statt 14,082125")
                                .arg(d.freqMhz, 0, 'f', 6)));

        // 2. In der Spotliste — das ist der Weg auf den Panadapter.
        bool inDerListe = false;
        for (const SpotData& sp : radio.spotModel()->spots()) {
            if (sp.callsign != QStringLiteral("DL1ABC")) { continue; }
            inDerListe = true;
            qInfo() << "Spot:" << sp.callsign << sp.rxFreqMhz
                    << sp.mode << sp.source << sp.lifetimeSeconds;
            QCOMPARE(sp.source, QStringLiteral("Longpath"));
            QVERIFY2(qAbs(sp.rxFreqMhz - 14.082125) < 1e-4,
                     qPrintable(QString::number(sp.rxFreqMhz, 'f', 6)));
            // 900 s, und das ist eine bewusste Wahl — die Begruendung
            // steht bei RadioModel::onNativeRttyDecode.
            QCOMPARE(sp.lifetimeSeconds, 900);
        }
        QVERIFY2(inDerListe,
                 "der Anruf steht nicht in der Spotliste — dann kommt er "
                 "auch nicht auf den Panadapter");
    }

    /// Dieselbe Station zweimal im Strom gibt EINEN Fund. Ohne das
    /// geht bei jedem weiteren Zeichen ein Fund hinaus, solange der
    /// Anruf noch im Puffer steht.
    void dieselbeStationNurEinmal()
    {
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14080000);
        slice->setDspMode(DSPMode::DIGU);

        RttyDecoderApplet applet(&radio);
        applet.setSlice(slice);

        const int vorher = radio.rxDecodeModel()->decodes().size();
        applet.speiseTextFuerTest(QStringLiteral("CQ DE DL1ABC "));
        applet.speiseTextFuerTest(QStringLiteral("DL1ABC K "));
        applet.speiseTextFuerTest(QStringLiteral("PSE K "));
        const int neu = radio.rxDecodeModel()->decodes().size() - vorher;
        QCOMPARE(neu, 1);

        // Eine ANDERE Station danach geht wieder durch — die Sperre
        // gilt der Wiederholung, nicht dem naechsten Anruf.
        applet.speiseTextFuerTest(QStringLiteral("CQ DE OK2XYZ K "));
        QCOMPARE(radio.rxDecodeModel()->decodes().size() - vorher, 2);
    }

    void unbindsWhenBoundSliceIsRemoved()
    {
        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();  // a second slice so removeSlice(idA) is allowed
        SliceModel* sliceA = radio.sliceById(idA);
        QVERIFY(sliceA);
        sliceA->setRttyMarkHz(2400);

        RttyDecoderApplet applet(&radio);
        applet.setSlice(sliceA);

        QLabel* label = markShiftLabelOf(applet);
        QVERIFY2(label, "expected a Mark/Shift info label in RttyDecoderApplet");
        QVERIFY2(label->text().contains(QStringLiteral("2400")),
                  qPrintable(QStringLiteral("expected bound slice's rttyMarkHz to show, got: ") + label->text()));

        radio.removeSlice(idA);

        // Verhaltensaenderung mit der Empfaengerwahl (2026-10-09):
        //
        // Frueher stand hier der Platzhalter. Das Applet liess seinen
        // Abgriff los und blieb leer, bis irgendwo geklickt wurde -- der
        // Grund war, dass ein veralteter sliceIndex im Abgriff den
        // naechsten Empfaenger hereingeroutet haette, der diesen Index
        // wiederverwendet.
        //
        // Mit der Empfaengerwahl steht die Bindung auf "folgt dem
        // aktiven" (Vorgabe), und das Applet bindet beim Wegfall sofort
        // auf den jetzt aktiven Empfaenger um. Die alte Gefahr ist damit
        // gebannt -- der Abgriff zeigt auf etwas Gueltiges statt auf
        // einen toten Index -- und im Betrieb laeuft der Decoder weiter,
        // statt bis zum naechsten Klick stillzustehen.
        //
        // Der Fall, fuer den der Platzhalter gedacht war, lebt eine Zeile
        // weiter unten weiter: bei einer FESTEN Wahl auf den entfernten
        // Empfaenger gibt es nichts, worauf umzubinden waere.
        QVERIFY2(!label->text().contains(QChar(0x2014)),
                 qPrintable(QStringLiteral("erwartet: Umbindung auf den noch "
                                           "vorhandenen Empfaenger, bekommen: ")
                                + label->text()));
        QVERIFY2(label->text().contains(QStringLiteral("Mark")),
                 qPrintable(QStringLiteral("erwartet Mark/Shift des neuen "
                                           "Empfaengers, bekommen: ") + label->text()));

        Q_UNUSED(idB);
    }

    void placeholderWhenTheFIXEDReceiverIsRemoved()
    {
        // Die Absicht des Falls darueber, auf dem Stand der
        // Empfaengerwahl: haengt das Applet FEST an einem Empfaenger und
        // der verschwindet, gibt es nichts, worauf umzubinden waere --
        // dann und nur dann steht der Platzhalter da.
        AppSettings::instance().remove(
            QStringLiteral("Applet/rtty-decoder/Receiver"));

        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();
        SliceModel* sliceA = radio.sliceById(idA);
        QVERIFY(sliceA);
        sliceA->setRttyMarkHz(2400);

        RttyDecoderApplet applet(&radio);
        // Fest auf den ERSTEN Empfaenger, nicht "folgt dem aktiven".
        // Die Wahl ist seine KENNUNG, keine Listenposition -- genau
        // darum ueberlebt sie das Nachruecken der anderen.
        applet.setReceiverChoice(sliceA->sliceIndex());
        applet.setSlice(applet.boundSlice());

        QLabel* label = markShiftLabelOf(applet);
        QVERIFY(label);
        QVERIFY2(label->text().contains(QStringLiteral("2400")),
                 qPrintable(QStringLiteral("erwartet Mark des fest gewaehlten "
                                           "Empfaengers, bekommen: ") + label->text()));

        radio.removeSlice(idA);

        QVERIFY2(label->text().contains(QChar(0x2014)),
                 qPrintable(QStringLiteral("erwartet den Platzhalter, nachdem der "
                                           "FEST gewaehlte Empfaenger weg ist, "
                                           "bekommen: ") + label->text()));
        Q_UNUSED(idB);

        AppSettings::instance().remove(
            QStringLiteral("Applet/rtty-decoder/Receiver"));
    }

    // Regression for a second, related bug found live 2026-09-07 against a
    // real ANAN 10e: MainWindow used to call setSlice() exactly once, at
    // slice-0-added time, and never again -- so when the ACTIVE slice's
    // IDENTITY changed later (a band click restoring a per-band mode, a
    // KiwiSDR profile), the applet kept showing the OLD slice's Mark/Shift
    // values and the old slice's rttyMarkHzChanged went on updating a
    // no-longer-active display. Fixed in MainWindow::
    // rebindRttyRadeAvailability(), which now re-calls setSlice() on every
    // RadioModel::activeSliceChanged, same as RxApplet and CommandBar
    // already did. This test exercises setSlice() itself: repeated calls
    // with a DIFFERENT slice must drop the old slice's connections, not
    // just add the new one's.
    void reboundSliceStopsFollowingThePreviousOne()
    {
        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();
        SliceModel* sliceA = radio.sliceById(idA);
        SliceModel* sliceB = radio.sliceById(idB);
        QVERIFY(sliceA);
        QVERIFY(sliceB);
        sliceA->setRttyMarkHz(2111);
        sliceB->setRttyMarkHz(2295);

        RttyDecoderApplet applet(&radio);
        applet.setSlice(sliceA);
        QLabel* label = markShiftLabelOf(applet);
        QVERIFY2(label, "expected a Mark/Shift info label in RttyDecoderApplet");
        QVERIFY2(label->text().contains(QStringLiteral("2111")),
                  qPrintable(QStringLiteral("expected slice A's rttyMarkHz to show, got: ") + label->text()));

        applet.setSlice(sliceB);
        QVERIFY2(label->text().contains(QStringLiteral("2295")),
                  qPrintable(QStringLiteral("expected slice B's rttyMarkHz after re-bind, got: ") + label->text()));

        // The old slice must no longer drive the display: changing it now
        // should leave the label showing slice B's value untouched.
        sliceA->setRttyMarkHz(9999);
        QVERIFY2(label->text().contains(QStringLiteral("2295")),
                  qPrintable(QStringLiteral("slice A should be unbound after re-bind to slice B, got: ") + label->text()));
        QVERIFY2(!label->text().contains(QStringLiteral("9999")),
                  qPrintable(QStringLiteral("stale slice A connection still updating the label: ") + label->text()));
    }
};

QTEST_MAIN(TstRttyDecoderApplet)
#include "tst_rtty_decoder_applet.moc"
