// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_scheibenregler.cpp  (Longpath)
// =================================================================
//
// Die Fortsetzung des Rauschsperren-Befunds vom 2026-10-31.
//
// `RadioModel::wireSliceSignals()` steigt in ihrer zweiten Zeile aus:
//
//     if (!slice || !m_connection) { return; }
//
// `m_connection` ist die OpenHPSDR-Verbindung; am Flex ist sie immer
// null. Die Funktion laeuft dort also ueberhaupt nicht -- und mit ihr
// haengt das ganze DSP-Bedienfeld der Scheibe an nichts. Was am Flex
// wirkt, ist genau das, was `MainWindow_Flex` SELBST verdrahtet.
//
// Die Schluessel sind am 2026-11-01 vom Geraet gelesen: eine eigene
// Scheibe angelegt, ihre 101 Zustandsschluessel mitgeschrieben,
// Scheibe wieder entfernt. Nur lesend, kein Senden.
// =================================================================

#include <QtTest/QtTest>

#include <QRegularExpression>
#include <QSet>

#include "core/FlexRadioConnection.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstFlexScheibenregler : public QObject
{
    Q_OBJECT

private slots:
    // Die Namen stehen so, wie die Maschine sie fuehrt.
    void dieBefehleTragenDieNamenDerMaschine()
    {
        const QString s = QStringLiteral("0");
        QCOMPARE(FlexCommandWire::setSliceAgcModeCommand(s, QStringLiteral("med")),
                 QStringLiteral("slice set 0 agc_mode=med"));
        QCOMPARE(FlexCommandWire::setSliceAudioLevelCommand(s, 50),
                 QStringLiteral("slice set 0 audio_level=50"));
        QCOMPARE(FlexCommandWire::setSliceAudioMuteCommand(s, true),
                 QStringLiteral("slice set 0 audio_mute=1"));
        QCOMPARE(FlexCommandWire::setSliceAudioPanCommand(s, 50),
                 QStringLiteral("slice set 0 audio_pan=50"));
        QCOMPARE(FlexCommandWire::setSliceApfCommand(s, true),
                 QStringLiteral("slice set 0 apf=1"));
    }

    // RIT und XIT gehen als EIN Befehl hinaus: Schalter und Zahl
    // zusammen. Getrennt stuende nach dem Einschalten die alte Zahl
    // daneben.
    void ritUndXitGehenAlsEinStueck()
    {
        QCOMPARE(FlexCommandWire::setSliceRitCommand(QStringLiteral("0"), true, -120),
                 QStringLiteral("slice set 0 rit_on=1 rit_freq=-120"));
        QCOMPARE(FlexCommandWire::setSliceXitCommand(QStringLiteral("0"), false, 0),
                 QStringLiteral("slice set 0 xit_on=0 xit_freq=0"));
    }

    // Die Maschine fuehrt vier AGC-Namen. Unser "Long" ist keiner
    // davon -- es geht gar nicht erst hinaus, statt eine Ablehnung zu
    // ernten und die Scheibe stillschweigend stehen zu lassen.
    void nurDieVierAgcNamenDerMaschine()
    {
        const QString s = QStringLiteral("0");
        for (const QString& gut : {QStringLiteral("off"), QStringLiteral("slow"),
                                   QStringLiteral("med"), QStringLiteral("fast")}) {
            QVERIFY2(!FlexCommandWire::setSliceAgcModeCommand(s, gut).isEmpty(),
                     qPrintable(gut));
        }
        for (const QString& schlecht : {QStringLiteral("long"),
                                        QStringLiteral("custom"),
                                        QStringLiteral("")}) {
            QVERIFY2(FlexCommandWire::setSliceAgcModeCommand(s, schlecht).isEmpty(),
                     qPrintable(QStringLiteral("die Maschine kennt '%1' nicht")
                                    .arg(schlecht)));
        }
    }

    // Ohne Scheibenkennung geht nichts hinaus, und Werte auszerhalb
    // des Bereichs auch nicht: ein Befehl an niemanden ist schlimmer
    // als keiner.
    void ohneScheibeUndAuszerhalbKeinBefehl()
    {
        QVERIFY(FlexCommandWire::setSliceAudioLevelCommand(QString(), 50).isEmpty());
        QVERIFY(FlexCommandWire::setSliceAudioMuteCommand(QString(), true).isEmpty());
        QVERIFY(FlexCommandWire::setSliceRitCommand(QString(), true, 0).isEmpty());
        QVERIFY(FlexCommandWire::setSliceAudioLevelCommand(
                    QStringLiteral("0"), 101).isEmpty());
        QVERIFY(FlexCommandWire::setSliceAudioLevelCommand(
                    QStringLiteral("0"), -1).isEmpty());
        QVERIFY(FlexCommandWire::setSliceAudioPanCommand(
                    QStringLiteral("0"), 101).isEmpty());
    }

    // ── Jeder Steckplatz trifft SEINEN Minderer ─────────────────────
    //
    // Betreiber, 2026-10-28: „aber jeder nr sollte anders
    // funktionieren!" Am Flex taten sie das nicht: NR1, NR2 und NR4
    // schickten alle dasselbe `nr=1`.
    //
    // Die Maschine fuehrt fuenf Verfahren; die Zuordnung geht nach
    // der FAMILIE, nicht nach der Nummer.
    void jederSteckplatzTrifftSeinenMinderer()
    {
        QCOMPARE(RadioModel::flexNrSchluessel(NrSlot::NR1),
                 QStringLiteral("lms_nr"));     // WDSP anr.c = LMS
        QCOMPARE(RadioModel::flexNrSchluessel(NrSlot::NR2),
                 QStringLiteral("nr"));         // WDSP emnr.c = spektral
        QCOMPARE(RadioModel::flexNrSchluessel(NrSlot::NR3),
                 QStringLiteral("rnnoise"));    // beide neuronal
        QCOMPARE(RadioModel::flexNrSchluessel(NrSlot::NR4),
                 QStringLiteral("speex_nr"));   // beide Spektralsubtraktion
        // Keine zwei auf denselben -- sonst klaenge wieder alles gleich.
        const QStringList alle{
            RadioModel::flexNrSchluessel(NrSlot::NR1),
            RadioModel::flexNrSchluessel(NrSlot::NR2),
            RadioModel::flexNrSchluessel(NrSlot::NR3),
            RadioModel::flexNrSchluessel(NrSlot::NR4)};
        QCOMPARE(QSet<QString>(alle.begin(), alle.end()).size(), 4);
        // Was wir selbst rechnen, hat dort nichts zu suchen.
        for (NrSlot eigen : {NrSlot::DFNR, NrSlot::BNR, NrSlot::MNR,
                             NrSlot::NNR, NrSlot::Off}) {
            QVERIFY2(RadioModel::flexNrSchluessel(eigen).isEmpty(),
                     "ein Verfahren, das die Maschine nicht fuehrt, darf "
                     "nicht auf einen fremden Schluessel zeigen");
        }
    }

    // Und die Maschine nimmt nur ihre eigenen Namen.
    void nurDieFuenfNamenDerMaschine()
    {
        const QString s = QStringLiteral("0");
        for (const QString& gut : {QStringLiteral("nr"), QStringLiteral("lms_nr"),
                                   QStringLiteral("speex_nr"),
                                   QStringLiteral("nrf"),
                                   QStringLiteral("rnnoise")}) {
            QVERIFY2(!FlexCommandWire::setSliceNrKindCommand(s, gut, true).isEmpty(),
                     qPrintable(gut));
        }
        QVERIFY(FlexCommandWire::setSliceNrKindCommand(
                    s, QStringLiteral("nr2"), true).isEmpty());
        // RNNoise hat keinen Pegel -- es ist ein Netz, kein Regler.
        QVERIFY(FlexCommandWire::setSliceNrLevelCommand(
                    s, QStringLiteral("rnnoise"), 50).isEmpty());
        QCOMPARE(FlexCommandWire::setSliceNrLevelCommand(
                     s, QStringLiteral("lms_nr"), 70),
                 QStringLiteral("slice set 0 lms_nr_level=70"));
    }

    // ── Die Staerke geht an das gewaehlte Verfahren ─────────────────
    //
    // Die Maschine steht ab Werk auf 50, und wir ruehrten sie nie an
    // -- das war „NR2 hört sich sehr dumpf an". Jetzt gibt es dafuer
    // einen Regler, und er musz an den Schluessel des GEWAEHLTEN
    // Verfahrens gehen, nicht an einen festen.
    void dieStaerkeGehtAnDasGewaehlteVerfahren()
    {
        const QString rumpf = quelle(QStringLiteral("/src/models/RadioModel.cpp"));
        const int a = rumpf.indexOf(QStringLiteral("void RadioModel::sendeFlexNrStufe"));
        QVERIFY2(a > 0, "sendeFlexNrStufe ist weg");
        const int e = rumpf.indexOf(QStringLiteral("\n}\n"), a);
        QVERIFY2(e > a, "die Funktion sieht anders aus als erwartet");
        QVERIFY2(rumpf.mid(a, e - a).contains(QStringLiteral("flexNrSchluessel")),
                 "die Staerke geht an einen festen Schluessel statt an den "
                 "des gewaehlten Verfahrens");

        // Und der Schnellgriff musz am Flex die Maschine zeigen.
        const QString fenster = quelle(QStringLiteral("/src/gui/MainWindow_Flex.cpp"));
        QVERIFY2(fenster.contains(QStringLiteral("setMaschinenNr")),
                 "der Schnellgriff zeigt am Flex weiterhin WDSP-Werte, die "
                 "dort nichts tun");
    }

    // ── Die AGC-Schwelle ist VERHAELTNISMAESZIG, nicht physikalisch ─
    //
    // Betreiber, 2026-11-01: „höre hier viel zu viel rauschen so denke
    // ich". AGC-T war am Flex nicht verdrahtet -- sein Regler stand
    // auf -20, die Maschine auf ihrem eigenen Wert (gelesen: 60).
    //
    // Unser Regler laeuft -160..0, die Maschine 0..100. Eine
    // physikalische Umrechnung gibt es ohne Quelle nicht; was es gibt,
    // ist dieselbe STELLUNG im jeweiligen Bereich. Dieser Fall haelt
    // genau das fest -- und die Enden, denn dort faellt ein
    // Vorzeichenfehler zuerst auf.
    void dieAgcSchwelleHaeltDieEnden()
    {
        const QString rumpf = quelle(QStringLiteral("/src/models/RadioModel.cpp"));
        const int a = rumpf.indexOf(QStringLiteral("void RadioModel::sendeFlexAgcSchwelle"));
        QVERIFY2(a > 0, "sendeFlexAgcSchwelle ist weg");
        const int e = rumpf.indexOf(QStringLiteral("\n}\n"), a);
        QVERIFY2(e > a, "die Funktion sieht anders aus als erwartet");
        const QString k = rumpf.mid(a, e - a);
        QVERIFY2(k.contains(QStringLiteral("kUnten = -160")),
                 "der Bereich des Reglers stimmt nicht mehr mit RxApplet "
                 "ueberein (m_agcTSlider->setRange(-160, 0))");
        QVERIFY2(k.contains(QStringLiteral("wert - kUnten")),
                 "die Abbildung ist verdreht: ganz links muss ganz links "
                 "bleiben");
        // Der Befehlsbauer nimmt nur 0..100.
        QVERIFY(!FlexCommandWire::setSliceAgcThresholdCommand(
                     QStringLiteral("0"), 0).isEmpty());
        QVERIFY(!FlexCommandWire::setSliceAgcThresholdCommand(
                     QStringLiteral("0"), 100).isEmpty());
        QVERIFY(FlexCommandWire::setSliceAgcThresholdCommand(
                    QStringLiteral("0"), 101).isEmpty());
        QCOMPARE(FlexCommandWire::setSliceAgcThresholdCommand(
                     QStringLiteral("0"), 60),
                 QStringLiteral("slice set 0 agc_threshold=60"));
    }

    // ── Die Balance ist die Umkehrung derselben Rechnung ────────────
    //
    // Unser Regler schreibt `(val - 50) / 50.0`, also -1 links,
    // 0 Mitte, +1 rechts. Die Maschine zaehlt 0..100 mit 50 als
    // Mitte. Wer das verdreht, schickt den Ton auf die falsche Seite.
    void dieBalanceLaeuftInDieRichtigeRichtung()
    {
        const QString rumpf = quelle(QStringLiteral("/src/models/RadioModel.cpp"));
        const int a = rumpf.indexOf(QStringLiteral("void RadioModel::sendeFlexBalance"));
        QVERIFY2(a > 0, "sendeFlexBalance ist weg");
        const int e = rumpf.indexOf(QStringLiteral("\n}\n"), a);
        QVERIFY2(e > a, "sendeFlexBalance sieht anders aus als erwartet");
        const QString koerper = rumpf.mid(a, e - a);
        QVERIFY2(koerper.contains(QRegularExpression(QStringLiteral(
                     "unserWert\\s*\\+\\s*1\\.0\\s*\\)\\s*\\*\\s*50\\.0"))),
                 "die Umrechnung der Balance stimmt nicht mehr: -1..+1 der "
                 "Oberflaeche gegen 0..100 der Maschine");
    }

    // ── Und der Flex-Weg verdrahtet sie wirklich ────────────────────
    //
    // Der eigentliche Fehler war nie ein fehlender Befehl, sondern
    // ein Regler ohne Ziel. Dieser Fall haelt fest, dasz jeder der
    // sieben am Signal haengt.
    void jederReglerHaengtAmSignal()
    {
        const QString src = quelle(QStringLiteral("/src/gui/MainWindow_Flex.cpp"));
        QVERIFY2(!src.isEmpty(), "MainWindow_Flex.cpp nicht lesbar");
        const QList<QPair<QString, QString>> paare{
            {QStringLiteral("SliceModel::agcModeChanged"),   QStringLiteral("sendeFlexAgcModus")},
            {QStringLiteral("SliceModel::agcThresholdChanged"), QStringLiteral("sendeFlexAgcSchwelle")},
            {QStringLiteral("SliceModel::afGainChanged"),    QStringLiteral("sendeFlexTonpegel")},
            {QStringLiteral("SliceModel::mutedChanged"),     QStringLiteral("sendeFlexStumm")},
            {QStringLiteral("SliceModel::audioPanChanged"),  QStringLiteral("sendeFlexBalance")},
            {QStringLiteral("SliceModel::ritEnabledChanged"),QStringLiteral("sendeFlexRit")},
            {QStringLiteral("SliceModel::ritHzChanged"),     QStringLiteral("sendeFlexRit")},
            {QStringLiteral("SliceModel::xitEnabledChanged"),QStringLiteral("sendeFlexXit")},
            {QStringLiteral("SliceModel::xitHzChanged"),     QStringLiteral("sendeFlexXit")},
            {QStringLiteral("SliceModel::apfEnabledChanged"),QStringLiteral("sendeFlexApf")},
        };
        for (const auto& p : paare) {
            QVERIFY2(src.contains(p.first),
                     qPrintable(p.first + QStringLiteral(
                         " haengt am Flex an nichts — wireSliceSignals laeuft "
                         "dort nicht")));
            QVERIFY2(src.contains(p.second),
                     qPrintable(p.second + QStringLiteral(
                         " wird nie gerufen — der Regler bewirkt dann nichts")));
        }
    }

    // ── Der Anmeldeschub kommt VOR der Scheibennummer ───────────────
    //
    // Gemessen am 2026-10-31: 143 ms davor. Wer in dieser Luecke
    // hinaus will, faellt still aus. Darum meldet die Maschine ihre
    // Scheibe, und der Nachtrag haengt daran.
    void derNachtragHaengtAmSignalDerMaschine()
    {
        const QString model = quelle(QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(model.contains(QStringLiteral("emit flexScheibennummerDa()")),
                 "die Maschine sagt nicht mehr Bescheid, wenn ihre Scheibe da "
                 "ist — dann geht der Anmeldestand der Regler nie hinaus");
        const QString fenster = quelle(QStringLiteral("/src/gui/MainWindow_Flex.cpp"));
        QVERIFY2(fenster.contains(QStringLiteral("RadioModel::flexScheibennummerDa")),
                 "der Flex-Weg haengt nicht mehr am Nachtrag");
    }

private:
    static QString quelle(const QString& relativ)
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { return QString(); }
        QFile f(QString::fromLocal8Bit(root) + relativ);
        if (!f.open(QIODevice::ReadOnly)) { return QString(); }
        return QString::fromUtf8(f.readAll());
    }
};

QTEST_MAIN(TstFlexScheibenregler)
#include "tst_flex_scheibenregler.moc"
