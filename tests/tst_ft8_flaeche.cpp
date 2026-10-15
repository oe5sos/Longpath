// =================================================================
// tests/tst_ft8_flaeche.cpp  (Longpath)
// =================================================================
//
// Die Flaeche zum nativen FT8/FT4-Dekoder — und der Weg vom Ton bis
// in die Liste.
//
// Warum dieser Lauf noetig ist: Ft8Decoder (reine Funktion) und
// Ft8Stream (Uhr und Fenster) waren beide gruen und TROTZDEM im
// Programm unerreichbar — kein Abgriff am Ton, keine Flaeche, kein
// Menueeintrag. Genau der Fehler vom 2026-10-14 („vier Wuensche, vier
// Mal laengst gebaut und nur unerreichbar"). Dieser Lauf prueft
// darum nicht das Dekodieren, sondern die VERDRAHTUNG:
//
//   * ein Fund landet als Zeile in der Liste, Spalten wie in WSJT-X
//   * und gleichzeitig als Eintrag in RxDecodeModel, mit Rufzeichen
//     und mit der Frequenz der Scheibe PLUS der Tonlage
//   * das leere Fenster wird gemeldet, nicht verschwiegen
//   * FT8/FT4 schaltet das Fenster um und die beiden Knoepfe mit
//   * eine entfernte Scheibe loest den Abgriff
//   * der sechste Abgriff am AudioEngine traegt Proben
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimeZone>

#include "core/AudioEngine.h"
#include "core/Ft8Stream.h"
#include "core/audio/AudioTapRing.h"
#include "gui/applets/Ft8Applet.h"
#include "models/RadioModel.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstFt8Flaeche : public QObject {
    Q_OBJECT

private:
    static QVector<Ft8Decode> einFund(const QString& text,
                                      float snr = -12.0f,
                                      float hz = 1234.0f)
    {
        Ft8Decode d;
        d.snrDb = snr;
        d.dtSeconds = 0.2f;
        d.freqHz = hz;
        d.score = 40;
        d.text = text;
        return {d};
    }

private slots:

    // ── Der Fund kommt an zwei Stellen an ───────────────────────────

    void einFundStehtInDerListeUndImModell()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QVERIFY(applet.streamForTest());

        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY2(slice, "ohne Scheibe sagt der Lauf nichts");
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);

        QVERIFY(radio.rxDecodeModel());
        const int vorher = radio.rxDecodeModel()->decodes().size();

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, fenster));

        // 1. Die Liste.
        const QString zeile = applet.listForTest()->toPlainText();
        qInfo().noquote() << "Zeile:" << zeile;
        QVERIFY2(zeile.contains(QStringLiteral("CQ OE5SOS JN78")),
                 qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("-12")), qPrintable(zeile));
        QVERIFY2(zeile.contains(QStringLiteral("1234")), qPrintable(zeile));
        // Die Uhrzeit des Fensters, nicht die Uhrzeit von jetzt.
        QVERIFY2(zeile.startsWith(fenster.toString(QStringLiteral("HHmmss"))),
                 qPrintable(zeile));

        // 2. Das Modell (RxDecodeModel, „was mein Geraet gerade
        //    gehoert hat"). Ohne diesen Schritt bleibt der Fund in
        //    einem Textfeld. Die Ueberlagerung am Panadapter liest
        //    heute NICHT aus diesem Modell, sondern aus SpotModel —
        //    die Bruecke dorthin ist bewusst noch nicht gebaut (sie
        //    braucht Lebensdauer, Farbe und einen eigenen Reiter im
        //    Spot Hub, nicht eine Zeile).
        const auto decodes = radio.rxDecodeModel()->decodes();
        QCOMPARE(decodes.size(), vorher + 1);
        const RxDecode& e = decodes.last();
        QCOMPARE(e.callsign, QStringLiteral("OE5SOS"));
        QCOMPARE(e.mode, QStringLiteral("FT8"));
        QCOMPARE(e.snr, -12);
        QCOMPARE(e.payload, QStringLiteral("CQ OE5SOS JN78"));
        QCOMPARE(e.utcTime, fenster);
        // Wahlfrequenz PLUS Tonlage: 14 074 000 + 1234 Hz.
        QVERIFY2(qAbs(e.freqMhz - 14.075234) < 1e-6,
                 qPrintable(QString::number(e.freqMhz, 'f', 6)));
        // Nicht „WSJT-X": das ist eine andere Quelle.
        QVERIFY2(e.source != QStringLiteral("WSJT-X"), qPrintable(e.source));
    }

    // ── Das Seitenband entscheidet das Vorzeichen ───────────────────
    //
    // In DIGU liegt der Ton oberhalb der Wahlfrequenz, in DIGL
    // darunter. Ohne die Unterscheidung stuende ein DIGL-Fund um bis
    // zu 3 kHz daneben — und zwar lautlos.

    void dasSeitenbandEntscheidetDasVorzeichen()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGL);
        applet.setSlice(slice);

        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));

        QVERIFY(radio.rxDecodeModel());
        QVERIFY(!radio.rxDecodeModel()->decodes().isEmpty());
        const double mhz = radio.rxDecodeModel()->decodes().last().freqMhz;
        qInfo() << "DIGL:" << mhz << "MHz";
        // 14 074 000 MINUS 1234 Hz.
        QVERIFY2(qAbs(mhz - 14.072766) < 1e-6,
                 qPrintable(QString::number(mhz, 'f', 6)));
    }

    // ── Das leere Fenster ist eine Aussage ──────────────────────────
    //
    // „Abwesenheit ist keine Aussage" gilt umgekehrt genauso: ein
    // Fenster ohne Fund und ein toter Dekoder duerfen nicht gleich
    // aussehen.

    void dasLeereFensterWirdGemeldet()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.lastPeriodCountForTest(), -1);   // noch nichts

        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>, QVector<Ft8Decode>{}),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));

        QCOMPARE(applet.lastPeriodCountForTest(), 0);
        QVERIFY2(applet.lastLabelForTest()->text().endsWith(QStringLiteral("0")),
                 qPrintable(applet.lastLabelForTest()->text()));
        QVERIFY(applet.listForTest()->toPlainText().isEmpty());
    }

    // ── FT8 / FT4 ───────────────────────────────────────────────────

    void ft4SchaltetFensterUndKnoepfe()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        QCOMPARE(applet.streamForTest()->slotMs(), 15000);
        QVERIFY(applet.ft8ButtonForTest()->isChecked());
        QVERIFY(!applet.ft4ButtonForTest()->isChecked());

        applet.ft4ButtonForTest()->click();
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft4);
        QCOMPARE(applet.streamForTest()->slotMs(), 7500);
        QVERIFY(applet.ft4ButtonForTest()->isChecked());
        QVERIFY2(!applet.ft8ButtonForTest()->isChecked(),
                 "beide Knoepfe gedrueckt — dann sagt keiner etwas");

        applet.ft8ButtonForTest()->click();
        QCOMPARE(applet.streamForTest()->slotMs(), 15000);
        QVERIFY(applet.ft8ButtonForTest()->isChecked());
        QVERIFY(!applet.ft4ButtonForTest()->isChecked());

        // Und der Modus steht in der Zeile, sonst liest man FT4-Funde
        // als FT8.
        applet.setProtocol(Ft8Decoder::Protocol::Ft4);
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));
        QVERIFY(radio.rxDecodeModel());
        QVERIFY(!radio.rxDecodeModel()->decodes().isEmpty());
        QCOMPARE(radio.rxDecodeModel()->decodes().last().mode,
                 QStringLiteral("FT4"));
    }

    // ── Rufzeichen aus der Nachricht ────────────────────────────────

    void derAbsenderWirdErkannt()
    {
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ OE5SOS JN78")),
                 QStringLiteral("OE5SOS"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ DX OE5SOS JN78")),
                 QStringLiteral("OE5SOS"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("OE5SOS DL1ABC -12")),
                 QStringLiteral("DL1ABC"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("OE5SOS DL1ABC RR73")),
                 QStringLiteral("DL1ABC"));
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ OE5SOS")),
                 QStringLiteral("OE5SOS"));
        // Der Feldkenner darf NICHT als Rufzeichen durchgehen — das war
        // der Fehler, auf den die „letzte Stelle keine Ziffer"-Regel
        // antwortet.
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("CQ DX JN78")),
                 QString());
        QCOMPARE(Ft8Applet::senderCallsign(QString()), QString());
        QCOMPARE(Ft8Applet::senderCallsign(QStringLiteral("TU 73")), QString());
    }

    // ── Der Abgriff ─────────────────────────────────────────────────

    void derSechsteAbgriffTraegtProben()
    {
        // Wie tst_audio_engine_tap_teardown_race: der Abgriff haengt am
        // AudioEngine DES Modells und an einer echten Scheibenkennung.
        RadioModel radio;
        AudioEngine* engine = radio.audioEngine();
        QVERIFY(engine);
        const int sliceId = radio.addSlice();

        AudioTapRing ring(4096);
        engine->setFt8Tap(&ring, sliceId);

        QVector<float> block(512);
        for (int i = 0; i < block.size(); ++i) {
            block[i] = (i % 2 == 0) ? 0.25f : -0.25f;
        }
        engine->rxBlockReady(sliceId, block.constData(), block.size() / 2);

        QVector<float> gelesen(4096);
        const int got = ring.read(gelesen.data(), gelesen.size());
        qInfo() << "aus dem Ring:" << got << "Werte";
        QVERIFY2(got > 0, "der Abgriff liefert nichts");

        // Gegenprobe: eine FREMDE Scheibe darf hier nicht hereinlaufen.
        engine->setFt8Tap(&ring, sliceId + 99);
        engine->rxBlockReady(sliceId, block.constData(), block.size() / 2);
        QCOMPARE(ring.read(gelesen.data(), gelesen.size()), 0);

        engine->setFt8Tap(nullptr, -1);
    }

    void eineEntfernteScheibeLoestDenAbgriff()
    {
        RadioModel radio;
        Ft8Applet applet(&radio);
        // ZWEI Scheiben: RadioModel entfernt die letzte nie
        // (Phase-3F-Zusage), ein Lauf mit nur einer prueft also
        // versehentlich gar nichts.
        radio.addSlice();
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        applet.setSlice(slice);

        radio.removeSlice(id);
        QVERIFY2(radio.sliceById(id) == nullptr, "die Scheibe ist noch da");
        // Kein Absturz, und die Flaeche haelt keine Scheibe mehr: der
        // naechste Empfaenger mit diesem Index laeuft nicht hier
        // herein.
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));
        QVERIFY(radio.rxDecodeModel());
        // .last() auf eine leere Liste ist undefiniert — ohne diese
        // Zeile bestand die Gegenprobe mit ausgehaengtem Modell
        // scheinbar.
        QVERIFY2(!radio.rxDecodeModel()->decodes().isEmpty(),
                 "kein Eintrag im Modell");
        const double mhz = radio.rxDecodeModel()->decodes().last().freqMhz;
        qInfo() << "nach dem Entfernen:" << mhz << "MHz";
        // Ohne Scheibe ist die Wahlfrequenz 0 — der Fund steht dann
        // nur mit seiner Tonlage da, nicht mit einer falschen
        // Bandfrequenz.
        QVERIFY2(mhz < 0.01, qPrintable(QString::number(mhz, 'f', 6)));
    }

    void dieFlaecheLaesstSichAbbilden()
    {
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (grabDir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        RadioModel radio;
        Ft8Applet applet(&radio);
        applet.resize(320, 260);
        QMetaObject::invokeMethod(
            &applet, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>,
                  einFund(QStringLiteral("CQ OE5SOS JN78"))),
            Q_ARG(QDateTime, QDateTime::currentDateTimeUtc()));
        const QPixmap pm = applet.grab();
        QVERIFY(!pm.isNull());
        QVERIFY(pm.save(grabDir + QStringLiteral("/longpath-grab-Ft8Applet.png")));
    }
};

QTEST_MAIN(TstFt8Flaeche)
#include "tst_ft8_flaeche.moc"
