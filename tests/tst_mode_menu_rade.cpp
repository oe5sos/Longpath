// no-port-check: prueft eine NereusSDR-eigene Betriebsart (RADE) im
// Betriebsartenwaehler. Kein uebernommener Code.

// =================================================================
// tests/tst_mode_menu_rade.cpp  (NereusSDR)
// =================================================================
//
// RADE ist eine Betriebsart wie jede andere und muss im Waehler
// stehen — beide Seitenbaender, und die Wahl muss die Scheibe
// erreichen. Phase 3R L3.
//
// ── Umgezogen am 2026-08-18 ─────────────────────────────────────────
//
// Der Test prueefte den Betriebsartenwaehler der VFO-Flagge. Die ist
// ersatzlos geloescht; der Waehler steht in der RxApplet, und dort
// gab es RADE schon (RxApplet.cpp:428-435, „the canonical Thetis 11 +
// NereusSDR-native RADE-U / RADE-L from Phase 3R Task J1").
//
// Die vier Faelle zum Reiter-Schild der Flagge („Mode"-Reiter zeigt
// die aktive Betriebsart, Farbchip wird lila bei RADE) sind
// ERSATZLOS weg: dieses Schild gab es nur an der Flagge. Die aktive
// Betriebsart steht jetzt in der unteren Leiste als Pille und im
// Waehler selbst.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11 — Created for Phase 3R L3 (VFO flag mode combo).
//   2026-08-18 — Auf die RxApplet umgehaengt, nachdem die VFO-Flagge
//                 geloescht wurde. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QComboBox>

#include "core/WdspTypes.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

/// Der Betriebsartenwaehler der RxApplet — der einzige QComboBox mit
/// „LSB" darin. Nach Inhalt gesucht, nicht nach Stelle: die Stelle
/// verschiebt sich beim naechsten Umbau, der Inhalt nicht.
QComboBox* findModeCombo(QWidget* w)
{
    for (QComboBox* c : w->findChildren<QComboBox*>()) {
        for (int i = 0; i < c->count(); ++i) {
            if (c->itemText(i) == QStringLiteral("LSB")) { return c; }
        }
    }
    return nullptr;
}

QStringList itemsOf(QComboBox* c)
{
    QStringList out;
    for (int i = 0; i < c->count(); ++i) { out << c->itemText(i); }
    return out;
}

} // namespace

class TestModeMenuRade : public QObject
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

    void modeComboHasBothRadeSidebands()
    {
        Harness h = make();
        QComboBox* combo = findModeCombo(h.applet.get());
        QVERIFY2(combo, "kein Betriebsartenwaehler in der RxApplet");

        const QStringList items = itemsOf(combo);
        QVERIFY2(items.contains(QStringLiteral("RADE-U")),
                 "RADE-U fehlt im Betriebsartenwaehler (Phase 3R L3)");
        QVERIFY2(items.contains(QStringLiteral("RADE-L")),
                 "RADE-L fehlt im Betriebsartenwaehler (Phase 3R L3)");
    }

    // Die Wahl muss ankommen, nicht nur dastehen.
    void choosingRadeReachesTheSlice()
    {
        Harness h = make();
        QComboBox* combo = findModeCombo(h.applet.get());
        QVERIFY(combo);

        combo->setCurrentText(QStringLiteral("RADE-U"));
        QCOMPARE(h.slice->dspMode(), DSPMode::RADE_U);

        combo->setCurrentText(QStringLiteral("RADE-L"));
        QCOMPARE(h.slice->dspMode(), DSPMode::RADE_L);
    }

    // Und zurueck: setzt das Modell die Betriebsart, folgt der Waehler.
    void theModelDrivesTheComboBack()
    {
        Harness h = make();
        QComboBox* combo = findModeCombo(h.applet.get());
        QVERIFY(combo);

        h.slice->setDspMode(DSPMode::RADE_L);
        QCOMPARE(combo->currentText(), QStringLiteral("RADE-L"));
    }
};

QTEST_MAIN(TestModeMenuRade)
#include "tst_mode_menu_rade.moc"
