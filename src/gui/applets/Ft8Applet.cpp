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
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/AudioTapRing.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "core/DxSpot.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QMouseEvent>
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
    // Doppelklick auf eine Zeile legt den Standardanruf ins
    // Eingabefeld (siehe antwortAuf). Am viewport, nicht am Widget:
    // die Mausereignisse einer Textflaeche kommen dort an.
    m_list->viewport()->installEventFilter(this);
    m_list->setToolTip(tr("Double-click a line to answer that station."));
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

    // Der Schalter fuer den Ablauf. Steht aus, und das ist Absicht:
    // eingeschaltet sendet das Programm ohne weiteren Druck, bis das
    // QSO fertig ist oder aufgegeben wird.
    m_autoBtn = styledButton(tr("Auto"), 48);
    m_autoBtn->setCheckable(true);
    m_autoBtn->setChecked(false);
    m_autoBtn->setToolTip(tr(
        "Run the QSO sequence by itself after a double-click on a "
        "decode.\nOff: a double-click only fills the message box."));
    connect(m_autoBtn, &QPushButton::toggled, this, [this](bool an) {
        if (!an) { m_ablauf.abbrechen(); zeigeAblauf(); }
    });
    sendRow->addWidget(m_autoBtn);
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
// ── Ein Fenster ist hinaus ───────────────────────────────────────────
//
// Ohne laufenden Ablauf ist danach Schluss, wie bisher. Mit Ablauf
// zaehlt das Fenster als „gesendet, keine Antwort" — und der naechste
// Schritt folgt, sobald ein Fund ihn weiterbringt oder die
// Wiederholung faellig ist.
void Ft8Applet::fensterZuEnde()
{
    abbrechen();
    if (!m_ablauf.laeuft()) { zeigeAblauf(); return; }
    m_ablauf.fensterVerstrichen();
    if (m_ablauf.laeuft()) {
        ablaufSchritt();
    } else {
        zeigeAblauf();
    }
}

// Den naechsten Zug des Ablaufs senden. Es ist derselbe Weg wie bei
// einem Druck auf „Send": Bandplan und Tasten inbegriffen.
void Ft8Applet::ablaufSchritt()
{
    const QString was = m_ablauf.naechsteNachricht();
    zeigeAblauf();
    if (was.isEmpty() || m_sendet) { return; }
    if (m_textFeld) { m_textFeld->setText(was); }
    sendeNachricht(was);
}

void Ft8Applet::zeigeAblauf()
{
    if (!m_status) { return; }
    switch (m_ablauf.zustand()) {
        case Ft8Ablauf::Zustand::Fertig:
            m_status->setText(tr("QSO with %1 complete").arg(m_ablauf.partner()));
            break;
        case Ft8Ablauf::Zustand::Abgebrochen:
            m_status->setText(m_ablauf.partner().isEmpty()
                ? tr("sequence stopped")
                : tr("gave up calling %1").arg(m_ablauf.partner()));
            break;
        default:
            break;      // die laufende Anzeige macht updateStatus()
    }
}

// ── Auf eine gehoerte Station antworten ──────────────────────────────
//
// Ein Doppelklick in die Fundliste legt den Standardanruf ins
// Eingabefeld: „<sein Ruf> <mein Ruf> <mein Locator>". Gesendet wird
// dadurch nichts — der Druck auf den Knopf bleibt beim Betreiber.
//
// Die Regel steht hier als statische Funktion, nicht im
// Mausereignis: so laesst sie sich ohne Fenster und ohne Maus
// pruefen, und der Doppelklick ist nur der Ausloeser.
QString Ft8Applet::antwortAuf(const QString& zeile,
                              const QString& meinRuf,
                              const QString& meinLocator)
{
    if (meinRuf.trimmed().isEmpty()) { return {}; }

    // Spalten wie in onDecoded(): HHMMSS dB DT Freq Nachricht. Die
    // Zahlen sind mit arg(..., breite) auf feste Breite gebracht, also
    // mit FUEHRENDEN Leerzeichen — simplified() macht daraus einfache
    // Trenner, danach ist die Nachricht alles ab dem fuenften Feld.
    const QString sauber = zeile.simplified();
    const QString nachricht = sauber.section(QLatin1Char(' '), 4);
    if (nachricht.isEmpty()) { return {}; }

    const QString seinRuf = senderCallsign(nachricht);
    if (seinRuf.isEmpty()) { return {}; }

    // Sich selbst anzurufen ergibt keinen Sinn — und waere der
    // haeufigste Fehlgriff, weil das eigene Rufzeichen in jeder
    // Antwort auf einen selbst vorkommt.
    if (seinRuf.compare(meinRuf.trimmed(), Qt::CaseInsensitive) == 0) {
        return {};
    }

    // Der Locator ist das vierstellige Feld (JN67VV -> JN67): mehr
    // passt nicht in eine FT8-Nachricht vom Standardtyp.
    const QString locator = meinLocator.trimmed().left(4).toUpper();
    if (locator.isEmpty()) {
        return QStringLiteral("%1 %2").arg(seinRuf, meinRuf.trimmed().toUpper());
    }
    return QStringLiteral("%1 %2 %3")
        .arg(seinRuf, meinRuf.trimmed().toUpper(), locator);
}

// `User/Callsign` ist der kanonische Schluessel — SpotHubDialog
// schreibt ihn („Writes canonical keys: User/Callsign,
// User/GridSquare"). `StationCallsign` wird dagegen von NIEMANDEM
// geschrieben und nur gelesen (SotaClient, MainWindow_KiwiSdr,
// QsoMapWindow); er steht hier als Rueckfall, damit eine von Hand
// gesetzte Fassung nicht ins Leere laeuft.
QString Ft8Applet::eigenesRufzeichen()
{
    auto& s = AppSettings::instance();
    QString ruf = s.value(QStringLiteral("User/Callsign")).toString().trimmed();
    if (ruf.isEmpty()) {
        ruf = s.value(QStringLiteral("StationCallsign")).toString().trimmed();
    }
    return ruf.toUpper();
}

QString Ft8Applet::eigenerLocator()
{
    auto& s = AppSettings::instance();
    QString loc = s.value(QStringLiteral("User/GridSquare")).toString().trimmed();
    if (loc.isEmpty()) {
        loc = s.value(QStringLiteral("StationGridSquare")).toString().trimmed();
    }
    return loc.toUpper();
}

bool Ft8Applet::eventFilter(QObject* obj, QEvent* ev)
{
    if (m_list && obj == m_list->viewport()
        && ev->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(ev);
        QTextCursor c = m_list->cursorForPosition(me->pos());
        c.select(QTextCursor::LineUnderCursor);
        const QString antwort = antwortAuf(c.selectedText(),
                                           eigenesRufzeichen(),
                                           eigenerLocator());
        if (antwort.isEmpty()) {
            if (m_status && eigenesRufzeichen().isEmpty()) {
                m_status->setText(tr("set your callsign first"));
            }
        } else if (m_textFeld) {
            m_textFeld->setText(antwort);
            m_textFeld->setFocus();

            // Mit Auto faengt hier der Ablauf an: dieselbe Antwort,
            // aber danach macht er von selbst weiter.
            if (m_autoBtn && m_autoBtn->isChecked() && !m_sendet) {
                const QString sauber = c.selectedText().simplified();
                const QString nachricht = sauber.section(QLatin1Char(' '), 4);
                const int snr = sauber.section(QLatin1Char(' '), 1, 1).toInt();
                m_ablauf.starteAlsAntworter(senderCallsign(nachricht),
                                            eigenesRufzeichen(),
                                            eigenerLocator(), snr);
                ablaufSchritt();
            }
        }
        return true;
    }
    return AppletWidget::eventFilter(obj, ev);
}

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

        // ── Wenn der Bandplan nein sagt, nicht weitermachen ──────────
        //
        // MoxController::setMox(true) darf abgelehnt werden: falsche
        // Betriebsart, Frequenz ausserhalb des Bandes, Cross-Band-TX.
        // Die Ablehnung kommt als moxRejected(reason) und sonst gar
        // nicht — setMox() gibt nichts zurueck.
        //
        // Ohne diese Verbindung lief der Ton danach volle 12,6 s
        // weiter, der Sendeweg blieb auf FT8 (also das Mikrofon
        // abgeklemmt), und in der Zeile stand „transmitting", waehrend
        // nichts hinausging. Jetzt bricht der Versuch sofort ab und
        // sagt, woran es lag.
        bool abgelehnt = false;
        if (auto* mox = m_model->moxController()) {
            QMetaObject::Connection c = connect(
                mox, &MoxController::moxRejected, this,
                [this, &abgelehnt](const QString& grund) {
                    abgelehnt = true;
                    if (m_status) {
                        m_status->setText(tr("not transmitting: %1").arg(grund));
                    }
                });
            mox->setMox(true);
            disconnect(c);
        }
        if (abgelehnt) {
            // abbrechen() setzt den Sendeweg zurueck und macht die
            // Knoepfe wieder frei; die Zeile oben bleibt stehen, weil
            // updateStatus() sie erst beim naechsten Takt ersetzt.
            const QString grund = m_status ? m_status->text() : QString();
            abbrechen();
            if (m_status && !grund.isEmpty()) { m_status->setText(grund); }
            return;
        }
        if (m_status) { m_status->setText(tr("transmitting")); }

        // Das Fenster laeuft; danach alles zurueck. Ein Zuschlag von
        // 300 ms, damit der letzte Ton draussen ist, bevor der Sender
        // abschaltet.
        QTimer::singleShot(Ft8Tx::fensterMs(protocol()) + 300, this,
                           [this]() { fensterZuEnde(); });
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

        // Der Fund geht an EINE Stelle: RadioModel verteilt ihn weiter
        // (Panadapter-Ueberlagerung ueber SpotModel, der Sammler
        // RxDecodeModel, und PSK Reporter, wenn er laeuft) — genau wie
        // beim WSJT-X-Weg. Vorher schrieb dieses Applet selbst ins
        // RxDecodeModel und erreichte die Ueberlagerung nie
        // (2026-10-15).
        // Bringt der Fund den Ablauf weiter, ist der naechste Zug
        // faellig — das ist der einzige Weg, auf dem er vorankommt.
        if (m_ablauf.laeuft()
            && m_ablauf.verarbeite(d.text,
                                   static_cast<int>(std::lround(d.snrDb)))) {
            ablaufSchritt();
        }

        const QString ruf = senderCallsign(d.text);
        if (m_model && !ruf.isEmpty()) {
            DxSpot spot;
            spot.dxCall   = ruf;
            spot.freqMhz  = (wahlHz + (unteresSeitenband ? -d.freqHz
                                                          : d.freqHz))
                            / 1.0e6;
            spot.snr      = static_cast<int>(std::lround(d.snrDb));
            // Der Modus steht im Kommentar: SpotTableModel::extractMode
            // liest ihn dort heraus, so wie bei den WSJT-X-Funden.
            spot.comment  = QStringLiteral("%1  %2").arg(modus, d.text);
            // Nicht „WSJT-X": das ist eine andere Quelle und wuerde die
            // beiden in der Liste vermischen.
            spot.source   = QStringLiteral("Longpath");
            spot.spotterCall = QStringLiteral("Longpath");
            spot.utcTime  = periodStart.toUTC().time();
            m_model->onNativeFt8Decode(spot);
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
