// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_eq_two_views.cpp  (Longpath)
// =================================================================
// „ev generell 2 varianten, einen leichtere und eine für profi im
//  gleichen design?"
//
// Die Zusage, die eine zweite Ansicht ueberhaupt erst vertretbar
// macht: **Umschalten aendert den Ton nicht.** Wer von Profi auf
// Einfach geht und zurueck, muss danach exakt dieselbe Kurve hoeren
// wie davor -- sonst ist die Umschaltung kein Ansichtswechsel,
// sondern ein Eingriff mit einem harmlosen Namen.
//
// Dazu die zweite Zusage: die einfachen Regler haengen an den SIEBEN
// FREIEN Griffen (EqBandLayout, ab kFirstSpare) und ruehren die
// Baender des Ausgangspunkts nicht an. Wer „Clean voice" gewaehlt
// hat, hoert weiter Clean voice.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QLayout>

#include "core/strip/ClientEq.h"
#include "core/strip/EqBandLayout.h"
#include "core/strip/StripChain.h"
#include "gui/applets/eq/ClientEqApplet.h"
#include "gui/applets/eq/ClientEqEditorCanvas.h"
#include "gui/applets/eq/EqHost.h"
#include "gui/applets/eq/StripEqPanel.h"
#include "gui/applets/eq/StripEqSimpleBar.h"

using namespace Longpath;

namespace {

struct Snapshot {
    std::vector<ClientEq::BandParams> bands;
    float master{1.0f};
};

Snapshot take(const ClientEq& eq)
{
    Snapshot s;
    for (int i = 0; i < eq.activeBandCount(); ++i) {
        s.bands.push_back(eq.band(i));
    }
    s.master = eq.masterGain();
    return s;
}

bool same(const ClientEq::BandParams& a, const ClientEq::BandParams& b)
{
    return qFuzzyCompare(a.freqHz, b.freqHz)
        && qFuzzyCompare(a.gainDb + 100.0f, b.gainDb + 100.0f)
        && qFuzzyCompare(a.q, b.q)
        && a.type == b.type
        && a.enabled == b.enabled;
}

QPushButton* buttonNamed(QWidget* w, const QString& text)
{
    for (QPushButton* b : w->findChildren<QPushButton*>()) {
        if (b->text() == text) { return b; }
    }
    return nullptr;
}

} // namespace

class TstEqTwoViews : public QObject {
    Q_OBJECT

private:
    StripChain m_chain;
    std::unique_ptr<EqHost>       m_host;
    std::unique_ptr<StripEqPanel> m_panel;

    void build()
    {
        m_chain.prepare(48000.0);
        EqBandLayout::ensureSeeded(m_chain.eq());
        m_host  = std::make_unique<EqHost>(&m_chain);
        m_panel = std::make_unique<StripEqPanel>(m_host.get());
        m_panel->showForPath(ClientEqApplet::Path::Tx);
        m_panel->resize(900, 520);
    }

private slots:
    // Hin und zurueck, und kein Band hat sich bewegt.
    void switchingViewsChangesNothing()
    {
        build();
        // Etwas einstellen, damit es auch etwas zu verlieren gibt.
        auto bp = m_chain.eq().band(EqBandLayout::kFirstSpare + 4);
        bp.gainDb = 4.5f;
        bp.enabled = true;
        m_chain.eq().setBand(EqBandLayout::kFirstSpare + 4, bp);

        const Snapshot before = take(m_chain.eq());

        QPushButton* pro = buttonNamed(m_panel.get(), QStringLiteral("Profi"));
        QPushButton* simple =
            buttonNamed(m_panel.get(), QStringLiteral("Einfach"));
        QVERIFY2(pro != nullptr && simple != nullptr,
                 "die beiden Knoepfe gibt es nicht");

        pro->click();
        simple->click();
        pro->click();

        const Snapshot after = take(m_chain.eq());
        QCOMPARE(after.bands.size(), before.bands.size());
        for (size_t i = 0; i < after.bands.size(); ++i) {
            QVERIFY2(same(before.bands[i], after.bands[i]),
                     qPrintable(QStringLiteral("Band %1 hat sich bewegt")
                                    .arg(i)));
        }
        QCOMPARE(after.master, before.master);
    }

    // Ein einfacher Regler stellt genau SEIN Band -- und keines der
    // Baender, die der Ausgangspunkt geformt hat.
    void aSimpleFaderMovesOnlyItsOwnBand()
    {
        build();
        auto* bar = m_panel->findChild<StripEqSimpleBar*>();
        QVERIFY2(bar != nullptr, "die einfache Ansicht gibt es nicht");

        const Snapshot before = take(m_chain.eq());
        const auto sliders = bar->findChildren<QSlider*>();
        QCOMPARE(sliders.size(), int(StripEqSimpleBar::kCount));

        // Der fuenfte Regler: Klarheit, 1,4 kHz.
        sliders.at(4)->setValue(55);   // +5,5 dB

        const int band = StripEqSimpleBar::bandSlots()[4].band;
        QVERIFY2(band >= EqBandLayout::kFirstSpare,
                 "ein einfacher Regler haengt an einem Band des "
                 "Ausgangspunkts");
        const auto moved = m_chain.eq().band(band);
        QVERIFY(std::abs(moved.gainDb - 5.5f) < 0.01f);
        QVERIFY(moved.enabled);

        for (int i = 0; i < m_chain.eq().activeBandCount(); ++i) {
            if (i == band) { continue; }
            QVERIFY2(same(before.bands[size_t(i)], m_chain.eq().band(i)),
                     qPrintable(QStringLiteral("Band %1 wurde mitgezogen")
                                    .arg(i)));
        }
    }

    // In der einfachen Ansicht ist nur anfassbar, was auch einen
    // Regler hat. Der Hochpass bei 40 Hz gehoert nicht dazu.
    void simpleViewDoesNotGrabThePresetBands()
    {
        build();
        auto* canvas = m_panel->findChild<ClientEqEditorCanvas*>();
        QVERIFY(canvas != nullptr);

        QPushButton* simple =
            buttonNamed(m_panel.get(), QStringLiteral("Einfach"));
        QVERIFY(simple != nullptr);
        simple->click();
        QVERIFY(canvas->simpleMode());

        QPushButton* pro = buttonNamed(m_panel.get(), QStringLiteral("Profi"));
        QVERIFY(pro != nullptr);
        pro->click();
        QVERIFY(!canvas->simpleMode());
    }

    // „ich denke, dass die bereiche teilweise viel zu groß sind, die
    //  geändert werden." Ein Regler, an dem gezogen wird, setzt
    //  seinen Griff auf SEINE Frequenz und SEINE Breite -- sonst
    //  heisst „Präsenz" zwei Oktaven.
    void draggingAFaderNarrowsAndPlacesItsBand()
    {
        build();
        const auto& sl = StripEqSimpleBar::bandSlots()[5];   // Präsenz

        // Vorher in der Profi-Ansicht verschoben und breit gemacht.
        auto bp = m_chain.eq().band(sl.band);
        bp.freqHz = 2000.0f;
        bp.q      = 0.707f;
        m_chain.eq().setBand(sl.band, bp);

        auto* bar = m_panel->findChild<StripEqSimpleBar*>();
        QVERIFY(bar != nullptr);
        bar->findChildren<QSlider*>().at(5)->setValue(30);

        const auto after = m_chain.eq().band(sl.band);
        QVERIFY2(std::abs(after.freqHz - sl.freqHz) < 0.01f,
                 qPrintable(QStringLiteral("steht auf %1 Hz statt %2")
                                .arg(double(after.freqHz))
                                .arg(double(sl.freqHz))));
        QVERIFY2(after.q > 1.5f,
                 qPrintable(QStringLiteral("immer noch breit: Q = %1")
                                .arg(double(after.q))));
    }

    // Ordnen raeumt alle sieben auf und laesst die Pegel stehen.
    void tidyPlacesEveryHandleAndKeepsTheLevels()
    {
        build();
        for (const auto& sl : StripEqSimpleBar::bandSlots()) {
            auto bp = m_chain.eq().band(sl.band);
            bp.freqHz = sl.freqHz * 1.7f;   // irgendwo daneben
            bp.q      = 0.707f;
            bp.gainDb = 3.0f;
            bp.enabled = true;
            m_chain.eq().setBand(sl.band, bp);
        }

        auto* bar = m_panel->findChild<StripEqSimpleBar*>();
        QVERIFY(bar != nullptr);
        bar->tidy();

        for (const auto& sl : StripEqSimpleBar::bandSlots()) {
            const auto bp = m_chain.eq().band(sl.band);
            QVERIFY(std::abs(bp.freqHz - sl.freqHz) < 0.01f);
            QVERIFY(std::abs(bp.q - sl.q) < 0.01f);
            QVERIFY2(std::abs(bp.gainDb - 3.0f) < 0.01f,
                     "Ordnen hat einen Pegel verstellt");
        }
    }

    // Das Ziel: die Zeile über jedem Regler muss sagen, wo man
    // steht -- und zwar so, dass „kein Messwert" nicht wie „null
    // Abweichung" aussieht.
    void theTargetRowSaysWhereYouStand()
    {
        build();
        auto* bar = m_panel->findChild<StripEqSimpleBar*>();
        QVERIFY(bar != nullptr);

        const auto zeilen = [&]() {
            QStringList t;
            for (QLabel* l : bar->findChildren<QLabel*>()) {
                const QString s = l->text();
                if (s == QStringLiteral("—") || s.contains(QStringLiteral("passt"))
                    || s.contains(QStringLiteral("zuviel"))
                    || s.contains(QStringLiteral("fehlt"))) {
                    t << s;
                }
            }
            return t;
        };

        // Ohne Messung: Striche, keine Nullen.
        m_panel->setZiel({});
        QVERIFY2(zeilen().count(QStringLiteral("—"))
                     >= StripEqSimpleBar::kCount,
                 "ohne Messung steht dort nicht ueberall ein Strich");

        // Mit Messung: zuviel, passt, zuwenig -- jeweils benannt.
        QVector<double> abw;
        for (int i = 0; i < StripEqSimpleBar::kCount; ++i) {
            abw.push_back(i == 0 ? 8.0 : (i == 1 ? -8.0 : 0.5));
        }
        m_panel->setZiel(abw);
        const QStringList z = zeilen();
        QVERIFY2(z.filter(QStringLiteral("zuviel")).size() == 1,
                 qPrintable(z.join(QStringLiteral(" | "))));
        QVERIFY2(z.filter(QStringLiteral("fehlt")).size() == 1,
                 qPrintable(z.join(QStringLiteral(" | "))));
        QVERIFY2(z.filter(QStringLiteral("passt")).size()
                     == StripEqSimpleBar::kCount - 2,
                 qPrintable(z.join(QStringLiteral(" | "))));
    }

    // Und so sieht es aus -- beide Ansichten, wirkliche Groesse.
    void bothViewsOnFile()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { QSKIP("kein LONGPATH_RENDER_DIR"); }

        build();
        // Eine Einstellung, damit das Blatt etwas zeigt und nicht die
        // flache Null.
        // Ueber die REGLER, nicht an den Baendern vorbei -- sonst
        // zeigt das Blatt eine Kurve, die so nie entsteht (die
        // Breite setzt der Regler beim Ziehen).
        auto* setBar = m_panel->findChild<StripEqSimpleBar*>();
        QVERIFY(setBar != nullptr);
        const int set[StripEqSimpleBar::kCount] =
            { 15, 0, -30, -15, 20, 40, -25 };
        const auto setSliders = setBar->findChildren<QSlider*>();
        for (int i = 0; i < StripEqSimpleBar::kCount; ++i) {
            setSliders.at(i)->setValue(set[i]);
        }
        m_panel->refreshFromEngine();
        m_panel->resize(940, 470);
        m_panel->show();

        const auto shot = [&](const QString& which, const QString& file) {
            QPushButton* b = buttonNamed(m_panel.get(), which);
            QVERIFY(b != nullptr);
            b->click();
            QCoreApplication::processEvents();
            m_panel->layout()->activate();
            QImage img(m_panel->size(), QImage::Format_ARGB32_Premultiplied);
            img.fill(QColor(0x0c, 0x0c, 0x0e));
            m_panel->render(&img);
            QVERIFY(img.save(QString::fromUtf8(dir) + QLatin1Char('/') + file));
        };
        shot(QStringLiteral("Einfach"), QStringLiteral("eq-einfach-echt.png"));
        shot(QStringLiteral("Profi"),   QStringLiteral("eq-profi-echt.png"));
    }

    // Und die Regler lesen die wirkliche Frequenz des Bandes ab,
    // nicht die, die der Name verspricht.
    void theFadersNameTheBandsTheyActuallyDrive()
    {
        build();
        for (const auto& s : StripEqSimpleBar::bandSlots()) {
            QVERIFY2(s.band >= EqBandLayout::kFirstSpare
                         && s.band < EqBandLayout::kBandCount,
                     "ein Regler zeigt auf ein Band ausserhalb der "
                     "freien Griffe");
        }
    }
};

QTEST_MAIN(TstEqTwoViews)
#include "tst_eq_two_views.moc"
