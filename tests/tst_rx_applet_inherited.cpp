// =================================================================
// tests/tst_rx_applet_inherited.cpp  (NereusSDR)
// =================================================================
//
// Was die RxApplet von der VFO-Flagge geerbt hat.
//
// Die Flagge faellt ersatzlos weg (Zielbild Punkt 1). Fuenf Gruppen
// lebten NUR dort. Wuerden sie beim Loeschen der Flagge still
// mitgehen, faende es niemand — es gibt keine Fehlermeldung fuer eine
// Bedienung, die es nicht mehr gibt, und die Pillen in der unteren
// Leiste sind ANZEIGEN, keine Schalter.
//
// Dieser Test ist die Sicherung dafuer. Er prueft nicht, wie die
// Knoepfe aussehen, sondern DASS ES SIE GIBT und dass sie am Modell
// haengen — in beide Richtungen.
//
// ── Warum Lautstaerke und Stumm besonders zaehlen ────────────────────
//
// Sie waren der siebte Verwaiste und wurden bei der ersten Zaehlung
// uebersehen. In RxApplet.cpp stand:
//
//   „AF gain slider removed: TitleBar master volume + VfoWidget
//    per-slice AF control are the canonical 2 surfaces."
//
// Diese Kopfleiste mit Hauptlautstaerke gibt es in NereusSDR NICHT —
// ein aus AetherSDR mitgewanderter Satz, der eine Flaeche benennt, die
// nie gebaut wurde. Ohne die Flagge haette das Programm keine
// Lautstaerke und keine Stummschaltung gehabt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>

#include <QPushButton>
#include <QSignalSpy>
#include <QApplication>
#include <QSlider>

#include "core/WdspTypes.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/DspParamPopup.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

/// Ein Knopf mit genau diesem Text.
QPushButton* button(RxApplet& a, const QString& text)
{
    for (QPushButton* b : a.findChildren<QPushButton*>()) {
        if (b && b->text() == text) { return b; }
    }
    return nullptr;
}

} // namespace

class TestRxAppletInherited : public QObject
{
    Q_OBJECT

private:
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        std::unique_ptr<RxApplet>   applet;
        SliceModel* slice{nullptr};
    };

    Harness make()
    {
        Harness h;
        h.radio = std::make_unique<RadioModel>();
        h.slice = h.radio->sliceById(0);
        if (!h.slice) {
            const int id = h.radio->addSlice();
            h.slice = h.radio->sliceById(id);
        }
        h.applet = std::make_unique<RxApplet>(h.slice, h.radio.get(), nullptr);
        return h;
    }

private slots:

    // ── Die Gruppen sind da ──────────────────────────────────────────

    void volumeAndMuteExistAtAll()
    {
        Harness h = make();
        QVERIFY2(button(*h.applet, QStringLiteral("MUTE")),
                 "ohne die Flagge gaebe es keine Stummschaltung mehr");
        QVERIFY2(button(*h.applet, QStringLiteral("BIN")),
                 "Binaural fehlt");
        // Der Lautstaerkeregler: ohne ihn hat das Programm keine
        // Lautstaerke. Erkennbar am Hinweistext, nicht an der Stelle in
        // einer Liste — die verschiebt sich beim naechsten Umbau.
        bool haveAf = false;
        for (QSlider* s : h.applet->findChildren<QSlider*>()) {
            if (s && s->toolTip().contains(QStringLiteral("audio level"))) {
                haveAf = true;
            }
        }
        QVERIFY2(haveAf, "kein Lautstaerkeregler in der RxApplet");
    }

    void allSevenNoiseReductionsExist()
    {
        Harness h = make();
        for (const QString& t : {QStringLiteral("NR1"), QStringLiteral("NR2"),
                                 QStringLiteral("NR3"), QStringLiteral("NR4"),
                                 QStringLiteral("DFNR"), QStringLiteral("BNR"),
                                 QStringLiteral("MNR")}) {
            QVERIFY2(button(*h.applet, t),
                     qPrintable(QStringLiteral("Rauschminderung %1 fehlt").arg(t)));
        }
        QVERIFY(button(*h.applet, QStringLiteral("ANF")));
    }

    void theBlankerFamilyExists()
    {
        Harness h = make();
        QVERIFY2(button(*h.applet, QStringLiteral("NB")),
                 "der Stoeraustaster fehlt — er stand bis 2026-04-22 "
                 "ausdruecklich NICHT hier, und die Begruendung dafuer "
                 "setzte die Flagge voraus");
        QVERIFY(button(*h.applet, QStringLiteral("SNB")));
        QVERIFY(button(*h.applet, QStringLiteral("APF")));
    }

    // ── Und sie haengen am Modell ────────────────────────────────────

    void muteReachesTheSlice()
    {
        Harness h = make();
        QPushButton* b = button(*h.applet, QStringLiteral("MUTE"));
        QVERIFY(b);
        QVERIFY(!h.slice->muted());
        b->click();
        QVERIFY2(h.slice->muted(),
                 "der Knopf ist da, aber er schaltet nichts");
        b->click();
        QVERIFY(!h.slice->muted());
    }

    void binauralReachesTheSlice()
    {
        Harness h = make();
        QPushButton* b = button(*h.applet, QStringLiteral("BIN"));
        QVERIFY(b);
        const bool before = h.slice->binauralEnabled();
        b->click();
        QCOMPARE(h.slice->binauralEnabled(), !before);
    }

    // Genau EINE laeuft, oder keine. Ein zweiter Klick auf die
    // laufende schaltet sie ab — das ist der einzige Weg zu „keine",
    // ohne einen achten Knopf zu bauen.
    void theNoiseReductionsAreMutuallyExclusive()
    {
        Harness h = make();
        QPushButton* nr1 = button(*h.applet, QStringLiteral("NR1"));
        QPushButton* nr2 = button(*h.applet, QStringLiteral("NR2"));
        QVERIFY(nr1 && nr2);

        nr1->click();
        QCOMPARE(h.slice->activeNr(), NrSlot::NR1);

        nr2->click();
        QCOMPARE(h.slice->activeNr(), NrSlot::NR2);
        QVERIFY2(!nr1->isChecked(),
                 "zwei Rauschminderungen stehen gleichzeitig an");

        nr2->click();
        QCOMPARE(h.slice->activeNr(), NrSlot::Off);
    }

    // Dreistufig, nicht an/aus. Die Beschriftung sagt, was LAEUFT.
    void theBlankerCyclesThroughThreeStates()
    {
        Harness h = make();
        QPushButton* nb = button(*h.applet, QStringLiteral("NB"));
        QVERIFY(nb);
        QCOMPARE(h.slice->nbMode(), NbMode::Off);

        nb->click();
        QCOMPARE(h.slice->nbMode(), NbMode::NB);
        nb->click();
        QCOMPARE(h.slice->nbMode(), NbMode::NB2);
        nb->click();
        QCOMPARE(h.slice->nbMode(), NbMode::Off);
    }

    void snbAndApfReachTheSlice()
    {
        Harness h = make();
        QPushButton* snb = button(*h.applet, QStringLiteral("SNB"));
        QPushButton* apf = button(*h.applet, QStringLiteral("APF"));
        QVERIFY(snb && apf);

        snb->click();
        QVERIFY(h.slice->snbEnabled());
        apf->click();
        QVERIFY(h.slice->apfEnabled());
    }

    // ── Der Schnellregler-Rechtsklick ────────────────────────────────
    //
    // Er ist die zweite Haelfte des Erbes: der Knopf schaltet, der
    // Rechtsklick fuehrt zu den Einstellungen. Ohne ihn waeren die
    // Regler nur noch ueber das Setup-Menue erreichbar.

    // ── DER FUND vom zeilenweisen Abgleich ───────────────────────────
    //
    // Der erste Umzug legte den Rechtsklick direkt auf die
    // Einstellungsseite. Auf der Flagge oeffnet er aber die drei bis
    // fuenf Regler DIESER Rauschminderung (DspParamPopup); die
    // Einstellungsseite ist darin nur der Verweis „More Settings…"
    // ganz unten. 28 der 30 SliceModel-Setzer, die die Flagge
    // ueberhaupt bediente, sind genau diese Regler — mit der Flagge
    // waeren sie unbedienbar geworden.
    void rightClickOnANoiseReductionOpensItsQuickControls()
    {
        Harness h = make();
        QPushButton* nr1 = button(*h.applet, QStringLiteral("NR1"));
        QVERIFY(nr1);

        emit nr1->customContextMenuRequested(QPoint(2, 2));

        // Der Schnellregler ist ein eigenes Fenster mit Schiebern.
        // Gesucht wird das Vorhandensein, nicht das Aussehen.
        DspParamPopup* popup = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* p = qobject_cast<DspParamPopup*>(w)) { popup = p; }
        }
        if (!popup) {
            popup = h.applet->findChild<DspParamPopup*>();
        }
        QVERIFY2(popup,
                 "Rechtsklick oeffnet keinen Schnellregler — dann sind "
                 "die NR-Parameter nach dem Loeschen der Flagge nur noch "
                 "ueber das Setup-Menue erreichbar");
        QVERIFY2(!popup->findChildren<QSlider*>().isEmpty(),
                 "der Schnellregler hat keine Regler");
        popup->hide();
        popup->deleteLater();
    }

    // Und der Verweis darin fuehrt weiter auf die volle Seite — das
    // war der Teil, den der erste Umzug als das Ganze genommen hatte.
    void theQuickControlsStillLinkToTheFullSetupPage()
    {
        Harness h = make();
        QSignalSpy spy(h.applet.get(), &RxApplet::openNrSetupRequested);

        QPushButton* nr3 = button(*h.applet, QStringLiteral("NR3"));
        QVERIFY(nr3);
        emit nr3->customContextMenuRequested(QPoint(2, 2));

        DspParamPopup* popup = h.applet->findChild<DspParamPopup*>();
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* p = qobject_cast<DspParamPopup*>(w)) { popup = p; }
        }
        QVERIFY(popup);
        QPushButton* more = nullptr;
        for (QPushButton* b : popup->findChildren<QPushButton*>()) {
            if (b && b->text().contains(QStringLiteral("More"))) { more = b; }
        }
        QVERIFY2(more, "kein Verweis auf die volle Einstellungsseite");
        more->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<NrSlot>(), NrSlot::NR3);
    }

    void rightClickOnTheBlankerAsksForItsSetupPage()
    {
        Harness h = make();
        QPushButton* nb = button(*h.applet, QStringLiteral("NB"));
        QVERIFY(nb);

        QSignalSpy spy(h.applet.get(), &RxApplet::openNbSetupRequested);
        emit nb->customContextMenuRequested(QPoint(2, 2));
        QCOMPARE(spy.count(), 1);
    }

    // ── Und zurueck: das Modell fuehrt die Knoepfe nach ──────────────

    void theModelDrivesTheButtonsBack()
    {
        Harness h = make();
        h.slice->setActiveNr(NrSlot::DFNR);
        h.slice->setMuted(true);
        h.applet->syncFromModel();

        QPushButton* dfnr = button(*h.applet, QStringLiteral("DFNR"));
        QPushButton* mute = button(*h.applet, QStringLiteral("MUTE"));
        QVERIFY(dfnr && mute);
        QVERIFY2(dfnr->isChecked(),
                 "das Modell sagt DFNR, der Knopf steht auf aus");
        QVERIFY(mute->isChecked());
    }
};

QTEST_MAIN(TestRxAppletInherited)
#include "tst_rx_applet_inherited.moc"
