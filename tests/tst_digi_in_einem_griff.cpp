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
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;
using Longpath::DigiFrequenzen::Digimodus;

namespace {

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
};

QTEST_MAIN(TstDigiInEinemGriff)
#include "tst_digi_in_einem_griff.moc"
