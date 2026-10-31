// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_band_behaelt_digi.cpp  (Longpath)
// =================================================================
//
// Ein Bandwechsel mitten im FT8-Betrieb darf den Dekoder nicht
// wegnehmen.
//
// Betreiber am 2026-10-31, mit laufendem FT8 auf 40 m: „habe von 40
// auf 20 meter gewechselt und der decoder ist verschwunden!"
//
// Die Ursache lag nicht im Dekoder. `RadioModel::onBandButtonClicked`
// holt fuer ein schon besuchtes Band Frequenz UND Betriebsart aus dem
// Bandgedaechtnis zurueck. Auf 20 m lagen dort 14.294700 MHz in USB,
// von einem Sprechabend -- damit fiel die Betriebsart aus DIGU
// heraus, und das Verfuegbarkeitstor in `rebindRttyRadeAvailability`
// (DIGU/DIGL, sonst nichts) nahm das Fenster weg. Beide Stellen taten
// genau das Richtige; zusammen ergaben sie das Falsche.
//
// Der Lauf haelt beides fest: dass Digi den Wechsel ueberlebt -- und
// dass der Griff NICHT zugreift, wenn gar kein Digi lief. Das zweite
// ist der wichtigere Teil: ein Bandwechsel in SSB muss das
// Bandgedaechtnis unangetastet lassen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest/QtTest>

#include "core/DigiFrequenzen.h"
#include "gui/MainWindow.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/widgets/CommandBar.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QPushButton>

using namespace Longpath;
using Longpath::DigiFrequenzen::Digimodus;

namespace {

SliceModel* scheibe(MainWindow& mw)
{
    RadioModel* rm = mw.radioModelForTest();
    if (!rm) { return nullptr; }
    if (SliceModel* s = rm->activeSlice()) { return s; }
    const int id = rm->addSlice();
    rm->setActiveSlice(id);
    return rm->activeSlice();
}

/// Einen Knopf der Kommandoleiste an seiner Beschriftung finden.
///
/// Ueber den echten Knopf, nicht ueber `bandUmschalten()` von Hand:
/// die Frage ist ja gerade, ob der Weg von der Leiste dorthin
/// existiert. Genau diese Luecke hatte der erste Entwurf von
/// tst_digi_in_einem_griff am 2026-10-28.
QPushButton* knopf(MainWindow& mw, const QString& text)
{
    CommandBar* leiste = mw.commandBarForTest();
    if (!leiste) { return nullptr; }
    for (QPushButton* b : leiste->findChildren<QPushButton*>()) {
        if (b->text() == text) { return b; }
    }
    return nullptr;
}

/// Ein Band so hinterlegen, wie es ein frueherer Besuch tut.
void legeBandAb(SliceModel* s, Band band, double hz, DSPMode modus)
{
    const double zurueckHz    = s->frequency();
    const DSPMode zurueckModus = s->dspMode();
    s->setFrequency(hz);
    s->setDspMode(modus);
    s->saveToSettings(band);
    s->setFrequency(zurueckHz);
    s->setDspMode(zurueckModus);
}

} // namespace

class TstBandBehaeltDigi : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // ── Der Befund vom 2026-10-31 ──────────────────────────────────
    //
    // Gegenprobe: mit `m_radioModel->onBandButtonClicked(band)` statt
    // `bandUmschalten(band)` in der Leisten-Verdrahtung steht hier
    // USB auf 14.294700 -- genau das, was der Betreiber sah.
    void bandwechselImFt8BetriebBehaeltDigi()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY2(s != nullptr, "keine aktive Scheibe");

        // 20 m war schon einmal dran, in SSB. Das ist die Falle.
        legeBandAb(s, Band::Band20m, 14'294'700.0, DSPMode::USB);

        // Und jetzt laeuft FT8 auf 40 m.
        mw.aufDigiStellen(Digimodus::Ft8);
        s->setFrequency(7'074'000.0);
        s->setDspMode(DSPMode::DIGU);

        QPushButton* b20 = knopf(mw, QStringLiteral("20m"));
        QVERIFY2(b20 != nullptr, "kein 20m-Knopf in der Leiste");
        b20->click();

        QCOMPARE(s->dspMode(), DSPMode::DIGU);
        QCOMPARE(qint64(s->frequency()), qint64(14'074'000));
    }

    // Wer zuletzt FT4 fuhr, landet auf der FT4-Stelle des neuen
    // Bandes -- nicht auf der von FT8.
    void ft4UeberlebtDenBandwechselAlsFt4()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);

        legeBandAb(s, Band::Band20m, 14'294'700.0, DSPMode::USB);

        s->setFrequency(7'047'500.0);
        mw.aufDigiStellen(Digimodus::Ft4);
        QCOMPARE(qint64(s->frequency()), qint64(7'047'500));

        QPushButton* b20 = knopf(mw, QStringLiteral("20m"));
        QVERIFY(b20 != nullptr);
        b20->click();

        QCOMPARE(s->dspMode(), DSPMode::DIGU);
        QCOMPARE(qint64(s->frequency()), qint64(14'080'000));
    }

    // ── Und das Gegenstueck: ohne Digi wird NICHTS angefasst ────────
    //
    // Der wichtigere der beiden Faelle. Wer in SSB das Band wechselt,
    // muss seinen letzten Sprechkanal wiederbekommen -- ein Griff,
    // der hier zupackte, waere schlimmer als der Befund.
    void bandwechselInSsbRuehrtDasBandgedaechtnisNichtAn()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);

        legeBandAb(s, Band::Band20m, 14'294'700.0, DSPMode::USB);

        s->setFrequency(7'120'000.0);
        s->setDspMode(DSPMode::LSB);

        QPushButton* b20 = knopf(mw, QStringLiteral("20m"));
        QVERIFY(b20 != nullptr);
        b20->click();

        QCOMPARE(s->dspMode(), DSPMode::USB);
        QCOMPARE(qint64(s->frequency()), qint64(14'294'700));
    }

    // DIGU allein genuegt nicht. Wer in DIGU irgendwo weitab von
    // jeder Anrufstelle sitzt und das Dekoderfenster zu hat, betreibt
    // kein FT8 -- dem wird auch nichts hingestellt.
    void diguOhneAnrufstelleUndOhneFensterGiltNichtAlsDigi()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);

        legeBandAb(s, Band::Band20m, 14'294'700.0, DSPMode::USB);

        AppletVisibilityController* vis = mw.appletVisibilityForTest();
        QVERIFY2(vis != nullptr, "kein Sichtbarkeitsregler");
        vis->setVisible(QStringLiteral("Ft8Decoder"), false);

        s->setFrequency(7'150'000.0);     // keine Anrufstelle weit und breit
        s->setDspMode(DSPMode::DIGU);

        QPushButton* b20 = knopf(mw, QStringLiteral("20m"));
        QVERIFY(b20 != nullptr);
        b20->click();

        QCOMPARE(s->dspMode(), DSPMode::USB);
        QCOMPARE(qint64(s->frequency()), qint64(14'294'700));
    }

    // Eine gesperrte Scheibe bleibt gesperrt. `onBandButtonClicked`
    // steigt dann aus -- und der Griff darf nicht hinterher doch noch
    // stellen, was der Bandwechsel gerade nicht durfte.
    void gesperrteScheibeBleibtStehen()
    {
        MainWindow mw;
        SliceModel* s = scheibe(mw);
        QVERIFY(s != nullptr);

        legeBandAb(s, Band::Band20m, 14'294'700.0, DSPMode::USB);

        s->setFrequency(7'074'000.0);
        s->setDspMode(DSPMode::DIGU);
        s->setLocked(true);

        QPushButton* b20 = knopf(mw, QStringLiteral("20m"));
        QVERIFY(b20 != nullptr);
        b20->click();

        QCOMPARE(qint64(s->frequency()), qint64(7'074'000));
        s->setLocked(false);
    }
};

QTEST_MAIN(TstBandBehaeltDigi)
#include "tst_band_behaelt_digi.moc"
