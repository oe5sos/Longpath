// =================================================================
// src/gui/applets/RttyDecoderApplet.cpp  (Longpath)
// =================================================================
// Attribution: see RttyDecoderApplet.h -- structural derivative of
// AetherSDR's RTTY control set (GPLv3, Jeremy [KK7GWY] /
// ten9876/AetherSDR).
//
// Modification history (Longpath):
//   2026-09-06 -- Created alongside RttyDecoderApplet.h; see that
//                 file's own Modification history section for detail.
// =================================================================

#include "RttyDecoderApplet.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/RttyDecoderSensitivity.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RttyDecoder.h"
#include "core/audio/AudioTapRing.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/DxSpot.h"       // DxSpot — der Fund, der hinausgeht

#include "core/MoxController.h"
#include "core/RttyTx.h"
#include "core/audio/CompositeTxMicRouter.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <cmath>

namespace Longpath {

namespace {
QString baudKey()        { return QStringLiteral("RttyDecoder_BaudRate"); }
QString reverseKey()     { return QStringLiteral("RttyDecoder_ReversePolarity"); }
QString sensitivityKey() { return QStringLiteral("RttyDecoder_Sensitivity"); }

// Standard ham HF RTTY baud rates. 45.45 (60 wpm ITU-R teleprinter
// standard) is the universal default; the others are real, commonly-used
// alternates, not an invented list.
constexpr double kBaudRates[] = {45.45, 50.0, 56.88, 75.0, 100.0};

// Half a second of interleaved-stereo float32 at the native WDSP RX rate
// (48 kHz) -- matches AsrService's own ring sizing rationale (see
// MainWindow_Asr.cpp): reichlich for a 50ms pump timer.
constexpr int kTapRingFloats = 48000;
constexpr int kPumpIntervalMs = 50;

// Cap the visible transcript so a long, unattended RTTY session doesn't
// grow the QPlainTextEdit's document without bound.
constexpr int kMaxCharCount = 4000;

// So viele Zeichen behaelt der Puffer, aus dem die Rufzeichensuche
// liest. „CQ CQ CQ DE DL1ABC DL1ABC DL1ABC K" sind 36 Zeichen; 120
// fasst das samt Vorlauf und einem zweiten Anruf, und ein alter
// Anruf fliegt nach rund zwei Zeilen heraus statt nachzuhallen.
constexpr int kRufPufferZeichen = 120;
} // namespace

RttyDecoderApplet::RttyDecoderApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    loadSettings();

    m_decoder = new RttyDecoder(this);
    connect(m_decoder, &RttyDecoder::textDecoded,
            this, &RttyDecoderApplet::onTextDecoded);
    connect(m_decoder, &RttyDecoder::statsUpdated,
            this, &RttyDecoderApplet::onStatsUpdated);
    m_decoder->setBaudRate(static_cast<float>(kBaudRates[m_baudCombo->currentIndex()]));
    m_decoder->setReversePolarity(m_reverseBtn->isChecked());
    m_decoder->start();

    // Unbind promptly when the bound slice goes away -- without this, the
    // stale sliceIndex left in AudioEngine's tap keeps routing whatever
    // slice RadioModel::addSlice() next reuses that index for (its
    // lowest-free-index reuse policy) into this decoder, silently -- not
    // a crash, but the wrong signal decodes under this applet's title.
    // This is the safety net for the slice DISAPPEARING outright; the
    // separate case of the ACTIVE slice's identity changing (a KiwiSDR
    // profile, a removed-and-replaced slice) without this one being
    // removed is MainWindow's job -- see
    // MainWindow::rebindRttyRadeAvailability(), which re-calls setSlice()
    // on every RadioModel::activeSliceChanged (bench-found 2026-09-07;
    // this applet used to be bound once, for the whole session).
    // Empfaengerwahl: liest die gemerkte Wahl, haengt sich an den
    // Empfaengerwechsel und schreibt den Zusatz ("RTTY DECODER · RX2") in
    // die Titelleiste. Die Reihenfolge zu buildUI() ist egal --
    // `initReceiverBinding()` setzt den Zusatz selbst nach.
    initReceiverBinding();
    connect(this, &AppletWidget::boundSliceChanged, this, [this](SliceModel* s) {
        setSlice(s);
    });

    if (m_model) {
        connect(m_model, &RadioModel::sliceRemoved, this, [this](int index) {
            if (m_slice && m_slice->sliceIndex() == index) {
                setSlice(nullptr);
            }
        });
    }

    m_tapRing = std::make_unique<AudioTapRing>(kTapRingFloats);

    m_pumpTimer = new QTimer(this);
    m_pumpTimer->setInterval(kPumpIntervalMs);
    connect(m_pumpTimer, &QTimer::timeout, this, [this]() {
        if (!m_tapRing || !m_decoder) { return; }
        // Fester Zwischenspeicher, wie beim ASR-Abholer (MainWindow_Asr.cpp):
        // ein Zeitgeber, der 20x/s laeuft, soll nicht 20x/s Speicher anfordern.
        static thread_local std::vector<float> scratch;
        const int want = kTapRingFloats / 4;  // ~125ms Stereo, reichlich Rand
        if (static_cast<int>(scratch.size()) < want) { scratch.resize(want); }
        const int got = m_tapRing->read(scratch.data(), want);
        if (got >= 2) {
            m_decoder->feedAudio(scratch.data(), got / 2);
        }
    });
    m_pumpTimer->start();
}

RttyDecoderApplet::~RttyDecoderApplet()
{
    // Stop the pump FIRST: with m_tapRing/m_decoder as a genuine C++ member
    // (unique_ptr) and a QObject child respectively, they are destroyed at
    // two DIFFERENT points during teardown (the member in this destructor's
    // own unwind, the QObject child later via ~QObject()'s automatic child
    // cleanup) -- stopping the timer explicitly here removes any question
    // of whether its queued lambda could still fire in between and touch
    // either one after it's gone.
    if (m_pumpTimer) { m_pumpTimer->stop(); }
    if (m_model && m_model->audioEngine()) {
        m_model->audioEngine()->setRttyTap(nullptr, -1);
    }
    if (m_decoder) { m_decoder->stop(); }
}

void RttyDecoderApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    // Kein eigener Kopf: Zelle bzw. Fenster tragen ihn (auf dem Blatt
    // vom 2026-09-17 stand "RTTY DECODER" zweimal uebereinander).

    // -- Row 1: status ---------------------------------------------------
    auto* statusRow = new QHBoxLayout();
    m_markShiftInfo = new QLabel(QStringLiteral("Mark — Hz · Shift — Hz"), this);
    m_markShiftInfo->setFont(Style::monoFont(font(), 11));
    m_markShiftInfo->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    statusRow->addWidget(m_markShiftInfo);
    statusRow->addStretch(1);

    m_snrValue = new QLabel(QStringLiteral("— dB"), this);
    m_snrValue->setFont(Style::monoFont(font(), 11));
    m_snrValue->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    statusRow->addWidget(m_snrValue);

    // Zustand als umrandete Kapsel, HAUSSTIL.md Regel 5.
    m_lockCapsule = new QLabel(tr("NO LOCK"), this);
    m_lockCapsule->setAlignment(Qt::AlignCenter);
    m_lockCapsule->setFixedHeight(20);
    // Schrift per setFont auf der Leiter (9 px versal), nicht als
    // "font-size: 10px" im Stylesheet — die drei Stellen hier waren die
    // letzten Groessen ausserhalb der Leiter (Drift-Ratsche).
    m_lockCapsule->setFont(Style::capsFont(font(), Style::kFontCaption));
    m_lockCapsule->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 10px; padding: 0 10px; }")
        .arg(Style::kBadgeOffBg, Style::kTextTertiary, Style::kBorder));
    statusRow->addWidget(m_lockCapsule);
    root->addLayout(statusRow);

    // -- Row 2: mark/space levels -----------------------------------------
    auto* levelRow = new QHBoxLayout();
    m_markLevel = new HGauge(this);
    m_markLevel->setTitle(QStringLiteral("MARK"));
    m_markLevel->setRange(0.0, 1.0);
    levelRow->addWidget(m_markLevel);
    m_spaceLevel = new HGauge(this);
    m_spaceLevel->setTitle(QStringLiteral("SPACE"));
    m_spaceLevel->setRange(0.0, 1.0);
    levelRow->addWidget(m_spaceLevel);
    root->addLayout(levelRow);

    root->addWidget(divider());

    // -- Row 3: decoded text ------------------------------------------------
    m_textOutput = new QPlainTextEdit(this);
    m_textOutput->setReadOnly(true);
    m_textOutput->setFont(Style::monoFont(font(), 12));
    m_textOutput->setMinimumHeight(90);
    m_textOutput->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    root->addWidget(m_textOutput);

    // -- Row 4: controls -----------------------------------------------------
    auto* ctrlRow = new QHBoxLayout();
    m_baudCombo = new QComboBox(this);
    for (double b : kBaudRates) {
        m_baudCombo->addItem(QStringLiteral("%1 Bd").arg(b, 0, 'f', 2), b);
    }
    connect(m_baudCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx < 0) { return; }
        if (m_decoder) { m_decoder->setBaudRate(static_cast<float>(kBaudRates[idx])); }
        saveSettings();
    });
    ctrlRow->addWidget(m_baudCombo);

    m_reverseBtn = blueToggle(QStringLiteral("REV"), 50);
    m_reverseBtn->setCheckable(true);
    m_reverseBtn->setToolTip(QStringLiteral(
        "Umgekehrte Polaritaet: Space liegt ueber statt unter Mark."));
    connect(m_reverseBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_decoder) { m_decoder->setReversePolarity(on); }
        saveSettings();
    });
    ctrlRow->addWidget(m_reverseBtn);

    ctrlRow->addWidget(new QLabel(QStringLiteral("Empf."), this));
    m_sensitivitySlider = new QSlider(Qt::Horizontal, this);
    m_sensitivitySlider->setRange(0, 100);
    m_sensitivityValue = insetValue(QStringLiteral("0"), 32);
    connect(m_sensitivitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_sensitivity = v;
        m_sensitivityValue->setText(QString::number(v));
        saveSettings();
    });
    ctrlRow->addLayout(sliderRow(QString(), m_sensitivitySlider, m_sensitivityValue, 0));

    m_clearBtn = styledButton(QStringLiteral("Leeren"), 60);
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_textOutput->clear();
        m_charCount = 0;
    });
    ctrlRow->addWidget(m_clearBtn);

    root->addLayout(ctrlRow);

    // ── Zeile 5: senden (2026-10-16) ─────────────────────────────────
    //
    // Der Sender (core/RttyTx.h) stand seit PR #257 fertig im Baum und
    // war von hier aus nicht erreichbar. Die Zeile ist absichtlich
    // schmal gehalten: ein Feld, zwei Knoepfe, eine Zeile Auskunft —
    // dasselbe Mass wie bei Ft8Applet, und kein Schalter, der von
    // selbst sendet.
    root->addWidget(divider());

    auto* sendRow = new QHBoxLayout();
    m_textFeld = new QLineEdit(this);
    m_textFeld->setPlaceholderText(tr("Text to send"));
    m_textFeld->setToolTip(tr(
        "Baudot has no lower case and no accented letters. Anything it "
        "does not know is dropped rather than altering the text."));
    connect(m_textFeld, &QLineEdit::returnPressed, this, [this]() {
        sendeText(m_textFeld->text());
    });
    sendRow->addWidget(m_textFeld, 1);

    m_sendBtn = styledButton(tr("Send"), 64);
    connect(m_sendBtn, &QPushButton::clicked, this, [this]() {
        sendeText(m_textFeld ? m_textFeld->text() : QString());
    });
    sendRow->addWidget(m_sendBtn);

    m_stopBtn = styledButton(tr("Stop"), 48);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() { abbrechen(); });
    sendRow->addWidget(m_stopBtn);

    root->addLayout(sendRow);

    m_sendeStatus = new QLabel(QString(), this);
    m_sendeStatus->setStyleSheet(
        QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    m_sendeStatus->setFont(Style::capsFont(font(), Style::kFontCaption));
    root->addWidget(m_sendeStatus);

    root->addStretch(1);
}

// ── Senden ───────────────────────────────────────────────────────────
//
// Die Reihenfolge ist dieselbe wie in Ft8Applet::sendeNachricht() und
// aus demselben Grund so herum: erst den Sendeweg legen, dann tasten.
// Andersherum ginge der Sender fuer einen Augenblick mit dem Mikrofon
// auf, nicht mit dem Ton.
void RttyDecoderApplet::sendeText(const QString& text)
{
    if (m_sendet) { return; }

    const QString sauber = text.trimmed();
    if (sauber.isEmpty()) {
        if (m_sendeStatus) { m_sendeStatus->setText(tr("nothing to send")); }
        return;
    }
    if (!m_model || !m_model->rttyTxSource() || !m_model->micRouter()) {
        if (m_sendeStatus) { m_sendeStatus->setText(tr("no transmit path")); }
        return;
    }

    // Mark und Shift kommen von der Scheibe, nicht aus einer zweiten
    // Einstellung — derselbe Grund, aus dem die Anzeige oben sie nur
    // liest: es gibt genau einen Ort, an dem sie stehen.
    const int markHz = m_slice ? m_slice->rttyMarkHz() : 2295;
    const int shiftHz = m_slice ? m_slice->rttyShiftHz() : 170;
    const float baud = m_baudCombo && m_baudCombo->currentIndex() >= 0
                           ? static_cast<float>(
                                 kBaudRates[m_baudCombo->currentIndex()])
                           : 45.45f;

    RttyTxSource* quelle = m_model->rttyTxSource();
    if (!quelle->vorbereiten(sauber, markHz, shiftHz, baud)) {
        if (m_sendeStatus) {
            m_sendeStatus->setText(tr("cannot send “%1”").arg(sauber));
        }
        return;
    }

    auto* router = m_model->micRouter();
    m_quelleVorher = static_cast<int>(router->activeSource());
    router->setActiveSource(MicSource::Rtty);
    quelle->starten();

    // ── Wenn der Bandplan nein sagt, nicht weitermachen ──────────────
    //
    // Wortgleich zu Ft8Applet: setMox(true) darf abgelehnt werden
    // (falsche Betriebsart, Frequenz ausserhalb des Bandes), und die
    // Ablehnung kommt ausschliesslich als moxRejected(grund) —
    // setMox() selbst gibt nichts zurueck. Ohne diese Verbindung
    // liefe der Ton die volle Laenge des Textes weiter, waehrend
    // nichts hinausgeht.
    bool abgelehnt = false;
    if (auto* mox = m_model->moxController()) {
        QMetaObject::Connection c = connect(
            mox, &MoxController::moxRejected, this,
            [this, &abgelehnt](const QString& grund) {
                abgelehnt = true;
                if (m_sendeStatus) {
                    m_sendeStatus->setText(tr("not transmitting: %1").arg(grund));
                }
            });
        mox->setMox(true);
        disconnect(c);
    }
    if (abgelehnt) {
        const QString grund = m_sendeStatus ? m_sendeStatus->text() : QString();
        abbrechen();
        if (m_sendeStatus && !grund.isEmpty()) { m_sendeStatus->setText(grund); }
        return;
    }

    m_sendet = true;
    if (m_sendBtn)  { m_sendBtn->setEnabled(false); }
    if (m_stopBtn)  { m_stopBtn->setEnabled(true); }
    if (m_textFeld) { m_textFeld->setEnabled(false); }

    // Wie lange der Text dauert, weiss die Quelle selbst: sie hat ihn
    // schon auf Proben gelegt. Ein Zuschlag von 300 ms, damit der
    // letzte Ton draussen ist, bevor der Sender abschaltet — dieselben
    // 300 ms wie bei FT8.
    const int dauerMs = quelle->proben() > 0
                            ? (quelle->proben() * 1000) / 48000
                            : 0;
    if (m_sendeStatus) {
        m_sendeStatus->setText(tr("sending (%1 s)")
                                   .arg(dauerMs / 1000.0, 0, 'f', 1));
    }

    if (!m_sendeEndeTimer) {
        m_sendeEndeTimer = new QTimer(this);
        m_sendeEndeTimer->setSingleShot(true);
        connect(m_sendeEndeTimer, &QTimer::timeout, this,
                [this]() { sendeEnde(); });
    }
    m_sendeEndeTimer->start(dauerMs + 300);
}

void RttyDecoderApplet::sendeEnde()
{
    if (!m_sendet) { return; }
    abbrechen();
    if (m_sendeStatus) { m_sendeStatus->setText(tr("sent")); }
}

void RttyDecoderApplet::abbrechen()
{
    if (m_sendeEndeTimer) { m_sendeEndeTimer->stop(); }

    // Erst den Sender aus, dann den Weg zurueck — in dieser
    // Reihenfolge ist zu keinem Zeitpunkt das Mikrofon auf Sendung.
    if (m_model && m_model->moxController()) {
        m_model->moxController()->setMox(false);
    }
    if (m_model && m_model->rttyTxSource()) {
        m_model->rttyTxSource()->abbrechen();
    }
    if (m_model && m_model->micRouter() && m_quelleVorher >= 0) {
        m_model->micRouter()->setActiveSource(
            static_cast<MicSource>(m_quelleVorher));
    }
    m_quelleVorher = -1;
    m_sendet = false;
    if (m_sendBtn)  { m_sendBtn->setEnabled(true); }
    if (m_stopBtn)  { m_stopBtn->setEnabled(false); }
    if (m_textFeld) { m_textFeld->setEnabled(true); }
}

void RttyDecoderApplet::loadSettings()
{
    auto& st = AppSettings::instance();

    const double savedBaud = st.value(baudKey(), QStringLiteral("45.45")).toString().toDouble();
    int baudIdx = 0;
    for (size_t i = 0; i < std::size(kBaudRates); ++i) {
        if (std::abs(kBaudRates[i] - savedBaud) < 0.01) { baudIdx = static_cast<int>(i); break; }
    }
    m_baudCombo->setCurrentIndex(baudIdx);

    const bool reverse = st.value(reverseKey(), QStringLiteral("False")).toString()
                          == QStringLiteral("True");
    m_reverseBtn->setChecked(reverse);

    const int sens = st.value(sensitivityKey(),
                              QString::number(kRttySensitivityDefault)).toInt();
    m_sensitivitySlider->setValue(sens);
    m_sensitivity = sens;
    m_sensitivityValue->setText(QString::number(sens));
}

void RttyDecoderApplet::saveSettings() const
{
    auto& st = AppSettings::instance();
    st.setValue(baudKey(), QString::number(kBaudRates[m_baudCombo->currentIndex()]));
    st.setValue(reverseKey(), m_reverseBtn->isChecked() ? QStringLiteral("True")
                                                         : QStringLiteral("False"));
    st.setValue(sensitivityKey(), QString::number(m_sensitivity));
}

void RttyDecoderApplet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }

    disconnect(m_markHzConn);
    disconnect(m_shiftHzConn);
    m_slice = slice;

    if (m_slice) {
        m_markHzConn = connect(m_slice, &SliceModel::rttyMarkHzChanged,
                               this, [this](int) { applyMarkShiftFromSlice(); });
        m_shiftHzConn = connect(m_slice, &SliceModel::rttyShiftHzChanged,
                                this, [this](int) { applyMarkShiftFromSlice(); });
    }
    applyMarkShiftFromSlice();
    updateAudioTap();
}

void RttyDecoderApplet::applyMarkShiftFromSlice()
{
    if (!m_slice) {
        m_markShiftInfo->setText(QStringLiteral("Mark — Hz · Shift — Hz"));
        return;
    }
    const int mark  = m_slice->rttyMarkHz();
    const int shift = m_slice->rttyShiftHz();
    m_markShiftInfo->setText(QStringLiteral("Mark %1 Hz · Shift %2 Hz")
                                  .arg(mark).arg(shift));
    if (m_decoder) {
        m_decoder->setMarkFreqHz(mark);
        m_decoder->setShiftHz(shift);
    }
}

void RttyDecoderApplet::updateAudioTap()
{
    if (!m_model || !m_model->audioEngine() || !m_tapRing) { return; }
    if (m_slice) {
        m_model->audioEngine()->setRttyTap(m_tapRing.get(), m_slice->sliceIndex());
    } else {
        m_model->audioEngine()->setRttyTap(nullptr, -1);
    }
}

void RttyDecoderApplet::syncFromModel()
{
    // `boundSlice()` statt `activeSlice()`: steht die Empfaengerwahl auf
    // "folgt dem aktiven" (Vorgabe), ist das dasselbe wie frueher. Hat
    // jemand einen festen Empfaenger gewaehlt, bleibt der RTTY-Decoder
    // an seinem, auch waehrend woanders gearbeitet wird.
    setSlice(boundSlice());
}

bool RttyDecoderApplet::hasExtendedSettings() const
{
    return true;
}

void RttyDecoderApplet::openExtendedSettings()
{
    // Ein frisches Blatt bei jedem Oeffnen, wie bei RxApplet: Qt::Popup
    // schliesst beim Klick daneben, WA_DeleteOnClose raeumt auf.
    auto* popup = new QWidget(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border: 1px solid %2; }")
        .arg(Style::kPanelBg, Style::kBorder));
    auto* vbox = new QVBoxLayout(popup);
    vbox->setContentsMargins(10, 10, 10, 10);
    vbox->addWidget(receiverChoiceWidget(popup));
    popup->adjustSize();
    popup->move(mapToGlobal(QPoint(width() - popup->width(), 0)));
    popup->show();
}

QString RttyDecoderApplet::rufzeichenNachDe(const QString& strom)
{
    // Wie ein Rufzeichen: drei bis elf Zeichen, mindestens eine
    // Ziffer UND ein Buchstabe, die letzte Stelle keine Ziffer.
    // Wortgleich die Pruefung aus Ft8Applet::senderCallsign — sie
    // haelt dort „JN78", „-12", „RR73" und „73" heraus, und hier
    // zusaetzlich „599" und „5NN", die in RTTY in jedem Rapport
    // stehen.
    const auto wieRufzeichen = [](const QString& f) {
        if (f.size() < 3 || f.size() > 11) { return false; }
        bool ziffer = false;
        bool buchstabe = false;
        for (const QChar c : f) {
            if (c.isDigit()) { ziffer = true; }
            else if (c.isLetter()) { buchstabe = true; }
            else if (c != QLatin1Char('/')) { return false; }
        }
        if (!ziffer || !buchstabe) { return false; }
        return !f.back().isDigit();
    };

    // Ob das LETZTE Feld abgeschlossen ist, steht nicht in der Liste
    // der Felder, sondern im rohen Strom: simplified() wirft genau
    // das Trennzeichen weg, auf das es hier ankommt. „DE DL1ABC" und
    // „DE DL1ABC " sehen danach gleich aus — nur ist das erste ein
    // halb angekommenes Rufzeichen und das zweite ein fertiges.
    // Baudot schickt auch CR und LF; isSpace() deckt beides mit ab.
    const bool letztesFeldFertig =
        !strom.isEmpty() && strom.back().isSpace();

    const QStringList felder = strom.simplified().toUpper()
        .split(QLatin1Char(' '), Qt::SkipEmptyParts);

    // Von hinten: das letzte DE gilt. Ein Strom kann mehrere Anrufe
    // enthalten, und der juengste ist der, der gerade sendet.
    //
    // Wie weit hinten ein DE stehen darf, haengt am Abschluss: ist das
    // letzte Feld fertig, darf DE an zweitletzter Stelle stehen; ist
    // es noch im Wachsen, muss DE eine Stelle weiter vorn liegen,
    // sonst wird ein halbes Rufzeichen gemeldet. Genau dieser Fall
    // kostete beim ersten Anlauf einen roten Lauf: „CQ CQ DE DL1AB"
    // gab „DL1AB" zurueck.
    const int letzterDeIndex = felder.size() - (letztesFeldFertig ? 2 : 3);
    for (int i = letzterDeIndex; i >= 0; --i) {
        if (felder.at(i) != QLatin1String("DE")) { continue; }
        const QString kandidat = felder.at(i + 1);
        if (wieRufzeichen(kandidat)) { return kandidat; }
        // Ein DE mit Unsinn dahinter ist kein Treffer, aber auch kein
        // Grund, weiter nach vorn zu suchen: dann war die Stelle
        // verstuemmelt, und ein aelteres DE waere geraten.
        return {};
    }
    return {};
}

// ── Aus einem Anruf ein Fund (2026-10-17) ───────────────────────────
//
// Bis heute endete ein RTTY-Anruf im Textfeld und nirgends sonst. Der
// eigene FT8-Dekoder reicht seine Funde an RadioModel weiter
// (Spotliste, Sammler, PSK Reporter) — RTTY nicht, obwohl derselbe
// Weg offen lag. Hier ist er.
//
// Die Frequenz rechnet sich wie bei FT8: Wahlfrequenz plus Tonlage,
// bei unterem Seitenband (DIGL) minus. Die Tonlage ist bei RTTY die
// MARK-Frequenz von der Scheibe — es gibt dort genau einen Ort, an
// dem sie steht (siehe sendeText()).
void RttyDecoderApplet::meldeFundWennRufzeichen()
{
    if (!m_model) { return; }

    const QString ruf = rufzeichenNachDe(m_rufPuffer);
    if (ruf.isEmpty() || ruf == m_letzterGemeldeterRuf) { return; }
    m_letzterGemeldeterRuf = ruf;

    const double wahlHz = m_slice ? static_cast<double>(m_slice->frequency())
                                  : 0.0;
    if (wahlHz <= 0.0) { return; }
    const int markHz = m_slice ? m_slice->rttyMarkHz() : 2295;
    const bool unteresSeitenband =
        m_slice && m_slice->dspMode() == DSPMode::DIGL;

    DxSpot spot;
    spot.dxCall = ruf;
    spot.freqMhz = (wahlHz + (unteresSeitenband ? -markHz : markHz)) / 1.0e6;
    spot.snr = m_letzterSnrDb;
    // Der Modus steht im Kommentar, wie bei FT8:
    // SpotTableModel::extractMode liest ihn dort heraus. Dahinter der
    // Ausschnitt des Stroms, in dem der Anruf stand — ohne ihn ist im
    // Spot nicht zu sehen, WORAUS er gelesen wurde.
    spot.comment = QStringLiteral("RTTY  %1")
                       .arg(m_rufPuffer.simplified().right(60));
    spot.source = QStringLiteral("Longpath");
    spot.spotterCall = QStringLiteral("Longpath");
    spot.utcTime = QDateTime::currentDateTimeUtc().time();
    m_model->onNativeRttyDecode(spot);
}

void RttyDecoderApplet::onTextDecoded(const QString& text, float confidence)
{
    if (confidence < rttyConfThresholdFor(m_sensitivity)) { return; }

    auto cursor = m_textOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_textOutput->setTextCursor(cursor);
    m_textOutput->ensureCursorVisible();

    m_charCount += text.size();
    if (m_charCount > kMaxCharCount) {
        // NextCharacter, not Down: RTTY text has no guaranteed line breaks
        // at all (only appears if the far station sends a Baudot CR/LF),
        // so a long unbroken run is one continuously WRAPPED paragraph --
        // "lines" moved by QTextCursor::Down are the widget's current
        // soft-wrapped visual rows (panel-width- and font-size-dependent),
        // not a fixed character count. Trimming by exact character count
        // instead removes precisely the excess regardless of wrap width.
        const int excess = m_charCount - kMaxCharCount;
        auto trimCursor = m_textOutput->textCursor();
        trimCursor.movePosition(QTextCursor::Start);
        trimCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, excess);
        trimCursor.removeSelectedText();
        m_charCount = m_textOutput->toPlainText().size();
    }

    // Und derselbe Text noch einmal in den kurzen Puffer, aus dem die
    // Rufzeichensuche liest. Kurz gehalten (siehe Kopfdatei): lang
    // genug fuer „... DE DL1ABC DL1ABC K", kurz genug, dass ein alter
    // Anruf nicht ewig nachhallt.
    m_rufPuffer += text;
    if (m_rufPuffer.size() > kRufPufferZeichen) {
        m_rufPuffer = m_rufPuffer.right(kRufPufferZeichen);
    }
    meldeFundWennRufzeichen();
}

void RttyDecoderApplet::onStatsUpdated(float markLevel, float spaceLevel, float snrDb, bool locked)
{
    m_markLevel->setValue(markLevel);
    m_spaceLevel->setValue(spaceLevel);

    m_snrValue->setText(QStringLiteral("%1 dB").arg(snrDb, 0, 'f', 1));
    // Fuer den Fund (siehe meldeFundWennRufzeichen): der Spot braucht
    // einen Rapport, und der steht nur hier.
    m_letzterSnrDb = static_cast<int>(std::lround(snrDb));

    if (locked) {
        m_lockCapsule->setText(QStringLiteral("LOCK"));
        m_lockCapsule->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 10px; padding: 0 10px; }")
            .arg(Style::kGreenBg, Style::kGreenText, Style::kGreenBorder));
    } else {
        m_lockCapsule->setText(tr("NO LOCK"));
        m_lockCapsule->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 10px; padding: 0 10px; }")
            .arg(Style::kBadgeOffBg, Style::kTextTertiary, Style::kBorder));
    }
}

} // namespace Longpath
