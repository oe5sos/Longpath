// =================================================================
// tests/tst_master_output_widget.cpp  (Longpath)
// =================================================================
//
// Exercises MasterOutputWidget — the menu-bar composite widget
// combining a speaker-mute button, a 0–100 master-volume slider,
// an inset value readout, and a right-click output-device picker.
// Phase 3O Sub-Phase 10 Task 10b.
//
// Test seams used:
//   - AudioEngine default constructor is valid without a real device
//     (m_paInitialized may be false on headless CI; rxBlockReady is a
//     no-op without a RadioModel, so bare AudioEngine is safe).
//   - MasterOutputWidget exposes findChild<QSlider*>("masterSlider")
//     and findChild<QPushButton*>("speakerBtn") via objectName so the
//     tests can drive them directly (no fragile eventFilter plumbing).
//
// Design spec: docs/architecture/2026-04-19-vax-design.md
//   §5.4 (AppSettings keys: audio/Master/Volume, audio/Master/Muted,
//   audio/Speakers/DeviceName)
//   §6.3 (wiring table for menu-bar MasterOutputWidget)
//   §7.3 (UI layout: speaker icon + 100px slider + inset readout)
// =================================================================

#include <QtTest/QtTest>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "gui/widgets/MasterOutputWidget.h"

using namespace Longpath;

class TstMasterOutputWidget : public QObject {
    Q_OBJECT

private:
    // Reset the AppSettings keys the widget reads/writes so each test
    // starts from a known-clean slate. The test sandbox (via
    // TestSandboxInit.cpp) guarantees we're not touching the real
    // ~/.config file.
    void clearMasterKeys() {
        auto& s = AppSettings::instance();
        s.remove(QStringLiteral("audio/Master/Volume"));
        s.remove(QStringLiteral("audio/Master/Muted"));
        s.remove(QStringLiteral("audio/Speakers/DeviceName"));
    }

private slots:

    void init() {
        clearMasterKeys();
    }

    // ── 1. Constructs without crashing ─────────────────────────────────────

    void constructsWithoutCrash() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        // Slider and speaker button must be reachable for downstream tests.
        QVERIFY(w.findChild<QSlider*>("masterSlider") != nullptr);
        QVERIFY(w.findChild<QPushButton*>("speakerBtn") != nullptr);
        QVERIFY(w.findChild<QLabel*>("dbLabel") != nullptr);
    }

    // ── 2. Slider move writes through to engine volume ─────────────────────

    void sliderWritesEngineVolume() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(slider);

        slider->setValue(75);

        QVERIFY(std::abs(engine.volume() - 0.75f) < 1e-3f);
    }

    // ── 3. Slider move persists audio/Master/Volume ────────────────────────

    void sliderPersistsVolume() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(slider);

        slider->setValue(75);

        const QString stored = AppSettings::instance()
            .value(QStringLiteral("audio/Master/Volume")).toString();
        QVERIFY2(!stored.isEmpty(), "audio/Master/Volume not written");
        const float val = stored.toFloat();
        QVERIFY(std::abs(val - 0.75f) < 1e-3f);
    }

    // ── 4. Mute button toggle drives engine masterMuted ────────────────────

    void muteButtonTogglesEngine() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);

        btn->click();  // press: muted
        QCOMPARE(engine.masterMuted(), true);

        btn->click();  // press again: unmuted
        QCOMPARE(engine.masterMuted(), false);
    }

    // ── 5. Mute persists audio/Master/Muted as "True"/"False" ──────────────

    void mutePersists() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);

        btn->click();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("True"));

        btn->click();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("False"));
    }

    // ── 5b. Ein gespeicherter Stummschalter wird beim Start geloest ────────
    //
    // Betreiber, 2026-11-13: „wieder kein ton" — und in den fuenf
    // Protokollen seit dem Vorabend stand die Warnung „Hauptausgang
    // startet STUMM" fuenfmal. Jeder Start war stumm, und zu sehen war
    // davon nur ein Knopf, der aussieht wie immer.
    //
    // Die Lautstaerke bleibt dabei unangetastet: ein Regler auf 0 ist
    // sichtbar, ein Stummschalter nicht.

    void gespeichertStummWirdBeimStartGeloest() {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Master/Muted"), QStringLiteral("True"));
        s.setValue(QStringLiteral("audio/Master/Volume"), QStringLiteral("0.4"));

        AudioEngine engine;
        MasterOutputWidget w(&engine);

        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);
        QVERIFY2(!btn->isChecked(), "der Lautsprecherknopf steht noch auf stumm");
        QVERIFY2(!engine.masterMuted(), "die Tonmaschine ist noch stumm");
        // Und in der Ablage auch, sonst stuende beim naechsten Start
        // wieder True da, falls diese Sitzung nichts mehr schreibt.
        QCOMPARE(s.value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("False"));

        // Die Lautstaerke hat das nicht angefasst.
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(slider);
        QCOMPARE(slider->value(), 40);
    }

    // ── 5c. Waehrend des Betriebs bleibt stumm stumm ───────────────────────
    //
    // Die Gegenrichtung: geloest wird nur der gespeicherte Zustand beim
    // Start. Wer im Betrieb stumm schaltet, will stumm sein.

    void stummImBetriebBleibt() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);

        btn->click();
        QVERIFY(btn->isChecked());
        QVERIFY(engine.masterMuted());
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("audio/Master/Muted")).toString(),
                 QStringLiteral("True"));
    }

    // ── 6. Engine volume echo updates slider without re-firing engine ──────

    void engineVolumeEchoesToSlider() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* slider = w.findChild<QSlider*>("masterSlider");
        QVERIFY(slider);

        // Seed with a distinct starting value so we can detect the echo.
        slider->setValue(10);

        QSignalSpy engineSpy(&engine, &AudioEngine::volumeChanged);

        engine.setVolume(0.30f);

        // Slider reflects new engine value (30 percent).
        QCOMPARE(slider->value(), 30);

        // The echo path must NOT cause the widget to call setVolume again.
        // setVolume(0.30f) itself emitted volumeChanged once; no extras.
        QCOMPARE(engineSpy.count(), 1);
    }

    // ── 7. Engine mute echo updates button without re-emission ─────────────

    void engineMuteEchoesToButton() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);
        auto* btn = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(btn);

        QSignalSpy engineSpy(&engine, &AudioEngine::masterMutedChanged);

        engine.setMasterMuted(true);

        QCOMPARE(btn->isChecked(), true);
        // Seit 2026-09-17 ist der Lautsprecher eine Zeichnung (QIcon),
        // kein Emoji-Text mehr: der Knopf traegt keinen Text, und das
        // Symbol wechselt mit dem Zustand -- also ein anderes Icon als
        // vor dem Stummschalten.
        QVERIFY(btn->text().isEmpty());
        QVERIFY(!btn->icon().isNull());

        // The engine emitted exactly once (its own setMasterMuted). The
        // button update via the echo slot must not cause a second emission.
        QCOMPARE(engineSpy.count(), 1);
    }

    // ── 8. setCurrentOutputDevice stores device name for the picker ────────
    //
    // The context-menu QActionGroup is awkward to exercise headlessly
    // (popup + action activation fights QTest without show()). Per the
    // task brief we take the lighter smoke: setCurrentOutputDevice()
    // stores the name the picker will use as its check-state anchor.

    void setCurrentOutputDeviceTracksName() {
        AudioEngine engine;
        MasterOutputWidget w(&engine);

        w.setCurrentOutputDevice(QStringLiteral("TestDevice"));
        // No public getter — verify via outputDeviceChangedSignal round-trip
        // is Task 10c's job. Smoke: the call does not crash, does not emit
        // a spurious outputDeviceChanged (setCurrentOutputDevice is a
        // sync-from-elsewhere path, not a user action).
        QSignalSpy spy(&w, &MasterOutputWidget::outputDeviceChanged);
        w.setCurrentOutputDevice(QStringLiteral("AnotherDevice"));
        QCOMPARE(spy.count(), 0);
    }

    // ── 9. Applied saved volume/mute at construction time ──────────────────
    //
    // AppSettings persistence round-trip: a widget built after a prior
    // session's values were saved should come up with the slider/button
    // reflecting those values AND the engine seeded to match.

    void restoresSavedValuesOnConstruction() {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Master/Volume"),
                   QStringLiteral("0.600"));
        s.setValue(QStringLiteral("audio/Master/Muted"),
                   QStringLiteral("True"));

        AudioEngine engine;

        // Der stumme Start gehoert ins Protokoll (2026-10-15).
        //
        // Am 2026-10-15 kostete „kein Ton" eine Stunde: hier stand
        // True, der Lautsprecherknopf sass stumm da, und sonst sagte
        // es nichts. Alles andere war in Ordnung — Geraet offen, WDSP
        // bereit, I/Q lief —, also suchte man ueberall sonst. Ohne
        // diese Zeile faellt der Hinweis beim naechsten Umbau still
        // wieder heraus.
        //
        // Der Wortlaut hat sich am 2026-11-13 geaendert, und mit ihm die
        // Erwartung weiter unten: der gespeicherte Stummschalter wird
        // jetzt GELOEST statt wiederhergestellt. Grund waren fuenf
        // stumme Starts in zwei Tagen („wieder kein ton") -- das
        // Protokoll half dem, der Protokolle liest, nicht dem, der
        // funken will. Siehe MasterOutputWidget.cpp.
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("Hauptausgang war STUMM gespeichert")));

        MasterOutputWidget w(&engine);

        auto* slider = w.findChild<QSlider*>("masterSlider");
        auto* btn    = w.findChild<QPushButton*>("speakerBtn");
        QVERIFY(slider && btn);

        // Die Lautstaerke kommt zurueck wie gespeichert ...
        QCOMPARE(slider->value(), 60);
        QVERIFY(std::abs(engine.volume() - 0.60f) < 1e-3f);
        // ... der Stummschalter nicht. Ein Regler auf 0 ist sichtbar,
        // ein Stummschalter nicht, und genau das war das Problem.
        QCOMPARE(btn->isChecked(), false);
        QCOMPARE(engine.masterMuted(), false);
    }

    // ── 10. Saved device name applies at construction (smoke) ─────────────
    //
    // Regression for the gap where audio/Speakers/DeviceName was read but
    // never pushed into AudioEngine on startup, leaving the engine on the
    // platform default until the user reopened the picker. AudioEngine
    // exposes no "current device" getter, so this covers the construction
    // path running cleanly with a non-empty saved device and seeding the
    // same value into the picker anchor.

    void restoresSavedDeviceOnConstruction() {
        AppSettings::instance().setValue(
            QStringLiteral("audio/Speakers/DeviceName"),
            QStringLiteral("SomeDevice"));

        AudioEngine engine;
        MasterOutputWidget w(&engine);

        // Picker anchor round-trip: setCurrentOutputDevice re-setting the
        // value the constructor already stored must not re-emit.
        QSignalSpy spy(&w, &MasterOutputWidget::outputDeviceChanged);
        w.setCurrentOutputDevice(QStringLiteral("SomeDevice"));
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TstMasterOutputWidget)
#include "tst_master_output_widget.moc"
