// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_strip_plugin_inserts.cpp  (Longpath)
// =================================================================
// Fremde Effekte in der Kette -- als Zusatz, und zwar nachweislich.
//
// Die Ansage des Betreibers am 2026-10-09: „plugins koennen nur als
// option zusaetzlich gelten, wenn gewollt. ist nicht der standard."
// Diese drei Faelle sind der Code gewordene Teil davon:
//
//   1. Ein leerer Platz aendert nichts. Bitgenau nichts.
//   2. Die Stelle in der Kette gilt. Vor dem Gate hoert ein Plugin
//      etwas anderes als hinter dem Begrenzer, und wer die Stelle
//      waehlen kann, muss sich darauf verlassen koennen.
//   3. Ein Plugin, das keine Zahlen liefert, bekommt den Block
//      weggenommen. Fremder Code ist das einzige Stueck dieser
//      Kette, das wir nicht gelesen haben.
//
// Es wird kein echtes Plugin geladen: ein Pruefstand, der eine
// bestimmte Audio Unit auf dem Rechner verlangt, prueft den Rechner.
// Hier haengt ein eigener Dummy in der Schnittstelle.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <QGuiApplication>

#include "core/strip/AudioPlugin.h"
#include "core/strip/AuPluginEditor.h"
#include "core/strip/StripChain.h"

#include <QtGlobal>

#include <cmath>
#include <limits>
#include <vector>

using namespace Longpath;

namespace {

// Hoert zu und merkt sich den lautesten Wert, den es bekommen hat.
class Listener final : public AudioPlugin {
public:
    PluginDescriptor descriptor() const override
    {
        PluginDescriptor d;
        d.identifier = QStringLiteral("test:listener");
        d.name = QStringLiteral("Listener");
        d.format = QStringLiteral("TEST");
        return d;
    }
    bool prepare(double, int) override { return true; }
    void processMono(float* s, int n) noexcept override
    {
        // Die ersten 100 ms zaehlen nicht: der Begrenzer hat eine
        // Einschwingzeit, und was in ihr durchkommt, sagt nichts
        // darueber, wo in der Kette dieser Platz haengt.
        for (int i = 0; i < n; ++i) {
            if (++seen > 4800) {
                heard = std::max(heard, std::fabs(double(s[i])));
            }
        }
    }
    void reset() noexcept override {}
    QVector<PluginParameter> parameters() const override { return {}; }
    float parameterValue(quint32) const override { return 0.0f; }
    void  setParameterValue(quint32, float) override {}

    double heard{0.0};
    long   seen{0};
};

// Liefert, was keine Zahl ist.
class Poisoner final : public AudioPlugin {
public:
    PluginDescriptor descriptor() const override
    {
        PluginDescriptor d;
        d.identifier = QStringLiteral("test:poison");
        d.name = QStringLiteral("Poisoner");
        d.format = QStringLiteral("TEST");
        return d;
    }
    bool prepare(double, int) override { return true; }
    void processMono(float* s, int n) noexcept override
    {
        for (int i = 0; i < n; ++i) {
            s[i] = std::numeric_limits<float>::quiet_NaN();
        }
        ++calls;
    }
    void reset() noexcept override { ++resets; }
    QVector<PluginParameter> parameters() const override { return {}; }
    float parameterValue(quint32) const override { return 0.0f; }
    void  setParameterValue(quint32, float) override {}

    int calls{0};
    int resets{0};
};

// Ein Nichts-Plugin mit Namen.
class Dummy final : public AudioPlugin {
public:
    explicit Dummy(QString name) : m_name(std::move(name)) {}
    PluginDescriptor descriptor() const override
    {
        PluginDescriptor d;
        d.identifier = QStringLiteral("test:") + m_name;
        d.name = m_name;
        d.format = QStringLiteral("TEST");
        return d;
    }
    bool prepare(double, int) override { return true; }
    void processMono(float*, int) noexcept override {}
    void reset() noexcept override {}
    QVector<PluginParameter> parameters() const override { return {}; }
    float parameterValue(quint32) const override { return 0.0f; }
    void  setParameterValue(quint32, float) override {}
private:
    QString m_name;
};

// Merkt sich, was ihm an Zustand gegeben wurde.
class Stateful final : public AudioPlugin {
public:
    PluginDescriptor descriptor() const override
    {
        PluginDescriptor d;
        d.identifier = QStringLiteral("test:stateful");
        d.name = QStringLiteral("Stateful");
        d.format = QStringLiteral("TEST");
        return d;
    }
    bool prepare(double, int) override { return true; }
    void processMono(float*, int) noexcept override {}
    void reset() noexcept override {}
    QVector<PluginParameter> parameters() const override { return {}; }
    float parameterValue(quint32) const override { return 0.0f; }
    void  setParameterValue(quint32, float) override {}
    QByteArray saveState() const override { return blob; }
    bool restoreState(const QByteArray& b) override
    {
        blob = b;
        ++restores;
        return true;
    }

    QByteArray blob;
    int restores{0};
};

std::vector<float> sine(double amp, int frames, double hz = 700.0)
{
    std::vector<float> x(static_cast<size_t>(frames));
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = float(amp * std::sin(2.0 * M_PI * hz * double(i) / 48000.0));
    }
    return x;
}

void run(StripChain& c, std::vector<float>& x)
{
    for (size_t at = 0; at + 64 <= x.size(); at += 64) {
        c.processMono(x.data() + at, 64);
    }
}

} // namespace

class TstStripPluginInserts : public QObject
{
    Q_OBJECT
private slots:

    // 1. Leer ist der Normalfall, und er kostet nichts.
    void anEmptySlotChangesNothing()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        QCOMPARE(c.insertPlugin(0), nullptr);
        QCOMPARE(c.insertPlugin(1), nullptr);

        std::vector<float> x = sine(0.5, 4800);
        const std::vector<float> before = x;
        run(c, x);

        for (size_t i = 0; i < x.size(); ++i) {
            QVERIFY2(x[i] == before[i],
                     qPrintable(QStringLiteral("Rahmen %1 veraendert").arg(i)));
        }
    }

    // 2. Die Stelle in der Kette gilt wirklich.
    //
    // Der Begrenzer deckelt auf -20 dB. Vor ihm hoert das Plugin den
    // lauten Ton, hinter ihm den gedeckelten. Waere die Stelle nur
    // Zierde, waeren beide Zahlen gleich -- und genau das waere das
    // stille Versagen, das man am Geraet nie findet.
    void thePositionInTheChainDecidesWhatThePluginHears()
    {
        const auto heardAt = [](int position) -> double {
            StripChain c;
            c.prepare(48000.0);
            c.setEnabled(true);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
            }
            c.setStageEnabled(StripChain::Stage::Limiter, true);
            c.limiter().setCeilingDb(-20.0f);
            c.limiter().setOutputTrimDb(0.0f);

            auto plugin = std::make_unique<Listener>();
            Listener* raw = plugin.get();
            c.setInsertPosition(0, position);
            c.setInsertEnabled(0, true);
            c.setInsertPlugin(0, std::move(plugin));
            if (c.insertPlugin(0) == nullptr) { return -1.0; }

            std::vector<float> x = sine(0.9, 48000);
            run(c, x);
            return raw->heard;
        };

        const double beforeChain = heardAt(0);
        const double afterLimiter = heardAt(StripChain::kStageCount);

        QVERIFY2(beforeChain > 0.8,
                 qPrintable(QStringLiteral("vor der Kette nur %1 gehoert")
                                .arg(beforeChain)));
        QVERIFY2(afterLimiter < beforeChain * 0.5,
                 qPrintable(QStringLiteral("hinter dem Begrenzer %1 gegen %2 "
                                           "-- die Stelle wirkt nicht")
                                .arg(afterLimiter).arg(beforeChain)));
    }

    // 3. Was keine Zahl ist, kommt nicht durch.
    void aPluginThatReturnsNonsenseLosesItsBlock()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }

        auto plugin = std::make_unique<Poisoner>();
        Poisoner* raw = plugin.get();
        c.setInsertPosition(1, 0);
        c.setInsertEnabled(1, true);
        c.setInsertPlugin(1, std::move(plugin));

        std::vector<float> x = sine(0.5, 640);
        const std::vector<float> before = x;
        run(c, x);

        QVERIFY(raw->calls > 0);
        QVERIFY2(raw->resets == raw->calls,
                 "jeder vergiftete Block muss das Plugin zuruecksetzen");
        QCOMPARE(c.nonFiniteInsertBlocks(1), uint32_t(raw->calls));
        for (size_t i = 0; i < x.size(); ++i) {
            QVERIFY(std::isfinite(x[i]));
        }
        // Und der Block kommt so heraus, wie er hereinkam.
        for (size_t i = 0; i + 64 <= x.size(); ++i) {
            QVERIFY(x[i] == before[i]);
        }
    }

    // Der Wirt selbst, soweit er sich ohne bestimmte Maschine pruefen
    // laesst: auf dem Mac antwortet er, anderswo sagt er ehrlich nein.
    // Was tatsaechlich installiert ist, darf kein Prueffall fordern --
    // deshalb wird geladen, WAS da ist, und wenn nichts da ist, ist
    // auch das kein Fehler.
    void theHostAnswersAndWhateverItFindsActuallyRuns()
    {
#ifdef Q_OS_MACOS
        QVERIFY(PluginHost::available());
        const QVector<PluginDescriptor> found = PluginHost::scan();
        qInfo() << "Audio Units auf dieser Maschine:" << found.size();
        if (found.isEmpty()) { return; }

        // Das erste, das sich oeffnen UND vorbereiten laesst.
        std::unique_ptr<AudioPlugin> p;
        for (const PluginDescriptor& d : found) {
            std::unique_ptr<AudioPlugin> candidate = PluginHost::load(d.identifier);
            if (candidate && candidate->prepare(48000.0, 512)) {
                qInfo() << "geladen:" << d.vendor << d.name
                        << "Parameter:" << candidate->parameters().size();
                p = std::move(candidate);
                break;
            }
        }
        QVERIFY2(p != nullptr, "kein einziges Audio Unit liess sich oeffnen");

        // Durchlaufen lassen und darauf bestehen, dass hinten Zahlen
        // herauskommen. Mehr wird einem fremden Effekt nicht
        // vorgeschrieben -- ob er laut oder leise macht, ist seine
        // Sache.
        std::vector<float> x = sine(0.3, 4800);
        for (size_t at = 0; at + 64 <= x.size(); at += 64) {
            p->processMono(x.data() + at, 64);
        }
        for (float v : x) { QVERIFY(std::isfinite(v)); }
#else
        QVERIFY(!PluginHost::available());
        QVERIFY(PluginHost::scan().isEmpty());
        QVERIFY(PluginHost::load(QStringLiteral("au:1:2:3")) == nullptr);
#endif
    }

    // Der gespeicherte Zustand kommt an der zweiten Instanz an.
    //
    // Der Mithoerer hat einen EIGENEN Kanalzug und damit eine eigene
    // Instanz desselben Plugins. Ohne diesen Weg bliebe sie auf dem
    // Stand vom Laden: man draeht an einem Regler und hoert nichts --
    // also genau dort nutzlos, wo Mithoeren gebraucht wird. Und die
    // Gegenrichtung zaehlt ebenso: ein unveraenderter Zustand darf
    // NICHT durchgereicht werden, sonst haelt der halbsekuendige
    // Abgleichtakt zweimal je Sekunde den Audiofaden an.
    void aChangedStateReachesASecondInstance()
    {
        StripChain c;
        c.prepare(48000.0);

        auto plugin = std::make_unique<Stateful>();
        Stateful* raw = plugin.get();
        c.setInsertPlugin(0, std::move(plugin));
        QVERIFY(c.insertPlugin(0) != nullptr);
        const int afterLoad = raw->restores;

        c.applyInsertState(0, QByteArrayLiteral("neu"));
        QCOMPARE(raw->restores, afterLoad + 1);
        QCOMPARE(raw->blob, QByteArrayLiteral("neu"));

        // Zweimal dasselbe ist kein Grund, irgendetwas anzuhalten.
        c.applyInsertState(0, QByteArrayLiteral("neu"));
        QCOMPARE(raw->restores, afterLoad + 1);

        c.applyInsertState(0, QByteArrayLiteral("noch neuer"));
        QCOMPARE(raw->restores, afterLoad + 2);

        // Und ein leerer Zustand ist kein Zustand.
        c.applyInsertState(0, QByteArray());
        QCOMPARE(raw->restores, afterLoad + 2);
    }

    // Das eigene Fenster eines Plugins -- und die Zusage, dass es im
    // Prueflauf NICHTS aufmacht.
    //
    // Unter QT_QPA_PLATFORM=offscreen gibt es keinen Fensterserver.
    // Ein NSWindow dort kostete im ersten macOS-CI-Lauf jeden
    // Fensterkonstruktor einen SIGSEGV (siehe die Warnung in
    // MacFloatingWindowBehavior.mm). Deshalb faengt available() das
    // eine Etage frueher ab, und dieser Fall haelt es fest.
    void theEditorStaysShutWhenThereIsNoScreen()
    {
        const bool headless =
            QGuiApplication::platformName() == QLatin1String("offscreen");
        QCOMPARE(AuPluginEditor::available(), !headless);

        // Ein Nullzeiger ist nie ein Fenster, auf keiner Plattform.
        QVERIFY(!AuPluginEditor::show(nullptr, QStringLiteral("nichts")));
        AuPluginEditor::close(nullptr);   // darf nicht knallen

        // Und ein Dummy-Plugin behauptet kein Fenster zu haben.
        Dummy d(QStringLiteral("ohne Fenster"));
        QVERIFY(!d.hasEditor());
        QVERIFY(!d.showEditor(QStringLiteral("x")));
    }

    // Ausgeschaltet heisst ausgeschaltet -- auch mit geladenem Plugin.
    void aSlotSwitchedOffIsNotRun()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        auto plugin = std::make_unique<Listener>();
        Listener* raw = plugin.get();
        c.setInsertPlugin(0, std::move(plugin));
        c.setInsertEnabled(0, false);

        std::vector<float> x = sine(0.5, 640);
        run(c, x);
        QCOMPARE(raw->heard, 0.0);
    }
};

QTEST_MAIN(TstStripPluginInserts)
#include "tst_strip_plugin_inserts.moc"
