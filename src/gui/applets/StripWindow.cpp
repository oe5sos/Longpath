// =================================================================
// src/gui/applets/StripWindow.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See StripWindow.h for the layout rule and for
// what is ported and what is not.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-27 — Stage pages scroll (StagePageScroll): the window no
//                 longer needs ~1000 px of height. Martin Fischer,
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "gui/applets/StripWindow.h"

#include "gui/StyleConstants.h"
#include "core/strip/EqBandLayout.h"
#include "core/strip/AetherPreset.h"
#include "core/strip/StripSettings.h"
#include "core/strip/BandLimit.h"
#include "core/strip/StripLiveMonitor.h"
#include "core/strip/AudioPlugin.h"
#include "core/strip/PluginPlacement.h"
#include "core/AppSettings.h"
#include "core/Sprache.h"
#include "gui/applets/eq/StripEqSimpleBar.h"
#include "core/VoiceAnalyzer.h"
#include "core/strip/LtasSpectrum.h"
#include "core/strip/PluginPlacement.h"
#include "gui/applets/eq/EqHost.h"
#include "gui/applets/eq/StripEqPanel.h"
#include "gui/applets/TxVoiceCheckDialog.h"
#include "core/strip/StripCharacters.h"
#include "core/AudioEngine.h"
#include "gui/widgets/TxSpectrumWidget.h"

#include <QInputDialog>
#include <QFileDialog>
#include <QStandardPaths>
#include <QScrollArea>
#include <QSizePolicy>
#include <QMessageBox>
#include "core/AudioEngine.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QApplication>
#include <QDrag>
#include <QFrame>
#include <QMimeData>
#include <QMap>
#include <QMouseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace Longpath {

namespace {

// A stage page in the tab bar, in a scroll area (2026-09-27). The
// compressor page alone is ~660 px tall; with the rows above the tabs
// the window's minimum height came to ~1000 px -- taller than a MacBook
// Air's usable screen (849 px). The operator's screenshot showed the
// bottom of the window behind the Dock, and it could not be dragged
// smaller. In a scroll area a page keeps its natural size wherever
// there is room and scrolls only where there is not.
class StagePageScroll : public QScrollArea {
public:
    explicit StagePageScroll(QWidget* page)
    {
        setWidget(page);
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        // The tab pane behind shows through, exactly as before.
        setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
        viewport()->setAutoFillBackground(false);
        page->setAutoFillBackground(false);
    }

    // QScrollArea caps its hint at 36 x 24 lines of text. The page's own
    // hint is the size the window should open at when the screen has it.
    QSize sizeHint() const override
    {
        return widget() ? widget()->sizeHint() : QScrollArea::sizeHint();
    }
};

QString dimStyle()
{
    return QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary));
}

// A slider with its name on the left and its value on the right, in the
// unit the operator thinks in.
//
// Sliders carry integers, so every control here has a scale factor.
// That factor is the one thing in this file that is easy to get wrong
// and impossible to see: a release time off by ten still moves, still
// looks plausible, and just behaves oddly. So the conversion lives in
// one place, both directions, per row.
struct Knob {
    QSlider* slider{nullptr};
    QLabel*  value{nullptr};
};

Knob addKnob(QFormLayout* form, const QString& name,
             double minVal, double maxVal, double step,
             double initial, const QString& suffix,
             std::function<void(double)> onChange,
             int decimals = 1,
             std::function<void()> after = {})
{
    auto* row = new QWidget(form->parentWidget());
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);

    Knob k;
    k.slider = new QSlider(Qt::Horizontal, row);
    const int steps = int(std::lround((maxVal - minVal) / step));
    k.slider->setRange(0, steps);
    k.slider->setValue(int(std::lround((initial - minVal) / step)));
    h->addWidget(k.slider, 1);

    k.value = new QLabel(row);
    k.value->setMinimumWidth(72);
    k.value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    k.value->setStyleSheet(dimStyle());
    h->addWidget(k.value);

    auto toValue = [minVal, step](int pos) { return minVal + pos * step; };
    auto show = [k, suffix, decimals, toValue](int pos) {
        k.value->setText(QStringLiteral("%1 %2")
                             .arg(toValue(pos), 0, 'f', decimals)
                             .arg(suffix));
    };
    show(k.slider->value());

    QObject::connect(k.slider, &QSlider::valueChanged, k.slider,
                     [show, toValue, onChange, after](int pos) {
        show(pos);
        if (onChange) { onChange(toValue(pos)); }
        // Saving on every slider step rather than on release: a crash
        // or a power cut mid-adjustment should not cost the operator
        // the twenty minutes before it. AppSettings coalesces the disk
        // writes, so this is a map insert per step.
        if (after) { after(); }
    });

    form->addRow(name, row);
    return k;
}

} // namespace

StripWindow::StripWindow(RadioModel* radio, QWidget* parent)
    : QDialog(parent)
    , m_radio(radio)
{
    setWindowTitle(QStringLiteral("Longpath Audio Channel Strip"));
    setModal(false);
    // Restore before the panels are built, so every control opens
    // showing what the chain actually holds rather than a default it
    // would then write back over the top.
    if (StripChain* c = chain()) {
        StripSettings::restore(*c);
        m_hadChain = true;
    }
    buildUi();
    refreshChainRow();
}

// Was oben unter dem Hauptschalter steht.
//
// Drei Lagen, und die mittlere ist neu: bis zum 2026-10-09 entstand
// der Kanalzug mit der Sendepumpe, also war ohne Funkgeraet nichts
// einzustellen und der Hinweis sagte das auch. Jetzt steht der
// Kanalzug immer -- einstellen und abhoeren geht am Schreibtisch,
// nur auf Sendung geht es eben erst mit Geraet.
QString StripWindow::noteText() const
{
    if (!chain()) {
        return Sprache::T(QStringLiteral(
            "No strip available in this window."));
    }
    if (!m_radio || !m_radio->isConnected()) {
        return Sprache::T(QStringLiteral(
            "No radio connected — set the strip up here anyway. Record "
            "and Play above run your voice through it off the sound "
            "card, so every stage can be heard and judged at the desk. "
            "The moment a radio comes up, this is what it transmits."));
    }
    return Sprache::T(QStringLiteral(
        "Sits in front of the radio's own processing, on the "
        "microphone before WDSP sees it. Switched off, the audio is "
        "bit-for-bit what it was before this window existed — so "
        "switching off is a real comparison, not an approximate one."));
}

void StripWindow::adoptChainIfArrived()
{
    const bool have = chain() != nullptr;
    if (have == m_hadChain) { return; }
    m_hadChain = have;
    if (!have) { return; }

    // The radio turned up after this window opened. Restore, lay the
    // bands out, and rebuild every panel — a rebuild rather than a
    // refresh for the same reason as applying a preset: each control
    // reads its own value from the chain when it is built, so there is
    // no second copy to forget.
    StripChain* c = chain();
    StripSettings::restore(*c);
    reloadControls();
    refreshStagePictures();
    refreshChainRow();
    if (m_note) { m_note->setText(noteText()); }
    if (m_master) {
        const QSignalBlocker block(m_master);
        m_master->setChecked(c->isEnabled());
    }
}

// Read a stage value for a control's initial position.
//
// Without this the panels are a lie after a restart: StripSettings has
// already put the operator's values into the chain, but every slider
// would open at the literal default written in the call below — and the
// first touch of any slider would then write that default over the
// restored value. Persistence that silently undoes itself on the next
// nudge is worse than none, because it looks like it works.
double StripWindow::cur(const std::function<float()>& read,
                        double fallback) const
{
    return chain() ? double(read()) : fallback;
}

StripWindow::~StripWindow()
{
    // Der Mithoerer hat kein Elternobjekt (siehe toggleLiveMonitor):
    // sein Destruktor haelt seinen Faden an und wartet auf ihn.
    delete m_live;
    m_live = nullptr;

    // If the window is destroyed with the apply-path bypass engaged,
    // the radio would keep a bypass it never asked for.
    setRadioBypass(false);
}

void StripWindow::setRadioBypass(bool on)
{
    if (!m_radio) { return; }
    TransmitModel& tx = m_radio->transmitModel();

    if (on) {
        if (m_radioBypassed) { return; }
        m_hadTxEq    = tx.txEqEnabled();
        m_hadLeveler = tx.txLevelerOn();
        m_hadCfc     = tx.cfcEnabled();
        m_radioBypassed = true;
        tx.setTxEqEnabled(false);
        tx.setTxLevelerOn(false);
        tx.setCfcEnabled(false);
        return;
    }
    if (!m_radioBypassed) { return; }
    m_radioBypassed = false;
    // Exactly as they were, not "off". An operator who had the leveler
    // on before opening this window must have it on afterwards, and
    // finding out otherwise on the air is not acceptable.
    tx.setTxEqEnabled(m_hadTxEq);
    tx.setTxLevelerOn(m_hadLeveler);
    tx.setCfcEnabled(m_hadCfc);
}

// setSelfMonitor is gone (2026-08-11) — the live listen path left this
// window with the "Hear myself" checkbox; the record-then-listen
// monitor never engages the off-air chain or the MON mix. setRadioBypass
// stays: the voice-check's strip apply-path still uses it.

// ── PUDU monitor button surface (AetherSDR contract) ────────────────

void StripWindow::setMonitorRecording(bool on)
{
    if (!m_monRecordBtn) { return; }
    m_monRecordBtn->setText(on ? QStringLiteral("■ Stop")
                               : Sprache::T(QStringLiteral("● Record")));
    if (m_monPlayBtn) {
        m_monPlayBtn->setEnabled(!on && m_monHasRecording);
    }
}

void StripWindow::setMonitorPlaying(bool on)
{
    if (!m_monPlayBtn) { return; }
    m_monPlayBtn->setText(on ? QStringLiteral("■ Stop")
                             : Sprache::T(QStringLiteral("▶ Play")));
}

void StripWindow::setMonitorHasRecording(bool has)
{
    m_monHasRecording = has;
    if (m_monPlayBtn) { m_monPlayBtn->setEnabled(has); }
    if (m_monSaveBtn) { m_monSaveBtn->setEnabled(has); }
}

void StripWindow::persist()
{
    StripChain* c = chain();
    if (!c) { return; }
    StripSettings::save(*c);
    // Sofort, nicht erst beim naechsten Abgleichtakt: wer an einem
    // Regler zieht, soll es hoeren, waehrend er zieht.
    if (m_live) { m_live->resync(); }
}

// Mithoeren an und aus.
//
// Der eigene Kanalzug des Mithoerers wird aus derselben gespeicherten
// Einstellung geholt wie die Sendekette -- deshalb der Abgleichtakt:
// persist() deckt die 49 Stellen ab, die durch dieses Fenster gehen,
// der halbsekuendige Takt faengt die, die es nicht tun (der portierte
// Entzerrer speichert selbst, ueber EqHost).
// ── Welche Kette die ANZEIGEN lesen ─────────────────────────────────
//
// Normalerweise die des Funkgeraets. Laeuft aber das Mithoeren und
// ist kein Geraet da, rechnet NUR der Kanalzug des Mithoerers -- und
// die Anzeigen standen trotzdem auf der leeren Sendekette. Am Geraet
// sah das so aus: Mithoeren an, Stimme hoerbar, und die Gate-Seite
// meldete „Silent -- Nothing is arriving at the gate", dazu
// „in -120.0 dBFS" und zwei leere Pegelbalken.
//
// Das ist die schlimmste Sorte Anzeige: sie ist nicht leer, sie
// BEHAUPTET etwas Falsches, und zwar genau das, was man beim Suchen
// nach einem Tonproblem als erstes glaubt.
//
// Verstellt wird nur, WORAUS gelesen wird. Geschrieben wird
// unveraendert in die Kette des Funkgeraets (chain()) -- sie bleibt
// die Vorlage, der Mithoerer spiegelt sie zehnmal in der Sekunde.
// Ein Regler, der in die Spiegelung schriebe, waere beim naechsten
// Takt wieder ueberschrieben.
void StripWindow::useChainForDisplay(StripChain* c)
{
    m_displayChain = (c == chain()) ? nullptr : c;
    if (m_chainView) { m_chainView->setChain(displayChain()); }
    if (m_levels)    { m_levels->setChain(displayChain()); }
    refreshStagePictures();
    refreshStageCards();
}

void StripWindow::toggleLiveMonitor(bool on)
{
    if (on) {
        if (!m_live) {
            // OHNE Elternobjekt, und das ist kein Versehen: das Ding
            // zieht auf einen eigenen Faden um, und Qt verschiebt
            // kein Objekt, das an einem Elternobjekt auf einem
            // anderen Faden haengt. Mit `this` als Eltern waere
            // moveToThread() stillschweigend fehlgeschlagen -- der
            // Mithoerer liefe weiter auf dem Bedienfaden, und die
            // kleinen Puffer, derentwegen er umgezogen ist, waeren
            // dann genau das Knacken, das sie vermeiden sollen.
            // Abgeraeumt wird er im Destruktor dieses Fensters.
            m_live = new StripLiveMonitor(nullptr);
            connect(m_live, &StripLiveMonitor::failed, this,
                    [this](const QString& why) {
                if (m_liveBtn) {
                    const QSignalBlocker b(m_liveBtn);
                    m_liveBtn->setChecked(false);
                }
                if (m_liveNote) { m_liveNote->setText(why); }
            });
            connect(m_live, &StripLiveMonitor::started,
                    this, &StripWindow::updateLiveNote);
            connect(m_live, &StripLiveMonitor::stopped,
                    this, &StripWindow::updateLiveNote);
        }
        // Die Entzerrerkurve mitversorgen. Ohne Funkgeraet fuellt
        // niemand sonst das Mikrofonspektrum, und eine Kurve ohne
        // Spektrum darunter ist beim Einstellen von Hand das
        // Entscheidende, was fehlt.
        if (StripChain* c = chain()) {
            m_live->setSpectrumSink(&c->micSpectrum());
            // Von der lebenden Kette abschreiben, nicht aus der
            // gespeicherten Datei -- sonst hoert man eine Aenderung
            // erst, wenn die Maus losgelassen wurde.
            m_live->setMirrorSource(c);
        }
        m_live->setLatencyPreset(static_cast<StripLiveMonitor::Latency>(
            AppSettings::instance()
                .value(QStringLiteral("StripMonitorLatency"), 0).toInt()));
        m_live->setVolume(m_liveVol ? m_liveVol->value() / 100.0 : 0.5);
        m_live->setMonitorChannel(
            m_liveChannel ? m_liveChannel->currentData().toInt() : -1);
        // ── Welcher EINGANG des Geraets ──────────────────────────
        //
        // Nicht zu verwechseln mit der Zeile darueber: die waehlt die
        // Bandgrenze des Senders, diese den Kanal des Wandlers. An
        // einem Apollo Twin mit sechzehn Eingaengen liegt die Stimme
        // auf einem davon. -1 legt alle zusammen (richtig fuer ein
        // Mikrofon), sonst der Index ab null.
        m_live->setInputPick(
            AppSettings::instance()
                .value(QStringLiteral("StripMonitorInChannel"), -1).toInt());
        m_live->setLoudnessMatch(
            m_liveGleich ? m_liveGleich->isChecked() : true);
        // ── Und die Geraete, VOR dem Start ───────────────────────
        //
        // Der Mithoerer wird hier erst erzeugt. Die
        // Wiederherstellung der Auswahlfelder lief beim Bau der
        // Seite, als es ihn noch nicht gab -- sie hat ins Leere
        // gesetzt. Am Geraet sah das so aus: beide Felder standen
        // auf „Universal Audio Thunderbolt", und die Zeile darunter
        // meldete „Ein: MacBook Air-Mikrofon". Zwei Angaben, die
        // einander widersprechen, und die falsche ist die, nach der
        // gehoert wird.
        if (m_liveIn) {
            m_live->setInputDeviceId(m_liveIn->currentData().toByteArray());
        }
        if (m_liveOut) {
            m_live->setOutputDeviceId(m_liveOut->currentData().toByteArray());
        }
        // Beim Einschalten den Hauptschalter uebernehmen, nicht erst
        // beim naechsten Umlegen.
        m_live->setStripBypassed(!(m_master && m_master->isChecked()));
        m_live->start();
        // Ohne Funkgeraet rechnet nur der Mithoerer -- dann sollen
        // die Anzeigen auch IHN lesen. Siehe useChainForDisplay().
        if (!m_radio || !m_radio->isConnected()) {
            useChainForDisplay(m_live->chain());
        }
        if (m_transfer) {
            m_transfer->setRings(&m_live->inputRing(), &m_live->outputRing());
        }
        if (!m_liveSync) {
            m_liveSync = new QTimer(this);
            // Zehnmal in der Sekunde. Das geht nur, weil seit dem
            // Spiegeln nichts mehr ueber die Einstellungsdatei laeuft
            // -- es ist eine Tabelle im Arbeitsspeicher und zurueck.
            m_liveSync->setInterval(100);
            connect(m_liveSync, &QTimer::timeout, this, [this]() {
                if (m_live && m_live->isRunning()) { m_live->resync(); }
                updateLiveNote();
            });
        }
        m_liveSync->start();
    } else {
        if (m_liveSync) { m_liveSync->stop(); }
        if (m_live) { m_live->stop(); }
        if (m_transfer) { m_transfer->setRings(nullptr, nullptr); }
        useChainForDisplay(chain());
    }
    updateLiveNote();
}

void StripWindow::updateLiveNote()
{
    if (!m_liveNote) { return; }
    if (!m_live || !m_live->isRunning()) {
        m_liveNote->setText(QStringLiteral(
            "Hear myself runs your microphone through the strip and "
            "back out of this computer — use headphones, and switch "
            "the strip off and on to hear what it is doing."));
        return;
    }
    // ── Was gerade zu hoeren ist, und was dafuer an sein muss ───────
    //
    // Zum Hoeren der eigenen Einstellung muessen ZWEI Schalter an
    // sein: dieser hier und der Hauptschalter des Kanalzugs. Das hat
    // niemand irgendwo stehen, und es kostete den Betreiber zwei
    // Runden -- erst „unterschied hoere ich keinen ob on oder off",
    // dann „wenn ich die EQ Kurve veraendere tut sich gar nichts".
    // Beides war richtig beobachtet: der Kanalzug stand aus.
    //
    // Also sagt die Zeile es im Klartext, samt dem Namen des
    // Knopfes, der fehlt. Ein Hinweis, der das Gegenmittel nicht
    // nennt, ist eine halbe Antwort.
    const bool through = m_master && m_master->isChecked();
    QString t;
    if (!through) {
        t = Sprache::T(QStringLiteral(
            "Listening to the raw microphone. The strip is switched "
            "off, so nothing you set — EQ curve included — is audible "
            "right now. Tick “Channel strip” at the top left to hear "
            "it, and untick it again to compare. "));
    } else {
        t = Sprache::T(QStringLiteral("Listening through the strip. "));
    }
    t += Sprache::T(QStringLiteral(
        "About %1 ms of delay inside Longpath; the sound card adds its "
        "own on each side. Headphones. While this is on, the EQ tab "
        "draws your own voice — speak a few sentences, then press Peak "
        "Hold there to freeze the shape you are aiming at."))
        .arg(m_live->latencyMs(), 0, 'f', 0);

    // ── Und dass man eine Stufe allein hoeren kann ──────────────────
    //
    // Eine Geste, die niemand findet, gibt es nicht. Sie steht
    // deshalb dort, wo er beim Vergleichen ohnehin liest.
    t += QStringLiteral(" ") + (m_soloStufe >= 0
        ? Sprache::T(QStringLiteral(
              "Right now you are hearing ONE stage on its own — ⌥-click "
              "that tile again to put the others back as they were."))
        : Sprache::T(QStringLiteral(
              "⌥-click a tile in the row to hear that stage on its own; "
              "⌥-click it again and everything comes back exactly as it "
              "was.")));

    // ── Welche Geraete, und mit welcher Rate ────────────────────────
    //
    // Der Mithoerer nimmt die SYSTEMVORGABE und hat keine eigene
    // Geraetewahl. Wer ein Audiogeraet anschliesst und es nicht zur
    // Vorgabe macht, hoert weiter das eingebaute Mikrofon und sucht
    // den Fehler in der Kette.
    t += QStringLiteral(" ") + Sprache::T(QStringLiteral(
        "In: %1 · out: %2 · %3 Hz."))
        .arg(m_live->inputName(), m_live->outputName())
        .arg(m_live->inputRate());

    // ── Und WELCHER Eingang, wenn das Geraet mehrere hat ────────────
    //
    // Ein Apollo Twin liefert sechzehn. Auf einem davon liegt das
    // Mikrofon, auf anderen liegt unter Umstaenden der eigene
    // Ausgang, den die Console zurueckschickt (am 2026-10-14
    // gemessen: Eingang 3 und 4 waren auf die Nachkommastelle
    // Longpaths eigenes Signal). Ohne diese Angabe sucht man den
    // Fehler in der Kette.
    if (m_live->inputChannels() > 2) {
        const int k = m_live->activeInputChannel();
        t += QStringLiteral(" ") + Sprache::T(QStringLiteral(
            "This interface has %1 inputs; heard is number %2."))
            .arg(m_live->inputChannels()).arg(k >= 0 ? k + 1 : 1);
    }

    // ── Der Kanal filtert auch bei abgeschaltetem Hauptschalter ─────
    //
    // Er bildet den SENDER nach und haengt deshalb absichtlich nicht
    // am Kanalzug. Nur stand das nirgends: am 2026-10-14 hat der
    // Betreiber bei ausgeschalteter Kette gegen seinen Apollo
    // verglichen, waehrend der Mithoerweg weiter bei 4050 Hz zumachte
    // -- und beides klang verschieden, zu Recht, aus einem Grund, den
    // die Oberflaeche verschwieg.
    if (m_liveChannel) {
        const int kanal = m_liveChannel->currentData().toInt();
        if (kanal >= 0) {
            t += QStringLiteral(" ") + Sprache::T(QStringLiteral(
                "The channel “%1” is in the monitor path as well — it "
                "stands for the transmitter and is deliberately not "
                "tied to the strip switch. Pick “raw” to compare "
                "against something unfiltered."))
                .arg(m_liveChannel->currentText());
        }
    }

    // Und die eine Rate, an der eine ganze Stufe haengt. rnnoise ist
    // auf 48 kHz trainiert; bei jeder anderen reicht das Entrauschen
    // unveraendert durch -- die Kachel steht auf „an" und die Stufe
    // tut nichts. Eine Falschaussage an genau der Stelle, an der man
    // hinsieht, und ohne diesen Satz faellt sie niemandem auf.
    if (m_live->inputRate() != 48000) {
        t += QStringLiteral(" ") + Sprache::T(QStringLiteral(
            "⚠ Denoise needs exactly 48000 Hz and is passing through "
            "untouched at this rate — set the interface to 48 kHz in "
            "Audio MIDI Setup."));
    }
    // Der Zaehler steht jetzt fuer EINZELNE Abtastwerte, nicht mehr
    // fuer Bloecke. Dass er laeuft, ist der Normalfall und kein
    // Fehler: Mikrofon und Kopfhoerer sind zwei Geraete mit zwei
    // Quarzen. Der Text sagt das, sonst liest man einen Alarm, wo
    // eine Selbstverstaendlichkeit steht.
    const int slipped = m_live->overruns() + m_live->underruns();
    if (slipped > 0) {
        t += Sprache::T(QStringLiteral("  %1 single sample(s) slipped so far, "
                            "keeping two sound-card clocks together — "
                            "that is normal and inaudible. Crackling is "
                            "something else: then the latency setting is "
                            "too tight for this computer."))
                 .arg(slipped);
    }
    m_liveNote->setText(t);
}

StripChain* StripWindow::chain() const
{
    return m_radio ? m_radio->stripChain() : nullptr;
}

// Woraus die ANZEIGEN lesen. Siehe useChainForDisplay() -- ohne
// Funkgeraet rechnet nur der Mithoerer, und eine Anzeige, die
// stattdessen die stille Sendekette liest, behauptet „es kommt
// nichts an".
//
// Geschrieben wird immer in chain(). Die beiden Namen stehen bewusst
// nebeneinander: wer hier versehentlich den falschen nimmt, soll es
// beim Lesen sehen.
StripChain* StripWindow::displayChain() const
{
    return m_displayChain != nullptr ? m_displayChain : chain();
}

// ── Die Kette gehoert zum Mikrofon ───────────────────────────────
//
// Siehe die Begruendung am Geraetewechsel. Der Schluessel haengt an
// der Kennung der Soundkarte; alles, was kein Buchstabe und keine
// Ziffer ist, wird zu einem Strich, damit daraus ein gueltiger
// XML-Name wird.
QString StripWindow::kettenSchluessel(const QByteArray& geraeteId)
{
    // Der erste Eintrag der Liste heisst „Systemvorgabe" und traegt
    // keine Kennung -- er ist trotzdem eine Wahl des Betreibers und
    // bekommt deshalb seinen eigenen Platz. Ohne das merkt sich der
    // Schalter ausgerechnet fuer den Eintrag nichts, der ab Werk
    // ausgewaehlt ist.
    if (geraeteId.isEmpty()) {
        return QStringLiteral("StripMasterFor/Systemvorgabe");
    }
    QString sauber;
    sauber.reserve(geraeteId.size());
    for (char c : geraeteId) {
        const QChar q = QLatin1Char(c);
        sauber += (q.isLetterOrNumber() ? q : QLatin1Char('_'));
    }
    return QStringLiteral("StripMasterFor/") + sauber;
}

void StripWindow::wendeKettenStandAn(const QByteArray& geraeteId)
{
    if (m_master == nullptr) { return; }
    const QString gespeichert = AppSettings::instance()
        .value(kettenSchluessel(geraeteId), QString()).toString();
    // ── Ein unbekanntes Mikrofon bekommt KEINE Kette ────────────
    //
    // Hier stand zuerst „nichts bekannt, nichts aendern". Das trug
    // den Stand des vorigen Geraets mit: vom Headset an der Klinke
    // (Kette an) auf irgendeinen dritten Eingang gewechselt, und die
    // Kette lief dort weiter -- unter Umstaenden als zweite
    // Bearbeitung hinter einer, die das Geraet schon macht. Die
    // sichere Richtung ist aus; wer sie dort will, schaltet sie
    // einmal ein, und ab dann merkt sie sich das.
    const bool soll = (gespeichert == QLatin1String("True"));
    if (m_master->isChecked() != soll) { m_master->setChecked(soll); }
}

void StripWindow::buildUi()
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(10);

    // ── Master ───────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        m_master = new QCheckBox(QStringLiteral("Channel strip"), this);
        m_master->setObjectName(QStringLiteral("stripMaster"));
        m_master->setChecked(chain() && chain()->isEnabled());
        row->addWidget(m_master);

        // ── The AetherSDR monitor buttons (2026-08-11) ───────────────
        //
        // The "Hear myself" live checkbox that stood here is gone —
        // asked for at the bench after a day of almost-right: no
        // direct listening anywhere, only AetherSDR's record-then-
        // listen loop. These are AetherialAudioStrip's two buttons,
        // verbatim in behaviour: Record toggles the capture (30 s cap,
        // auto-plays when it stops), Play replays the last take. The
        // monitor itself lives in MainWindow; this window only hosts
        // the buttons — same division of labour as upstream.
        m_monRecordBtn = new QPushButton(Sprache::T(QStringLiteral("● Record")), this);
        m_monRecordBtn->setStyleSheet(Style::buttonBaseStyle());
        m_monRecordBtn->setToolTip(QStringLiteral(
            "Record your processed voice off the strip (up to 30 s) — "
            "it plays back by itself when you stop. Nothing is "
            "transmitted; the band is quiet while you record and "
            "listen."));
        connect(m_monRecordBtn, &QPushButton::clicked,
                this, &StripWindow::monitorRecordClicked);
        row->addWidget(m_monRecordBtn);

        m_monPlayBtn = new QPushButton(Sprache::T(QStringLiteral("▶ Play")), this);
        m_monPlayBtn->setStyleSheet(Style::buttonBaseStyle());
        m_monPlayBtn->setEnabled(false);
        m_monPlayBtn->setToolTip(QStringLiteral(
            "Replay the last take."));
        connect(m_monPlayBtn, &QPushButton::clicked,
                this, &StripWindow::monitorPlayClicked);
        row->addWidget(m_monPlayBtn);

        // ── Die Aufnahme sichern ────────────────────────────────────
        //
        // „ein speichern sollte auch möglich sein, sodass ich die
        //  aufnahme am desktop speichern kann."
        //
        // Gespeichert wird, was „Abspielen" hören lässt: die Stimme
        // durch die Kette UND durch den gewählten Sendekanal. Eine
        // Datei, die anders klingt als das, was man eben gehört hat,
        // taugt zu keinem Vergleich.
        m_monSaveBtn = new QPushButton(
            Sprache::T(QStringLiteral("Save take…")), this);
        m_monSaveBtn->setObjectName(QStringLiteral("stripMonSave"));
        m_monSaveBtn->setStyleSheet(Style::buttonBaseStyle());
        m_monSaveBtn->setEnabled(false);
        m_monSaveBtn->setToolTip(QStringLiteral(
            "Legt die letzte Aufnahme als WAV ab (48 kHz, mono) — so, "
            "wie sie gerade klingt: mit der eingestellten Kette und "
            "durch den gewählten Kanal."));
        connect(m_monSaveBtn, &QPushButton::clicked,
                this, &StripWindow::monitorSaveClicked);
        row->addWidget(m_monSaveBtn);

        // ── Mithoeren ───────────────────────────────────────────────
        //
        // Das Haekchen, das am 2026-08-11 hinausflog, in einer
        // Fassung, die nicht am Funkgeraet haengt. Siehe
        // core/strip/StripLiveMonitor.h -- und toggleLiveMonitor()
        // fuer die Rueckkopplungswarnung, die kein Schmuck ist:
        // Mikrofon und Lautsprecher im selben Raum pfeifen, und zwar
        // laut.
        m_liveBtn = new QCheckBox(QStringLiteral("Hear myself"), this);
        m_liveBtn->setObjectName(QStringLiteral("stripLiveBtn"));
        m_liveBtn->setToolTip(QStringLiteral(
            "Runs the microphone through the strip and straight back "
            "out of this computer's sound output, so every change is "
            "heard while it is made. Headphones — a loudspeaker here "
            "feeds back. The radio is not touched and nothing is "
            "transmitted."));
        connect(m_liveBtn, &QCheckBox::toggled,
                this, &StripWindow::toggleLiveMonitor);
        row->addWidget(m_liveBtn);

        // ── Die Mithoerreihe steht in ihrer EIGENEN Zeile ────────────
        //
        // Gemessen am 2026-10-12 (tst_tool_windows_fit_the_screen):
        // in einer Zeile mit Kette/Aufnahme/Abspielen verlangte das
        // Fenster 1312 px Mindestbreite -- Latenz 116, Kanal 205,
        // Eingang 199, Ausgang 199, dazu die Knoepfe. Es oeffnet 980
        // und der Schirm des Betreibers hat 1470: die hinteren Teile
        // der Reihe lagen ueber dem Rand, und was ueber dem Rand
        // liegt, ist nicht da. „im channel strip das live hoeren
        // nicht mehr zu sehen!"
        auto* mithoer = new QHBoxLayout;
        {
            auto* lbl = new QLabel(Sprache::T(QStringLiteral("Monitor:")), this);
            lbl->setStyleSheet(dimStyle());
            mithoer->addWidget(lbl);
        }

        // Wie knapp es laufen darf, waehlt der Rechner nicht und ich
        // auch nicht -- es waehlt, wer es hoert. Vorgabe ist die
        // sichere Stufe; die knappen stehen daneben, und der Zaehler
        // unter dem Haekchen sagt sofort, ob die Maschine sie traegt.
        m_liveLatency = new QComboBox(this);
        m_liveLatency->setObjectName(QStringLiteral("stripLiveLatency"));
        m_liveLatency->addItem(QStringLiteral("Safe"),
                               int(StripLiveMonitor::Latency::Safe));
        m_liveLatency->addItem(QStringLiteral("Low"),
                               int(StripLiveMonitor::Latency::Low));
        m_liveLatency->addItem(QStringLiteral("Lowest"),
                               int(StripLiveMonitor::Latency::Lowest));
        m_liveLatency->setToolTip(QStringLiteral(
            "How tight the monitor is allowed to run. Safe first — if "
            "the count of dropped blocks below stays at zero, try the "
            "next one down. Crackling means this computer will not "
            "carry that setting, and nothing but trying tells you."));
        connect(m_liveLatency, &QComboBox::activated, this, [this](int i) {
            const int v = m_liveLatency->itemData(i).toInt();
            AppSettings::instance().setValue(
                QStringLiteral("StripMonitorLatency"), v);
            if (m_live) {
                m_live->setLatencyPreset(
                    static_cast<StripLiveMonitor::Latency>(v));
            }
        });
        {
            const int saved = AppSettings::instance()
                .value(QStringLiteral("StripMonitorLatency"), 0).toInt();
            const int at = m_liveLatency->findData(saved);
            if (at >= 0) { m_liveLatency->setCurrentIndex(at); }
        }
        mithoer->addWidget(m_liveLatency);

        // ── Durch welchen Kanal gehoert wird ─────────────────────
        //
        // Bisher lief im Kopfhoerer die volle Bandbreite der
        // Soundkarte, 20 Hz bis 20 kHz. Der Sender macht daraus 200
        // bis 2500 Hz, oder 50 bis 4050. Wer „Waerme bei 300" und
        // „Schaerfe bei 3,8 k" nach Gehoer einstellt, stellt damit
        // zum Teil Energie ein, die der Sender anschliessend
        // wegwirft -- jedes einzelne Urteil richtig, nur ueber das
        // falsche Signal.
        m_liveChannel = new QComboBox(this);
        m_liveChannel->setSizeAdjustPolicy(
            QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_liveChannel->setMinimumContentsLength(16);
        m_liveChannel->setObjectName(QStringLiteral("stripLiveChannel"));
        m_liveChannel->addItem(QStringLiteral("roh (volle Bandbreite)"), -1);
        for (int i = 0; i < BandLimit::kChannelCount; ++i) {
            m_liveChannel->addItem(
                QString::fromUtf8(BandLimit::channels()[size_t(i)].name), i);
        }
        m_liveChannel->setToolTip(QStringLiteral(
            "Legt die Bandgrenze des Senders in den Mithörweg — so "
            "klingt es auf der Gegenseite, nicht im Studio. Das "
            "Messbild oben bleibt davon unberührt: es zeigt weiter, "
            "was die KETTE tut, nicht was der Kanal davon durchlässt."));
        connect(m_liveChannel, &QComboBox::activated, this, [this](int i) {
            const int v = m_liveChannel->itemData(i).toInt();
            AppSettings::instance().setValue(
                QStringLiteral("StripMonitorChannel"), v);
            if (m_live) { m_live->setMonitorChannel(v); }
            updateLiveNote();
        });
        {
            const int saved = AppSettings::instance()
                .value(QStringLiteral("StripMonitorChannel"), -1).toInt();
            const int at = m_liveChannel->findData(saved);
            if (at >= 0) { m_liveChannel->setCurrentIndex(at); }
        }
        mithoer->addWidget(m_liveChannel);

        // ── Geraetewahl ──────────────────────────────────────────
        //
        // Ohne sie nimmt der Mithoerer die SYSTEMVORGABE. Am Geraet
        // des Betreibers hing ein Apollo Twin X am Thunderbolt und
        // Longpath hoerte trotzdem das eingebaute MacBook-Mikrofon,
        // weil die Vorgabe nie umgestellt war -- jede Beurteilung
        // der Kette lief ueber das falsche Mikrofon.
        const auto baueGeraete = [this](QComboBox* box, bool eingang) {
            box->clear();
            box->addItem(Sprache::T(QStringLiteral("System default")),
                         QByteArray());
            const auto liste = eingang ? QMediaDevices::audioInputs()
                                       : QMediaDevices::audioOutputs();
            for (const QAudioDevice& d : liste) {
                box->addItem(d.description(), d.id());
            }
        };

        m_liveIn = new QComboBox(this);
        m_liveIn->setSizeAdjustPolicy(
            QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_liveIn->setMinimumContentsLength(14);
        m_liveIn->setObjectName(QStringLiteral("stripLiveIn"));
        baueGeraete(m_liveIn, true);
        m_liveIn->setToolTip(Sprache::T(QStringLiteral(
            "Which microphone the strip listens to. Without this it "
            "takes whatever macOS has as the default — plug in an "
            "interface and forget to make it the default, and you "
            "judge the chain through the wrong microphone.")));
        mithoer->addWidget(m_liveIn);

        m_liveOut = new QComboBox(this);
        m_liveOut->setSizeAdjustPolicy(
            QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_liveOut->setMinimumContentsLength(14);
        m_liveOut->setObjectName(QStringLiteral("stripLiveOut"));
        baueGeraete(m_liveOut, false);
        m_liveOut->setToolTip(Sprache::T(QStringLiteral(
            "Where you hear it. Use the headphone output of the same "
            "interface: a different device means a second clock, and "
            "speakers mean the microphone hears itself.")));
        mithoer->addWidget(m_liveOut);

        {
            auto& st = AppSettings::instance();
            const QByteArray inId = st.value(
                QStringLiteral("StripMonitorInput")).toByteArray();
            const QByteArray outId = st.value(
                QStringLiteral("StripMonitorOutput")).toByteArray();
            const int ai = m_liveIn->findData(inId);
            const int ao = m_liveOut->findData(outId);
            if (ai >= 0) { m_liveIn->setCurrentIndex(ai); }
            if (ao >= 0) { m_liveOut->setCurrentIndex(ao); }
            // ── Ein abgestecktes Geraet ist nicht mehr das Geraet ──
            //
            // Gemerkt wird die Kennung, auch wenn der Wandler gerade
            // nicht da ist -- beim naechsten Anstecken soll die Wahl
            // wieder stimmen. Gehoert wird dann aber die
            // Systemvorgabe, und die Kette muss zu DIESER gehoeren.
            // Am 2026-10-14 war der Apollo abgesteckt und das Headset
            // wieder an der Klinke, waehrend die Einstellung noch auf
            // den Apollo zeigte: ohne diese Zeile haette der
            // Hauptschalter den Stand eines Geraets uebernommen, das
            // gar nicht angeschlossen ist.
            const QByteArray wirklich = (ai >= 0)
                ? inId : m_liveIn->currentData().toByteArray();
            if (m_live) {
                m_live->setInputDeviceId(inId);
                m_live->setOutputDeviceId(outId);
            }
            // Und der Hauptschalter so, wie er zu DIESEM Mikrofon
            // gehoert -- siehe wendeKettenStandAn(). Ohne diese Zeile
            // gilt die Zuordnung erst ab dem ersten Wechsel von Hand.
            wendeKettenStandAn(wirklich);
        }

        // Ein Geraetewechsel im Lauf: anhalten, umstellen, anlaufen.
        // Eine Soundkarte laesst sich nicht unter einem laufenden
        // Strom austauschen.
        const auto wechsle = [this]() {
            const QByteArray inId  = m_liveIn->currentData().toByteArray();
            const QByteArray outId = m_liveOut->currentData().toByteArray();
            AppSettings::instance().setValue(
                QStringLiteral("StripMonitorInput"), QString::fromLatin1(inId));
            AppSettings::instance().setValue(
                QStringLiteral("StripMonitorOutput"), QString::fromLatin1(outId));
            // ── Die Kette gehoert zum Mikrofon ───────────────────
            //
            // „wenn ich mit longpath arbeite, dann soll keine kette
            // erfolgen. wenn externes mikrofon, sprich teufel, dann
            // kette" (2026-10-14). Am Apollo steht die Bearbeitung
            // schon in der Console -- eine zweite Kette danach macht
            // sie zweimal. Am Headset an der Klinke ist Longpath die
            // einzige, die es gibt.
            //
            // Also merkt sich der Hauptschalter seinen Stand JE
            // GERAET und stellt sich beim Wechsel selbst. Ein Geraet,
            // zu dem noch nichts gespeichert ist, aendert nichts --
            // die alte Zusicherung „laedt jedes Mal aus" gilt dort
            // weiter.
            wendeKettenStandAn(inId);
            if (!m_live) { return; }
            const bool lief = m_live->isRunning();
            if (lief) { m_live->stop(); }
            m_live->setInputDeviceId(inId);
            m_live->setOutputDeviceId(outId);
            if (lief) { m_live->start(); }
            updateLiveNote();
        };
        connect(m_liveIn, &QComboBox::activated, this,
                [wechsle](int) { wechsle(); });
        connect(m_liveOut, &QComboBox::activated, this,
                [wechsle](int) { wechsle(); });

        m_liveVol = new QSlider(Qt::Horizontal, this);
        m_liveVol->setObjectName(QStringLiteral("stripLiveVol"));
        // 0..200 statt 0..100: die obere Haelfte ist Verstaerkung bis
        // +6 dB. Bisher endete der Regler bei „so laut wie die Kette"
        // und stand ab Werk 3 dB darunter -- wer dann sagte, die
        // Kette habe zu wenig Kraft, hatte recht und meinte diesen
        // Regler.
        m_liveVol->setRange(0, 200);
        // ── Die Lautstärke wird gemerkt, und sie startet leise ──────
        //
        // „musste sofort den kopfhörer abnehmen" (2026-10-14). Der
        // Regler stand bei JEDEM Öffnen auf 100 — voller Pegel, und
        // die Kette drückt ihre Spitzen bis an die Decke des
        // Begrenzers. Mit Kopfhörern ist das kein Fehler, sondern eine
        // Gefahr.
        //
        // Jetzt: gemerkt wie jede andere Einstellung, und ohne
        // gespeicherten Wert auf 50 — hörbar, aber mit Abstand nach
        // oben. Lauter machen kann man in einer Sekunde; was in den
        // Ohren war, bekommt man nicht zurück.
        m_liveVol->setValue(AppSettings::instance()
                                .value(QStringLiteral("StripMonitorVolume"), 50)
                                .toInt());
        m_liveVol->setFixedWidth(110);
        m_liveVol->setToolTip(QStringLiteral(
            "Monitor level. Above the middle it amplifies — up to "
            "+6 dB. It changes what you hear, never what is sent."));
        connect(m_liveVol, &QSlider::valueChanged, this, [this](int v) {
            AppSettings::instance().setValue(
                QStringLiteral("StripMonitorVolume"), v);
            if (m_live) { m_live->setVolume(v / 100.0); }
        });
        mithoer->addWidget(m_liveVol);

        // ── Gleich laut ─────────────────────────────────────────
        //
        // Siehe core/strip/LoudnessMatch.h. Ohne das springt beim
        // Umschalten auf eine Voreinstellung die Lautstärke um
        // zwanzig bis vierzig Dezibel -- jede Kette endet an der
        // Decke des Begrenzers, das rohe Mikrofon liegt weit
        // darunter. Am 2026-10-14 ist der Betreiber deshalb viermal
        // an den Kopfhörer gegangen, zuletzt mit „muss sofort
        // abdrehen und longpath zu machen".
        //
        // Ab Werk an, und zwar nicht nur zum Schutz: lauter klingt
        // immer besser. Solange die Kette lauter ist als das rohe
        // Signal, beurteilt das Ohr die Lautstärke statt der Arbeit.
        m_liveGleich = new QCheckBox(QStringLiteral("same loudness"), this);
        m_liveGleich->setObjectName(QStringLiteral("stripLiveGleichLaut"));
        m_liveGleich->setChecked(
            AppSettings::instance()
                .value(QStringLiteral("StripMonitorLoudnessMatch"),
                       QStringLiteral("True")).toString()
            != QLatin1String("False"));
        m_liveGleich->setToolTip(QStringLiteral(
            "Holds the strip down to the loudness of the raw "
            "microphone, in the headphones only — never on the air. "
            "Without it every preset jumps 20 to 40 dB, and the "
            "louder one always sounds better, whatever it does."));
        connect(m_liveGleich, &QCheckBox::toggled, this, [this](bool on) {
            AppSettings::instance().setValue(
                QStringLiteral("StripMonitorLoudnessMatch"),
                on ? QStringLiteral("True") : QStringLiteral("False"));
            if (m_live) { m_live->setLoudnessMatch(on); }
        });
        mithoer->addWidget(m_liveGleich);

        row->addStretch(1);
        col->addLayout(row);
        mithoer->addStretch(1);
        col->addLayout(mithoer);

        m_liveNote = new QLabel(this);
        m_liveNote->setWordWrap(true);
        m_liveNote->setStyleSheet(dimStyle());
        col->addWidget(m_liveNote);
        updateLiveNote();

        m_note = new QLabel(this);
        m_note->setWordWrap(true);
        m_note->setStyleSheet(dimStyle());
        m_note->setText(noteText());
        col->addWidget(m_note);
    }

    // ── Starting points ──────────────────────────────────────────────
    //
    // First, above everything, because "what should I set all this to"
    // is the question an operator actually arrives with. Eight stages
    // at their defaults is not an answer.
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(QStringLiteral("Preset:"), this));
        m_presetBox = new QComboBox(this);
        m_presetBox->setObjectName(QStringLiteral("stripPresetBox"));
        m_presetBox->setMinimumWidth(200);
        row->addWidget(m_presetBox);

        // „sehe aber keine speicherung der profile" (2026-10-13). Der
        // Knopf war da und hiess „Speichern unter…" -- wer nach einem
        // PROFIL sucht, liest darueber hinweg. Die Zeile heisst
        // „Profil:", also heisst der Knopf jetzt auch so.
        m_presetSave = new QPushButton(
            Sprache::T(QStringLiteral("Save profile…")), this);
        m_presetSave->setToolTip(QStringLiteral(
            "Legt die KOMPLETTE Kette unter einem Namen ab — alle "
            "Stufen, ihre Werte, welche an sind und in welcher "
            "Reihenfolge sie laufen. Deine Änderungen sind auch ohne "
            "das nicht verloren: der Stand wird laufend gespeichert "
            "und kommt beim nächsten Start zurück. Ein Profil "
            "brauchst du, um ZWISCHEN Einstellungen zu wechseln."));
        m_presetSave->setStyleSheet(Style::buttonBaseStyle());
        row->addWidget(m_presetSave);
        m_presetDelete = new QPushButton(QStringLiteral("Delete"), this);
        m_presetDelete->setObjectName(QStringLiteral("stripPresetDelete"));
        m_presetDelete->setStyleSheet(Style::buttonBaseStyle());
        row->addWidget(m_presetDelete);

        // ── Aether-Profil ───────────────────────────────────────────
        //
        // „die einstellungen bei aether perfekt! wir müssen unsere
        //  grafik verwenden, aber alle einstellungen usw. von aether
        //  verwenden." (2026-10-13)
        //
        // Geht nur, weil unsere Stufen aus AetherSDR portiert sind und
        // Zeile fuer Zeile dasselbe rechnen. Was NICHT uebernommen
        // werden kann, sagt der Importeur selbst und steht danach im
        // Hinweis unter der Zeile.
        m_aetherImport = new QPushButton(
            Sprache::T(QStringLiteral("Aether profile…")), this);
        m_aetherImport->setStyleSheet(Style::buttonBaseStyle());
        m_aetherImport->setToolTip(QStringLiteral(
            "Lädt ein Kanalzugprofil aus AetherSDR (.json) — mit "
            "Reihenfolge der Stufen, allen Werten und der Zusage, dass "
            "nichts mitläuft, was es in Aether nicht gab."));
        row->addWidget(m_aetherImport);
        row->addStretch(1);

        // A/B. Held down, not toggled: a comparison you have to keep
        // your finger on is one you cannot walk away from and forget,
        // and forgetting which state you left it in is the failure that
        // makes people distrust their own ears.
        m_compareBtn = new QPushButton(QStringLiteral("A/B — hold"), this);
        m_compareBtn->setStyleSheet(Style::buttonBaseStyle());
        m_compareBtn->setToolTip(QStringLiteral(
            "While held, the strip is bypassed so you hear the raw "
            "microphone. Releasing puts it back exactly as it was."));
        row->addWidget(m_compareBtn);
        col->addLayout(row);

        m_presetNote = new QLabel(this);
        m_presetNote->setObjectName(QStringLiteral("stripPresetNote"));
        m_presetNote->setWordWrap(true);
        m_presetNote->setStyleSheet(dimStyle());
        col->addWidget(m_presetNote);

        rebuildPresetBox();

        connect(m_presetBox, &QComboBox::currentIndexChanged, this,
                [this](int) {
            const QString n = m_presetBox->currentData().toString();
            // Eine andere Kette gewählt: die Scharfstellung des
            // Löschknopfs gilt nicht mehr.
            m_presetDeleteArmed.clear();
            if (!n.isEmpty()) { applyPreset(n); }
            // Der Löschknopf bleibt bedienbar, auch wenn gerade nichts
            // Löschbares gewählt ist: ein ausgegrauter Knopf
            // beantwortet die Frage „warum kann ich das nicht
            // löschen" nicht, er stellt sie. Die Antwort steht nach
            // dem Druck in der Zeile darunter.
        });
        connect(m_presetSave, &QPushButton::clicked,
                this, &StripWindow::saveUserPreset);
        connect(m_presetDelete, &QPushButton::clicked,
                this, &StripWindow::deleteUserPreset);
        connect(m_aetherImport, &QPushButton::clicked,
                this, &StripWindow::importAetherPreset);

        connect(m_compareBtn, &QPushButton::pressed, this, [this]() {
            if (StripChain* c = chain()) {
                m_masterBeforeCompare = c->isEnabled();
                c->setEnabled(false);
                refreshChainRow();
            }
        });
        connect(m_compareBtn, &QPushButton::released, this, [this]() {
            if (StripChain* c = chain()) {
                c->setEnabled(m_masterBeforeCompare);
                refreshChainRow();
            }
        });
    }

    // ── Chain row ────────────────────────────────────────────────────
    //
    // Painted rather than eight labels: each tile carries its own
    // gain-reduction bar, and the whole chain's behaviour becomes one
    // glance instead of eight numbers to read while speaking.
    m_chainView = new StripChainView(this);
    m_chainView->setChain(chain());
    connect(m_chainView, &StripChainView::stageClicked, this, [this](int i) {
        // +1: vorne steht jetzt der Uebersichtsreiter.
        if (m_tabs) { m_tabs->setCurrentIndex(i + 1); }
    });
    // ── Nur diese Stufe hoeren, und zurueck ──────────────────────
    //
    // „hier sollten wir morgen jedes einzelne probieren"
    // (2026-10-14). Die Wahltaste auf einer Kachel laesst genau diese
    // Stufe laufen und legt alle anderen schlafen; dieselbe Geste
    // noch einmal stellt den Stand von vorher wieder her -- und zwar
    // den GANZEN Stand, nicht „alles an". Wer vier Stufen bewusst aus
    // hatte, bekommt sie nicht ungefragt zurueck.
    //
    // Der Hauptschalter wird dabei EINGESCHALTET, wenn er aus war:
    // „nur das Gate hoeren" bei abgeschalteter Kette waere ein Griff
    // ins Leere, und genau solche stillen Nichtwirkungen haben den
    // 2026-10-14 so lang gemacht.
    connect(m_chainView, &StripChainView::stageSoloRequested,
            this, [this](int i) {
        StripChain* c = chain();
        if (c == nullptr || i < 0 || i >= StripChain::kStageCount) { return; }
        const auto stufe = static_cast<StripChain::Stage>(i);
        if (m_soloStufe == i) {
            for (int k = 0; k < StripChain::kStageCount; ++k) {
                c->setStageEnabled(static_cast<StripChain::Stage>(k),
                                   m_soloVorher[size_t(k)]);
            }
            if (m_master && m_soloMasterVorher != m_master->isChecked()) {
                m_master->setChecked(m_soloMasterVorher);
            }
            m_soloStufe = -1;
        } else {
            if (m_soloStufe < 0) {
                for (int k = 0; k < StripChain::kStageCount; ++k) {
                    m_soloVorher[size_t(k)] = c->stageEnabled(
                        static_cast<StripChain::Stage>(k));
                }
                m_soloMasterVorher = m_master && m_master->isChecked();
            }
            for (int k = 0; k < StripChain::kStageCount; ++k) {
                c->setStageEnabled(static_cast<StripChain::Stage>(k),
                                   k == i);
            }
            if (m_master && !m_master->isChecked()) {
                m_master->setChecked(true);
            }
            m_soloStufe = i;
            if (m_tabs) { m_tabs->setCurrentIndex(i + 1); }
        }
        refreshChainRow();
        updateLiveNote();
        persist();
        Q_UNUSED(stufe);
    });

    connect(m_chainView, &StripChainView::pluginDropped,
            this, &StripWindow::dropPluginAt);
    connect(m_chainView, &StripChainView::insertRemoveRequested,
            this, &StripWindow::clearInsert);
    connect(m_chainView, &StripChainView::insertClicked, this, [this](int) {
        if (m_tabs) { m_tabs->setCurrentIndex(m_tabs->count() - 1); }
    });
    connect(m_chainView, &StripChainView::insertEditorRequested,
            this, [this](int slot) {
        StripChain* c = chain();
        AudioPlugin* p = c != nullptr ? c->insertPlugin(slot) : nullptr;
        if (p != nullptr) { p->showEditor(p->descriptor().name); }
    });
    col->addWidget(m_chainView);

    // In, out, and the difference. Directly under the chain, because
    // the chain is what changed the level and this is by how much.
    m_levels = new StripLevelBars(this);
    m_levels->setChain(chain());
    col->addWidget(m_levels);

    // ── One tab per stage ────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    // Die Uebersicht steht vorn, die acht Stufenseiten dahinter --
    // siehe buildAllStagesSurface(). Die Karten springen per Klick in
    // ihre Seite, die unveraendert weiterlebt.
    m_tabs->addTab(new StagePageScroll(buildAllStagesSurface()),
                   QStringLiteral("Strip"));
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<StripChain::Stage>(i);
        QWidget* page = nullptr;
        switch (s) {
        case StripChain::Stage::Gate:  page = buildGatePanel();  break;
        case StripChain::Stage::Eq:    page = buildEqPanel();    break;
        case StripChain::Stage::DeEss: page = buildDeEssPanel(); break;
        case StripChain::Stage::Comp:    page = buildCompPanel();    break;
        case StripChain::Stage::Denoise: page = buildDenoisePanel(); break;
        case StripChain::Stage::Clip:    page = buildClipperPanel(); break;
        case StripChain::Stage::Tube:    page = buildTubePanel();    break;
        case StripChain::Stage::Pudu:    page = buildPuduPanel();    break;
        case StripChain::Stage::Reverb:  page = buildReverbPanel();  break;
        case StripChain::Stage::Limiter: page = buildLimiterPanel(); break;
        default:                         page = buildPlaceholder(s); break;
        }
        m_tabs->addTab(new StagePageScroll(page),
                       QString::fromLatin1(StripChain::stageName(s)));
    }

    // After all the stages, because it measures the result of them.
    m_tabs->addTab(buildTxSpectrumPanel(), QStringLiteral("On air"));

    // ── Voice check, embedded (2026-08-11) ───────────────────────────
    // Asked for at the bench: one window for the whole loop — change a
    // stage, record, listen, compare. The dialog already knows how;
    // embedding it here retires the standalone window. Created once and
    // parented to the window (see the member note), then handed to the
    // tab bar.
    m_voiceCheck = new TxVoiceCheckDialog(m_radio, this, /*embedded=*/true);
    // In a scroll area like every stage page, and for the same reason
    // the comment on StagePageScroll gives: a page taller than the
    // window loses its bottom behind the Dock with no way to reach
    // it. This tab was the one exception, and it got away with it
    // until the broadcast optimiser added a section to the end of it
    // — at which point the whole optimiser was simply off the bottom
    // of the window, invisible, with the tab looking perfectly
    // normal. Found at the bench, 2026-10-09.
    m_tabs->addTab(new StagePageScroll(m_voiceCheck),
                   QStringLiteral("Voice check"));

    // Zuletzt, und das ist die Aussage: Einschuebe sind ein Zusatz,
    // kein Weg. Siehe buildInsertsPanel().
    m_tabs->addTab(new StagePageScroll(buildInsertsPanel()),
                   QStringLiteral("Plugins"));

    // ── From a profile straight to its curve ─────────────────────────
    //
    // Double-clicking a width in the broadcast optimiser, or its
    // "Edit curves…" button, puts that profile into the chain and
    // then wants the equaliser on screen. The dialog has already done
    // the applying by the time this arrives; all that is left is to
    // go there. Asked for 2026-10-09.
    //
    // The tab is found by the stage's own name rather than by a
    // remembered index: the tabs are built in a loop over the stages,
    // and an index captured here would quietly point at the wrong
    // page the first time a stage is added or reordered.
    connect(m_voiceCheck, &TxVoiceCheckDialog::curveEditRequested,
            this, [this]() {
        if (!m_tabs) { return; }
        const QString eq = Sprache::T(QString::fromLatin1(
            StripChain::stageName(StripChain::Stage::Eq)));
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (m_tabs->tabText(i) == eq) {
                m_tabs->setCurrentIndex(i);
                return;
            }
        }
    });

    col->addWidget(m_tabs, 1);

    // Die Kachel der aufgeschlagenen Seite bekommt ihren Ring. Ueber
    // den Reiternamen, nicht ueber die Nummer: die Reiter werden beim
    // Umbauen neu gesetzt, und die Voice-check-Seite kommt und geht --
    // eine Nummer waere nach dem ersten Umbau die falsche Kachel.
    const auto markOpenStage = [this]() {
        if (m_tabs == nullptr || m_chainView == nullptr) { return; }
        const QString t = m_tabs->tabText(m_tabs->currentIndex());
        int found = -1;
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            const auto s = static_cast<StripChain::Stage>(i);
            if (t == Sprache::T(
                    QString::fromLatin1(StripChain::stageName(s)))) {
                found = i;
                break;
            }
        }
        m_chainView->setOpenStage(found);

        // ── Von selbst suchen ────────────────────────────────────
        //
        // „ich muss auch immer auf look for plugins klicken. diese
        //  sollten automatisch zur auswahl stehen."
        //
        // Der Knopf bleibt -- als „Erneut suchen" --, aber er ist
        // nicht mehr die Voraussetzung. Der Grund, aus dem beim
        // Programmstart NICHT gesucht wird, bleibt davon unberührt:
        // wer die Seite nie aufmacht, lädt nie fremden Code. Das
        // Aufschlagen des Reiters IST die Zustimmung; ein zweiter
        // Klick danach war nur eine Hürde.
        if (!m_pluginsScanned
            && t == Sprache::T(QStringLiteral("Plugins"))) {
            scanPlugins();
        }
    };
    connect(m_tabs, &QTabWidget::currentChanged,
            this, [markOpenStage](int) { markOpenStage(); });
    markOpenStage();

    // The enable boxes are created unchecked and the chain may already
    // have stages on from the saved settings. Sync with signals blocked
    // so this does not write the pre-sync state straight back out.
    if (StripChain* c = chain()) {
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            QCheckBox* box = m_stageBoxes[static_cast<size_t>(i)];
            if (!box) { continue; }
            const QSignalBlocker block(box);
            box->setChecked(c->stageEnabled(static_cast<StripChain::Stage>(i)));
        }
    }

    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    closeRow->addWidget(closeBtn);
    col->addLayout(closeRow);

    connect(m_master, &QCheckBox::toggled, this, [this](bool on) {
        // The master itself is not saved — it loads off every time, on
        // purpose — but toggling it is a good moment to flush the rest.
        if (StripChain* c = chain()) { c->setEnabled(on); }
        // Und das Mithoeren mit. Das fehlte, und es war der Fehler, den
        // der Betreiber als „komische modulation im lautsprecher [...]
        // wie ueberschlagen" gehoert hat: der Hauptschalter stand AUS,
        // der Mithoerer lief trotzdem durch die ganze Kette, und ein
        // Gate mit -38 dBFS Schwelle und 320 ms Nachlauf auf dem
        // eingebauten Mikrofon eines MacBook pumpt genau so. Der Satz
        // unter dem Haekchen versprach das Richtige („switch the strip
        // off and on to hear what it is doing") -- nur tat es niemand.
        if (m_live) { m_live->setStripBypassed(!on); }
        // Und die Zeile darunter nachziehen, sonst steht dort weiter
        // „through the strip", waehrend der Schalter aus ist -- und
        // dann glaubt man dem Schalter nicht mehr.
        updateLiveNote();
        refreshChainRow();
        // Und merken, zu welchem Mikrofon dieser Stand gehoert --
        // siehe wendeKettenStandAn().
        if (m_liveIn) {
            AppSettings::instance().setValue(
                kettenSchluessel(m_liveIn->currentData().toByteArray()),
                on ? QStringLiteral("True") : QStringLiteral("False"));
        }
        persist();
    });

    // Meters only. Parameters are pushed on change, not polled — a
    // window that writes settings on a timer fights the operator's
    // hand.
    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(100);
    connect(m_meterTimer, &QTimer::timeout, this, &StripWindow::refreshMeters);
    m_meterTimer->start();

    setMinimumWidth(560);

    // ── Zum Schluss: alles auf Deutsch ───────────────────────────────
    //
    // Ein Durchgang durch den fertigen Baum statt zweihundert
    // Änderungen an den Aufrufen. Warum so und nicht mit tr(), steht
    // in core/Sprache.h. Was in der Tabelle fehlt, bleibt englisch --
    // sichtbar, und damit auffindbar.
    Sprache::uebersetzeBaum(this);
}

// ── Gate ─────────────────────────────────────────────────────────────

// ── Eine Beschriftung, die sich laufend ändert ───────────────────────
//
// „schrift springt noch immer herum."
//
// Jede Beschriftung, deren Text sich zehnmal je Sekunde ändert, ist
// eine Gefahr für das Fensterlayout: Qt rechnet aus ihrem Text eine
// Wunschbreite, und sobald aus „-9.1 dB" ein „-29.1 dB" wird, wächst
// die Spalte, in der sie steht -- und alles daneben rutscht. Zehnmal
// je Sekunde sieht man das als Zittern.
//
// Ignored als waagrechte Grösse heisst: diese Beschriftung meldet
// KEINEN Platzbedarf an. Sie bekommt, was da ist, und schneidet ab,
// was nicht hineinpasst. Dazu eine feste Höhe, damit auch ein
// Umbruch nichts verschieben kann, und die Hausschrift für Zahlen.
void lebendeZeile(QLabel* l)
{
    if (l == nullptr) { return; }
    l->setWordWrap(false);
    l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    l->setFixedHeight(18);
    l->setTextInteractionFlags(Qt::NoTextInteraction);
}

QWidget* StripWindow::buildGatePanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Gate);
    auto* on = new QCheckBox(QStringLiteral("Gate on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_gateCurve = new StripDynamicsCurve(StripDynamicsCurve::Stage::Gate, page);
    m_gateCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_gateCurve,
                                StripChain::Stage::Gate));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Gate, v);
        }
        refreshChainRow();
        persist();
    });

    // The two-entry "Character" combo that used to sit here is gone. It
    // wrote ratio and depth, and so does the character picker on the
    // card above — two controls owning one piece of state, which is the
    // drift this project has already paid for more than once. The
    // picker supersedes it and offers four more entries.

    addKnob(form, QStringLiteral("Threshold"),
        -80.0, 0.0, 1.0, cur([this]{ return chain()->gate().thresholdDb(); }, -40.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setThresholdDb(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1, cur([this]{ return chain()->gate().attackMs(); }, 0.5),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    // The step used to be `cur(comp().attackMs(), 5.0)` — a copy-paste
    // from the compressor panel that made the gate's Hold slider step by
    // whatever the COMPRESSOR's attack happened to be. Invisible while
    // both defaulted to 5 ms, and a 5000-step slider the moment anyone
    // set a fast compressor attack.
    addKnob(form, QStringLiteral("Hold"), 0.0, 500.0, 5.0,
        cur([this]{ return chain()->gate().holdMs(); }, 20.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setHoldMs(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Release"), 5.0, 2000.0, 5.0, cur([this]{ return chain()->gate().releaseMs(); }, 100.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setReleaseMs(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Depth"), -80.0, 0.0, 1.0, cur([this]{ return chain()->gate().floorDb(); }, -15.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setFloorDb(float(v)); }
        }, 0);
    addKnob(form, QStringLiteral("Hysteresis"), 0.0, 20.0, 0.5, cur([this]{ return chain()->gate().returnDb(); }, 2.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->gate().setReturnDb(float(v)); }
        }, 1, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Threshold is the level below which the gate starts working — set "
        "it between your voice and the room, not at either. Hysteresis is "
        "what stops it chattering when you sit right on the threshold: the "
        "gate stays open until the level drops this much further."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);

    m_gateMeter = new QLabel(page);
    m_gateMeter->setStyleSheet(dimStyle());
    lebendeZeile(m_gateMeter);
    form->addRow(m_gateMeter);

    return page;
}

// ── EQ ───────────────────────────────────────────────────────────────
//
// Sixteen bands is what the DSP offers and far more than a voice needs,
// so this panel does not expose sixteen sliders. It lays the bands out
// once, in a fixed order, and gives each group the control that group
// is actually for:
//
//   band 0      high-pass — the single most useful control on a
//               transmit EQ, and the one that fixes rumble and hum
//   bands 1-3   notches at the mains frequency and its first two
//               harmonics, switched as a group
//   bands 4-6   low, presence and high tone controls
//
// The fixed layout is also what makes the saved settings meaningful:
// band 2 is always the first mains harmonic, in the file as on screen.






QWidget* StripWindow::buildEqPanel()
{
    // ── AetherSDR's equaliser, whole ─────────────────────────────────
    //
    // Asked for at the bench: "delete what is here and take AetherSDR's".
    // What stood here before was roughly 700 lines of Longpath-original
    // interface — the fifteen-second take recorder, editable target
    // curves with A/B slots, loudness-matched bypass, match-EQ from a
    // WAV, band solo, the numeric table, the folding sections. All of it
    // is gone from the tab; all of it is still in the history, and this
    // commit is a single `git revert` away from bringing it back.
    //
    // The DSP underneath never changed: core/strip/ClientEq was already
    // a verbatim port of AetherSDR's at the same revision. This puts the
    // interface that was written for it back on top of it.
    //
    // EqHost is the seam. See gui/applets/eq/EqHost.h — the ported
    // widgets talk to an AudioEngine with five methods on it, and rather
    // than editing five borrowed files to reach into StripChain, the
    // five names are presented over Longpath's own parts. The borrowed
    // code stays byte-comparable against upstream.
    if (!m_eqHost) {
        m_eqHost = std::make_unique<EqHost>(chain());
    } else {
        m_eqHost->setChain(chain());
    }

    m_eqPanel = new StripEqPanel(m_eqHost.get(), this);
    m_eqPanel->showForPath(ClientEqApplet::Path::Tx);

    // ── Ein Schalter, wie ihn jede andere Stufe hat ──────────────────
    //
    // „den EQ kann man eigentlich nicht on und off schalten." Stimmt:
    // die sieben anderen Stufenseiten tragen oben ihr Haekchen, diese
    // nicht -- sie ist als ganzes Panel von AetherSDR uebernommen und
    // hat dort keines, weil der Entzerrer dort das ganze Werkzeug ist
    // und nicht eine Stufe von acht.
    //
    // Gerade beim Entzerren ist es aber der wichtigste Knopf: eine
    // Kurve beurteilt man, indem man sie wegnimmt. Also davor
    // gesetzt, mit demselben Namen und demselben Verhalten wie bei
    // Gate und Kompressor -- und in dasselbe m_stageBoxes-Feld, damit
    // reloadControls() ihn mitzieht.
    auto* page = new QWidget(this);
    auto* col = new QVBoxLayout(page);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(6);

    const int idx = static_cast<int>(StripChain::Stage::Eq);
    auto* on = new QCheckBox(QStringLiteral("EQ on"), page);
    on->setToolTip(QStringLiteral(
        "Off, the curve is drawn but not applied — the audio passes "
        "through untouched. That is how you judge a curve: by taking "
        "it away."));
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Eq, v);
        }
        refreshChainRow();
        refreshStageCards();
        persist();
    });
    // ── „Flat" -- die Kurve wirklich loeschen ────────────────────────
    //
    // „ein loeschen der kurve sollte auch im EQ vorhanden sein."
    //
    // Den portierten Knopf „Reset" gibt es, aber er tut etwas
    // anderes: er stellt die VORGABEKURVE her (Hochpass bei 100 Hz,
    // Kuhschwanz unten, eine Glocke bei 2 kHz). Das ist nicht nichts.
    // Und er steht am rechten Ende der uebernommenen Knopfreihe, wo
    // er bei schmalem Fenster als erstes verschwindet.
    //
    // Dieser hier macht die Kurve flach: jedes Band auf 0 dB und
    // ueberbrueckt. Danach geht wirklich nichts mehr durch den
    // Entzerrer, und man faengt von vorne an. Ein Zug an einem
    // Punkt schaltet das Band von selbst wieder ein
    // (ClientEqEditorCanvas.cpp:143), also ist der Weg zurueck ein
    // Handgriff.
    //
    // Ueber EqHost gespeichert und damit EIN Widerrufsschritt wie
    // jeder andere -- eine Loeschung, die man nicht zuruecknehmen
    // kann, traut sich niemand zu druecken.
    auto* flat = new QPushButton(QStringLiteral("Flat"), page);
    flat->setStyleSheet(Style::buttonBaseStyle());
    flat->setToolTip(QStringLiteral(
        "Clear the curve: every band to 0 dB and bypassed, so the "
        "equaliser does nothing at all. One undo step. “Reset” in the "
        "row below is different — that brings back the default curve, "
        "which is not the same as none."));
    connect(flat, &QPushButton::clicked, this, [this]() {
        StripChain* c = chain();
        if (!c) { return; }
        for (int i = 0; i < EqBandLayout::kBandCount; ++i) {
            ClientEq::BandParams p = c->eq().band(i);
            p.gainDb = 0.0f;
            p.enabled = false;
            c->eq().setBand(i, p);
        }
        // EINS, nicht null. setMasterGain nimmt einen LINEAREN
        // Faktor (Vorgabe 1,0, gedeckelt auf 0..4), keine Dezibel.
        // Hier stand 0.0f -- damit multiplizierte der Entzerrer alles
        // mit null, und der Betreiber meldete zu Recht: „wenn ich
        // channel strip aktiviere hoere ich gar nichts". Das Protokoll
        // zeigte es unmissverstaendlich: in -33 dBFS, out -120 dBFS.
        //
        // Der Fehler ist die naheliegendste Verwechslung in diesem
        // ganzen Fenster: jeder andere Pegel hier ist in Dezibel, wo
        // null „unveraendert" heisst. Genau bei dem einen, der es
        // nicht ist, heisst null „aus".
        c->eq().setMasterGain(1.0f);
        if (m_eqHost) { m_eqHost->saveClientEqSettings(); }
        if (m_eqPanel) { m_eqPanel->refreshFromEngine(); }
        refreshStageCards();
        persist();
    });

    // ── Wenn der Ausgangssteller unten steht ─────────────────────────
    //
    // Der Entzerrer hat einen eigenen Ausgangspegel (den Schieber
    // rechts im portierten Panel). Steht er auf null, ist die ganze
    // Kette stumm, und nichts sagt es -- man sucht den Fehler
    // ueberall sonst. Ich weiss das, weil ich ihn selbst auf null
    // gesetzt habe und eine Stunde lang woanders gesucht wurde.
    m_eqMuteWarning = new QLabel(page);
    m_eqMuteWarning->setWordWrap(true);
    m_eqMuteWarning->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; }")
        .arg(QString::fromLatin1(Style::kAmberText)));
    m_eqMuteWarning->hide();

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->addWidget(on);
    head->addSpacing(12);
    head->addWidget(flat);
    head->addStretch(1);
    col->addLayout(head);
    col->addWidget(m_eqMuteWarning);
    col->addWidget(m_eqPanel, 1);
    return page;
}

// ── What actually goes out ───────────────────────────────────────────

QWidget* StripWindow::buildTxSpectrumPanel()
{
    auto* page = new QWidget(this);
    auto* col = new QVBoxLayout(page);
    col->setContentsMargins(8, 8, 8, 8);
    col->setSpacing(7);

    m_txSpectrum = new TxSpectrumWidget(page);
    if (m_radio && m_radio->audioEngine()) {
        m_txSpectrum->setSource(&m_radio->audioEngine()->txSiphonSpectrum());
    }
    col->addWidget(m_txSpectrum, 1);

    auto* row = new QHBoxLayout;
    row->setSpacing(7);

    auto* hold = new QPushButton(QStringLiteral("Hold"), page);
    hold->setCheckable(true);
    hold->setStyleSheet(Style::buttonBaseStyle());
    hold->setToolTip(QStringLiteral(
        "Keep the highest level reached in every bin. A live curve "
        "during a call is unreadable; this is what you look at "
        "afterwards."));
    row->addWidget(hold);

    auto* reset = new QPushButton(QStringLiteral("Reset"), page);
    reset->setStyleSheet(Style::buttonBaseStyle());
    row->addWidget(reset);
    row->addStretch(1);
    col->addLayout(row);

    connect(hold, &QPushButton::toggled, this, [this](bool on) {
        if (m_txSpectrum) { m_txSpectrum->setHold(on); }
    });
    connect(reset, &QPushButton::clicked, this, [this]() {
        if (m_txSpectrum) { m_txSpectrum->resetHold(); }
    });

    // ── The sentence, which is the point ─────────────────────────────
    auto* advice = new QLabel(page);
    advice->setWordWrap(true);
    advice->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(advice);

    connect(m_txSpectrum, &TxSpectrumWidget::measured, this,
            [advice](const TxSpectrumAnalysis::Occupancy& occ) {
        // The filter width the advice is judged against. 2.7 kHz is the
        // usual SSB filter; reading the real one out of the radio would
        // be better and is a wire that does not exist yet, so the
        // number is stated rather than assumed silently.
        const QString a = TxSpectrumAnalysis::advice(occ, 2700.0);
        advice->setText(a.isEmpty()
            ? Sprache::T(QStringLiteral("Within a normal 2.7 kHz SSB filter."))
            : a);
        advice->setVisible(true);
    });

    auto* note = new QLabel(QStringLiteral(
        "This is the post-modulator siphon: what the transmit chain "
        "produced, before the amplifier, the filters and the antenna. "
        "A clean reading here with a dirty signal on the air means the "
        "problem is downstream of this point."), page);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextScale)));
    col->addWidget(note);

    return page;
}

// ── De-esser ─────────────────────────────────────────────────────────

QWidget* StripWindow::buildDeEssPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::DeEss);
    auto* on = new QCheckBox(QStringLiteral("De-esser on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_deEssCurve = new StripBandCurve(StripBandCurve::Stage::DeEsser, page);
    m_deEssCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_deEssCurve,
                                StripChain::Stage::DeEss));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::DeEss, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Frequency"), 1000.0, 12000.0, 100.0, cur([this]{ return chain()->deEss().frequencyHz(); }, 6000.0),
        QStringLiteral("Hz"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setFrequencyHz(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Width"), 0.5, 5.0, 0.1, cur([this]{ return chain()->deEss().q(); }, 2.0),
        QStringLiteral("Q"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setQ(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Threshold"), -60.0, 0.0, 1.0, cur([this]{ return chain()->deEss().thresholdDb(); }, -25.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setThresholdDb(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Amount"), -24.0, 0.0, 0.5, cur([this]{ return chain()->deEss().amountDb(); }, -6.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setAmountDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Attack"), 0.1, 30.0, 0.1, cur([this]{ return chain()->deEss().attackMs(); }, 1.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Release"), 10.0, 500.0, 5.0, cur([this]{ return chain()->deEss().releaseMs(); }, 80.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->deEss().setReleaseMs(float(v)); }
        }, 0, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Set the frequency by ear on the word \u201csix\u201d, not by the number: "
        "where sibilance lives depends on the voice and the microphone, "
        "and 6 kHz is only a place to start.\n\n"
        "Amount is a limit, not a setting — it is the most the de-esser "
        "may take off. Too much of it turns an s into a th, which is "
        "harder to listen to than the sibilance was.\n\n"
        "Threshold is read on the SIDECHAIN, not on the whole signal — "
        "and the sidechain runs about 13 dB quieter, because it is one "
        "narrow band out of the voice. Measured on speech peaking at "
        "\u221212 dBFS, the sidechain reaches \u221225 dBFS: a threshold of "
        "\u221228 never fires at all, and \u221236 is where it starts to "
        "work. If this stage shows no reduction, the threshold is too "
        "high, not the voice too clean.\n\n"
        "And watch the channel: at 6 kHz the 200\u20132500 Hz channel cuts "
        "64 dB and the 2700 one 58 dB, so there the transmitter throws "
        "away everything this stage is guarding. It earns its place in "
        "the 4 kHz channel."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);

    m_deEssMeter = new QLabel(page);
    m_deEssMeter->setStyleSheet(dimStyle());
    lebendeZeile(m_deEssMeter);
    form->addRow(m_deEssMeter);

    return page;
}

// ── Compressor ───────────────────────────────────────────────────────

QWidget* StripWindow::buildCompPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Comp);
    auto* on = new QCheckBox(QStringLiteral("Compressor on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_compCurve = new StripDynamicsCurve(StripDynamicsCurve::Stage::Compressor, page);
    m_compCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_compCurve,
                                StripChain::Stage::Comp));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Comp, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Threshold"), -60.0, 0.0, 1.0, cur([this]{ return chain()->comp().thresholdDb(); }, -20.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setThresholdDb(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Ratio"), 1.0, 20.0, 0.5, cur([this]{ return chain()->comp().ratio(); }, 3.0),
        QStringLiteral(": 1"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setRatio(float(v)); }
        }, 1, [this]{ persist(); });
    // Opened at a literal 5.0 rather than at the chain's value, so the
    // compressor's attack was reset to 5 ms every time the panel was
    // built — and rebuilt is what the panel does on every preset and
    // every character.
    addKnob(form, QStringLiteral("Attack"), 0.1, 100.0, 0.1,
        cur([this]{ return chain()->comp().attackMs(); }, 5.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setAttackMs(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Release"), 5.0, 2000.0, 5.0, cur([this]{ return chain()->comp().releaseMs(); }, 120.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setReleaseMs(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Knee"), 0.0, 24.0, 0.5, cur([this]{ return chain()->comp().kneeDb(); }, 6.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setKneeDb(float(v)); }
        }, 1, [this]{ persist(); });
    // The saved value was in the MINIMUM slot and the value was a
    // literal zero, so the knob's range started wherever make-up
    // happened to be and the knob itself always opened at the bottom of
    // it. Slots are (min, max, step, value).
    addKnob(form, QStringLiteral("Make-up"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->comp().makeupDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->comp().setMakeupDb(float(v)); }
        }, 1, [this]{ persist(); });

    // ── Der Ausgleich ────────────────────────────────────────────────
    //
    // Direkt unter dem Makeup, weil er dasselbe tut -- nur
    // ausgerechnet statt geraten. Ohne ihn macht mehr Kompression die
    // Stimme DUENNER: der Kompressor nimmt den Pegel weg, den der
    // Clipper braucht, und der Clipper ist die Stufe, die Druck
    // macht. Gemessen am 2026-10-10 ueber vier Einstellungen
    // (tst_bandbreiten_genau::autoMakeupWouldStopTheInversion).
    auto* autoMk = new QCheckBox(
        QStringLiteral("Make-up follows threshold and ratio"), page);
    autoMk->setChecked(chain() ? chain()->comp().autoMakeup() : false);
    autoMk->setToolTip(QStringLiteral(
        "Adds back exactly what the curve takes off a −12 dBFS peak: "
        "(−12 − threshold) × (1 − 1/ratio). Without it, turning the "
        "compressor up makes the voice thinner, because it steals the "
        "level the clipper needs — measured −15.9 → −19.4 LUFS across "
        "3:1 to 8:1. With it, the same range reads −13.0 → −10.0."));
    form->addRow(autoMk);
    connect(autoMk, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) { c->comp().setAutoMakeup(v); }
        persist();
    });

    // The phase rotator earns its own row and its own sentence: it is
    // the one control here that raises average power without making
    // anything louder, and nobody guesses that from the name.
    auto* rot = new QSpinBox(page);
    rot->setRange(0, 8);
    rot->setValue(chain() ? chain()->comp().phaseRotatorStages() : 0);
    form->addRow(QStringLiteral("Phase rotator"), rot);
    connect(rot, &QSpinBox::valueChanged, this, [this](int v) {
        if (StripChain* c = chain()) { c->comp().setPhaseRotatorStages(v); }
        persist();
    });

    // ── How to set it ────────────────────────────────────────────────
    //
    // The compressor had no setting guidance at all — the only help text
    // on this tab was about the phase rotator, which is the smallest
    // control on it. Asked for at the bench and correct: a compressor is
    // four interacting numbers and the order you set them in is most of
    // the skill.
    auto* order = new QLabel(QStringLiteral(
        "<b>Set them in this order.</b> Each one changes what the next "
        "one should be, so going round the panel left to right means "
        "doing it twice."
        "<ol style='margin-left:-22px'>"
        "<li><b>Ratio</b> first — it decides what kind of processing this "
        "is. 2:1 is levelling, 6:1 is talk power, 10:1 and up is an "
        "effect.</li>"
        "<li><b>Threshold</b> next, watching the gain reduction. Aim for "
        "6 to 8 dB on your loudest words and nothing at all on your "
        "quietest. If it never moves, the threshold is too high; if it "
        "never returns to zero, too low.</li>"
        "<li><b>Attack</b>. Fast catches the peak and dulls the "
        "consonant that made it; slow lets the consonant through and "
        "leaves a peak for the limiter. 5–10 ms keeps the words crisp; "
        "under 2 ms is for when the limiter is working too hard.</li>"
        "<li><b>Release</b> last, by ear on continuous speech. Too fast "
        "and the background pumps up between words; too slow and one "
        "loud word ducks the sentence after it. 100–200 ms suits most "
        "voices.</li>"
        "</ol>"), page);
    order->setWordWrap(true);
    order->setStyleSheet(dimStyle());
    form->addRow(order);

    auto* knee = new QLabel(QStringLiteral(
        "<b>Knee</b> is how abruptly the ratio arrives. A wide knee "
        "starts compressing below the threshold and reaches the full "
        "ratio above it, which is why a 3:1 setting with a 10 dB knee "
        "sounds gentler than a 3:1 with a hard one — the number is the "
        "same and the sound is not.<br><br>"
        "<b>Make-up</b> is not a tone control. Set it so the stage is as "
        "loud switched on as it is switched off, then judge. Everything "
        "sounds better louder, and a comparison that is not level-matched "
        "is not a comparison — it is the make-up gain winning."), page);
    knee->setWordWrap(true);
    knee->setStyleSheet(dimStyle());
    form->addRow(knee);

    auto* help = new QLabel(QStringLiteral(
        "The phase rotator makes speech more symmetrical without changing "
        "how it sounds. Speech has lopsided peaks — one polarity reaches "
        "the limit while the other still has room — so evening them out "
        "buys a few decibels of average power for free. Four stages is a "
        "typical setting; more is not better."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);

    m_compMeter = new QLabel(page);
    m_compMeter->setStyleSheet(dimStyle());
    lebendeZeile(m_compMeter);
    form->addRow(m_compMeter);

    return page;
}

// ── Denoise ──────────────────────────────────────────────────────────
//
// Die neunte Stufe, und die einzige mit einer Laufzeit, die man
// hoert. Deshalb steht der Preis auf der Seite und nicht in einer
// Anleitung: 30 ms, solange sie an ist.
//
// Hier stand 10 ms, weil rnnoise in Bloecken von 480 Abtastwerten
// rechnet. Am Geraet gemessen (tst_bandbreiten_genau) kommt ein Ton
// aber 1440 Abtastwerte spaeter heraus: rnnoise liefert seinen
// Ausgang zwei Rahmen hinter seinem Eingang, und der trockene Anteil
// laeuft seit dem 2026-10-10 genauso lange mit, damit die Mischung
// nicht kaemmt.

QWidget* StripWindow::buildDenoisePanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Denoise);
    auto* on = new QCheckBox(QStringLiteral("Denoise on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Denoise, v);
        }
        refreshChainRow();
        updateLiveNote();
        persist();
    });

    auto* what = new QLabel(
        QStringLiteral(
            "Takes steady room noise out of the voice — fan, hum, traffic — "
            "and it keeps working while you speak. That is what the gate "
            "cannot do: the gate only ducks the gaps between words.\n\n"
            "It runs first in the chain, so everything after it sees a "
            "clean voice. That matters most for the compressor: 10 dB of "
            "make-up on a quiet fan is a fan on the air.\n\n"
            "Measured on real speech: at every setting of the dial the "
            "voice itself loses 0.1 dB, and the room drops by 2 dB at "
            "0.20, 6 dB at 0.50, 10 dB at 0.70 and 29 dB wide open. "
            "There is no setting at which this stage dulls the voice — "
            "if it sounds dull, it is not this stage."),
        page);
    what->setWordWrap(true);
    what->setStyleSheet(dimStyle());
    form->addRow(what);

    auto* cost = new QLabel(
        QStringLiteral("Costs 30 ms of delay while it is on — measured, "
                       "not calculated — audible when you listen to "
                       "yourself, and the only stage in the strip that "
                       "costs any. Switch it off while you adjust the EQ "
                       "by ear."),
        page);
    cost->setWordWrap(true);
    cost->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                            .arg(QLatin1String(Style::kAmberText)));
    form->addRow(cost);

    addKnob(form, QStringLiteral("Amount"), 0.0, 1.0, 0.05,
        cur([this]{ return double(chain()->nr().amount()); }, 0.8),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->nr().setAmount(float(v)); }
        }, 2, [this]{ persist(); });

    auto* hint = new QLabel(
        QStringLiteral("Fully up can sound processed on speech. Around "
                       "0.6–0.8 the noise is gone and the voice is whole."),
        page);
    hint->setWordWrap(true);
    hint->setStyleSheet(dimStyle());
    form->addRow(hint);

    m_nrMeter = new QLabel(page);
    m_nrMeter->setStyleSheet(dimStyle());
    lebendeZeile(m_nrMeter);
    form->addRow(m_nrMeter);

    return page;
}

// ── Clipper ──────────────────────────────────────────────────────────
//
// Die Stufe, die bei SSB wirklich Sprechleistung bringt -- und die
// einzige, deren Nebenwirkung Verzerrung ist. Deshalb steht auf der
// Seite, wieviel sie gerade wegnimmt, und daneben auf der Übersicht
// der Scheitelfaktor. Nach Gefühl einzustellen ist hier besonders
// verführerisch und besonders falsch.

QWidget* StripWindow::buildClipperPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Clip);
    auto* on = new QCheckBox(QStringLiteral("Clipper on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Clip, v);
        }
        refreshChainRow();
        persist();
    });

    auto* what = new QLabel(
        QStringLiteral(
            "Your amplifier is limited by the PEAK; what is heard is the "
            "AVERAGE. The gap between the two — the crest factor, on the "
            "Strip tab — is power you paid for and did not send. A "
            "compressor evens out the loudness but leaves the single peak "
            "in the waveform standing. This takes it off.\n\n"
            "3 dB clipped is roughly 3 dB more average power at the same "
            "peak, and that is more than any EQ curve will ever give you."),
        page);
    what->setWordWrap(true);
    what->setStyleSheet(dimStyle());
    form->addRow(what);

    auto* cost = new QLabel(
        QStringLiteral("The cost is distortion, and it is deliberate. "
                       "Start at 3 dB, listen through the channel, and "
                       "watch the crest factor on the Strip tab — not "
                       "your ear alone: clipped audio flatters."),
        page);
    cost->setWordWrap(true);
    cost->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                            .arg(QLatin1String(Style::kAmberText)));
    form->addRow(cost);

    addKnob(form, QStringLiteral("Drive"), 0.0, 18.0, 0.5,
        cur([this]{ return double(chain()->clipper().driveDb()); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->clipper().setDriveDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Knee"), 0.0, 1.0, 0.05,
        cur([this]{ return double(chain()->clipper().knee()); }, 0.3),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->clipper().setKnee(float(v)); }
        }, 2, [this]{ persist(); });

    auto* makeup = new QCheckBox(
        QStringLiteral("Level-matched — for judging, not for air"), page);
    makeup->setChecked(chain() ? chain()->clipper().levelMatch() : false);
    makeup->setToolTip(QStringLiteral(
        "Matches the measured loudness so switching the stage on and off "
        "compares the SOUND of the clipping, not the volume — otherwise "
        "you compare loud against quiet and call loud better, which is "
        "the commonest way to fool yourself here. Switch it off again "
        "before you transmit: matched, the stage gives away exactly the "
        "power it just won."));
    form->addRow(makeup);
    connect(makeup, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) { c->clipper().setLevelMatch(v); }
        persist();
    });

    m_clipMeter = new QLabel(page);
    m_clipMeter->setStyleSheet(dimStyle());
    lebendeZeile(m_clipMeter);
    form->addRow(m_clipMeter);

    return page;
}

// ── Tube ─────────────────────────────────────────────────────────────

QWidget* StripWindow::buildTubePanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Tube);
    auto* on = new QCheckBox(QStringLiteral("Tube on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_tubeCurve = new StripShaperCurve(page);
    m_tubeCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_tubeCurve,
                                StripChain::Stage::Tube));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Tube, v);
        }
        refreshChainRow();
        persist();
    });

    auto* model = new QComboBox(page);
    model->addItem(QStringLiteral("A — soft"), int(ClientTube::Model::A));
    model->addItem(QStringLiteral("B — hard"), int(ClientTube::Model::B));
    model->addItem(QStringLiteral("C — asymmetric"), int(ClientTube::Model::C));
    {
        const int have = chain() ? int(chain()->tube().model())
                                 : int(ClientTube::Model::A);
        const int at = model->findData(have);
        if (at >= 0) { model->setCurrentIndex(at); }
    }
    // Labelled "Waveshaper", not "Character". The stage card above now
    // carries a Character picker like every other stage, and two combos
    // both called Character in one panel is a panel nobody can describe
    // to anybody else. This one is the genuine algorithm choice; the
    // character sets it along with drive, bias and mix.
    form->addRow(QStringLiteral("Waveshaper"), model);
    connect(model, &QComboBox::currentIndexChanged, this, [this, model](int) {
        if (StripChain* c = chain()) {
            c->tube().setModel(
                static_cast<ClientTube::Model>(model->currentData().toInt()));
        }
        persist();
    });

    addKnob(form, QStringLiteral("Drive"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->tube().driveDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setDriveDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Tone"), -1.0, 1.0, 0.05,
        cur([this]{ return chain()->tube().tone(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setTone(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Mix"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->tube().dryWet(); }, 1.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setDryWet(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Output"), -24.0, 24.0, 0.5,
        cur([this]{ return chain()->tube().outputGainDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->tube().setOutputGainDb(float(v)); }
        }, 1, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Saturation adds harmonics that were not in your voice. On SSB "
        "that is not free: they land inside the transmitted bandwidth and "
        "some of them sound like distortion to the far end even when they "
        "sound flattering in headphones.\n\n"
        "Mix is the useful control. A little of the saturated signal "
        "under the clean one thickens a thin voice; all of it is an "
        "effect. Drive past about 6 dB with Mix at 1.0 is where operators "
        "start being told they sound rough."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// ── PUDU — the AetherVoice exciter ───────────────────────────────────

QWidget* StripWindow::buildPuduPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Pudu);
    auto* on = new QCheckBox(QStringLiteral("Exciter on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_puduCurve = new StripBandCurve(StripBandCurve::Stage::Exciter, page);
    m_puduCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_puduCurve,
                                StripChain::Stage::Pudu));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Pudu, v);
        }
        refreshChainRow();
        persist();
    });

    auto* mode = new QComboBox(page);
    mode->addItem(QStringLiteral("Aural exciter + big bottom"),
                  int(ClientPudu::Mode::Aphex));
    mode->addItem(QStringLiteral("Sonic exciter"),
                  int(ClientPudu::Mode::Behringer));
    {
        const int have = chain() ? int(chain()->pudu().mode())
                                 : int(ClientPudu::Mode::Aphex);
        const int at = mode->findData(have);
        if (at >= 0) { mode->setCurrentIndex(at); }
    }
    form->addRow(QStringLiteral("Model"), mode);
    connect(mode, &QComboBox::currentIndexChanged, this, [this, mode](int) {
        if (StripChain* c = chain()) {
            c->pudu().setMode(
                static_cast<ClientPudu::Mode>(mode->currentData().toInt()));
        }
        persist();
    });

    auto* lowLbl = new QLabel(QStringLiteral("<b>Low end</b>"), page);
    form->addRow(lowLbl);
    addKnob(form, QStringLiteral("Tune"), 50.0, 160.0, 1.0,
        cur([this]{ return chain()->pudu().pooTuneHz(); }, 90.0),
        QStringLiteral("Hz"), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setPooTuneHz(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Drive"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->pudu().pooDriveDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setPooDriveDb(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Amount"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->pudu().pooMix(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setPooMix(float(v)); }
        }, 2, [this]{ persist(); });

    auto* hiLbl = new QLabel(QStringLiteral("<b>Top end</b>"), page);
    form->addRow(hiLbl);
    addKnob(form, QStringLiteral("Tune "), 1000.0, 10000.0, 100.0,
        cur([this]{ return chain()->pudu().dooTuneHz(); }, 3000.0),
        QStringLiteral("Hz"), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setDooTuneHz(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Harmonics"), 0.0, 24.0, 0.5,
        cur([this]{ return chain()->pudu().dooHarmonicsDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) {
                c->pudu().setDooHarmonicsDb(float(v));
            }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Amount "), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->pudu().dooMix(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->pudu().setDooMix(float(v)); }
        }, 2, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "An exciter does not lift the top end — it invents harmonics "
        "above what is there and mixes them in, which the ear reads as "
        "detail that was always present. That is why a little sounds "
        "like a better microphone and a lot sounds artificial.\n\n"
        "On SSB the top-end tune is the control to watch. Set it above "
        "about 2.5 kHz and most of what it generates lands outside the "
        "transmit filter: you hear the effect in the monitor and the far "
        "end gets nothing but the intermodulation that came with it.\n\n"
        "The low end works the other way round — it synthesises an "
        "octave below the tune frequency, which on a 2.8 kHz SSB channel "
        "is largely inaudible at the far end too. Both are worth trying "
        "and neither is free."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// ── Reverb ───────────────────────────────────────────────────────────

QWidget* StripWindow::buildReverbPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Reverb);
    auto* on = new QCheckBox(QStringLiteral("Reverb on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Reverb, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Size"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->reverb().size(); }, 0.3),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setSize(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Decay"), 0.3, 5.0, 0.1,
        cur([this]{ return chain()->reverb().decayS(); }, 0.8),
        QStringLiteral("s"), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setDecayS(float(v)); }
        }, 1, [this]{ persist(); });
    addKnob(form, QStringLiteral("Damping"), 0.0, 1.0, 0.05,
        cur([this]{ return chain()->reverb().damping(); }, 0.5),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setDamping(float(v)); }
        }, 2, [this]{ persist(); });
    addKnob(form, QStringLiteral("Pre-delay"), 0.0, 100.0, 1.0,
        cur([this]{ return chain()->reverb().preDelayMs(); }, 20.0),
        QStringLiteral("ms"), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setPreDelayMs(float(v)); }
        }, 0, [this]{ persist(); });
    addKnob(form, QStringLiteral("Mix"), 0.0, 1.0, 0.01,
        cur([this]{ return chain()->reverb().mix(); }, 0.0),
        QString(), [this](double v) {
            if (StripChain* c = chain()) { c->reverb().setMix(float(v)); }
        }, 2, [this]{ persist(); });

    auto* help = new QLabel(QStringLiteral(
        "Almost nobody should use this, and it is here because the DSP "
        "is and hiding a stage that is running would be worse.\n\n"
        "Reverb on transmit audio fills the gaps between words with a "
        "tail, and those gaps are what a listener uses to separate you "
        "from the noise. On a weak signal it costs readability directly. "
        "If you want it at all, Mix below 0.1 with a short decay is the "
        "only setting that will not draw comment."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// ── Limiter ──────────────────────────────────────────────────────────

QWidget* StripWindow::buildLimiterPanel()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    const int idx = static_cast<int>(StripChain::Stage::Limiter);
    auto* on = new QCheckBox(QStringLiteral("Limiter on"), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    form->addRow(on);

    m_limiterCurve =
        new StripDynamicsCurve(StripDynamicsCurve::Stage::Limiter, page);
    m_limiterCurve->setChain(chain());
    form->addRow(buildStageCard(page, m_limiterCurve,
                                StripChain::Stage::Limiter));
    connect(on, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) {
            c->setStageEnabled(StripChain::Stage::Limiter, v);
        }
        refreshChainRow();
        persist();
    });

    addKnob(form, QStringLiteral("Ceiling"), -12.0, 0.0, 0.1,
        cur([this]{ return chain()->limiter().ceilingDb(); }, -1.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) { c->limiter().setCeilingDb(float(v)); }
        }, 1, [this]{ persist(); });
    // Hiess „Output trim". Der Quelltext wendet den Wert VOR der
    // Huellkurve an und schneidet danach hart auf die Decke
    // (ClientFinalLimiter.cpp) -- ein positiver Wert macht den Ausgang
    // also nicht lauter, er treibt staerker in die Deckelung. Genau
    // der Regler, nach dem man greift, wenn „die Kraft fehlt", und
    // genau der, der sie nicht liefert. Jetzt heisst er, was er tut.
    addKnob(form, QStringLiteral("Drive into limiter"), -12.0, 12.0, 0.5,
        cur([this]{ return chain()->limiter().outputTrimDb(); }, 0.0),
        QStringLiteral("dB"), [this](double v) {
            if (StripChain* c = chain()) {
                c->limiter().setOutputTrimDb(float(v));
            }
        }, 1, [this]{ persist(); });

    auto* dc = new QCheckBox(QStringLiteral("Remove DC offset"), page);
    dc->setChecked(chain() ? chain()->limiter().dcBlockEnabled() : true);
    form->addRow(dc);
    connect(dc, &QCheckBox::toggled, this, [this](bool v) {
        if (StripChain* c = chain()) { c->limiter().setDcBlockEnabled(v); }
        persist();
    });

    auto* help = new QLabel(QStringLiteral(
        "The last thing in the chain, and it should stay that way. "
        "Everything above it can add gain — the tube, the exciter, the "
        "compressor's make-up — and this is what stops the sum arriving "
        "at the modulator hotter than it should.\n\n"
        "Leave it on. A ceiling of -1 dB gives the transmit chain a "
        "little room without costing anything audible; the reason not to "
        "use 0 is that everything downstream — resampling, the "
        "modulator — can overshoot slightly, and a signal already at "
        "full scale has nowhere to put the overshoot.\n\n"
        "DC offset removal matters more than it sounds: a microphone "
        "with a small offset spends part of the transmitter's headroom "
        "on a signal nobody can hear."), page);
    help->setWordWrap(true);
    help->setStyleSheet(dimStyle());
    form->addRow(help);
    return page;
}

// Every stage now has a real panel, so this is only reached if a stage
// is added and its panel is not. Kept for exactly that: a new stage
// should appear with an honest empty tab rather than crash or silently
// vanish from the window.
QWidget* StripWindow::buildPlaceholder(StripChain::Stage s)
{
    auto* page = new QWidget(this);
    auto* col = new QVBoxLayout(page);

    const int idx = static_cast<int>(s);
    auto* on = new QCheckBox(
        Sprache::T(QStringLiteral("%1 on"))
            .arg(QString::fromLatin1(StripChain::stageName(s))), page);
    m_stageBoxes[static_cast<size_t>(idx)] = on;
    col->addWidget(on);
    connect(on, &QCheckBox::toggled, this, [this, s](bool v) {
        if (StripChain* c = chain()) { c->setStageEnabled(s, v); }
        refreshChainRow();
        persist();
    });

    auto* note = new QLabel(QStringLiteral(
        "The DSP for this stage is in place and tested; its controls are "
        "not built yet, so it runs at its defaults. Switching it on here "
        "does exactly that and nothing more.\n\n"
        "An empty tab is more honest than one full of controls that go "
        "nowhere."), page);
    note->setWordWrap(true);
    note->setStyleSheet(dimStyle());
    col->addWidget(note);
    col->addStretch(1);
    return page;
}

// ── Refresh ──────────────────────────────────────────────────────────

void StripWindow::rebuildPresetBox(const QString& select)
{
    if (!m_presetBox) { return; }
    const QSignalBlocker block(m_presetBox);
    m_presetBox->clear();
    m_presetBox->addItem(QStringLiteral("— choose —"), QString());

    // Built-ins first and the operator's own below, separated, so it is
    // always obvious which of the two a name is. A flat merged list is
    // where someone deletes a built-in they cannot delete and concludes
    // the button is broken.
    for (const auto& p : StripSettings::builtInPresets()) {
        m_presetBox->addItem(p.name, p.name);
        m_presetBox->setItemData(m_presetBox->count() - 1, false,
                                 Qt::UserRole + 1);
    }
    const QStringList mine = StripSettings::userPresetNames();
    if (!mine.isEmpty()) {
        m_presetBox->insertSeparator(m_presetBox->count());
        for (const QString& n : mine) {
            m_presetBox->addItem(n, n);
            m_presetBox->setItemData(m_presetBox->count() - 1, true,
                                     Qt::UserRole + 1);
        }
    }
    if (!select.isEmpty()) {
        const int i = m_presetBox->findData(select);
        if (i >= 0) { m_presetBox->setCurrentIndex(i); }
    }
    // Siehe die Begründung beim Auswahlfeld: der Löschknopf bleibt
    // bedienbar und erklärt sich beim Druck.
}

void StripWindow::saveUserPreset()
{
    StripChain* c = chain();
    if (!c) { return; }

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Save preset"),
        QStringLiteral("Name:"), QLineEdit::Normal,
        m_presetBox ? m_presetBox->currentData().toString() : QString(), &ok);
    if (!ok) { return; }

    if (!StripSettings::saveUserPreset(name, *c)) {
        QMessageBox::warning(this, QStringLiteral("Save preset"),
            Sprache::T(QStringLiteral("That name can't be used. It needs at least one "
                           "character and no slashes.")));
        return;
    }
    rebuildPresetBox(name.trimmed());
    m_presetNote->setText(QStringLiteral("Saved as \u201c%1\u201d.")
                              .arg(name.trimmed()));
}

void StripWindow::deleteUserPreset()
{
    const QString name = m_presetBox
        ? m_presetBox->currentData().toString() : QString();
    // „kann ich die audioprofile nicht löschen." (2026-10-14) Der Knopf
    // war aus, und ein ausgegrauter Knopf sagt nicht, was fehlt.
    if (name.isEmpty()) {
        if (m_presetNote) {
            m_presetNote->setText(Sprache::T(QStringLiteral(
                "Wähle zuerst eine eigene Kette in der Liste — "
                "gelöscht werden kann nur, was du selbst abgelegt "
                "hast.")));
        }
        return;
    }
    if (!m_presetBox->currentData(Qt::UserRole + 1).toBool()) {
        if (m_presetNote) {
            m_presetNote->setText(Sprache::T(QStringLiteral(
                "„%1\" ist ein mitgeliefertes Ausgangsprofil und bleibt "
                "— eigene Ketten stehen in der Liste darunter."))
                .arg(name));
        }
        return;
    }

    // ── Zweimal drücken statt Fenster ───────────────────────────────
    //
    // „löschen kann ich die profile aber immer noch nicht."
    // (2026-10-14, nach dem Knopf, der sich erklärt.) Dazwischen stand
    // ein modales Rückfragefenster — und ein Fenster, das aus welchem
    // Grund auch immer nicht ankommt, lässt den Knopf tot aussehen.
    //
    // Dieselbe Lehre steht schon im Empfangsprofil-Blatt: dort wurde
    // die Rückfrage aus genau diesem Grund in die Fläche geholt.
    // Also: erster Druck fragt in der Zeile darunter, zweiter Druck
    // löscht. Nichts kann sich davorlegen, und ein Prüfstand kann es
    // fahren.
    if (m_presetDeleteArmed != name) {
        m_presetDeleteArmed = name;
        if (m_presetNote) {
            m_presetNote->setText(
                Sprache::T(QStringLiteral("„%1\" löschen? Noch einmal auf "
                                          "Löschen drücken. (Die Kette, die "
                                          "gerade läuft, bleibt.)"))
                    .arg(name));
        }
        return;
    }
    m_presetDeleteArmed.clear();
    const bool weg = StripSettings::removeUserPreset(name);
    rebuildPresetBox();
    if (m_presetNote) {
        m_presetNote->setText(weg
            ? Sprache::T(QStringLiteral("„%1\" gelöscht. Die Kette, die "
                                        "gerade läuft, bleibt wie sie ist."))
                  .arg(name)
            : Sprache::T(QStringLiteral("„%1\" stand gar nicht in der "
                                        "Liste.")).arg(name));
    }
}

QString StripWindow::kLetzterStand()
{
    return Sprache::T(QStringLiteral("Previously edited"));
}

void StripWindow::applyPreset(const QString& name)
{
    StripChain* c = chain();
    if (!c) { return; }

    // ── Erst wegschreiben, dann überschreiben ───────────────────────
    //
    // Eine Voreinstellung ersetzt JEDE Stufe. Wer eine Stunde von Hand
    // eingestellt hat und dann in die Liste greift -- aus Neugier, was
    // „Clean voice" wäre --, hat seine Stunde verloren, ohne dass
    // irgendwo etwas stand. Genau das ist am 2026-10-13 um 21:05 am
    // Gerät des Betreibers passiert; seine Frage danach lautete, wo
    // man Profile speichert.
    //
    // Keine Rückfrage, kein Fenster: der bestehende Stand wandert
    // vorher unter einen festen Namen, und der steht gleich danach in
    // derselben Liste. Ein Klick zurück, und alles ist wieder da.
    if (name != kLetzterStand()) {
        StripSettings::saveUserPreset(kLetzterStand(), *c);
    }

    // A user preset of the same name wins: it is the one the operator
    // made, and shadowing a built-in with your own version of it is a
    // reasonable thing to want.
    bool applied = StripSettings::applyUserPreset(name, *c);
    if (!applied) { applied = StripSettings::applyBuiltIn(name, *c); }
    if (!applied) { return; }

    m_presetNote->setText(Sprache::T(QStringLiteral("Your own setting: %1")).arg(name));
    for (const auto& p : StripSettings::builtInPresets()) {
        if (p.name == name) { m_presetNote->setText(p.description); break; }
    }

    // A preset changes the chain underneath every control in the
    // window. Rebuilding the panels is the only version of "refresh"
    // that cannot drift: each control reads its own value from the
    // chain when it is built, so there is no second copy to forget.
    reloadControls();
    refreshChainRow();
    persist();

    // Die Liste neu aufbauen, damit „Zuletzt bearbeitet" darin steht --
    // mit der eben gewählten Voreinstellung als Auswahl, nicht mit dem
    // Sicherungsstand.
    if (name != kLetzterStand()) {
        rebuildPresetBox(name);
        if (m_presetNote && !m_presetNote->text().isEmpty()) {
            m_presetNote->setText(
                m_presetNote->text() + QStringLiteral(" · Dein vorheriger "
                                                      "Stand liegt als „%1\".")
                                           .arg(kLetzterStand()));
        }
    }
}

// ── Ein Profil aus AetherSDR ─────────────────────────────────────────
//
// Die Datei kommt von einem anderen Programm, und die Werte darin
// koennen aus einer neueren Fassung stammen als der, gegen die der
// Importeur geschrieben wurde. Deshalb wird nicht stumm uebernommen:
// was nicht abgebildet werden konnte, steht hinterher im Hinweis --
// lieber eine Zeile zu viel als eine Stufe, die heimlich anders
// eingestellt ist als in der Datei.
void StripWindow::importAetherPreset()
{
    StripChain* c = chain();
    if (!c) { return; }

    const QString pfad = QFileDialog::getOpenFileName(
        this, Sprache::T(QStringLiteral("Open AetherSDR channel strip profile")),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStringLiteral("AetherSDR (*.json);;%1 (*)")
            .arg(Sprache::T(QStringLiteral("All files"))));
    if (pfad.isEmpty()) { return; }

    // Dieselbe Sicherung wie beim Wechsel einer Voreinstellung: ein
    // Import ersetzt die ganze Kette, und auch davor soll der
    // bestehende Stand abrufbar bleiben.
    StripSettings::saveUserPreset(kLetzterStand(), *c);

    const AetherPreset::Ergebnis e = AetherPreset::applyFile(pfad, *c);
    if (!e.ok) {
        m_presetNote->setText(e.fehler);
        return;
    }

    // Unter dem Namen des Profils ablegen, damit er nach dem naechsten
    // Start in derselben Liste steht wie alles andere -- ein Import,
    // den man nur einmal hat, ist kein Grundstock.
    if (!e.name.isEmpty()) {
        StripSettings::saveUserPreset(e.name, *c);
        rebuildPresetBox();
        const int at = m_presetBox->findData(e.name);
        if (at >= 0) { QSignalBlocker b(m_presetBox); m_presetBox->setCurrentIndex(at); }
    }

    QString text = Sprache::T(QStringLiteral("From AetherSDR: %1")).arg(e.name);
    if (!e.hinweise.isEmpty()) {
        text += QStringLiteral(" — ") + e.hinweise.join(QStringLiteral(" "));
    }
    m_presetNote->setText(text);

    reloadControls();
    refreshChainRow();
    persist();
}


// ── Alle Stufen auf einer Flaeche ────────────────────────────────────

QWidget* StripWindow::buildAllStagesSurface()
{
    auto* host = new QWidget(this);
    auto* col = new QVBoxLayout(host);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(10);

    // ── Was die Kette tut, gemessen ─────────────────────────────────
    //
    // Ganz oben, vor den Karten. Die Karten sagen, was EINGESTELLT
    // ist; dieses Bild sagt, was davon ankommt -- Einschuebe
    // eingerechnet. Einen Tag lang wurde diese Frage beantwortet,
    // indem jemand hingehoert und etwas vermutet hat.
    // ── Der Eingang, ganz oben ──────────────────────────────────────
    //
    // „die rohdaten sind bei allen profilen um weiten voraus."
    //
    // Der Grund stand in seinen eigenen Zahlen: die Stimme kam mit
    // -25 bis -35 dBFS an. Jede Schwelle der Kette ist in dBFS --
    // bei 15 dB zu wenig Pegel macht das Gate zu, der Kompressor
    // greift nie, der Clipper schneidet nichts. Die Kette verschluckt
    // dann Wörter und färbt, und das rohe Signal gewinnt zu Recht.
    //
    // Darum steht das hier als Erstes auf der Übersicht: die
    // gemessene Zahl, die gebrauchte Zahl, und ein Knopf, der die
    // Differenz setzt. Gemessen statt geraten, gesetzt vom
    // Betreiber statt von selbst.
    {
        auto* box = new QWidget(host);
        auto* z = new QVBoxLayout(box);
        z->setContentsMargins(0, 0, 0, 0);
        z->setSpacing(4);

        // ── Warum hier KEIN Zeilenumbruch steht ──────────────────
        //
        // „die schriften springen herum."
        //
        // Eine umbrechende Beschriftung, deren Text sich zehnmal je
        // Sekunde ändert, wechselt zwischen einer und zwei Zeilen --
        // und schiebt bei jedem Wechsel alles darunter. Also: eine
        // Zeile, fester Satzbau, nur die Zahlen ändern sich. Und
        // die Höhe wird festgenagelt, damit auch ein längerer Satz
        // nichts verschiebt.
        m_eingangNote = new QLabel(box);
        m_eingangNote->setObjectName(QStringLiteral("stripEingangNote"));
        m_eingangNote->setStyleSheet(dimStyle());
        lebendeZeile(m_eingangNote);
        z->addWidget(m_eingangNote);

        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(QStringLiteral("Input"), box);
        lbl->setStyleSheet(dimStyle());
        row->addWidget(lbl);

        m_eingangSlider = new QSlider(Qt::Horizontal, box);
        m_eingangSlider->setRange(-120, 360);      // -12 .. +36 dB
        // ── Den Wert der Kette, nicht die Zahl Null ──────────────
        //
        // Hier stand setValue(0). Der Schieber zeigte also bei JEDEM
        // Öffnen des Fensters 0,0 dB, egal was gespeichert war und
        // was die Kette gerade fuhr -- und die erste Berührung
        // schrieb diese angezeigte Null in die Kette zurück. Wer
        // einmal „Übernehmen" gedrückt und das Fenster wieder
        // geöffnet hatte, verlor seine Einstellung beim nächsten
        // Anfassen, ohne dass irgendwo etwas davon stand.
        //
        // Zwei unabhängige Durchsichten am 2026-10-10 haben das als
        // die wahrscheinlichste Einzelursache für „mir geht die
        // Kraft als Vorverstärker ab" benannt.
        m_eingangSlider->setValue(
            chain() ? int(std::lround(chain()->inputGainDb() * 10.0f)) : 0);
        m_eingangSlider->setStyleSheet(Style::sliderHStyle());
        row->addWidget(m_eingangSlider, 1);

        m_eingangWert = new QLabel(
            QStringLiteral("%1 dB")
                .arg(chain() ? double(chain()->inputGainDb()) : 0.0,
                     0, 'f', 1), box);
        m_eingangWert->setMinimumWidth(64);
        m_eingangWert->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_eingangWert->setStyleSheet(dimStyle());
        row->addWidget(m_eingangWert);

        m_eingangBtn = new QPushButton(QStringLiteral("Set it"), box);
        m_eingangBtn->setObjectName(QStringLiteral("stripEingangBtn"));
        m_eingangBtn->setStyleSheet(Style::buttonBaseStyle());
        m_eingangBtn->setToolTip(QStringLiteral(
            "Sets the gain so your speech peaks land near -12 dBFS — "
            "the level every threshold in this strip was written for."));
        row->addWidget(m_eingangBtn);

        // ── Und derselbe Pegel, von selbst ───────────────────────
        //
        // Ein Vorverstärker, bei dem man erst einen Knopf drücken
        // muss, ehe er verstärkt, ist keiner. Mit dem Haken sucht
        // die Kette den Arbeitspunkt selbst: alle zwei Sekunden ein
        // Schritt, und nur wenn in diesen zwei Sekunden gesprochen
        // wurde.
        m_eingangAuto = new QCheckBox(QStringLiteral("automatic"), box);
        m_eingangAuto->setChecked(chain() ? chain()->inputAuto() : false);
        m_eingangAuto->setToolTip(QStringLiteral(
            "Finds the level by itself and holds it: one step every two "
            "seconds, and only while someone is talking — in silence it "
            "does not move, or it would pull the room up until the "
            "chain mistakes it for a voice. Every threshold in this "
            "strip is written for speech peaking at −12 dBFS."));
        row->addWidget(m_eingangAuto);
        connect(m_eingangAuto, &QCheckBox::toggled, this, [this](bool v) {
            if (StripChain* c = chain()) { c->setInputAuto(v); }
            if (StripChain* d = displayChain(); d != nullptr && d != chain()) {
                d->setInputAuto(v);
            }
            if (m_eingangSlider) { m_eingangSlider->setEnabled(!v); }
            if (m_eingangBtn) {
                m_eingangBtn->setEnabled(
                    !v && chain() != nullptr
                    && chain()->speechPeakDb() >= -90.0f);
            }
            persist();
        });
        if (m_eingangSlider) {
            m_eingangSlider->setEnabled(!m_eingangAuto->isChecked());
        }
        // Aus, bis wirklich etwas gehört wurde -- ein Druck ins
        // Leere hat am 2026-10-14 +36 dB gesetzt.
        m_eingangBtn->setEnabled(
            !m_eingangAuto->isChecked()
            && chain() != nullptr && chain()->speechPeakDb() >= -90.0f);

        z->addLayout(row);
        col->addWidget(box);

        connect(m_eingangSlider, &QSlider::valueChanged, this, [this](int v) {
            if (StripChain* c = chain()) {
                c->setInputGainDb(float(v) / 10.0f);
            }
            if (m_eingangWert) {
                m_eingangWert->setText(QStringLiteral("%1 dB")
                                           .arg(v / 10.0, 0, 'f', 1));
            }
            persist();
        });
        connect(m_eingangBtn, &QPushButton::clicked, this, [this]() {
            StripChain* c = displayChain();
            if (c == nullptr || m_eingangSlider == nullptr) { return; }
            // ── Die Spitze seiner STIMME, nicht die des Augenblicks ──
            //
            // Hier stand rawInputPeakDb() -- die Spitze des letzten
            // Blocks, rund zehn Millisekunden. Am 2026-10-14 fiel ein
            // Druck auf diesen Knopf in eine Sprechpause: gemessen
            // wurde der Störteppich bei -48 dBFS, gesetzt wurden die
            // vollen +36 dB, und der Kopfhörer stand an der Decke des
            // Begrenzers. Dritter Vorfall desselben Tages.
            const double roh = double(c->speechPeakDb());
            if (roh < -90.0) { return; }           // nichts gehört
            const double soll = -12.0;
            const int v = int(std::lround(
                std::clamp(soll - roh, -12.0, 36.0) * 10.0));
            m_eingangSlider->setValue(v);
        });
    }

    m_transfer = new StripTransferCurve(host);
    col->addWidget(m_transfer);

    // ── Der Scheitelfaktor, als Zahl ─────────────────────────────────
    //
    // Bei SSB entscheidet er über die Reichweite: die Endstufe wird
    // von der Spitze begrenzt, gehört wird der Effektivwert. Die
    // Zahl stand bisher nur im verworfenen Automatikteil und war
    // nirgends zu sehen -- dabei macht gerade sie aus „stärker
    // komprimieren?" eine Entscheidung statt eines Gefühls.
    m_crestNote = new QLabel(host);
    m_crestNote->setStyleSheet(dimStyle());
    // Kein Umbruch, feste Hoehe -- siehe die Begruendung bei
    // m_eingangNote. Dasselbe Springen, dieselbe Ursache.
    lebendeZeile(m_crestNote);
    col->addWidget(m_crestNote);

    auto* cards = new QWidget(host);
    auto* grid = new QGridLayout(cards);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    col->addWidget(cards, 1);

    // ── Warum Karten und nicht die Seiten selbst ────────────────────
    //
    // Der erste Versuch stellte Longpaths acht Stufenseiten in ein
    // zweispaltiges Raster, wie Aether es tut. Das ging nicht auf:
    // Aethers Panels sind von Haus aus kompakt, Longpaths sind fuer
    // eine ganze Reiterseite gebaut -- allein das Gate bringt ein
    // 280 px hohes Kennlinienfeld und sechs Regler ueber die volle
    // Breite mit. Zwei Spalten davon erzeugten waagrechtes Scrollen
    // und zeigten trotzdem eine Stufe.
    //
    // Was Aether wirklich gibt, ist nicht das Raster, sondern der
    // UEBERBLICK: die ganze Kette im Blick, waehrend man an einer
    // Stufe dreht. Den geben diese Karten -- Name, Zustand,
    // Charakter und die drei Zahlen, die diese Stufe ausmachen. Ein
    // Klick oeffnet die volle Seite, die unveraendert daneben
    // weiterlebt.
    //
    // Das Ablesegitter ist das des Hausstils (§Instrumente): gleich
    // grosse Felder, Ueberschrift versal klein, Wert monospaced.
    int r = 0;
    int c = 0;
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto st = static_cast<StripChain::Stage>(i);
        grid->addWidget(buildStageCard(st), r, c);
        if (++c == 2) { c = 0; ++r; }
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(r + 1, 1);

    // Einmal fuellen, sonst stehen die Karten bis zum ersten Takt des
    // Messzeigers leer da -- und beim ersten Blick auf das Fenster
    // sieht eine leere Karte wie ein Fehler aus, nicht wie eine
    // Stufe, die noch nichts gemessen hat.
    refreshStageCards();

    return host;
}

QWidget* StripWindow::buildStageCard(StripChain::Stage stage)
{
    const int idx = static_cast<int>(stage);
    StageCard& card = m_stageCards[static_cast<size_t>(idx)];

    auto* frame = new QFrame(this);
    frame->setCursor(Qt::PointingHandCursor);
    frame->setStyleSheet(QStringLiteral(
        "QFrame { background: %1; border: 1px solid %2; "
        "border-radius: 8px; }")
        .arg(QString::fromLatin1(Style::kPanelBg),
             QString::fromLatin1(Style::kBorderSubtle)));

    auto* col = new QVBoxLayout(frame);
    col->setContentsMargins(12, 10, 12, 12);
    col->setSpacing(7);

    // Kopf: Name versal links, Zustand als Kapsel rechts (Hausstil
    // Regel 5 -- Zustand immer an derselben Stelle).
    {
        auto* head = new QHBoxLayout;
        auto* name = new QLabel(
            QString::fromLatin1(StripChain::stageName(stage)).toUpper(),
            frame);
        name->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; letter-spacing: "
            "0.16em; border: none; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
        head->addWidget(name);
        head->addStretch(1);

        card.state = new QLabel(frame);
        card.state->setAlignment(Qt::AlignCenter);
        head->addWidget(card.state);
        col->addLayout(head);
    }

    // Was der Optimierer hier gesetzt hat, einzeilig.
    card.note = new QLabel(frame);
    card.note->setWordWrap(true);
    card.note->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; border: none; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    card.note->hide();
    col->addWidget(card.note);

    // Die drei Zahlen dieser Stufe.
    card.values = new QLabel(frame);
    card.values->setFont(Style::monoFontPt(11));
    card.values->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; border: none; }")
        .arg(QString::fromLatin1(Style::kAmberText)));
    col->addWidget(card.values);

    auto* open = new QPushButton(QStringLiteral("Open"), frame);
    open->setStyleSheet(Style::buttonBaseStyle());
    open->setToolTip(QStringLiteral(
        "Open this stage's own page, with every control it has."));
    connect(open, &QPushButton::clicked, this, [this, stage]() {
        if (!m_tabs) { return; }
        const QString want =
            QString::fromLatin1(StripChain::stageName(stage));
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (m_tabs->tabText(i) == want) {
                m_tabs->setCurrentIndex(i);
                return;
            }
        }
    });
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(open);
    col->addLayout(row);

    return frame;
}

void StripWindow::refreshStageCards()
{
    // Der Ausgangssteller des Entzerrers, falls er unten steht.
    if (m_eqMuteWarning != nullptr) {
        StripChain* c = chain();
        const bool mute = c != nullptr && c->eq().masterGain() < 0.001f;
        m_eqMuteWarning->setVisible(mute);
        if (mute) {
            m_eqMuteWarning->setText(QStringLiteral(
                "The equaliser's own output is at zero — everything "
                "through it is silent, whatever the curve says. Press "
                "Flat above, or raise the output fader on the right of "
                "the panel below."));
        }
    }

    StripChain* c = chain();
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto stage = static_cast<StripChain::Stage>(i);
        StageCard& card = m_stageCards[static_cast<size_t>(i)];
        if (card.state == nullptr) { continue; }

        const bool on = c != nullptr && c->stageEnabled(stage);
        card.state->setText(on ? QStringLiteral("ON")
                               : QStringLiteral("OFF"));
        card.state->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: 1px solid %2; "
            "border-radius: 6px; padding: 1px 8px; font-size: 9px; "
            "letter-spacing: 0.14em; }")
            .arg(QString::fromLatin1(on ? Style::kGreenText
                                        : Style::kTextInactive),
                 QString::fromLatin1(on ? Style::kGreenText
                                        : Style::kBorderSubtle)));

        card.values->setText(c != nullptr ? stageSummary(stage)
                                          : QStringLiteral("——"));

        // Frueher stand hier, was der Optimierer an dieser Stufe
        // gesetzt hatte. Seit die Herleitung entfallen ist, setzt
        // niemand mehr etwas von aussen -- die Karte zeigt nur noch
        // den Zustand und die Zahlen, und das ist die Wahrheit.
        card.note->hide();
    }
}

// Die drei Zahlen, die eine Stufe ausmachen. Sie stehen hier und nicht
// in den Panels, weil eine Uebersicht eine eigene Auswahl braucht: auf
// der Karte ist Platz fuer das, wonach man sucht, nicht fuer alles.
QString StripWindow::stageSummary(StripChain::Stage stage) const
{
    StripChain* c = chain();
    if (c == nullptr) { return QStringLiteral("——"); }

    const auto dash = QStringLiteral("——");
    switch (stage) {
    case StripChain::Stage::Gate:
        return QStringLiteral("%1 dBFS · floor %2 dB · rel %3 ms")
            .arg(double(c->gate().thresholdDb()), 0, 'f', 0)
            .arg(double(c->gate().floorDb()),     0, 'f', 0)
            .arg(double(c->gate().releaseMs()),   0, 'f', 0);
    case StripChain::Stage::Eq:
        return QStringLiteral("%1 bands").arg(c->eq().activeBandCount());
    case StripChain::Stage::DeEss:
        return QStringLiteral("%1 Hz · %2 dB")
            .arg(double(c->deEss().frequencyHz()), 0, 'f', 0)
            .arg(double(c->deEss().amountDb()),    0, 'f', 1);
    case StripChain::Stage::Comp:
        return QStringLiteral("%1:1 · %2 dBFS · mk %3 dB")
            .arg(double(c->comp().ratio()),       0, 'f', 1)
            .arg(double(c->comp().thresholdDb()), 0, 'f', 0)
            .arg(double(c->comp().makeupDb()),    0, 'f', 1);
    case StripChain::Stage::Limiter:
        return QStringLiteral("ceiling %1 dB")
            .arg(double(c->limiter().ceilingDb()), 0, 'f', 1);
    default:
        break;
    }
    // Roehre, PUDU und Nachhall haben keine Zahl, die fuer sich
    // sprechen wuerde -- ihr Charakter IST die Angabe, und der steht
    // auf der Seite selbst. Ein Strich ist dafuer die ehrliche
    // Anzeige (Hausstil Regel 7).
    return dash;
}

void StripWindow::reloadControls()
{
    if (!m_tabs) { return; }
    const int keep = m_tabs->currentIndex();
    // The voice check is not a stage panel and may hold a recording the
    // operator is comparing against — pull it out before the loop
    // deletes pages, put it back at the end. Parent goes back to the
    // window so the widget is owned even while off the tab bar.
    if (m_voiceCheck) {
        const int vcIdx = m_tabs->indexOf(m_voiceCheck);
        if (vcIdx >= 0) { m_tabs->removeTab(vcIdx); }
        m_voiceCheck->setParent(this);
        m_voiceCheck->hide();
    }
    while (m_tabs->count() > 0) {
        QWidget* w = m_tabs->widget(0);
        m_tabs->removeTab(0);
        w->deleteLater();
    }
    // Owned by the pages just removed. The equaliser panel goes with
    // them; the host it talks through is owned by this window and
    // survives, so the rebuilt panel gets a live engine rather than a
    // dangling one.
    m_eqPanel = nullptr;
    m_nrMeter = m_clipMeter = nullptr;
    m_gateMeter = m_deEssMeter = m_compMeter = nullptr;
    // Owned by the pages just removed. Left dangling, the next
    // refreshMeters() would call isVisible() on freed memory — the same
    // class of fault as the late-binding one this window already had,
    // and one that only shows up under a rebuild.
    m_gateCurve = m_compCurve = m_limiterCurve = nullptr;
    m_tubeCurve = nullptr;
    m_deEssCurve = m_puduCurve = nullptr;
    // Same trap, missed once. The "On air" tab was added in buildUi()
    // and nowhere else, so the first preset or character applied made it
    // disappear for the rest of the session and left this pointer aimed
    // at a deleted widget. Both halves are fixed: nulled here, re-added
    // below. Anything buildUi() puts in the tab bar has to be put back
    // here too — the loop covers the eight stages and nothing else.
    m_txSpectrum = nullptr;
    // Gehoeren der Uebersichtsseite, die gleich neu gebaut wird.
    //
    // m_transfer stand hier nicht, und das war derselbe Fehler, den
    // die drei Absaetze darueber beschreiben -- nur juenger: das
    // Messbild wird in buildAllStagesSurface() erzeugt, also bei
    // JEDEM Umbau neu, und der Zeiger zeigte bis zur naechsten
    // Zuweisung auf eine geloeschte Anzeige. Sichtbar wurde es
    // anders herum: nach einem Profilwechsel blieb die Kurve leer,
    // weil die Ringe des Mithoerers am alten Bild hingen.
    m_transfer = nullptr;
    m_crestNote = nullptr;
    m_eingangNote = nullptr;
    m_eingangSlider = nullptr;
    m_eingangWert = nullptr;
    m_eingangBtn = nullptr;
    m_eingangAuto = nullptr;
    m_stageText.fill(StageText{});
    m_stageBoxes.fill(nullptr);

    // Derselbe Aufbau wie in buildUi() -- Uebersicht, dann die acht
    // Seiten. Beide Stellen muessen dasselbe erzeugen; dass hier eine
    // zweite Fassung stand, war schon die Quelle zweier Fehler (siehe
    // die Hinweise oben ueber "On air" und die haengenden Zeiger).
    m_stageCards.fill(StageCard{});
    m_tabs->addTab(new StagePageScroll(buildAllStagesSurface()),
                   QStringLiteral("Strip"));
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<StripChain::Stage>(i);
        QWidget* page = nullptr;
        switch (s) {
        case StripChain::Stage::Gate:  page = buildGatePanel();  break;
        case StripChain::Stage::Eq:    page = buildEqPanel();    break;
        case StripChain::Stage::DeEss: page = buildDeEssPanel(); break;
        case StripChain::Stage::Comp:    page = buildCompPanel();    break;
        case StripChain::Stage::Denoise: page = buildDenoisePanel(); break;
        case StripChain::Stage::Clip:    page = buildClipperPanel(); break;
        case StripChain::Stage::Tube:    page = buildTubePanel();    break;
        case StripChain::Stage::Pudu:    page = buildPuduPanel();    break;
        case StripChain::Stage::Reverb:  page = buildReverbPanel();  break;
        case StripChain::Stage::Limiter: page = buildLimiterPanel(); break;
        default:                         page = buildPlaceholder(s); break;
        }
        m_tabs->addTab(new StagePageScroll(page),
                       QString::fromLatin1(StripChain::stageName(s)));
    }
    m_tabs->addTab(buildTxSpectrumPanel(), QStringLiteral("On air"));
    // Back where buildUi() put it, same instance, recording intact.
    if (m_voiceCheck) {
        m_tabs->addTab(new StagePageScroll(m_voiceCheck),
                       QStringLiteral("Voice check"));
    }
    m_tabs->addTab(new StagePageScroll(buildInsertsPanel()),
                   QStringLiteral("Plugins"));
    if (StripChain* c = chain()) {
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            QCheckBox* box = m_stageBoxes[static_cast<size_t>(i)];
            if (!box) { continue; }
            const QSignalBlocker block(box);
            box->setChecked(c->stageEnabled(static_cast<StripChain::Stage>(i)));
        }
    }
    if (keep >= 0 && keep < m_tabs->count()) { m_tabs->setCurrentIndex(keep); }

    // Die Seiten sind NEU gebaut, also haengen ihre Bilder wieder an
    // chain(). Laeuft der Mithoerer ohne Funkgeraet, muessen sie
    // zurueck auf ihn -- sonst sind die Anzeigen nach jedem
    // Profilwechsel wieder tot, und das sieht aus wie „das Profil hat
    // etwas kaputtgemacht".
    refreshStagePictures();
    refreshStageCards();

    // Und das Messbild bekommt seine Ringe zurueck. Ohne diese drei
    // Zeilen blieb es nach jedem Profilwechsel leer, obwohl der
    // Mithoerer weiterlief.
    if (m_transfer != nullptr && m_live != nullptr && m_live->isRunning()) {
        m_transfer->setRings(&m_live->inputRing(), &m_live->outputRing());
    }

    // Die Seiten sind neu und damit wieder englisch.
    Sprache::uebersetzeBaum(this);
}

void StripWindow::showVoiceCheckTab()
{
    if (m_tabs && m_voiceCheck) {
        m_tabs->setCurrentWidget(m_voiceCheck);
    }
}





// Which character the operator last chose for a stage, and where it is
// kept. One settings key per stage, named after the stage, so a new
// stage needs no new key and a removed one leaves no orphan behind.
QString StripWindow::characterSettingsKey(StripChain::Stage s)
{
    return QStringLiteral("StripCharacter/")
           + QString::fromLatin1(StripChain::stageName(s));
}

QString StripWindow::characterKeyFor(StripChain::Stage s)
{
    return AppSettings::instance()
        .value(characterSettingsKey(s), QString()).toString();
}

void StripWindow::rememberCharacter(StripChain::Stage s, const QString& name)
{
    AppSettings::instance().setValue(characterSettingsKey(s), name);
    AppSettings::instance().save();
}

QWidget* StripWindow::buildStageCard(QWidget* page, QWidget* picture,
                                     StripChain::Stage stage)
{
    auto* card = new QWidget(page);
    auto* row  = new QHBoxLayout(card);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    // Square, and it stays square: a transfer curve has the same unit
    // on both axes and a stretched one lies about the 45° diagonal.
    picture->setParent(card);
    picture->setMinimumSize(260, 260);
    picture->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    row->addWidget(picture, 1);

    auto* words = new QWidget(card);
    auto* col   = new QVBoxLayout(words);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(7);

    const int idx = static_cast<int>(stage);
    StageText& st = m_stageText[static_cast<size_t>(idx)];
    st.picture = picture;

    st.live = new QLabel(words);
    st.live->setWordWrap(true);
    st.live->setTextFormat(Qt::RichText);
    st.live->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    st.live->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 13px; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    col->addWidget(st.live);

    st.legend = new QLabel(words);
    st.legend->setWordWrap(true);
    st.legend->setTextFormat(Qt::RichText);
    st.legend->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(st.legend);

    // ── The character picker ─────────────────────────────────────────
    //
    // Only where there is something to pick. An empty combo on the
    // stages that have no characters would be a control that teaches
    // people the controls are decorative.
    const QVector<StripCharacters::Character> chars =
        StripCharacters::forStage(stage);
    if (!chars.isEmpty()) {
        auto* pickRow = new QHBoxLayout;
        pickRow->addWidget(new QLabel(QStringLiteral("Character:"), words));
        auto* box = new QComboBox(words);
        for (const auto& ch : chars) { box->addItem(ch.name, ch.name); }
        box->setMinimumWidth(150);
        pickRow->addWidget(box, 1);

        // ── Show which one is actually in effect ─────────────────────
        //
        // Reported from the bench: change the tube's character and the
        // curve moves but the name does not. The combo was rebuilt from
        // scratch on every reloadControls() — which is what applying a
        // character triggers — so it always came back reading its first
        // entry while the chain held the values of whichever one had
        // just been picked. The picture and the label disagreed, and
        // the label was the one lying.
        //
        // The answer is not to remember what was clicked. It is to ask
        // the chain what it currently is: StripCharacters::inEffect()
        // compares the stage against every character and names the one
        // it matches, or none. That is true after a preset, after a
        // restore from disk, and after a knob has been moved — the
        // three cases a remembered name gets wrong.
        //
        // The remembered name is still kept, for one job only: when the
        // stage matches nothing, it says which character the operator
        // started from, so the note reads "Voodoo — edited" rather than
        // going blank and losing the explanation with it.
        //
        // Signals are blocked while setting the index. Otherwise
        // restoring it would re-apply the character and undo whatever
        // was edited.
        // Wired up here, kept current by refreshStageText() on the
        // meter timer. Doing it at build time only would mean the mark
        // appeared at the next panel rebuild rather than when the knob
        // moved — and for a stage nobody applies a preset to, that is
        // never.
        st.charBox = box;

        // A word after the combo when the stage no longer matches what
        // the combo names. Beside it rather than inside the entry, so
        // the list stays a list of characters and does not grow an
        // entry called "Voodoo — edited" that cannot be chosen.
        st.charMark = new QLabel(QStringLiteral("edited"), words);
        st.charMark->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 11px; "
                           "font-style: italic; }")
                .arg(QString::fromLatin1(Style::kAmberText)));
        st.charMark->setToolTip(QStringLiteral(
            "The stage has been adjusted since this character was "
            "chosen. Picking it again puts the numbers back."));
        st.charMark->hide();
        pickRow->addWidget(st.charMark);
        col->addLayout(pickRow);

        st.charNote = new QLabel(words);
        st.charNote->setWordWrap(true);
        st.charNote->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 11px; "
                           "background: %2; border: 1px solid %3; "
                           "padding: 6px; }")
                .arg(QString::fromLatin1(Style::kTextSecondary),
                     QString::fromLatin1(Style::kInsetBg),
                     QString::fromLatin1(Style::kInsetBorder)));
        col->addWidget(st.charNote);

        // `activated`, not `currentIndexChanged`. It fires on a choice
        // the operator made, including re-choosing the entry already
        // shown — which is what "picking it again puts the numbers
        // back" in the mark's tooltip depends on, and what
        // currentIndexChanged would swallow.
        connect(box, &QComboBox::activated, this,
                [this, box, stage](int) {
            // Applying a character moves several knobs at once, so the
            // panel is rebuilt rather than refreshed — every control
            // reads its own value from the chain when it is built, and
            // that is the only version of "refresh" that cannot drift.
            if (StripChain* c = chain()) {
                const QString name = box->currentData().toString();
                if (StripCharacters::apply(*c, stage, name)) {
                    // Remembered BEFORE the rebuild, because the rebuild
                    // destroys this combo and reads the name back.
                    rememberCharacter(stage, name);
                    persist();
                    reloadControls();
                    refreshChainRow();
                }
            }
        });

        // Fill the note now rather than leaving an empty box until the
        // first meter tick. lastSeen is left empty on purpose so the
        // comparison inside sees a change and does the work.
        if (StripChain* c = chain()) { refreshCharacter(st, stage, *c); }
    }

    col->addStretch(1);
    row->addWidget(words, 1);       // half and half
    return card;
}

void StripWindow::refreshStageText()
{
    // Die Uebersichtskarten leben vom selben Takt: sie zeigen
    // dieselben Werte wie die Seiten, nur in Auswahl.
    refreshStageCards();

    StripChain* c = displayChain();
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        StageText& st = m_stageText[static_cast<size_t>(i)];
        if (!st.picture || !st.picture->isVisible()) { continue; }
        QString live, legend;
        if (auto* d = qobject_cast<StripDynamicsCurve*>(st.picture)) {
            live = d->explain(); legend = d->legend();
        } else if (auto* sh = qobject_cast<StripShaperCurve*>(st.picture)) {
            live = sh->explain(); legend = sh->legend();
        } else if (auto* b = qobject_cast<StripBandCurve*>(st.picture)) {
            live = b->explain(); legend = b->legend();
        }
        if (st.live)   { st.live->setText(live); }
        if (st.legend) { st.legend->setText(legend); }

        if (c && st.charBox) {
            refreshCharacter(st, static_cast<StripChain::Stage>(i), *c);
        }
    }
}

// ── Keeping the character honest ─────────────────────────────────────
//
// Runs on the meter timer, for the visible tab only. Recognising a
// character means trying every one of them against the stage, which is
// cheap but not free — so the stage's parameters are compared against
// the previous tick's first, and the work only happens when a number
// has actually moved.
void StripWindow::refreshCharacter(StageText& st, StripChain::Stage stage,
                                   const StripChain& c)
{
    const QVector<float> now = StripCharacters::captureStage(c, stage);
    if (now == st.lastSeen) { return; }
    st.lastSeen = now;

    const QString active   = StripCharacters::inEffect(c, stage);
    const QString started  = characterKeyFor(stage);
    const bool    edited   = active.isEmpty() && !started.isEmpty();
    const QString show     = edited ? started : active;

    if (st.charMark) { st.charMark->setVisible(edited); }

    // Blocked: setting the index fires nothing here, but `activated`
    // aside, a stray signal would re-apply the character and undo the
    // very edit being reported.
    if (!show.isEmpty()) {
        const int at = st.charBox->findData(show);
        if (at >= 0 && at != st.charBox->currentIndex()) {
            const QSignalBlocker block(st.charBox);
            st.charBox->setCurrentIndex(at);
        }
    }

    if (!st.charNote) { return; }
    for (const auto& ch : StripCharacters::forStage(stage)) {
        if (ch.name != show) { continue; }
        // A character is a starting point and adjusting from it is the
        // intended use, so this is not a warning. It is the difference
        // between a label describing the STAGE and one describing a
        // button somebody pressed once — without it the description
        // below explains settings the stage no longer has.
        // Name UND Beschreibung durch die Sprache: beide kommen aus
        // dem Katalog in core/strip/StripCharacters und stehen dort
        // englisch -- wie jeder Quelltext in diesem Baum.
        st.charNote->setText(edited
            ? Sprache::T(
                  QStringLiteral("Started from %1, adjusted since — what "
                                 "follows is where you began, not where "
                                 "you are.\n\n%2"))
                  .arg(Sprache::T(ch.name), Sprache::T(ch.description))
            : Sprache::T(ch.description));
        return;
    }
    // Matches nothing and nothing was ever chosen: a hand-built stage,
    // or one restored from a preset. Say that rather than describing a
    // character it does not have.
    if (show.isEmpty()) {
        st.charNote->setText(QStringLiteral(
            "Set by hand. Pick a character above to start from a known "
            "point — it will say what it is for, and you can adjust from "
            "there."));
    }
}

void StripWindow::refreshStagePictures()
{
    // displayChain(), nicht chain(): sonst haengt diese Funktion die
    // Stufenbilder beim naechsten Aufruf wieder auf die stille
    // Sendekette zurueck. Genau so ist mein erster Anlauf
    // wirkungslos geblieben.
    StripChain* ch = displayChain();
    if (m_gateCurve)     { m_gateCurve->setChain(ch); }
    if (m_compCurve)     { m_compCurve->setChain(ch); }
    if (m_limiterCurve)  { m_limiterCurve->setChain(ch); }
    if (m_tubeCurve)     { m_tubeCurve->setChain(ch); }
    if (m_deEssCurve)    { m_deEssCurve->setChain(ch); }
    if (m_puduCurve)     { m_puduCurve->setChain(ch); }
}


void StripWindow::refreshChainRow()
{
    if (m_chainView) {
        m_chainView->setChain(displayChain());
        m_chainView->update();
    }
    if (m_levels) { m_levels->setChain(displayChain()); }
    // Same late-binding trap as before, now one line: the host is what
    // the ported panel reaches through, so pointing it at the current
    // chain is enough for every widget inside it.
    if (m_eqHost) { m_eqHost->setChain(chain()); }
    if (m_eqPanel) { m_eqPanel->refreshFromEngine(); }
}



// ── Eine Plugin-Karte, die man anfassen kann ─────────────────────────
//
// „diese sollte man dann in der timeline oben reinziehen koennen."
//
// Also keine Auswahlliste, sondern etwas, das man in die Hand nimmt.
// Der Mime-Typ traegt nur den Bezeichner; die Kachel oben entscheidet,
// WO es landet, und das ist genau die richtige Arbeitsteilung -- die
// Stelle gehoert der Kette, nicht der Liste.
class PluginChip : public QFrame {
public:
    PluginChip(const PluginDescriptor& d, QWidget* parent,
               QLabel* zeile = nullptr)
        : QFrame(parent), m_id(d.identifier), m_zeile(zeile)
    {
        // Was es tut, in einem Satz -- siehe PluginPlacement.h. Die
        // Kurzhilfe braucht eine Sekunde Geduld, die Zeile unter der
        // Liste nicht: „beim drüber scrollen eine kurze beschreibung"
        // heisst sofort, nicht nach einer Wartezeit.
        m_satz = PluginPlacement::beschreibung(d.name, d.vendor);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
        setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border: 1px solid %2;"
            " border-radius: 5px; }"
            "QFrame:hover { border-color: %3; }")
            .arg(QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kBorderSubtle),
                 QString::fromLatin1(Style::kBlueBorder)));

        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(9, 6, 9, 6);
        row->setSpacing(8);

        auto* grip = new QLabel(QStringLiteral("⠿"), this);
        grip->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: none; }")
            .arg(QString::fromLatin1(Style::kTextInactive)));
        row->addWidget(grip);

        auto* name = new QLabel(d.name, this);
        name->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: none; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
        row->addWidget(name);
        row->addStretch(1);

        if (!d.vendor.isEmpty()) {
            auto* vendor = new QLabel(d.vendor, this);
            vendor->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; border: none; }")
                .arg(QString::fromLatin1(Style::kTextInactive)));
            row->addWidget(vendor);
        }

        setToolTip(m_satz + QStringLiteral("\n\n")
                   + Sprache::T(QStringLiteral(
                       "Drag onto the chain at the top of this window and "
                       "drop it where you want it. The gap you are over "
                       "lights up.")));
    }

protected:
    void enterEvent(QEnterEvent* ev) override
    {
        if (m_zeile != nullptr) { m_zeile->setText(m_satz); }
        QFrame::enterEvent(ev);
    }

    void mousePressEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton) { m_from = ev->pos(); }
        QFrame::mousePressEvent(ev);
    }

    void mouseMoveEvent(QMouseEvent* ev) override
    {
        if (!(ev->buttons() & Qt::LeftButton)) { return; }
        if ((ev->pos() - m_from).manhattanLength()
            < QApplication::startDragDistance()) {
            return;
        }
        auto* mime = new QMimeData;
        mime->setData(QLatin1String(StripChainView::kPluginMime),
                      m_id.toUtf8());
        auto* drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(grab());
        drag->setHotSpot(ev->pos());
        drag->exec(Qt::CopyAction);
    }

private:
    QString m_id;
    QPoint  m_from;
    QString m_satz;
    QLabel* m_zeile{nullptr};
};

// ── Einschubplaetze: fremde Effekte, als Zusatz ──────────────────────
//
// Die Ansage dazu war unmissverstaendlich: "plugins koennen nur als
// option zusaetzlich gelten, wenn gewollt. ist nicht der standard."
// Diese Seite ist deshalb die letzte im Fenster, sie ist beim ersten
// Aufschlagen leer, und sie sucht erst dann nach Plugins -- wer sie
// nie aufmacht, hat auf seinem Rechner nie eine fremde Bibliothek
// geladen.
//
// Gezeichnet werden die Regler des Plugins in UNSEREN Reglern, nicht
// in seiner Oberflaeche. Warum, steht in core/strip/AudioPlugin.h;
// der Preis ebenfalls.
QWidget* StripWindow::buildInsertsPanel()
{
    auto* page = new QWidget(this);
    auto* col = new QVBoxLayout(page);
    // Rechts Platz lassen. Am gerenderten Blatt gesehen: ohne ihn
    // schob der Rollbalken der Seite die dritte Spalte und das Ende
    // jeder Zeile unter sich -- „Apple" endete als „Appl".
    col->setContentsMargins(0, 0, 14, 0);
    col->setSpacing(12);

    m_insertsNote = new QLabel(page);
    m_insertsNote->setWordWrap(true);
    m_insertsNote->setStyleSheet(dimStyle());
    col->addWidget(m_insertsNote);

    m_insertScanBtn = new QPushButton(
        QStringLiteral("Search again"), page);
    m_insertScanBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(m_insertScanBtn, &QPushButton::clicked,
            this, &StripWindow::scanPlugins);
    {
        auto* row = new QHBoxLayout;
        row->addWidget(m_insertScanBtn);
        row->addStretch(1);
        col->addLayout(row);
    }

    for (int slot = 0; slot < StripChain::kInsertCount; ++slot) {
        auto& ui = m_insertUi[static_cast<size_t>(slot)];

        auto* card = new QWidget(page);
        card->setStyleSheet(QStringLiteral(
            "QWidget { background: %1; border: 1px solid %2;"
            " border-radius: 6px; }")
            .arg(QString::fromLatin1(Style::kPanelBg),
                 QString::fromLatin1(Style::kBorderSubtle)));
        auto* inner = new QVBoxLayout(card);
        inner->setContentsMargins(12, 10, 12, 12);
        inner->setSpacing(8);

        auto* head = new QHBoxLayout;
        auto* title = new QLabel(QStringLiteral("INSERT %1").arg(slot + 1), card);
        title->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; letter-spacing: 1px; font-weight: 600;"
            " border: none; }").arg(QString::fromLatin1(Style::kTextSecondary)));
        head->addWidget(title);
        head->addStretch(1);

        ui.on = new QCheckBox(QStringLiteral("On"), card);
        ui.on->setStyleSheet(QStringLiteral("QCheckBox { border: none; }"));
        connect(ui.on, &QCheckBox::toggled, this, [this, slot](bool v) {
            if (StripChain* c = chain()) { c->setInsertEnabled(slot, v); }
            persist();
            refreshInsertCard(slot);
        });
        head->addWidget(ui.on);
        inner->addLayout(head);

        auto* pick = new QHBoxLayout;
        ui.plugin = new QComboBox(card);
        ui.plugin->setMinimumWidth(260);
        ui.plugin->addItem(QStringLiteral("— empty —"), QString());
        connect(ui.plugin, &QComboBox::activated, this, [this, slot](int) {
            chooseInsertPlugin(slot);
        });
        pick->addWidget(ui.plugin, 1);

        pick->addWidget(new QLabel(QStringLiteral("at"), card));
        ui.position = new QComboBox(card);
        for (int at = 0; at <= StripChain::kStageCount; ++at) {
            ui.position->addItem(insertPositionName(at), at);
        }
        connect(ui.position, &QComboBox::activated, this, [this, slot](int i) {
            if (StripChain* c = chain()) {
                c->setInsertPosition(
                    slot, m_insertUi[static_cast<size_t>(slot)]
                              .position->itemData(i).toInt());
            }
            persist();
            if (m_chainView) { m_chainView->refreshInserts(); }
            refreshInsertCard(slot);
        });
        pick->addWidget(ui.position);

        // ── Das eigene Fenster des Plugins ──────────────────────────
        //
        // „haette gehofft, dass die plugins ev. graphisch vorhanden
        // sind." Hier ist es. Die Regler darunter bleiben trotzdem
        // stehen -- sie sind das, was auch dann noch da ist, wenn ein
        // Plugin keine eigene Oberflaeche mitbringt, und das
        // Einzige, was spaeter auf Windows und Linux gleich aussieht.
        ui.editor = new QPushButton(QStringLiteral("Open window"), card);
        ui.editor->setStyleSheet(Style::buttonBaseStyle());
        ui.editor->setToolTip(QStringLiteral(
            "Open the plugin's own window — the one you know from a "
            "DAW. Plugins without one get Apple's parameter sheet "
            "instead. The sliders below keep working either way."));
        connect(ui.editor, &QPushButton::clicked, this, [this, slot]() {
            StripChain* c = chain();
            AudioPlugin* p = c != nullptr ? c->insertPlugin(slot) : nullptr;
            if (p == nullptr) { return; }
            if (!p->showEditor(p->descriptor().name)) {
                auto& ui2 = m_insertUi[static_cast<size_t>(slot)];
                if (ui2.note) {
                    ui2.note->setText(QStringLiteral(
                        "This plugin would not open a window. The "
                        "sliders below are the way in."));
                }
            }
        });
        pick->addWidget(ui.editor);
        inner->addLayout(pick);

        ui.note = new QLabel(card);
        ui.note->setWordWrap(true);
        ui.note->setStyleSheet(dimStyle() + QStringLiteral("QLabel{border:none;}"));
        inner->addWidget(ui.note);

        ui.params = new QWidget(card);
        ui.paramForm = new QFormLayout(ui.params);
        ui.paramForm->setContentsMargins(0, 4, 0, 0);
        ui.paramForm->setSpacing(4);
        inner->addWidget(ui.params);

        col->addWidget(card);
        refreshInsertCard(slot);
    }

    // Der Katalog steht UNTER den Plaetzen.
    //
    // Erst stand er darueber -- erst sehen, was es gibt, dann was
    // haengt. Am laufenden Fenster war das falsch herum: nach dem
    // Suchlauf schob sich eine Auslage mit sechsundzwanzig Eintraegen
    // dazwischen, und wer die Regler eines geladenen Plugins
    // erreichen wollte, scrollte daran vorbei. Was man eingestellt
    // hat, gehoert naeher an den Anfang als das, was man noch
    // einbauen koennte.
    m_catalogue = new QWidget(page);
    col->addWidget(m_catalogue);

    // Eine feste Zeile statt einer Kurzhilfe: die Kurzhilfe kommt
    // erst nach einer Wartezeit, und wer eine Liste durchfährt,
    // wartet nicht. Siehe PluginChip::enterEvent.
    m_pluginSatz = new QLabel(page);
    m_pluginSatz->setWordWrap(true);
    m_pluginSatz->setMinimumHeight(34);
    m_pluginSatz->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 6px 8px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary),
             QString::fromLatin1(Style::kInsetBg),
             QString::fromLatin1(Style::kBorderSubtle)));
    m_pluginSatz->setText(Sprache::T(QStringLiteral(
        "Point at a plugin to read what it does.")));
    col->addWidget(m_pluginSatz);

    // Die Auswahllisten aus dem, was schon gefunden wurde. Ohne das
    // steht nach jedem Profilwechsel „— leer —" und sonst nichts.
    refillInsertCombos();
    rebuildPluginCatalogue();

    // Den Hinweistext jetzt erst setzen, wo alles steht. Vorher war
    // er beim ersten Aufschlagen LEER -- refreshInsertCards() lief
    // nur im Suchlauf, also sah man den Satz, der erklaert, was
    // dieser Reiter ueberhaupt ist, erst nachdem man ihn benutzt
    // hatte. Am laufenden Fenster gesehen.
    refreshInsertCards();

    col->addStretch(1);
    return page;
}

QString StripWindow::insertPositionName(int at)
{
    if (at >= StripChain::kStageCount) {
        return Sprache::T(QStringLiteral("after the limiter"));
    }
    return Sprache::T(QStringLiteral("before %1")).arg(
        QString::fromLatin1(
            StripChain::stageName(static_cast<StripChain::Stage>(at))));
}

// Der Suchlauf. Nur auf Knopfdruck, und das ist Absicht: ein Programm,
// das beim Start jede installierte Audio Unit oeffnet, laedt fremden
// Code, den niemand bestellt hat, und braucht dafuer Sekunden.
void StripWindow::scanPlugins()
{
    if (!PluginHost::available()) {
        m_pluginsScanned = true;
        rebuildPluginCatalogue();
        refreshInsertCards();
        return;
    }
    m_plugins = PluginHost::scan();
    m_pluginsScanned = true;

    refillInsertCombos();
    rebuildPluginCatalogue();
    refreshInsertCards();
}

// ── Die beiden Auswahllisten fuellen ────────────────────────────────
//
// „kann keine auswählen."
//
// Das Fuellen stand bis hierher IN scanPlugins(), und scanPlugins()
// laeuft nur einmal (m_pluginsScanned). Beim Umbau der Reiter -- und
// der passiert bei jedem Profilwechsel -- wird die Einschubseite neu
// gebaut, also stehen dort zwei FRISCHE, leere Auswahlfelder. Der
// Kachelkatalog wurde aus m_plugins neu gefuellt, die Listen nicht:
// unten standen alle Plugins, und oben war nichts auszuwaehlen.
//
// Jetzt fuellt diese Funktion aus dem, was bereits gefunden wurde,
// und sie wird von BEIDEN Stellen gerufen: vom Suchlauf und vom
// Aufbau der Seite.
void StripWindow::refillInsertCombos()
{
    for (int slot = 0; slot < StripChain::kInsertCount; ++slot) {
        QComboBox* box = m_insertUi[static_cast<size_t>(slot)].plugin;
        if (!box) { continue; }
        const QSignalBlocker block(box);
        box->clear();
        box->addItem(Sprache::T(QStringLiteral("— empty —")), QString());
        for (const PluginDescriptor& d : m_plugins) {
            box->addItem(d.vendor.isEmpty()
                             ? d.name
                             : QStringLiteral("%1 — %2").arg(d.vendor, d.name),
                         d.identifier);
            // Auch in der Auswahlliste, nicht nur bei den Kacheln:
            // dort wird genauso gesucht.
            box->setItemData(box->count() - 1,
                             PluginPlacement::beschreibung(d.name, d.vendor),
                             Qt::ToolTipRole);
        }
    }
}

// Der Katalog: nach Gruppen, und jede Gruppe sagt, wohin sie gehoert.
//
// „wichtig ist, dass man weiss, wo man diese einbauen soll. das muss
// deutlich markiert sein, vielleicht in gruppen oder so."
//
// Die Zuordnung ist geraten -- eine Audio Unit nennt ihre Gattung
// nicht. Geraten wird mit Netz (core/strip/PluginPlacement.h), und was
// nicht zuzuordnen ist, steht sichtbar unter „noch nicht eingeordnet"
// statt stillschweigend in der erstbesten Gruppe.
void StripWindow::rebuildPluginCatalogue()
{
    if (!m_catalogue) { return; }

    auto* old = m_catalogue->layout();
    if (old != nullptr) {
        while (QLayoutItem* it = old->takeAt(0)) {
            if (QWidget* w = it->widget()) { w->deleteLater(); }
            delete it;
        }
        delete old;
    }
    auto* col = new QVBoxLayout(m_catalogue);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(14);

    if (m_plugins.isEmpty()) { return; }

    using PluginPlacement::Group;
    QMap<int, QVector<PluginDescriptor>> byGroup;
    for (const PluginDescriptor& d : m_plugins) {
        byGroup[int(PluginPlacement::guess(d.name, d.vendor))].push_back(d);
    }

    static const Group kOrder[] = {
        Group::Clean, Group::Eq, Group::DeEss, Group::Compress,
        Group::Colour, Group::Final, Group::Measure, Group::Unknown
    };

    for (const Group g : kOrder) {
        const auto at = byGroup.constFind(int(g));
        if (at == byGroup.constEnd() || at->isEmpty()) { continue; }

        auto* head = new QHBoxLayout;
        auto* title = new QLabel(PluginPlacement::groupName(g), m_catalogue);
        title->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; letter-spacing: 0.16em; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
        head->addWidget(title);

        const int at2 = PluginPlacement::suggestedPosition(g);
        if (at2 >= 0) {
            auto* where = new QLabel(
                QStringLiteral("→  %1").arg(insertPositionName(at2)),
                m_catalogue);
            where->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; }")
                .arg(QString::fromLatin1(Style::kBlueBorder)));
            head->addWidget(where);
        }
        head->addStretch(1);
        col->addLayout(head);

        auto* hint = new QLabel(PluginPlacement::groupHint(g), m_catalogue);
        hint->setWordWrap(true);
        hint->setStyleSheet(dimStyle());
        col->addWidget(hint);

        // Drei nebeneinander. Eine Spalte waere eine Liste; am
        // gerenderten Blatt waren sechsundzwanzig volle Zeilen genau
        // das -- etwas, durch das man scrollt, statt einer Auslage,
        // die man ueberblickt.
        auto* grid = new QGridLayout;
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(5);
        int n = 0;
        for (const PluginDescriptor& d : *at) {
            grid->addWidget(new PluginChip(d, m_catalogue, m_pluginSatz),
                            n / 3, n % 3);
            ++n;
        }
        for (int column = 0; column < 3; ++column) {
            grid->setColumnStretch(column, 1);
        }
        col->addLayout(grid);
    }
}

void StripWindow::chooseInsertPlugin(int slot)
{
    StripChain* c = chain();
    QComboBox* box = m_insertUi[static_cast<size_t>(slot)].plugin;
    if (!c || !box) { return; }

    const QString id = box->currentData().toString();
    if (id.isEmpty()) {
        c->setInsertPlugin(slot, nullptr);
    } else {
        std::unique_ptr<AudioPlugin> p = PluginHost::load(id);
        // Ein Plugin, das sich nicht oeffnen laesst, laesst den Platz
        // leer. Keine Meldung mit OK-Knopf: die Karte sagt es, und
        // zwar dort, wo man gerade hinsieht.
        c->setInsertPlugin(slot, std::move(p));
    }
    persist();
    if (m_chainView) { m_chainView->refreshInserts(); }
    refreshInsertCard(slot);
}

// Ein Plugin ist auf der Kette gelandet.
//
// Welcher der zwei Plaetze es aufnimmt, entscheidet nicht der
// Betreiber -- er hat auf eine STELLE gezielt, nicht auf einen Platz.
// Also: ein freier Platz, sonst der, der schon an dieser Stelle
// haengt (dann ist es ein Austausch, und das ist offensichtlich das
// Gemeinte), sonst gar nichts und ein Satz dazu.
void StripWindow::dropPluginAt(const QString& identifier, int position)
{
    StripChain* c = chain();
    if (!c || identifier.isEmpty()) { return; }

    int slot = -1;
    for (int i = 0; i < StripChain::kInsertCount; ++i) {
        if (c->insertPlugin(i) == nullptr) { slot = i; break; }
    }
    if (slot < 0) {
        for (int i = 0; i < StripChain::kInsertCount; ++i) {
            if (c->insertPosition(i) == position) { slot = i; break; }
        }
    }
    if (slot < 0) {
        if (m_insertsNote) {
            m_insertsNote->setText(QStringLiteral(
                "Both slots are full. Take one out with the × on its "
                "tile, then drop this one in."));
        }
        if (m_tabs) { m_tabs->setCurrentIndex(m_tabs->count() - 1); }
        return;
    }

    std::unique_ptr<AudioPlugin> p = PluginHost::load(identifier);
    if (!p) {
        if (m_insertsNote) {
            m_insertsNote->setText(QStringLiteral(
                "That plugin would not open. The slot is left empty."));
        }
        return;
    }
    c->setInsertPosition(slot, position);
    c->setInsertEnabled(slot, true);
    c->setInsertPlugin(slot, std::move(p));
    persist();
    if (m_chainView) { m_chainView->refreshInserts(); }
    refreshInsertCard(slot);
}

void StripWindow::clearInsert(int slot)
{
    StripChain* c = chain();
    if (!c) { return; }
    c->setInsertPlugin(slot, nullptr);
    persist();
    if (m_chainView) { m_chainView->refreshInserts(); }
    refreshInsertCard(slot);
}

void StripWindow::refreshInsertCards()
{
    for (int slot = 0; slot < StripChain::kInsertCount; ++slot) {
        refreshInsertCard(slot);
    }
    if (!m_insertsNote) { return; }
    if (!PluginHost::available()) {
        m_insertsNote->setText(QStringLiteral(
            "Longpath hosts Audio Units on macOS. This build has no "
            "host for this platform, so the slots stay empty — the "
            "eight stages above are the whole strip here."));
        if (m_insertScanBtn) { m_insertScanBtn->setEnabled(false); }
        return;
    }
    if (!m_pluginsScanned) {
        m_insertsNote->setText(QStringLiteral(
            "Two slots for a plugin of your own, anywhere in the chain "
            "you like. Nothing is loaded and nothing is searched for "
            "until you ask — the eight stages above are what Longpath "
            "does on its own, and they stay that way."));
        return;
    }
    m_insertsNote->setText(Sprache::T(QStringLiteral(
        "%1 effect(s) found, sorted by where they usually belong. "
        "Drag one onto the chain at the top of this window — the gap "
        "you are over lights up, and that is where it lands. The × on "
        "its tile takes it out again. The groups are a suggestion, not "
        "a rule: the position is yours to pick."))
        .arg(m_plugins.size()));
}

void StripWindow::refreshInsertCard(int slot)
{
    if (slot < 0 || slot >= StripChain::kInsertCount) { return; }
    auto& ui = m_insertUi[static_cast<size_t>(slot)];
    StripChain* c = chain();
    if (!ui.note || !ui.params) { return; }

    AudioPlugin* p = c != nullptr ? c->insertPlugin(slot) : nullptr;

    if (ui.on) {
        const QSignalBlocker b(ui.on);
        ui.on->setChecked(c != nullptr && c->insertEnabled(slot));
        ui.on->setEnabled(p != nullptr);
    }
    if (ui.editor) {
        ui.editor->setEnabled(p != nullptr && p->hasEditor());
    }
    if (ui.position && c != nullptr) {
        const QSignalBlocker b(ui.position);
        const int at = c->insertPosition(slot);
        const int idx = ui.position->findData(at);
        if (idx >= 0) { ui.position->setCurrentIndex(idx); }
    }
    if (ui.plugin) {
        const QSignalBlocker b(ui.plugin);
        const QString id = p != nullptr ? p->descriptor().identifier : QString();
        const int idx = ui.plugin->findData(id);
        ui.plugin->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    // Die Regler neu aufbauen. Ein Plugin wechselt seine Parameter
    // nicht im Betrieb, also geschieht das nur beim Wechsel -- und
    // dann vollstaendig, statt zu versuchen, alte Regler umzuwidmen.
    while (QLayoutItem* item = ui.paramForm->takeAt(0)) {
        if (QWidget* w = item->widget()) { w->deleteLater(); }
        delete item;
    }

    if (p == nullptr) {
        ui.note->setText(m_pluginsScanned
            ? Sprache::T(QStringLiteral("Empty. Pick one above."))
            : QStringLiteral("Empty."));
        return;
    }

    const PluginDescriptor d = p->descriptor();
    const int lat = p->latencyFrames();
    QString t = QStringLiteral("%1 · %2").arg(d.format, d.vendor.isEmpty()
                                                  ? QStringLiteral("—")
                                                  : d.vendor);
    if (lat > 0 && c != nullptr && c->sampleRate() > 0.0) {
        t += Sprache::T(QStringLiteral("  ·  adds %1 ms of delay"))
                 .arg(1000.0 * lat / c->sampleRate(), 0, 'f', 1);
    }
    const uint32_t bad = c != nullptr ? c->nonFiniteInsertBlocks(slot) : 0;
    if (bad > 0) {
        t += Sprache::T(QStringLiteral("  ·  %1 block(s) came back as not-a-number "
                            "and were discarded")).arg(bad);
    }
    ui.note->setText(t);

    // Hoechstens vierzig Regler. Darueber ist eine Liste kein
    // Bedienelement mehr, und ein Plugin mit hundert Parametern will
    // ohnehin in seiner eigenen Oberflaeche bedient werden -- die wir
    // bewusst nicht laden.
    const QVector<PluginParameter> params = p->parameters();
    constexpr int kMaxShown = 40;
    const int shown = std::min(int(params.size()), kMaxShown);
    for (int i = 0; i < shown; ++i) {
        const PluginParameter& pp = params.at(i);
        auto* row = new QWidget(ui.params);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);

        auto* value = new QLabel(row);
        value->setMinimumWidth(96);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setStyleSheet(QStringLiteral(
            "QLabel { font-family: " LP_MONO_QSS "; color: %1;"
            " border: none; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));

        const float span = pp.maxValue - pp.minValue;
        auto* slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(0, 1000);
        const float now = p->parameterValue(pp.id);
        slider->setValue(span > 0.0f
            ? int(std::lround(1000.0 * double(now - pp.minValue) / double(span)))
            : 0);
        value->setText(QStringLiteral("%1 %2")
            .arg(double(now), 0, 'f', 2).arg(pp.unit));

        const quint32 id = pp.id;
        const float lo = pp.minValue, hi = pp.maxValue;
        const QString unit = pp.unit;
        connect(slider, &QSlider::valueChanged, this,
                [this, slot, id, lo, hi, unit, value](int v) {
            StripChain* cc = chain();
            AudioPlugin* pl = cc != nullptr ? cc->insertPlugin(slot) : nullptr;
            if (pl == nullptr) { return; }
            const float val = lo + (hi - lo) * float(v) / 1000.0f;
            pl->setParameterValue(id, val);
            value->setText(QStringLiteral("%1 %2")
                .arg(double(val), 0, 'f', 2).arg(unit));
            persist();
        });

        h->addWidget(slider, 1);
        h->addWidget(value);
        ui.paramForm->addRow(pp.name, row);
    }
    if (params.size() > shown) {
        auto* more = new QLabel(
            Sprache::T(QStringLiteral("%1 further parameters are not shown."))
                .arg(params.size() - shown), ui.params);
        more->setStyleSheet(dimStyle() + QStringLiteral("QLabel{border:none;}"));
        ui.paramForm->addRow(QString(), more);
    }
}

void StripWindow::refreshMeters()
{
    // ── Hat sich der Mithörer selbst stumm geschaltet? ───────────────
    //
    // Die Wache sitzt im Audiofaden (StripLiveMonitor.h); hier wird
    // nur sichtbar gemacht, was sie getan hat — und das Häkchen geht
    // aus, damit das Einschalten wieder eine bewusste Handlung ist.
    // Rückkopplungsverdacht: NUR melden. Zweimal heute hat ein
    // automatischer Eingriff (stummschalten, leise drehen) das
    // Mithören unbrauchbar gemacht — der Regler gehört dem, der ihn
    // hört.
    if (m_live && m_liveNote && m_live->feedbackCut()
        && !m_liveNote->text().startsWith(QStringLiteral("Verdacht"))) {
        m_liveNote->setText(QStringLiteral(
            "Verdacht auf Rückkopplung: der Eingang steht seit drei "
            "Sekunden ohne Pause über −30 dBFS (%1 → %2). Wer spricht, "
            "macht Pausen — das hier macht keine. Dreh den Regler "
            "herunter oder nimm getrennte Geräte.")
            .arg(m_live->inputName(), m_live->outputName()));
    }


    adoptChainIfArrived();

    // Der Hinweistext haengt jetzt an der Verbindung, nicht mehr am
    // Vorhandensein des Kanalzugs -- also hier nachziehen, wenn das
    // Geraet kommt oder geht.
    const bool connected = m_radio != nullptr && m_radio->isConnected();
    if (connected != m_wasConnected) {
        m_wasConnected = connected;
        if (m_note) { m_note->setText(noteText()); }
    }

    // Die Kette, die wirklich rechnet -- ohne Funkgeraet ist das die
    // des Mithoerers. Hier stand chain(), und deshalb meldete die
    // Gate-Seite „Silent ... in -120.0 dBFS", waehrend die Stimme im
    // Kopfhoerer stand: die Widgets waren umgehaengt, dieser Zeiger
    // nicht. Ein halber Umbau sieht aus wie gar keiner.
    StripChain* c = displayChain();
    if (!c) { return; }

    // The tiles are the primary reading; the text below each panel is
    // for someone who wants the number rather than the shape.
    if (m_levels) { m_levels->tick(); }
    // The ported equaliser panel drives its own analyser on its own
    // timer, as AetherSDR does; nothing here needs to pump it.

    // Each stage's picture, from that stage's own atomics. Only the
    // visible tab is worth repainting — the others are hidden and a
    // repaint of a hidden widget is work nobody sees.
    for (QWidget* w : {static_cast<QWidget*>(m_gateCurve),
                       static_cast<QWidget*>(m_compCurve),
                       static_cast<QWidget*>(m_limiterCurve),
                       static_cast<QWidget*>(m_tubeCurve),
                       static_cast<QWidget*>(m_deEssCurve),
                       static_cast<QWidget*>(m_puduCurve)}) {
        if (!w || !w->isVisible()) { continue; }
        if (auto* d = qobject_cast<StripDynamicsCurve*>(w)) { d->refresh(); }
        else if (auto* sh = qobject_cast<StripShaperCurve*>(w)) { sh->refresh(); }
        else if (auto* b = qobject_cast<StripBandCurve*>(w))  { b->refresh(); }
    }
    // Das gemessene Bild. Rechnet nur, wenn es sichtbar ist -- eine
    // Langzeitmessung ueber zwei Sekunden zehnmal in der Sekunde fuer
    // einen verdeckten Reiter waere reine Heizung.
    if (m_transfer) { m_transfer->refresh(); }

    // ── Wo stehst du gegenüber dem Sprachziel ────────────────────────
    //
    // „Ich weiss nicht, welche Kurve richtig waere."
    //
    // Gemessen wird die eigene Stimme VOR der Kette (der Eingangsring
    // des Mithoerers), als Langzeitspektrum. Verglichen wird mit dem
    // Sprachziel aus VoiceAnalyzer::targetDb -- derselben Meinung,
    // die auch das Messblatt benutzt; zwei Ziele in einem Programm
    // waeren eines zuviel.
    //
    // Beide werden auf dieselbe Bezugsgroesse gebracht: das
    // Langzeitspektrum liegt im Mittel von 300..3000 Hz auf null, das
    // Ziel wird genauso verschoben. Verglichen wird damit die FORM
    // und nicht die Lautstaerke -- die haengt an der
    // Mikrofonverstaerkung und sagt ueber die Kurve nichts.
    //
    // Es wird nichts verstellt. Das ist der Unterschied zur
    // verworfenen Automatik, und er ist der ganze Punkt.
    // Der Takt steht ausserhalb: an ihm haengen auch die Eingangs-
    // und Scheitelfaktorzeile weiter unten, und die gibt es auch
    // ohne Entzerrerseite.
    if (++m_zielTick >= 5) { m_zielTick = 0; }   // zweimal je Sekunde
    if (m_eqPanel != nullptr) {
        if (m_zielTick == 0) {
            QVector<double> abw;
            if (m_live != nullptr && m_live->isRunning()) {
                const int rate = int(m_live->inputRate());
                const int want = rate * 2;      // zwei Sekunden
                if (m_zielScratch.size() < size_t(want)) {
                    m_zielScratch.resize(size_t(want));
                }
                const int got = m_live->inputRing().snapshot(
                    m_zielScratch.data(), want);
                const std::vector<double> ltas =
                    Ltas::analyse(m_zielScratch.data(), got, rate);
                const std::vector<double>& mitten = Ltas::centresHz();
                if (!ltas.empty() && ltas.size() == mitten.size()) {
                    // Das Ziel auf dieselbe Bezugsgroesse legen.
                    double sum = 0.0;
                    int    n   = 0;
                    for (double hz : mitten) {
                        if (hz >= 300.0 && hz <= 3000.0) {
                            sum += VoiceAnalyzer::targetDb(hz);
                            ++n;
                        }
                    }
                    const double tMean = n > 0 ? sum / n : 0.0;

                    const auto gemessenBei = [&](double hz) {
                        if (hz <= mitten.front()) { return ltas.front(); }
                        if (hz >= mitten.back())  { return ltas.back(); }
                        for (size_t i = 1; i < mitten.size(); ++i) {
                            if (hz <= mitten[i]) {
                                const double t = std::log(hz / mitten[i - 1])
                                               / std::log(mitten[i] / mitten[i - 1]);
                                return ltas[i - 1]
                                     + t * (ltas[i] - ltas[i - 1]);
                            }
                        }
                        return ltas.back();
                    };

                    for (const auto& sl : StripEqSimpleBar::bandSlots()) {
                        const double hz = double(sl.freqHz);
                        abw.push_back(gemessenBei(hz)
                                      - (VoiceAnalyzer::targetDb(hz) - tMean));
                    }
                }
            }
            m_eqPanel->setZiel(abw);
        }
    }
    // Zweimal je Sekunde reicht fuer einen Pegel, und es ist ruhiger
    // anzusehen als zehnmal. Derselbe Takt wie die Zielanzeige.
    if (m_eingangNote && m_zielTick == 0) {
        // Dieselbe Zahl, mit der der Knopf rechnet -- sonst zeigt die
        // Zeile das eine und der Druck tut das andere.
        //
        // Und: steht der Hauptschalter aus, misst die Kette NICHT --
        // processMono() kehrt vorher um. Die Zahl von vorhin stehen
        // zu lassen hat am 2026-10-14 eine Stunde gekostet: die Zeile
        // sagte „0,0 dBFS, 12 dB zu viel", waehrend am Apollo in
        // Wahrheit -55 dBFS ankamen. Ein Messgeraet, das bei
        // abgeschaltetem Messwerk den letzten Zeigerstand behaelt,
        // ist schlimmer als keines.
        const bool laeuft = c->isEnabled();
        const double roh = laeuft ? double(c->speechPeakDb()) : -120.0;
        if (!laeuft) {
            // ── Aber der Mithörer misst weiter ──────────────────
            //
            // „Kette aus — nichts zu messen" war ehrlich, aber taub.
            // Am 2026-10-14 hat genau dieser blinde Fleck den halben
            // Abend gekostet: am Apollo kamen erst -50 dBFS an und
            // später 0,0, und an der Stelle, an der man hinsieht,
            // stand beide Male nichts. Der Mithörer misst unabhängig
            // vom Hauptschalter — also zeigt die Zeile seine Zahl.
            const double mit = (m_live && m_live->isRunning())
                ? double(m_live->inputPeakDbFs()) : -120.0;
            if (mit > -119.0) {
                m_eingangNote->setText(
                    mit >= -1.0
                        ? Sprache::T(QStringLiteral(
                              "Input: %1 dBFS  ·  ⚠ clipping — turn the "
                              "interface gain down"))
                              .arg(mit, 0, 'f', 1)
                        : Sprache::T(QStringLiteral(
                              "Input: %1 dBFS  ·  strip off, chain not "
                              "measured"))
                              .arg(mit, 0, 'f', 1));
            } else {
                m_eingangNote->setText(Sprache::T(QStringLiteral(
                    "Input: strip is off — nothing being measured")));
            }
            if (m_eingangBtn) { m_eingangBtn->setEnabled(false); }
        } else {
        if (m_eingangBtn && m_eingangAuto) {
            m_eingangBtn->setEnabled(!m_eingangAuto->isChecked()
                                     && roh >= -90.0);
        }
        if (roh < -90.0) {
            m_eingangNote->setText(Sprache::T(QStringLiteral(
                "Input: ——  ·  target −12 dBFS  ·  speak")));
        } else {
            const double fehlt = -12.0 - roh;
            // Übersteuerung schlägt jede Empfehlung: unter einer
            // abgeschnittenen Spitze ist keine Einstellung zu
            // beurteilen, und der Satz „4 dB fehlen" waere dort
            // schlicht falsch.
            const double mit = (m_live && m_live->isRunning())
                ? double(m_live->inputPeakDbFs()) : -120.0;
            if (mit >= -1.0) {
                m_eingangNote->setText(Sprache::T(QStringLiteral(
                    "Input: %1 dBFS  ·  ⚠ clipping — turn the interface "
                    "gain down")).arg(mit, 0, 'f', 1));
            } else
            m_eingangNote->setText(
                std::fabs(fehlt) < 4.0
                    ? Sprache::T(QStringLiteral(
                          "Input: %1 dBFS  ·  target −12 dBFS  ·  fits"))
                          .arg(roh, 0, 'f', 1)
                    : Sprache::T(QStringLiteral(
                          "Input: %1 dBFS  ·  target −12 dBFS  ·  %2 dB %3"))
                          .arg(roh, 0, 'f', 1)
                          .arg(std::fabs(fehlt), 0, 'f', 0)
                          .arg(fehlt > 0 ? Sprache::T(QStringLiteral("missing"))
                                         : Sprache::T(QStringLiteral("too hot"))));
        }
        }
    }
    if (m_crestNote && m_zielTick == 0) {
        StripChain* cc = displayChain();
        const float in  = cc ? cc->inputCrestDb()  : 0.0f;
        const float out = cc ? cc->outputCrestDb() : 0.0f;
        if (in <= 0.0f) {
            // „Unbekannt ist ein Strich, keine Null" (HAUSSTIL §7).
            m_crestNote->setText(QStringLiteral(
                "Scheitelfaktor —— · sprechen, dann steht hier, wieviel "
                "Spitze über dem Effektivwert liegt"));
        } else {
            m_crestNote->setText(QStringLiteral(
                "Scheitelfaktor: vor der Kette %1 dB · danach %2 dB · "
                "%3 — die Endstufe begrenzt die Spitze, gehört wird der "
                "Effektivwert")
                    .arg(double(in),  0, 'f', 1)
                    .arg(double(out), 0, 'f', 1)
                    .arg(out > 0.0f && out < in - 0.5f
                             ? QStringLiteral("%1 dB mehr Sprechleistung")
                                   .arg(double(in - out), 0, 'f', 1)
                             : QStringLiteral("die Kette gewinnt hier nichts")));
        }
    }

    refreshStageText();
    if (m_chainView) {
        m_chainView->setReduction(StripChain::Stage::Gate,
                                  c->gate().gainReductionDb());
        m_chainView->setReduction(StripChain::Stage::Comp,
                                  c->comp().gainReductionDb());
        m_chainView->setReduction(StripChain::Stage::DeEss,
                                  c->deEss().gainReductionDb());
    }

    if (m_clipMeter) {
        m_clipMeter->setText(Sprache::T(QStringLiteral(
            "Clipper: %1 dB of peak taken off · crest now %2 dB"))
                .arg(c->clipper().clippedDb(), 0, 'f', 1)
                .arg(c->outputCrestDb() > 0.0f
                         ? QString::number(double(c->outputCrestDb()),
                                           'f', 1)
                         : QStringLiteral("——")));
    }
    if (m_nrMeter) {
        const auto& nr = c->nr();
        m_nrMeter->setText(
            !nr.isUsable()
                ? QStringLiteral("Denoise: not available — needs 48 kHz "
                                 "and the bundled model")
                : Sprache::T(QStringLiteral("Denoise: %1 dB taken out · voice %2 %"))
                      .arg(nr.reductionDb(), 0, 'f', 1)
                      .arg(int(nr.voiceProbability() * 100.0f + 0.5f)));
    }
    if (m_gateMeter) {
        m_gateMeter->setText(Sprache::T(QStringLiteral(
            "Gate: %1 · %2 dB of attenuation · in %3 dBFS"))
                .arg(c->gate().gateOpen() ? QStringLiteral("open")
                                          : QStringLiteral("shut"))
                .arg(c->gate().gainReductionDb(), 0, 'f', 1)
                .arg(c->gate().inputPeakDb(), 0, 'f', 1));
    }
    if (m_deEssMeter) {
        m_deEssMeter->setText(Sprache::T(QStringLiteral(
            "De-esser: %1 dB off the sibilance · sidechain %2 dBFS"))
                .arg(c->deEss().gainReductionDb(), 0, 'f', 1)
                .arg(c->deEss().sidechainPeakDb(), 0, 'f', 1));
    }
    if (m_compMeter) {
        // Der ausgerechnete Ausgleich gehoert hierher und nicht in
        // eine Hilfe: er ist der Unterschied zwischen „der Regler tut
        // nichts" und „der Regler tut genau das".
        m_compMeter->setText(Sprache::T(QStringLiteral(
            "Compressor: %1 dB of reduction · %2 dB made up · "
            "%3 dB caught by its own brickwall · in %4 dBFS · out %5 dBFS"))
                .arg(c->comp().gainReductionDb(), 0, 'f', 1)
                .arg(c->comp().makeupDb() + c->comp().autoMakeupDb(),
                     0, 'f', 1)
                .arg(c->comp().limiterGrDb(), 0, 'f', 1)
                .arg(c->comp().inputPeakDb(), 0, 'f', 1)
                .arg(c->comp().outputPeakDb(), 0, 'f', 1));
    }
}

} // namespace Longpath
