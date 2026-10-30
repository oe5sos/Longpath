// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_digi_in_einem_griff.cpp  (Longpath)
// =================================================================
//
// Ein Druck auf FT8 richtet das Geraet her — oder er taugt nichts.
//
// Betreiber am 2026-10-21, mit angeschlossener Antenne vor einem
// grauen „FT8 Decoder OFF": „ich will, sobald ich auf ft8 klicke,
// dass alles automatisch funktioniert", „ich will nicht die freq
// suchen", „quasi wie jtdx".
//
// Vorher brauchte es drei Handgriffe in drei Ecken, und wer einen
// vergass, sah einen grauen Eintrag ohne Hinweis, was fehlt. Dieser
// Lauf haelt fest, dass EIN Griff genuegt — und ebenso, was er NICHT
// anfasst: ein Band ohne Anrufstelle, und jemanden, der sich
// absichtlich danebengestellt hat.
//
// Die Tafel selbst prueft tst_digi_frequenzen; hier geht es um die
// Verdrahtung, wie bei tst_ft8_flaeche.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest/QtTest>

#include "core/DigiFrequenzen.h"
#include "gui/MainWindow.h"
#include "gui/applets/Ft8Applet.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/widgets/CommandBar.h"
#include <QPushButton>
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;
using Longpath::DigiFrequenzen::Digimodus;

namespace {

/// Einen Knopf der Kommandoleiste an seiner Beschriftung finden.
///
/// NICHT das Signal von Hand senden: dann prueft der Lauf nur die
/// Haelfte hinter dem Signal, und ein fehlendes `emit` faellt nicht
/// auf. Genau so war der erste Entwurf dieses Laufs am 2026-10-28 --
/// die Gegenprobe blieb gruen, obwohl das Signal entfernt war.
QPushButton* knopf(MainWindow& mw, const QString& text)
{
    CommandBar* leiste = mw.commandBarForTest();
    if (!leiste) { return nullptr; }
    for (QPushButton* b : leiste->findChildren<QPushButton*>()) {
        if (b->text() == text) { return b; }
    }
    return nullptr;
}

/// Die aktive Scheibe des Fensters -- notfalls eine angelegt.
///
/// Ohne Funkgeraet hat ein frisch gebautes Fenster keine. Der Griff
/// soll aber genau dann auch nichts tun, und das waere hier kein
/// Befund, sondern eine Luecke: der Lauf pruefte dann gar nichts.
SliceModel* scheibe(MainWindow& mw)
{
    RadioModel* rm = mw.radioModelForTest();
    if (!rm) { return nullptr; }
    if (SliceModel* s = rm->activeSlice()) { return s; }
    const int id = rm->addSlice();
    rm->setActiveSlice(id);
    return rm->activeSlice();
}

} // namespace

class TstDigiInEinemGriff : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // ── DIGU richtet genauso her wie FT8 (2026-10-28) ───────────────
    //
    // Betreiber am Geraet: „sobald ich auf digi klicke muss das digi
    // fenster automatisch aufgehen und die frequenz dort auch
    // hinspringen". Bis dahin stellte dieser Knopf NUR die
    // Betriebsart -- man stand danach in DIGU auf der alten Frequenz,
    // ohne Dekoderfenster, und sah dem Knopf den Unterschied zu den
    // Pillen FT8/FT4 daneben nicht an.
    //
    // Geprueft wird ueber das SIGNAL der Leiste, nicht durch einen
    // Aufruf von aufDigiStellen(): die Frage ist ja gerade, ob der Weg
    // vom Knopf dorthin existiert.
    //
    // Gegenprobe (2026-10-28): ohne das `emit digimodusGewaehlt()` in
    // CommandBar::pushModeToModel bleibt die Frequenz auf 14.225 --
    // genau der Stand, den der Betreiber am Geraet hatte.
    void diguKnopfStelltAuchDieFrequenz()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY2(s != nullptr, "keine aktive Scheibe");

        CommandBar* leiste = mw.commandBarForTest();
        QVERIFY2(leiste != nullptr, "keine Kommandoleiste");

        s->setDspMode(DSPMode::USB);
        s->setFrequency(14'225'000.0);

        // Derselbe Weg, den das Ueberlaufmenue nimmt -- NICHT das
        // Signal von Hand (siehe waehleBetriebsartForTest).
        leiste->waehleBetriebsartForTest(DSPMode::DIGU);

        QCOMPARE(s->dspMode(), DSPMode::DIGU);
        QCOMPARE(qint64(s->frequency()), qint64(14'074'000));
    }

    // Wer zuletzt FT4 gefahren hat, landet mit DIGU wieder dort -- der
    // Knopf traegt keinen Modus, also muss sich das Fenster den
    // letzten merken.
    void diguNimmtDenZuletztBenutztenDigimodus()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);
        CommandBar* leiste = mw.commandBarForTest();
        QVERIFY(leiste != nullptr);
        QPushButton* ft4 = knopf(mw, QStringLiteral("FT4"));
        QVERIFY2(ft4 != nullptr, "kein FT4-Knopf");

        s->setFrequency(14'225'000.0);
        ft4->click();                       // echter Knopf, FT4 ist sichtbar
        QCOMPARE(qint64(s->frequency()), qint64(14'080'000));

        // Woanders hin, dann nur DIGU -- es muss wieder FT4 sein.
        s->setFrequency(14'225'000.0);
        leiste->waehleBetriebsartForTest(DSPMode::DIGU);
        QCOMPARE(qint64(s->frequency()), qint64(14'080'000));
    }

    void einDruckStelltModusFrequenzUndFilter()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY2(s != nullptr, "keine aktive Scheibe");

        // Ausgangslage: genau die, in der der Betreiber stand — 20 m,
        // CW, schmales Filter.
        s->setDspMode(DSPMode::CWL);
        s->setFrequency(14'075'500.0);
        s->setFilterLow(400);
        s->setFilterHigh(800);

        mw.aufDigiStellen(Digimodus::Ft8);

        QCOMPARE(s->dspMode(), DSPMode::DIGU);
        QCOMPARE(qint64(s->frequency()), qint64(14'074'000));
        QVERIFY2(s->filterHigh() - s->filterLow() >= 2500,
                 qPrintable(QStringLiteral("Durchlass nur %1 Hz — der "
                                           "Dekoder sieht 0..3000")
                                .arg(s->filterHigh() - s->filterLow())));
    }

    void ft4LandetNichtAufDerFt8Stelle()
    {
        // Der Fehler, der sonst eine halbe Stunde Suche im Dekoder
        // kostet: gleiches Band, andere Stelle.
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);
        s->setFrequency(14'075'500.0);

        mw.aufDigiStellen(Digimodus::Ft4);
        QCOMPARE(qint64(s->frequency()), qint64(14'080'000));
    }

    void werSchonRichtigStehtBleibtStehen()
    {
        // Zweimal druecken darf niemanden wegreissen, der sich
        // absichtlich ein paar hundert Hertz danebengestellt hat.
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);
        s->setFrequency(14'074'300.0);

        mw.aufDigiStellen(Digimodus::Ft8);
        QCOMPARE(qint64(s->frequency()), qint64(14'074'300));
    }

    void ohneAnrufstelleBleibtDieFrequenzStehen()
    {
        // Auf einem Band ohne Eintrag (hier: FT4 auf 60 m) wird NICHT
        // geraten. Lieber stehenbleiben als das Geraet irgendwohin
        // stellen — der Modus und das Filter stimmen trotzdem.
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);
        s->setDspMode(DSPMode::CWL);
        s->setFrequency(5'357'000.0);

        mw.aufDigiStellen(Digimodus::Ft4);

        QCOMPARE(qint64(s->frequency()), qint64(5'357'000));
        QCOMPARE(s->dspMode(), DSPMode::DIGU);
    }

    void derDekoderHoertAufDieselbeBetriebsart()
    {
        // Betreiber am 2026-10-21: „bei ft4 und ft8 muss er glaube ich
        // auch die frequenz anpassen und aendern." Es gab zwei Stellen
        // mit demselben Namen und verschiedener Wirkung: die Pillen in
        // der Leiste stellten das Geraet, die Knoepfe IM Fenster den
        // Dekoder -- und keiner tat beides.
        //
        // Ohne das stellte ein Druck auf FT4 die Skala auf 14.080 und
        // liess den Dekoder auf FT8: 15-s-Fenster auf einem Band, das
        // im 7,5-s-Takt sendet. Das Fenster haette geschwiegen, und
        // niemand haette gewusst, warum.
        MainWindow mw;
        QVERIFY(scheibe(mw) != nullptr);
        Ft8Applet* ft8 = mw.ft8AppletForTest();
        QVERIFY2(ft8 != nullptr, "kein FT8-Fenster");

        mw.aufDigiStellen(Digimodus::Ft4);
        QCOMPARE(ft8->protocol(), Ft8Decoder::Protocol::Ft4);

        mw.aufDigiStellen(Digimodus::Ft8);
        QCOMPARE(ft8->protocol(), Ft8Decoder::Protocol::Ft8);
    }

    void dasDekoderfensterWirdSichtbar()
    {
        MainWindow mw;
        QVERIFY(scheibe(mw) != nullptr);
        AppletVisibilityController* vis = mw.appletVisibilityForTest();
        QVERIFY2(vis != nullptr, "keine Fensterverwaltung");

        vis->setVisible(QStringLiteral("Ft8Decoder"), false);
        mw.aufDigiStellen(Digimodus::Ft8);
        QVERIFY2(vis->isVisible(QStringLiteral("Ft8Decoder")),
                 "das Fenster bleibt aus, obwohl FT8 gewaehlt wurde");
    }

    // ── Der Bildausschnitt (2026-10-30) ───────────────────────────
    //
    // Betreiber, mit einem auf 7,8 kHz gezoomten Panadapter, waehrend
    // der Empfaenger 32 kHz weiter auf der Anrufstelle stand:
    // „interessant ist hier nur der reine ft8 - ft4 bereich".
    void derBildausschnittDecktDenDurchlassbereich()
    {
        using namespace DigiFrequenzen;
        const auto a = bildausschnitt(7'074'000);

        // ── Die Mitte liegt NICHT auf der Anrufstelle ─────────
        //
        // Dort ist nichts. Die Skalenfrequenz ist der TRAEGER; die
        // Signale liegen darueber, im Durchlassbereich. Eine Mitte
        // auf der Anrufstelle haette die halbe Flaeche auf leeres
        // Band gelegt und die obersten Signale abgeschnitten.
        QVERIFY2(a.mitteHz > 7'074'000.0,
                 "die Mitte darf nicht auf dem Traeger liegen");
        QCOMPARE(a.mitteHz, 7'074'000.0
                            + (kDurchlassVonHz + kDurchlassBisHz) / 2.0);

        // Der ganze Durchlassbereich passt hinein, mit Luft.
        const double unten = a.mitteHz - a.breiteHz / 2.0;
        const double oben  = a.mitteHz + a.breiteHz / 2.0;
        QVERIFY(unten <= 7'074'000.0 + kDurchlassVonHz);
        QVERIFY(oben  >= 7'074'000.0 + kDurchlassBisHz);

        // Und er ist schmal genug, dass man die Signale sieht --
        // unter 10 kHz, nicht die 200 kHz der Bild-FFT.
        QVERIFY2(a.breiteHz < 10'000.0,
                 "zu breit, dann sieht man die Toene nicht mehr");
    }

    // Null heisst „ich weiss es nicht" -- dann bleibt das Bild
    // stehen, statt irgendwohin zu springen. Dieselbe Regel wie bei
    // `anruffrequenz`.
    void ohneFrequenzBleibtDasBildStehen()
    {
        const auto a = DigiFrequenzen::bildausschnitt(0);
        QCOMPARE(a.breiteHz, 0.0);
        const auto b = DigiFrequenzen::bildausschnitt(-1);
        QCOMPARE(b.breiteHz, 0.0);
    }
};

QTEST_MAIN(TstDigiInEinemGriff)
#include "tst_digi_in_einem_griff.moc"
