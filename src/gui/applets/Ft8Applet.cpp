// =================================================================
// src/gui/applets/Ft8Applet.cpp  (Longpath)
// =================================================================
// Longpath-original. Siehe Ft8Applet.h.
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "Ft8Applet.h"

#include "core/AudioEngine.h"
#include "core/Ft8Stream.h"
#include "core/Ft8Tx.h"
#include "core/MoxController.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/AudioTapRing.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <vector>

namespace Longpath {

namespace {

// Der Ring muss ein FT8-Fenster ueberbruecken koennen, wenn der
// Taktgeber einmal spaet kommt: 48 kHz, Stereo, eine Sekunde. Das ist
// das Doppelte des CW-Rings, weil hier in 15-Sekunden-Fenstern
// gerechnet wird und ein verlorenes Stueck nicht ein Zeichen kostet,
// sondern das ganze Fenster.
constexpr int kTapRingFloats = 96000;

// Alle 200 ms leeren. Der Ring haelt eine Sekunde; fuenf Mal
// Sicherheit ist genug und kostet nichts.
constexpr int kPumpIntervalMs = 200;

// So viele Zeilen bleiben stehen. Ein FT8-Fenster bringt auf 20 m
// leicht zwanzig Funde — 400 Zeilen sind also etwa die letzten zehn
// Minuten.
constexpr int kMaxLines = 400;

// Die Tonlage, auf der gesendet wird. 1500 Hz ist die Mitte des
// Durchlassbereichs und das, was WSJT-X ab Werk nimmt.
constexpr float kSendeTonHz = 1500.0f;

} // namespace

Ft8Applet::Ft8Applet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    m_stream = new Ft8Stream(this);
    connect(m_stream, &Ft8Stream::decoded, this, &Ft8Applet::onDecoded);

    if (m_model) {
        // Dieselbe Sicherung wie beim CW-Dekoder: eine fallengelassene
        // Scheibe darf keinen alten sliceIndex im Abgriff hinterlassen,
        // sonst laeuft der naechste Empfaenger, der diesen Index
        // bekommt, hier herein.
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
        if (!m_tapRing || !m_stream) { return; }
        static thread_local std::vector<float> scratch;
        const int want = kTapRingFloats / 2;   // eine halbe Sekunde Stereo
        if (static_cast<int>(scratch.size()) < want) { scratch.resize(want); }
        const int got = m_tapRing->read(scratch.data(), want);
        if (got >= 2) {
            m_stream->feedAudio(scratch.data(), got / 2);
        } else {
            // Kein Ton: die Fenstergrenze trotzdem pruefen, sonst
            // friert die Anzeige auf dem vorletzten Stand ein und
            // sieht aus wie ein abgestuerzter Dekoder.
            m_stream->poll();
        }
        updateStatus();
    });
    m_pumpTimer->start();

    updateStatus();
}

Ft8Applet::~Ft8Applet()
{
    // Reihenfolge wie beim CW-Dekoder: erst der Taktgeber, dann der
    // Abgriff. Ft8Stream haengt als Kind an diesem Widget.
    if (m_pumpTimer) { m_pumpTimer->stop(); }
    if (m_model && m_model->audioEngine()) {
        m_model->audioEngine()->setFt8Tap(nullptr, -1);
    }
}

void Ft8Applet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->addWidget(appletTitleBar(QStringLiteral("FT8 DECODER")));

    // ── Kopfzeile: Verfahren + Zustand ──────────────────────────────
    auto* head = new QHBoxLayout();

    m_ft8Btn = blueToggle(QStringLiteral("FT8"), 48);
    m_ft8Btn->setCheckable(true);
    m_ft8Btn->setChecked(true);
    m_ft8Btn->setToolTip(tr("15 second periods (FT8)"));
    connect(m_ft8Btn, &QPushButton::clicked, this, [this]() {
        setProtocol(Ft8Decoder::Protocol::Ft8);
    });
    head->addWidget(m_ft8Btn);

    m_ft4Btn = blueToggle(QStringLiteral("FT4"), 48);
    m_ft4Btn->setCheckable(true);
    m_ft4Btn->setToolTip(tr("7.5 second periods (FT4)"));
    connect(m_ft4Btn, &QPushButton::clicked, this, [this]() {
        setProtocol(Ft8Decoder::Protocol::Ft4);
    });
    head->addWidget(m_ft4Btn);

    head->addSpacing(8);

    m_status = new QLabel(QStringLiteral("—"), this);
    m_status->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    head->addWidget(m_status);
    head->addStretch(1);
    root->addLayout(head);

    // Der Fuellstand des laufenden Fensters. Ohne ihn sieht ein
    // Dekoder, der gerade sammelt, genauso aus wie einer, der nichts
    // bekommt — und bei fuenfzehn Sekunden Fenster ist das die
    // haeufigste Fehldeutung.
    m_fill = new QProgressBar(this);
    m_fill->setRange(0, 100);
    m_fill->setValue(0);
    m_fill->setTextVisible(false);
    m_fill->setFixedHeight(4);
    m_fill->setStyleSheet(QStringLiteral(
        "QProgressBar { background: %1; border: none; }"
        "QProgressBar::chunk { background: %2; }")
        .arg(Style::kInsetBg, Style::kAccent));
    root->addWidget(m_fill);

    root->addWidget(divider());

    // ── Die Liste ───────────────────────────────────────────────────
    m_list = new QPlainTextEdit(this);
    m_list->setReadOnly(true);
    m_list->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_list->setMinimumHeight(120);
    m_list->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_list->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    root->addWidget(m_list, 1);

    // ── Fusszeile ───────────────────────────────────────────────────
    // ── Senden ──────────────────────────────────────────────────────
    //
    // Eine Zeile: was gesendet wird, und zwei Knoepfe. Kein
    // Folgeschalter, keine Automatik — siehe Ft8Applet.h.
    auto* sendRow = new QHBoxLayout();
    m_textFeld = new QLineEdit(this);
    m_textFeld->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_textFeld->setPlaceholderText(tr("CQ <call> <grid>"));
    m_textFeld->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " padding: 2px 4px; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    sendRow->addWidget(m_textFeld, 1);

    m_cqBtn = styledButton(tr("Send"), 56);
    m_cqBtn->setToolTip(tr("Send this message once, in the next own "
                           "period. No auto-sequencing."));
    connect(m_cqBtn, &QPushButton::clicked, this, [this]() {
        sendeNachricht(m_textFeld ? m_textFeld->text() : QString());
    });
    sendRow->addWidget(m_cqBtn);

    m_stopBtn = styledButton(tr("Stop"), 48);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() { abbrechen(); });
    sendRow->addWidget(m_stopBtn);
    root->addLayout(sendRow);

    auto* foot = new QHBoxLayout();
    m_lastLabel = new QLabel(tr("last period: —"), this);
    m_lastLabel->setFont(Style::monoFont(font(), Style::kFontCaption));
    m_lastLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextTertiary));
    foot->addWidget(m_lastLabel);
    foot->addStretch(1);
    m_clearBtn = styledButton(tr("Clear"), 60);
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_list->clear();
        m_lineCount = 0;
    });
    foot->addWidget(m_clearBtn);
    root->addLayout(foot);
}

void Ft8Applet::setProtocol(Ft8Decoder::Protocol p)
{
    if (m_stream) { m_stream->setProtocol(p); }
    const bool ft4 = (p == Ft8Decoder::Protocol::Ft4);
    if (m_ft8Btn) { m_ft8Btn->setChecked(!ft4); }
    if (m_ft4Btn) { m_ft4Btn->setChecked(ft4); }
    m_lastPeriodCount = -1;
    updateStatus();
}

Ft8Decoder::Protocol Ft8Applet::protocol() const
{
    return m_stream ? m_stream->protocol() : Ft8Decoder::Protocol::Ft8;
}

void Ft8Applet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    m_slice = slice;
    updateAudioTap();
    updateStatus();
}

void Ft8Applet::updateAudioTap()
{
    if (!m_model || !m_model->audioEngine() || !m_tapRing) { return; }
    if (m_slice) {
        m_model->audioEngine()->setFt8Tap(m_tapRing.get(), m_slice->sliceIndex());
    } else {
        m_model->audioEngine()->setFt8Tap(nullptr, -1);
    }
}

void Ft8Applet::syncFromModel()
{
    setSlice(m_model ? m_model->activeSlice() : nullptr);
}

// ── Senden ──────────────────────────────────────────────────────────
//
// Der ganze Ablauf an einer Stelle, damit man ihn lesen kann, ohne zu
// springen: vorbereiten, warten bis das eigene Fenster beginnt,
// Mikrofonweg umlegen, tasten, Fenster laufen lassen, alles
// zuruecknehmen.
//
// Was absichtlich fehlt: jede Automatik. Ein Druck = EIN Fenster.
void Ft8Applet::sendeNachricht(const QString& text)
{
    if (m_sendet) { return; }
    const QString sauber = text.trimmed().toUpper();
    if (sauber.isEmpty()) {
        if (m_status) { m_status->setText(tr("nothing to send")); }
        return;
    }
    if (!m_model || !m_model->ft8TxSource() || !m_model->micRouter()) {
        if (m_status) { m_status->setText(tr("no transmit path")); }
        return;
    }

    Ft8TxSource* quelle = m_model->ft8TxSource();
    if (!quelle->vorbereiten(sauber, kSendeTonHz, protocol())) {
        if (m_status) {
            m_status->setText(tr("cannot encode “%1”").arg(sauber));
        }
        return;
    }

    // Wann faengt mein naechstes Fenster an? Die Gegenperiode zu der,
    // in der gehoert wird — wer in derselben sendet, hoert sich selbst
    // und niemand sonst.
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    const qint64 wartet = Ft8Tx::bisZumNaechsten(jetzt, protocol(),
                                                 m_periode);
    m_sendet = true;
    if (m_cqBtn)   { m_cqBtn->setEnabled(false); }
    if (m_stopBtn) { m_stopBtn->setEnabled(true); }
    if (m_status) {
        m_status->setText(tr("sending in %1 s")
                              .arg(wartet / 1000.0, 0, 'f', 1));
    }

    if (!m_sendeTimer) {
        m_sendeTimer = new QTimer(this);
        m_sendeTimer->setSingleShot(true);
    }
    m_sendeTimer->disconnect();
    connect(m_sendeTimer, &QTimer::timeout, this, [this]() {
        if (!m_sendet || !m_model) { return; }
        Ft8TxSource* q = m_model->ft8TxSource();
        auto* router = m_model->micRouter();
        if (!q || !router) { abbrechen(); return; }

        // Erst den Weg legen, dann tasten: andersherum ginge der
        // Sender mit dem Mikrofon auf, nicht mit dem Ton.
        m_quelleVorher = static_cast<int>(router->activeSource());
        router->setActiveSource(MicSource::Ft8);
        q->starten();
        if (m_model->moxController()) {
            m_model->moxController()->setMox(true);
        }
        if (m_status) { m_status->setText(tr("transmitting")); }

        // Das Fenster laeuft; danach alles zurueck. Ein Zuschlag von
        // 300 ms, damit der letzte Ton draussen ist, bevor der Sender
        // abschaltet.
        QTimer::singleShot(Ft8Tx::fensterMs(protocol()) + 300, this,
                           [this]() { abbrechen(); });
    });
    m_sendeTimer->start(static_cast<int>(wartet));
}

void Ft8Applet::abbrechen()
{
    if (m_sendeTimer) { m_sendeTimer->stop(); }

    // Erst den Sender aus, dann den Weg zurueck — in dieser
    // Reihenfolge ist zu keinem Zeitpunkt das Mikrofon auf Sendung.
    if (m_model && m_model->moxController()) {
        m_model->moxController()->setMox(false);
    }
    if (m_model && m_model->ft8TxSource()) {
        m_model->ft8TxSource()->abbrechen();
    }
    if (m_model && m_model->micRouter() && m_quelleVorher >= 0) {
        m_model->micRouter()->setActiveSource(
            static_cast<MicSource>(m_quelleVorher));
    }
    m_quelleVorher = -1;
    m_sendet = false;
    if (m_cqBtn)   { m_cqBtn->setEnabled(true); }
    if (m_stopBtn) { m_stopBtn->setEnabled(false); }
    updateStatus();
}

void Ft8Applet::updateStatus()
{
    if (!m_status || !m_stream) { return; }

    const int imFenster = m_stream->bufferedSamples();
    const double sekunden = static_cast<double>(imFenster)
                            / Ft8Decoder::kSampleRate;
    const double voll = m_stream->slotMs() / 1000.0;
    if (m_fill) {
        m_fill->setValue(voll > 0.0
            ? qBound(0, static_cast<int>(100.0 * sekunden / voll), 100)
            : 0);
    }

    // Kurz halten: die Applet-Spalte ist schmal, und eine abgeschnittene
    // Zeile ist schlimmer als eine knappe (im ersten Bild vom 15.10.
    // stand „letztes Fenster: 1" halb ausserhalb).
    m_status->setText(QStringLiteral("%1 / %2 s")
                          .arg(sekunden, 0, 'f', 1)
                          .arg(voll, 0, 'f', 1));
    if (m_lastLabel) {
        m_lastLabel->setText(m_lastPeriodCount < 0
            ? tr("last period: —")
            : tr("last period: %1").arg(m_lastPeriodCount));
    }
}

// Der Absender einer FT8-Nachricht.
//
// Zwei Formen reichen fuer alles, was im Durchlassbereich steht:
//   „CQ OE5SOS JN78"       — Anruf, der Absender steht hinter CQ
//   „CQ DX OE5SOS JN78"    — mit Zusatz; dann eins weiter
//   „DL1ABC OE5SOS -12"    — Antwort: erst der Angerufene, dann der
//                             Absender
// In beiden Faellen ist der Absender das ERSTE Feld, das wie ein
// Rufzeichen aussieht und nicht an erster Stelle steht — ausser bei
// CQ, wo das erste Feld der Anruf selbst ist.
QString Ft8Applet::senderCallsign(const QString& message)
{
    const QStringList felder =
        message.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (felder.isEmpty()) { return {}; }

    // Wie ein Rufzeichen: drei bis elf Zeichen, mindestens eine
    // Ziffer UND ein Buchstabe, und die letzte Stelle KEINE Ziffer.
    // Die letzte Bedingung ist die wichtige: ohne sie gilt auch der
    // Feldkenner (JN78, FN20) als Rufzeichen, und bei „CQ DX OE5SOS
    // JN78" zaehlt nur, welches Feld zuerst passt. „DX", „-12",
    // „RR73" und „73" fallen ohnehin heraus.
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

    // Erst ab Feld 1: an Stelle 0 steht der Anruf („CQ") oder der
    // ANGERUFENE, nie der Absender.
    for (int i = 1; i < felder.size(); ++i) {
        if (wieRufzeichen(felder.at(i))) { return felder.at(i); }
    }
    return {};
}

void Ft8Applet::onDecoded(const QVector<Longpath::Ft8Decode>& funde,
                          const QDateTime& periodStart)
{
    m_lastPeriodCount = funde.size();
    updateStatus();
    if (funde.isEmpty()) { return; }

    const QString uhrzeit = periodStart.toUTC().toString(QStringLiteral("HHmmss"));
    const QString modus = protocol() == Ft8Decoder::Protocol::Ft4
        ? QStringLiteral("FT4") : QStringLiteral("FT8");
    // Die Wahlfrequenz der Scheibe liegt in Hz; der Fund steht als
    // Tonlage IM Durchlassbereich. Erst zusammen ergeben sie die
    // Frequenz, unter der das Rufzeichen im Logbuch steht.
    //
    // Und das Vorzeichen haengt am Seitenband: in DIGU (so wird FT8
    // ueblicherweise gehoert) liegt der Ton OBERHALB der Wahlfrequenz,
    // in DIGL darunter. Ohne diese Unterscheidung stuende ein
    // DIGL-Fund um bis zu 3 kHz daneben.
    const double wahlHz = m_slice ? m_slice->frequency() : 0.0;
    const bool unteresSeitenband =
        m_slice && m_slice->dspMode() == DSPMode::DIGL;

    for (const Ft8Decode& d : funde) {
        // Spalten wie in WSJT-X „Band Activity".
        const QString zeile = QStringLiteral("%1 %2 %3 %4  %5")
            .arg(uhrzeit)
            .arg(static_cast<int>(std::lround(d.snrDb)), 4)
            .arg(d.dtSeconds, 5, 'f', 1)
            .arg(static_cast<int>(std::lround(d.freqHz)), 5)
            .arg(d.text);
        m_list->appendPlainText(zeile);
        ++m_lineCount;

        if (m_model && m_model->rxDecodeModel()) {
            RxDecode eintrag;
            eintrag.callsign = senderCallsign(d.text);
            eintrag.freqMhz  = (wahlHz + (unteresSeitenband ? -d.freqHz
                                                             : d.freqHz))
                               / 1.0e6;
            eintrag.snr      = static_cast<int>(std::lround(d.snrDb));
            eintrag.mode     = modus;
            // Nicht „WSJT-X": das ist eine andere Quelle und wuerde
            // die beiden in der Liste vermischen.
            eintrag.source   = QStringLiteral("Longpath");
            eintrag.utcTime  = periodStart.toUTC();
            eintrag.payload  = d.text;
            m_model->rxDecodeModel()->addDecode(eintrag);
        }
    }

    if (m_lineCount > kMaxLines) {
        const int zuviel = m_lineCount - kMaxLines;
        auto cursor = m_list->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, zuviel);
        cursor.removeSelectedText();
        m_lineCount = kMaxLines;
    }

    m_list->moveCursor(QTextCursor::End);
    m_list->ensureCursorVisible();
}

} // namespace Longpath
