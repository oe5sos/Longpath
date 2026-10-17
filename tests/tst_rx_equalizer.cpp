// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Rueckfallpruefstand (das Verhalten stammt
// aus Thetis und ist im gepruefen Code zitiert).
//
// Empfangs-Equalizer (2026-10-11). Longpath hatte ihn nicht: WDSPs eqp laeuft
// im RXA-Durchlauf (RXA.c:673), aber kein SetRXAEQ* war in wdsp_api.h
// deklariert, und das EqApplet zeigte zehn Schieber, einen ON-Knopf und eine
// RX/TX-Umschaltung, die alle sechs mit NyiOverlay markiert waren.
//
// Geprueft wird die ganze Kette und jede Stelle, an der sie bisher riss:
//   - der Zustand klammert selbst (-12..+15, Thetis eqform.cs:1871-1872);
//   - ein Index neben die zehn Baender aendert nichts und schweigt nicht;
//   - rxEqGraph10() hat die Form, die SetRXAGrphEQ10 erwartet;
//   - die Einstellungen ueberleben einen Rundlauf und sind slice-weit,
//     nicht per Band;
//   - der WDSP-Kanal nimmt die Kurve an und meldet den run-Zustand zurueck;
//   - das EqApplet steuert BEIDE Seiten und folgt dem Modell;
//   - die Vorwahl "Sprache" setzt genau WDSPs belegte Kurve.

#include <QtTest/QtTest>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <array>

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "gui/MainWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/EqApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

namespace {

QPushButton* knopf(QWidget& w, const QString& text)
{
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b && b->text() == text) { return b; }
    }
    return nullptr;
}

// Die zehn Bandschieber in Aufbaureihenfolge. Die dB-Skala links und rechts
// besteht aus QLabel, nicht aus Schiebern, also sind das genau die zehn.
QList<QSlider*> schieber(QWidget& w)
{
    QList<QSlider*> out;
    for (QSlider* s : w.findChildren<QSlider*>()) {
        if (s && s->orientation() == Qt::Vertical) { out.append(s); }
    }
    return out;
}

} // namespace

class TstRxEqualizer : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // ── Zustand ─────────────────────────────────────────────────────────────

    void derAnfangszustandIstAusUndFlach()
    {
        // WDSP legt eqp aus und flach an (RXA.c:259-275). Das Modell muss
        // dasselbe sagen, sonst steht beim Verbinden eine Kurve im Feld, die
        // im DSP nicht gilt.
        SliceModel s(0);
        QVERIFY(!s.rxEqEnabled());
        QCOMPARE(s.rxEqPreampDb(), 0);
        QCOMPARE(s.rxEqNumBands(), 10);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.rxEqBandDb(i), 0);
        }
        // Die Mitten sind WDSPs feste (RXA.c:260, default_F ohne das Polster).
        const std::array<int, 10> erwartet{{32, 63, 125, 250, 500,
                                            1000, 2000, 4000, 8000, 16000}};
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.rxEqFreqHz(i), erwartet[static_cast<std::size_t>(i)]);
        }
    }

    void derZustandKlammertSelbst()
    {
        // Geklammert wird im Modell, nicht erst in der Oberflaeche: ein Wert
        // aus AppSettings oder aus einem Frequenzspeicher hat kein Feld
        // davor, das ihn abfaengt.
        // Grenzen aus Thetis eqform.cs:1871-1872 / :1946-1947 [v2.10.3.15].
        SliceModel s(0);
        s.setRxEqBandDb(0, 99);
        QCOMPARE(s.rxEqBandDb(0), 15);
        s.setRxEqBandDb(0, -99);
        QCOMPARE(s.rxEqBandDb(0), -12);
        s.setRxEqPreampDb(99);
        QCOMPARE(s.rxEqPreampDb(), 15);
        s.setRxEqPreampDb(-99);
        QCOMPARE(s.rxEqPreampDb(), -12);
    }

    void einIndexNebenDieBaenderAendertNichts()
    {
        SliceModel s(0);
        QSignalSpy spy(&s, &SliceModel::rxEqBandDbChanged);
        s.setRxEqBandDb(-1, 10);
        s.setRxEqBandDb(10, 10);
        s.setRxEqBandDb(999, 10);
        QCOMPARE(spy.count(), 0);
        // Und das Lesen daneben gibt 0, nicht Speichermuell.
        QCOMPARE(s.rxEqBandDb(-1), 0);
        QCOMPARE(s.rxEqBandDb(10), 0);
        QCOMPARE(s.rxEqFreqHz(-1), 0);
        QCOMPARE(s.rxEqFreqHz(10), 0);
    }

    void dasSignalFeuertNurBeiEchterAenderung()
    {
        SliceModel s(0);
        QSignalSpy spy(&s, &SliceModel::rxEqBandDbChanged);
        s.setRxEqBandDb(3, 5);
        QCOMPARE(spy.count(), 1);
        s.setRxEqBandDb(3, 5);       // derselbe Wert
        QCOMPARE(spy.count(), 1);
        // Geklammert auf denselben Wert ist auch keine Aenderung.
        s.setRxEqBandDb(3, 5);
        QCOMPARE(spy.count(), 1);
    }

    void dieKurveHatDieFormDieWdspErwartet()
    {
        // SetRXAGrphEQ10 nimmt int[11]: [0] Vorverstaerkung, [1..10] Baender
        // (eq.c:385-411). Eine Verschiebung um eins waere im DSP nicht zu
        // sehen, nur zu hoeren -- darum hier festgenagelt.
        SliceModel s(0);
        s.setRxEqPreampDb(-3);
        for (int i = 0; i < 10; ++i) {
            s.setRxEqBandDb(i, i - 5);   // -5..+4, jedes Band verschieden
        }
        const std::array<int, 11> k = s.rxEqGraph10();
        QCOMPARE(k[0], -3);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(k[static_cast<std::size_t>(i) + 1], i - 5);
        }
    }

    // ── Persistenz ──────────────────────────────────────────────────────────

    void dieEinstellungUeberlebtDenRundlaufUndGiltSliceWeit()
    {
        auto& st = AppSettings::instance();
        {
            SliceModel s(0);
            s.setRxEqEnabled(true);
            s.setRxEqPreampDb(-6);
            s.setRxEqBandDb(0, -12);
            s.setRxEqBandDb(9, 15);
            s.setRxEqFreqHz(5, 1500);
            s.saveToSettings(Band::Band20m);
        }
        {
            SliceModel s(0);
            s.restoreFromSettings(Band::Band20m);
            QVERIFY(s.rxEqEnabled());
            QCOMPARE(s.rxEqPreampDb(), -6);
            QCOMPARE(s.rxEqBandDb(0), -12);
            QCOMPARE(s.rxEqBandDb(9), 15);
            QCOMPARE(s.rxEqFreqHz(5), 1500);
        }
        {
            // Slice-weit, nicht per Band: eine Klangeinstellung soll beim
            // Bandwechsel nicht umspringen. Auf einem ANDEREN Band muss
            // dasselbe herauskommen.
            SliceModel s(0);
            s.restoreFromSettings(Band::Band40m);
            QVERIFY(s.rxEqEnabled());
            QCOMPARE(s.rxEqPreampDb(), -6);
            QCOMPARE(s.rxEqBandDb(9), 15);
        }
        st.remove(QStringLiteral("Slice0/RxEqEnabled"));
    }

    void eineVerdorbeneEinstellungKommtGeklammertAn()
    {
        // Gegenprobe zum Klammern: von Hand +40 dB in die Datei schreiben.
        // Ohne das Klammern IM SETTER stuende das so im WDSP.
        auto& st = AppSettings::instance();
        SliceModel schreib(0);
        schreib.saveToSettings(Band::Band20m);
        st.setValue(QStringLiteral("Slice0/RxEqBand1Db"), 40);
        st.setValue(QStringLiteral("Slice0/RxEqPreampDb"), -40);

        SliceModel s(0);
        s.restoreFromSettings(Band::Band20m);
        QCOMPARE(s.rxEqBandDb(0), 15);
        QCOMPARE(s.rxEqPreampDb(), -12);
    }

    // ── WDSP-Kanal ──────────────────────────────────────────────────────────

    void derKanalMerktSichKurveUndSchalter()
    {
        // Ohne offenen WDSP-Kanal (kTestChannel-Lage) darf nichts
        // hinausgehen, der Mitschrieb aber stimmen.
        RxChannel ch(0, 64, 48000);
        QVERIFY(!ch.rxEqRunningForTest());
        ch.setRxEqRunning(true);
        QVERIFY(ch.rxEqRunningForTest());
        ch.setRxEqRunning(false);
        QVERIFY(!ch.rxEqRunningForTest());

        std::array<int, 11> k{{-2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
        ch.setRxEqGraph10(k);   // darf nicht abstuerzen, auch ohne Kanal
    }

    void einKanalAusserhalbDerWdspReichweiteWirdNichtAngefasst()
    {
        // Pruefstaende in diesem Baum bauen RxChannel mit Kanal 99; ein
        // WDSP-Ruf darauf schreibt ueber das Feldende hinaus. Derselbe
        // Schutz, den NbFamily nach Linux-CI #238 schon traegt.
        RxChannel ch(99, 64, 48000);
        ch.setRxEqRunning(true);
        ch.setRxEqNc(4096);
        ch.setRxEqMp(true);
        ch.setRxEqCtfmode(1);
        ch.setRxEqWintype(1);
        std::array<int, 11> k{};
        ch.setRxEqGraph10(k);
        // Kein Absturz, und die Abfrage sagt ehrlich "weiss ich nicht".
        QCOMPARE(ch.rxEqRunningInWdsp(), -1);
    }

    void eineKurveFalscherLaengeWirdVerworfen()
    {
        RxChannel ch(0, 64, 48000);
        // setRxEqProfile prueft die Groessen VOR dem WDSP-Ruf -- sonst liest
        // WDSP ueber das Ende der Vektoren hinaus.
        ch.setRxEqProfile({1.0, 2.0}, std::vector<double>(11, 0.0));
        ch.setRxEqProfile(std::vector<double>(10, 100.0), {0.0});
        // Mit den richtigen Groessen geht er durch.
        ch.setRxEqProfile(std::vector<double>(10, 1000.0), std::vector<double>(11, 0.0));
    }

    // ── Verdrahtung Modell -> Kanal ─────────────────────────────────────────

    void derSliceErreichtDenKanal()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);
        // Ohne WDSP-Kanal darf das Umstellen nicht abstuerzen -- die
        // Verdrahtung laeuft durch rxChannel(), das hier null gibt.
        s->setRxEqEnabled(true);
        s->setRxEqBandDb(4, 8);
        s->setRxEqPreampDb(-4);
        s->setRxEqFreqHz(4, 700);
        QVERIFY(s->rxEqEnabled());
        QCOMPARE(s->rxEqBandDb(4), 8);
    }

    // ── EqApplet ────────────────────────────────────────────────────────────

    void dasFeldStartetAufDerSendeseiteUndSchaltetUm()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.addSlice();
        EqApplet feld(&model);
        // TX war im Aufbau vorbelegt (m_txBtn->setChecked(true)).
        QCOMPARE(feld.seiteFuerTest(), EqApplet::Seite::Senden);

        QPushButton* rx = knopf(feld, QStringLiteral("RX"));
        QPushButton* tx = knopf(feld, QStringLiteral("TX"));
        QVERIFY(rx);
        QVERIFY(tx);
        rx->click();
        QCOMPARE(feld.seiteFuerTest(), EqApplet::Seite::Empfang);
        QVERIFY(rx->isChecked());
        QVERIFY(!tx->isChecked());
        // Ein zweiter Klick auf den schon gewaehlten Knopf darf nicht beide
        // Seiten dunkel lassen -- die Knoepfe sind checkable und in keiner
        // QButtonGroup.
        rx->click();
        QCOMPARE(feld.seiteFuerTest(), EqApplet::Seite::Empfang);
        QVERIFY(rx->isChecked());
    }

    void einSchieberAufDerEmpfangsseiteStelltDenSlice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);
        const int vorher = model.transmitModel().txEqBand(2);
        EqApplet feld(&model);
        knopf(feld, QStringLiteral("RX"))->click();

        QList<QSlider*> sl = schieber(feld);
        QCOMPARE(sl.size(), 10);
        sl[2]->setValue(7);
        QCOMPARE(s->rxEqBandDb(2), 7);
        sl[9]->setValue(-9);
        QCOMPARE(s->rxEqBandDb(9), -9);
        // Und die Sendeseite blieb unangetastet. NICHT gegen 0 geprueft:
        // der Sende-Equalizer startet mit WDSPs Sprachkurve (TXA.c:113 --
        // dort ist die vorentzerrte Kurve die AKTIVE und die flache
        // auskommentiert, auf der Empfangsseite RXA.c:261 genau umgekehrt).
        // Gegen 0 geprueft faellt der Fall, obwohl nichts kaputt ist --
        // beim ersten Lauf genau so passiert.
        QCOMPARE(model.transmitModel().txEqBand(2), vorher);
    }

    void einSchieberAufDerSendeseiteStelltDenSender()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);
        EqApplet feld(&model);   // startet auf Senden

        QList<QSlider*> sl = schieber(feld);
        sl[1]->setValue(6);
        QCOMPARE(model.transmitModel().txEqBand(1), 6);
        // Und die Empfangsseite blieb unangetastet -- hier IST 0 richtig,
        // der Empfangs-Equalizer startet flach (RXA.c:261).
        QCOMPARE(s->rxEqBandDb(1), 0);
    }

    void dieBeidenSeitenStartenVerschiedenUndDasIstBelegt()
    {
        // WDSP legt die zwei Equalizer ABSICHTLICH anders an, und beide
        // Fassungen stehen im Quelltext direkt uebereinander:
        //   TXA.c:113-114 -- vorentzerrte Kurve AKTIV, flache auskommentiert
        //   RXA.c:260-261 -- flache AKTIV, vorentzerrte auskommentiert
        // Der Sendeweg kommt also vorentzerrt, der Empfangsweg neutral.
        // Festgenagelt, damit niemand die eine Seite "zur anderen passend"
        // macht und dabei den Sendeton verstellt.
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);

        const std::array<int, 10> txVorgabe{
            {-12, -12, -12, -1, 1, 4, 9, 12, -10, -10}};
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(model.transmitModel().txEqBand(i),
                     txVorgabe[static_cast<std::size_t>(i)]);
            QCOMPARE(s->rxEqBandDb(i), 0);
        }
    }

    void derOnKnopfTrifftDieGewaehlteSeite()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);
        EqApplet feld(&model);
        QPushButton* on = knopf(feld, QStringLiteral("ON"));
        QVERIFY(on);

        on->click();                       // Senden
        QVERIFY(model.transmitModel().txEqEnabled());
        QVERIFY(!s->rxEqEnabled());

        knopf(feld, QStringLiteral("RX"))->click();
        // Der Knopf muss jetzt den Empfangszustand zeigen (aus), nicht den
        // Sendezustand stehen lassen -- sonst leuchtet er, waehrend der
        // Empfangs-Equalizer aus ist.
        QVERIFY(!on->isChecked());
        on->click();
        QVERIFY(s->rxEqEnabled());
    }

    void dasFeldFolgtDemModell()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);
        EqApplet feld(&model);
        knopf(feld, QStringLiteral("RX"))->click();

        // Von aussen am Modell gedreht (Setup-Seite, Frequenzspeicher, TCI):
        // die Schieber muessen nachkommen.
        s->setRxEqBandDb(5, -8);
        QCOMPARE(schieber(feld)[5]->value(), -8);
        s->setRxEqEnabled(true);
        QVERIFY(knopf(feld, QStringLiteral("ON"))->isChecked());
    }

    void dieVorwahlSpracheSetztWdspsBelegteKurve()
    {
        // Die Kurve steht in WDSPs Quelltext (RXA.c:260, auskommentiert):
        //   {0.0, -12.0, -12.0, -12.0, -1.0, +1.0, +4.0, +9.0, +12.0, -10.0, -10.0}
        // Nicht erfunden -- das war der Grund, "Music" ganz zu streichen.
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);
        EqApplet feld(&model);
        knopf(feld, QStringLiteral("RX"))->click();

        auto* combo = feld.findChild<QComboBox*>();
        QVERIFY(combo);
        QCOMPARE(combo->count(), 3);   // Flach / Sprache / Eigen, kein "Music"
        combo->setCurrentIndex(1);     // Sprache

        const std::array<int, 11> erwartet{
            {0, -12, -12, -12, -1, 1, 4, 9, 12, -10, -10}};
        QCOMPARE(s->rxEqPreampDb(), erwartet[0]);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s->rxEqBandDb(i), erwartet[static_cast<std::size_t>(i) + 1]);
        }
    }

    void einHandzugSetztDieVorwahlAufEigen()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.addSlice();
        EqApplet feld(&model);
        knopf(feld, QStringLiteral("RX"))->click();
        auto* combo = feld.findChild<QComboBox*>();
        QVERIFY(combo);
        combo->setCurrentIndex(0);         // Flach
        schieber(feld)[3]->setValue(4);    // von Hand gezogen
        // Sonst behauptete die Combo weiter "Flach", waehrend die Kurve es
        // nicht mehr ist.
        QCOMPARE(combo->currentIndex(), 2);
    }

    void resetGehtZurueckAufFlach()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        SliceModel* s = model.sliceById(id);
        QVERIFY(s);
        EqApplet feld(&model);
        knopf(feld, QStringLiteral("RX"))->click();
        auto* combo = feld.findChild<QComboBox*>();
        combo->setCurrentIndex(1);         // Sprache
        QCOMPARE(s->rxEqBandDb(0), -12);

        // Der Reset-Knopf hat keinen Text, nur ein gemaltes Zeichen -- er ist
        // der 22x22 grosse in der Knopfzeile.
        QPushButton* reset = nullptr;
        for (QPushButton* b : feld.findChildren<QPushButton*>()) {
            if (b && b->text().isEmpty() && b->width() <= 24) { reset = b; break; }
        }
        QVERIFY(reset);
        reset->click();
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s->rxEqBandDb(i), 0);
        }
        QCOMPARE(s->rxEqPreampDb(), 0);
        QCOMPARE(combo->currentIndex(), 0);
    }

    // ── Erreichbarkeit ──────────────────────────────────────────────────────

    void dasFeldHaengtWirklichImEchtenFenster()
    {
        // Der entscheidende Fall, und der einzige, der ein ECHTES
        // MainWindow braucht. tst_every_applet_is_reachable vergleicht die
        // beiden Anmeldelisten gegeneinander und ist gruen, sobald die
        // Namen zusammenpassen -- ob das Applet im Panel haengt, sagt es
        // nicht. Genau diese Luecke hat am 2026-08-23 dreimal zugeschlagen
        // ("wo ist die applet mitschrift"): gebaut, eingehaengt, unsichtbar.
        //
        // Beim ersten Versuch, das hier am laufenden Programm nachzusehen,
        // hat die Textsuche im Widget-Baum NICHTS gefunden -- auch nicht
        // die Applets, die seit Monaten haengen. Die Abwesenheit war also
        // keine Aussage, sondern ein untaugliches Messmittel. Dieser Fall
        // misst stattdessen am Objekt.
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 20000));

        const QList<EqApplet*> gefunden = mw->findChildren<EqApplet*>();
        QCOMPARE(gefunden.size(), 1);
        QVERIFY(gefunden.first() != nullptr);
        QCOMPARE(gefunden.first()->appletId(), QStringLiteral("Eq"));
        QCOMPARE(gefunden.first()->appletTitle(), QStringLiteral("Equalizer"));

        // Und es steht in der Liste des PANELS.
        //
        // WAS DIESER FALL NICHT BEWACHT -- nachgemessen, nicht vermutet:
        // die Gegenprobe hat BEIDE Anmeldewege einzeln entfernt
        // (`panel->addApplet(m_eqApplet)` und den Eintrag in
        // m_appletsById), und dieser Fall blieb jedes Mal gruen. Das
        // Applet kommt auf dem jeweils anderen Weg trotzdem ins Panel
        // (MainWindow.cpp:1346/1523/1810 docken aus m_appletsById an).
        //
        // Den Waechter dafuer gibt es schon: tests/tst_every_applet_is_
        // reachable.cpp vergleicht die beiden Anmeldelisten gegeneinander
        // und FAELLT bei entferntem m_appletsById-Eintrag mit genau der
        // richtigen Meldung -- "Diese Applets sind gebaut, aber ueber den
        // Auswaehler nicht erreichbar: Equalizer (EQ)". Gegengeprobt am
        // 2026-10-11.
        //
        // Dieser Fall hier belegt also nur: es gibt genau EIN Equalizer-
        // Feld, es traegt Kennung und Titel, und es steht im Panel. Das
        // ist weniger, als die erste Fassung behauptet hat.
        auto* panel = mw->findChild<AppletPanelWidget*>();
        QVERIFY(panel);
        QVERIFY(panel->applets().contains(gefunden.first()));

        delete mw;
    }
};

QTEST_MAIN(TstRxEqualizer)
#include "tst_rx_equalizer.moc"
