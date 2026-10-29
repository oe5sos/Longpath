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
#include "core/TransverterSetup.h"

#include "core/AudioEngine.h"
#include "core/Ft8Stream.h"
#include "core/Ft8Tx.h"
#include "models/Band.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDateTime>
#include "core/AppSettings.h"
#include <QLoggingCategory>
#include "core/DigiFrequenzen.h"
#include "core/DxccColorProvider.h"
#include "core/DxccWorkedStatus.h"
#include "gui/widgets/Ft8Wasserfall.h"
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
#include <QSplitter>
#include <QSpinBox>
#include <QColor>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <cstdlib>
#include <vector>

// Dieselbe Kategorie wie der Dekoderstrom („longpath.ft8"), eigener
// Zeiger: Ft8Stream.cpp meldet sie nicht ueber einen Kopf an, und
// zwei Objekte mit demselben Namen sind in Qt zulaessig.
//
// Warum es sie hier ueberhaupt braucht: der Sendeweg schrieb bis zum
// 2026-10-28 KEINE einzige Zeile. Er setzte nur eine Statuszeile, und
// die wird vom Takt („last period: N") binnen 200 ms ueberschrieben.
// Der Betreiber sah einen Send-Knopf, der nichts tut, und im
// Protokoll stand nichts — eine Stunde Suche fuer einen Fall, den
// eine Zeile beantwortet haette.
Q_LOGGING_CATEGORY(lcFt8Senden, "longpath.ft8", QtInfoMsg)

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

// Die Tonlage, auf der gesendet wird: Vorgabe und Grenzen. 1500 Hz ist
// die Mitte des Durchlassbereichs und das, was WSJT-X ab Werk nimmt --
// seit 2026-10-18 nur noch die VORGABE, nicht mehr die einzige Wahl
// (siehe m_sendeTonHz in Ft8Applet.h). Die Grenzen sind der nutzbare
// Durchlassbereich eines gewoehnlichen SSB-Filters; darueber hinaus
// kaeme der Ton gar nicht erst heraus.
constexpr int kSendeTonVorgabeHz = 1500;
constexpr int kSendeTonMinHz = 200;
constexpr int kSendeTonMaxHz = 2800;
constexpr auto kSendeTonSchluessel = "Ft8/SendeTonHz";

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
            // Derselbe Ton in den Streifen -- ein Abgriff, zwei
            // Verbraucher. Kein zweiter Ring, nichts zu halten.
            if (m_wasserfall) {
                m_wasserfall->speiseStereo(scratch.data(), got / 2, 48000);
            }
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

    // ── Das ⚙ rechts oben in der Kopfzeile ───────────────────────────
    //
    // Betreiber 2026-10-21: „sollt in der taksleiste im decoder eine
    // optionsmöglichkeit geben". Vorausgegangen war die Frage, WO man
    // Rufzeichen und Locator eintraegt -- die Antwort war bis dahin
    // „irgendwo im Setup, oder im Spot Hub", und das ist keine.
    //
    // Die Stelle ist seine Regel vom 2026-09-20: Optionen immer rechts
    // oben im Panelkopf.
    QWidget* leiste = appletTitleBar(QStringLiteral("FT8 DECODER"));
    if (auto* kopf = qobject_cast<QHBoxLayout*>(leiste->layout())) {
        // Der Knopf traegt "...", nicht ⚙ (2026-10-21).
        //
        // Hergeleitet, nicht geraten: eine Probe mit magenta Hintergrund
        // zeigte, dass der Knopf GEZEICHNET wird -- 176 Bildpunkte an
        // der richtigen Stelle -- nur sein Zeichen nicht. Weder ⚙
        // (U+2699) noch ⋯ (U+22EF) kommen an; die Schrift dieser Taste
        // kennt sie nicht, obwohl das ⋮⋮ des Griffs daneben (ein QLabel,
        // keine Taste) anstandslos erscheint.
        //
        // "..." ist ausserdem die Form, die in der Kommandoleiste schon
        // fuer "mehr davon" steht (Band, Mode, Filter). Die Stelle --
        // rechts oben im Panelkopf -- ist die Regel des Betreibers, das
        // Zeichen war es nie.
        auto* zahnrad = new QPushButton(QStringLiteral("..."), leiste);
        // 16x14 wie der geteilte Optionsknopf in GridCellWidget: die
        // Kopfzeile ist kTitleBarH = 16 Punkte hoch, ein 18 Punkte
        // hoher Knopf passt NICHT hinein und wurde am 2026-10-21
        // schlicht nicht gezeichnet.
        zahnrad->setFixedSize(26, 15);
        zahnrad->setCursor(Qt::PointingHandCursor);
        zahnrad->setToolTip(tr("Callsign and grid square"));
        zahnrad->setStyleSheet(QStringLiteral(
            // Ruhend in der Schrift der Kopfzeile, nicht in der des
            // Griffs: mit kTextScale (#7e7e85) auf diesem Grund war der
            // Knopf im Entwurfsblatt schlicht NICHT zu sehen -- er war
            // da, an der richtigen Stelle, sichtbar laut Qt, und doch
            // unsichtbar. Beim Zeigen das Blau des Hauses: anfassbar.
            "QPushButton { color: %1; background: transparent;"
            " border: none; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { color: %2; }")
            .arg(Style::kTitleText, Style::kAccent));
        connect(zahnrad, &QPushButton::clicked, this,
                &Ft8Applet::zeigeOptionen);
        kopf->addWidget(zahnrad);
    }
    root->addWidget(leiste);

    // ── Kopfzeile: Verfahren + Zustand ──────────────────────────────
    auto* head = new QHBoxLayout();

    m_ft8Btn = blueToggle(QStringLiteral("FT8"), 48);
    m_ft8Btn->setCheckable(true);
    m_ft8Btn->setChecked(true);
    m_ft8Btn->setToolTip(tr("15 second periods (FT8)"));
    connect(m_ft8Btn, &QPushButton::clicked, this, [this]() {
        emit digiGewuenscht(DigiFrequenzen::Digimodus::Ft8);
        setProtocol(Ft8Decoder::Protocol::Ft8);
    });
    head->addWidget(m_ft8Btn);

    m_ft4Btn = blueToggle(QStringLiteral("FT4"), 48);
    m_ft4Btn->setCheckable(true);
    m_ft4Btn->setToolTip(tr("7.5 second periods (FT4)"));
    connect(m_ft4Btn, &QPushButton::clicked, this, [this]() {
        emit digiGewuenscht(DigiFrequenzen::Digimodus::Ft4);
        setProtocol(Ft8Decoder::Protocol::Ft4);
    });
    head->addWidget(m_ft4Btn);

    head->addSpacing(8);

    // ── „CQ only" ───────────────────────────────────────────────────
    //
    // Betreiber 2026-10-18, nach dem ersten Blick in das Fenster:
    // „dachte das sind dann mehr optionen usw. wie ein richtiges ft8
    // programm". In WSJT-X ist das der haeufigste Griff ueberhaupt --
    // man sucht jemanden, den man rufen KANN, und das sind die
    // CQ-Zeilen.
    //
    // Nur die Ansicht wird gefiltert. Der Ablauf bekommt weiter jeden
    // Fund (siehe m_nurCq in Ft8Applet.h): sonst verschwaende ein
    // laufendes QSO, sobald der Haken sitzt.
    m_nurCqBtn = blueToggle(tr("CQ only"), 64);
    m_nurCqBtn->setCheckable(true);
    m_nurCqBtn->setToolTip(tr("Show only CQ calls. The QSO still runs "
                              "on every decode."));
    connect(m_nurCqBtn, &QPushButton::clicked, this, [this](bool an) {
        m_nurCq = an;
    });
    head->addWidget(m_nurCqBtn);

    // ── „New DXCC only" (2026-10-27) ────────────────────────────────
    //
    // Das letzte Stueck aus dem Vergleich vom 2026-10-21, das JTDX hat
    // und wir nicht. Der Auflaeser dafuer liegt laengst im Modell --
    // er faerbt die neuen Laender in diesem Fenster schon ein. Hier
    // wird er nur zum zweiten Mal gefragt, statt eine eigene Liste zu
    // fuehren.
    //
    // Der Landesname kommt aus cty.dat und steht ohnehin schon in der
    // Zeile. Ob ein Land NEU ist, weiss aber erst der Abgleich mit dem
    // Logbuch (DxccWorkedStatus); fehlt es, meldet der Aufloeser gar
    // nichts und der Filter zeigt nur noch Rufe an die eigene Station.
    // Das sieht aus wie ein Fehler und ist keiner -- darum steht es im
    // Hinweis am Knopf. Am 2026-10-27 im Prueflauf genau so gesehen.
    m_nurNeueLaenderBtn = blueToggle(tr("New DXCC"), 78);
    m_nurNeueLaenderBtn->setCheckable(true);
    m_nurNeueLaenderBtn->setToolTip(
        tr("Show only countries not yet in the log. Needs an ADIF log — "
           "without one nothing counts as new and only calls to your own "
           "station remain. Those always get through."));
    connect(m_nurNeueLaenderBtn, &QPushButton::clicked, this, [this](bool an) {
        m_nurNeueLaender = an;
    });
    head->addWidget(m_nurNeueLaenderBtn);

    head->addSpacing(8);

    // ── Die Sendestelle ─────────────────────────────────────────────
    auto* tonBez = new QLabel(tr("TX Hz"), this);
    tonBez->setFont(Style::monoFont(font(), Style::kFontSmall));
    tonBez->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    head->addWidget(tonBez);

    m_sendeTonHz = AppSettings::instance()
                       .value(QLatin1String(kSendeTonSchluessel),
                              kSendeTonVorgabeHz)
                       .toInt();
    if (m_sendeTonHz < kSendeTonMinHz || m_sendeTonHz > kSendeTonMaxHz) {
        m_sendeTonHz = kSendeTonVorgabeHz;
    }
    m_tonFeld = new QSpinBox(this);
    m_tonFeld->setRange(kSendeTonMinHz, kSendeTonMaxHz);
    m_tonFeld->setSingleStep(10);
    m_tonFeld->setValue(m_sendeTonHz);
    m_tonFeld->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_tonFeld->setToolTip(tr(
        "Where in the passband you transmit. Pick a clear spot — "
        "every call of yours goes out here."));
    connect(m_tonFeld, &QSpinBox::valueChanged, this, [this](int hz) {
        m_sendeTonHz = hz;
        AppSettings::instance().setValue(QLatin1String(kSendeTonSchluessel), hz);
        if (m_wasserfall) { m_wasserfall->setzeSendeTon(hz); }
    });
    head->addWidget(m_tonFeld);

    head->addSpacing(8);

    // ── Wie viele Runden der Dekoder ueber ein Fenster geht ──────────
    //
    // WSJT-X hat dasselbe als „Decode: Fast / Normal / Deep", JTDX
    // ebenso. Zwei ist die Vorgabe: nach der ersten Runde werden die
    // gefundenen Signale abgezogen, damit schwaechere darunter
    // auffindbar werden (core/Ft8Subtract.h, portiert aus MSHV).
    //
    // Der Knopf sitzt hier und nicht in einer Einstellseite, weil er
    // die Gegenprobe AN DER ANTENNE moeglich macht: was die zweite
    // Runde am Pruefstand leistet, ist belegt -- was sie auf dem Band
    // leistet, sieht man nur, wenn man dieselbe Minute einmal so und
    // einmal so hoeren kann, ohne das Programm neu zu starten.
    auto* rundenBez = new QLabel(tr("Passes"), this);
    rundenBez->setFont(Style::monoFont(font(), Style::kFontSmall));
    rundenBez->setStyleSheet(QStringLiteral("color: %1;")
                                 .arg(Style::kTextSecondary));
    head->addWidget(rundenBez);

    m_rundenFeld = new QSpinBox(this);
    m_rundenFeld->setRange(1, 3);
    m_rundenFeld->setValue(AppSettings::instance()
                               .value(QStringLiteral("Ft8/Durchgaenge"),
                                      QStringLiteral("2"))
                               .toInt());
    m_rundenFeld->setFixedWidth(46);
    m_rundenFeld->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_rundenFeld->setToolTip(tr(
        "How often the decoder goes over the same period. After each "
        "pass the signals it found are subtracted, so weaker ones "
        "underneath become readable.\n1 is one pass, no subtraction. "
        "More passes cost a little more time per period."));
    connect(m_rundenFeld, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [](int n) {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Durchgaenge"),
                                         QString::number(n));
    });
    head->addWidget(m_rundenFeld);

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
    // ── Der Streifen ───────────────────────────────────────────────
    //
    // Zwischen Kopfzeile und Fuellstand: erst sehen, wo frei ist, dann
    // die Stelle waehlen. Ein Klick darin setzt das Feld „TX Hz" und
    // umgekehrt -- eine Wahrheit, zwei Wege sie zu fassen.
    m_wasserfall = new Ft8Wasserfall(this);
    m_wasserfall->setzeSendeTon(m_sendeTonHz);
    connect(m_wasserfall, &Ft8Wasserfall::sendeTonGewaehlt, this,
            [this](int hz) {
                if (m_tonFeld) { m_tonFeld->setValue(hz); }
            });
    root->addWidget(m_wasserfall);

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

    // Zweite Spalte: „Rx Frequency" wie in WSJT-X. Links alles, rechts
    // nur, was auf der eigenen Stelle liegt oder an mich geht. In
    // einem vollen Band ist die linke Liste nach zwei Durchgaengen
    // voll -- die eine Zeile, auf die es ankommt, scrollt weg.
    m_listRx = new QPlainTextEdit(this);
    m_listRx->setReadOnly(true);
    m_listRx->viewport()->installEventFilter(this);
    m_listRx->setToolTip(tr("Decodes on your own frequency, and anything "
                            "addressed to you. Double-click to answer."));
    m_listRx->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_listRx->setMinimumHeight(120);
    m_listRx->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_listRx->setStyleSheet(m_list->styleSheet());

    // Die Ueberschriften stehen ueber den Listen, nicht in ihnen: eine
    // Kopfzeile IN einer Textflaeche scrollt mit weg.
    const auto spalte = [this](QPlainTextEdit* liste, const QString& titel) {
        auto* huelle = new QWidget(this);
        auto* v = new QVBoxLayout(huelle);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        auto* kopf = new QLabel(titel, huelle);
        kopf->setFont(Style::monoFont(font(), Style::kFontMicro));
        kopf->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                .arg(Style::kTextInactive));
        v->addWidget(kopf);
        v->addWidget(liste, 1);
        return huelle;
    };

    m_listenTeiler = new QSplitter(Qt::Horizontal, this);
    m_listenTeiler->addWidget(spalte(m_list,   tr("Band Activity")));
    m_listenTeiler->addWidget(spalte(m_listRx, tr("Rx Frequency")));
    m_listenTeiler->setStretchFactor(0, 3);
    m_listenTeiler->setStretchFactor(1, 2);
    m_listenTeiler->setChildrenCollapsible(false);
    root->addWidget(m_listenTeiler, 1);

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

    // ── CQ rufen ────────────────────────────────────────────────────
    //
    // Ft8Ablauf kann seit dem QSO-Ablauf auch die ANRUFER-Rolle
    // (starteAlsAnrufer, dreizehn Prueffaelle) — nur rief sie
    // niemand auf. Antworten ging per Doppelklick, selbst CQ rufen
    // gar nicht. Dieser Knopf schliesst das, mit demselben Verhalten
    // wie der Doppelklick: er faengt den Ablauf an und laesst ihn
    // bis 73 laufen.
    // ── Tx1..Tx6 ────────────────────────────────────────────────────
    //
    // Eine Reihe ueber dem Textfeld. Jeder Knopf LEGT den Satz nur ins
    // Feld -- gesendet wird weiterhin erst mit „Send".
    auto* txReihe = new QHBoxLayout();
    txReihe->setSpacing(3);
    for (int n = 1; n <= 6; ++n) {
        QPushButton* b = styledButton(QStringLiteral("Tx%1").arg(n), 34);
        b->setToolTip(tr("Put standard message Tx%1 in the box.").arg(n));
        connect(b, &QPushButton::clicked, this, [this, n]() {
            const QString gegen = aktuellerPartner();
            const QString was = txKnopfNachricht(
                n, gegen, eigenesRufzeichen(), eigenerLocator(),
                m_letzterRapport.value(gegen, 0));
            if (was.isEmpty()) {
                if (m_status) {
                    m_status->setText(n == 6 ? tr("set your callsign first")
                                             : tr("no station selected"));
                }
                return;
            }
            if (m_textFeld) { m_textFeld->setText(was); }
        });
        txReihe->addWidget(b);
    }
    txReihe->addStretch(1);
    root->addLayout(txReihe);

    m_cqRufenBtn = styledButton(tr("CQ"), 40);
    m_cqRufenBtn->setToolTip(tr(
        "Call CQ. It repeats by itself for as many periods as the "
        "box beside it says, and stops as soon as somebody answers.\n"
        "From there the QSO runs through to 73. Stop ends it."));
    connect(m_cqRufenBtn, &QPushButton::clicked, this, [this]() {
        const QString ruf = cqRuf(eigenesRufzeichen(), eigenerLocator());
        if (ruf.isEmpty()) {
            if (m_status) { m_status->setText(tr("set your callsign first")); }
            return;
        }
        if (m_textFeld) { m_textFeld->setText(ruf); }
        if (m_sendet) { return; }

        // Ein Knopf namens CQ ruft CQ.
        //
        // Betreiber am 2026-10-22: „der cq ruf funktioniert nicht."
        // Er tat bis dahin nur eines: den Text ins Feld schreiben.
        // Gesendet wurde nur, wenn zufaellig „Auto" an war -- sonst
        // musste man danach noch „Send" druecken. Das ist nicht, was
        // der Knopf verspricht.
        m_qsoGemeldet = false;
        m_ablauf.starteAlsAnrufer(eigenesRufzeichen(), eigenerLocator());
        m_cqVersucheOffen = m_cqVersucheSpin ? m_cqVersucheSpin->value() : 1;
        ablaufSchritt();
    });
    sendRow->addWidget(m_cqRufenBtn);

    // Wie oft von selbst. Betreiber am 2026-10-22: „man soll auch
    // entscheiden koennen, wie oft er das automatisch probiert."
    //
    // Zehn ist die Vorgabe und keine erfundene Zahl: zehn Durchgaenge
    // sind zweieinhalb Minuten auf derselben Stelle -- lange genug,
    // dass jemand antwortet, kurz genug, dass ein vergessener Ruf
    // nicht den ganzen Abend laeuft.
    m_cqVersucheSpin = new QSpinBox(this);
    m_cqVersucheSpin->setRange(1, 30);
    m_cqVersucheSpin->setValue(AppSettings::instance()
                                   .value(QStringLiteral("Ft8/CqVersuche"),
                                          QStringLiteral("10"))
                                   .toInt());
    m_cqVersucheSpin->setPrefix(tr("x"));
    m_cqVersucheSpin->setFixedWidth(56);
    m_cqVersucheSpin->setToolTip(tr(
        "How many periods a CQ call repeats by itself.\nIt stops "
        "earlier when somebody answers, or on Stop."));
    connect(m_cqVersucheSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("Ft8/CqVersuche"),
                                         QString::number(v));
    });
    sendRow->addWidget(m_cqVersucheSpin);

    m_cqBtn = styledButton(tr("Send"), 56);
    m_cqBtn->setToolTip(tr("Send this message once, in the next own "
                           "period. No auto-sequencing."));
    connect(m_cqBtn, &QPushButton::clicked, this, [this]() {
        sendeNachricht(m_textFeld ? m_textFeld->text() : QString());
    });
    sendRow->addWidget(m_cqBtn);

    m_stopBtn = styledButton(tr("Stop"), 48);
    m_stopBtn->setToolTip(tr("Stop transmitting and drop the QSO "
                             "sequence."));
    m_stopBtn->setEnabled(false);
    // „Stop" heisst Stop. abbrechen() allein nimmt nur das laufende
    // Fenster zurueck — der Ablauf blieb stehen und waere beim
    // naechsten passenden Fund von selbst weitergelaufen. Seit der
    // Doppelklick den Ablauf immer startet, ist das der einzige Weg
    // hinaus, und er muss halten.
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        m_ablauf.abbrechen();
        m_cqVersucheOffen = 0;
        abbrechen();
        zeigeAblauf();
    });
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
        if (m_listRx) { m_listRx->clear(); }
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
    if (!m_ablauf.laeuft()) {
        m_cqVersucheOffen = 0;
        zeigeAblauf();
        return;
    }

    // Ein laufender CQ-Ruf: noch einmal, solange Versuche offen sind.
    // Antwortet jemand, uebernimmt der Ablauf von selbst -- dann steht
    // er nicht mehr im Anruferzustand, und diese Stelle greift nicht.
    if (m_cqVersucheOffen > 0) {
        --m_cqVersucheOffen;
        if (m_cqVersucheOffen == 0
            && m_ablauf.zustand() == Ft8Ablauf::Zustand::RufeCq) {
            m_ablauf.abbrechen();
            if (m_status) {
                m_status->setText(tr("CQ finished — nobody answered"));
            }
            zeigeAblauf();
            return;
        }
    }
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

// ── Ein fertiges QSO ins Logbuch ─────────────────────────────────────
//
// Nicht selbst schreiben: dasselbe Signal wie bei einem QSO aus
// WSJT-X, und MainWindow haengt es an RotorLogbookPanel::
// logExternalQso. Dort sitzt die Doppelt-Regel, das Anhaengen an die
// Datei und das Hochladen — alles einmal, nicht zweimal.
LogEntry Ft8Applet::logEintrag(const Ft8Ablauf& ablauf,
                               double wahlHz,
                               bool unteresSeitenband,
                               Ft8Decoder::Protocol p,
                               const QString& meinLocator,
                               int sendeTonHz)
{
    LogEntry e;
    e.call   = ablauf.partner();
    e.timeOn = QDateTime::currentDateTimeUtc();

    // Dieselbe Rechnung wie fuer die Funde: die Wahlfrequenz der
    // Scheibe plus die Tonlage, und das Vorzeichen haengt am
    // Seitenband. Gesendet wird auf der gewaehlten Tonlage.
    const double hz = wahlHz + (unteresSeitenband ? -double(sendeTonHz)
                                                  : double(sendeTonHz));
    e.freqMHz = hz / 1.0e6;
    e.band    = bandLabel(bandFromFrequency(hz));

    // ── Transverter (2026-10-28) ─────────────────────────────
    //
    // Steht einer davor, traegt der Satz die Frequenz der ANTENNE
    // und deren ADIF-Bandnamen. Ohne Transverter gibt
    // logbuchFrequenz() false zurueck und die zwei Zeilen darueber
    // bleiben, wie sie waren.
    TransverterSetup::logbuchFrequenz(std::llround(hz), &e.freqMHz, &e.band);

    // ADIF kennt FT8 und FT4 als eigene Betriebsarten — kein SUBMODE.
    e.mode = p == Ft8Decoder::Protocol::Ft4
                 ? QStringLiteral("FT4") : QStringLiteral("FT8");

    // In FT8 ist der Rapport die Signalstaerke in dB, nicht RST. So
    // schreibt WSJT-X ihn auch: „-12", „+03".
    const auto alsText = [](int r) {
        return r == Ft8Ablauf::kKeinReport
                   ? QString()
                   : QStringLiteral("%1%2")
                         .arg(r < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                         .arg(qAbs(r), 2, 10, QLatin1Char('0'));
    };
    e.rstSent = alsText(ablauf.gesendeterReport());
    e.rstRcvd = alsText(ablauf.empfangenerReport());

    e.gridSquare   = ablauf.partnerLocator();   // leer, wenn nie einer kam
    e.myGridSquare = meinLocator;
    return e;
}

void Ft8Applet::fundForTest(const Ft8Decode& d)
{
    onDecoded({d}, QDateTime::currentDateTimeUtc());
}

void Ft8Applet::meldeFertigesQso()
{
    if (m_qsoGemeldet || m_ablauf.partner().isEmpty()) { return; }
    m_qsoGemeldet = true;
    emit qsoFertig(logEintrag(m_ablauf,
                              m_slice ? m_slice->frequency() : 0.0,
                              m_slice && m_slice->dspMode() == DSPMode::DIGL,
                              protocol(), eigenerLocator(),
                              m_sendeTonHz));
}

void Ft8Applet::zeigeAblauf()
{
    if (!m_status) { return; }
    switch (m_ablauf.zustand()) {
        case Ft8Ablauf::Zustand::Fertig:
            m_status->setText(tr("QSO with %1 complete").arg(m_ablauf.partner()));
            meldeFertigesQso();
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
// ── Der Locator der Gegenstation, falls die Nachricht einen hat ──────
//
// „CQ HA1BF JN86" hat einen, „OE5SOS DL1ABC -12" nicht — dort waere
// das letzte Wort der Rapport. Bisher ging das letzte Wort
// ungeprueft weiter und stand als Feld im Logbuch; mit der
// Landspalte stand dort sogar „Hungary".
//
// Geprueft wird die Form eines Maidenhead-Feldes: zwei Buchstaben,
// zwei Ziffern. Mehr braucht eine FT8-Nachricht vom Standardtyp
// nicht, und weniger reicht nicht aus, um es von „R-09" oder „+02"
// zu unterscheiden.
QString Ft8Applet::locatorAusNachricht(const QString& nachricht)
{
    const QString letztes =
        nachricht.simplified().section(QLatin1Char(' '), -1).toUpper();
    if (letztes.size() != 4) { return {}; }
    if (!letztes.at(0).isLetter() || !letztes.at(1).isLetter()) { return {}; }
    if (!letztes.at(2).isDigit() || !letztes.at(3).isDigit()) { return {}; }
    return letztes;
}

// ── Die Nachricht aus einer Anzeigezeile ─────────────────────────────
//
// Die Spalten stehen in zeilentext(): vier Zahlenfelder (mit
// FUEHRENDEN Leerzeichen auf feste Breite gebracht), dann die
// Nachricht, dann — durch ZWEI Leerzeichen getrennt — das Land.
//
// Das Land kam mit #311 dazu, und seitdem log `section(' ', 4)` nach
// einem simplified(): es lieferte „CQ HA1BF JN86 Hungary" statt „CQ
// HA1BF JN86", und `section(' ', -1)` gab „Hungary" als Locator aus.
// Innerhalb einer FT8-Nachricht stehen nie zwei Leerzeichen
// hintereinander, also trennt das erste Doppelte sauber; ohne
// Landspalte gibt es keines und die Zeile bleibt, wie sie ist.
QString Ft8Applet::nachrichtAusZeile(const QString& zeile)
{
    int feld = 0;
    int i = 0;
    const int n = zeile.size();
    while (i < n && feld < 4) {
        while (i < n && zeile.at(i).isSpace())  { ++i; }
        while (i < n && !zeile.at(i).isSpace()) { ++i; }
        ++feld;
    }
    while (i < n && zeile.at(i).isSpace()) { ++i; }
    if (i >= n) { return {}; }

    QString rest = zeile.mid(i);
    const int trenner = rest.indexOf(QStringLiteral("  "));
    if (trenner > 0) { rest = rest.left(trenner); }
    return rest.trimmed();
}

QString Ft8Applet::antwortAuf(const QString& zeile,
                              const QString& meinRuf,
                              const QString& meinLocator)
{
    if (meinRuf.trimmed().isEmpty()) { return {}; }

    const QString nachricht = nachrichtAusZeile(zeile);
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

// Der eigene CQ-Ruf. Derselbe Aufbau wie eine Antwort, nur ohne
// Gegenstation: „CQ OE5SOS JN67". Ohne Locator bleibt „CQ OE5SOS" —
// das ist zulaessig und kommt auf dem Band vor.
QString Ft8Applet::cqRuf(const QString& meinRuf, const QString& meinLocator)
{
    const QString ruf = meinRuf.trimmed().toUpper();
    if (ruf.isEmpty()) { return {}; }
    const QString loc = meinLocator.trimmed().left(4).toUpper();
    return loc.isEmpty() ? QStringLiteral("CQ %1").arg(ruf)
                         : QStringLiteral("CQ %1 %2").arg(ruf, loc);
}

// `User/Callsign` ist der kanonische Schluessel — SpotHubDialog
// schreibt ihn („Writes canonical keys: User/Callsign,
// User/GridSquare"). `StationCallsign` wird dagegen von NIEMANDEM
// geschrieben und nur gelesen (SotaClient, MainWindow_KiwiSdr,
// QsoMapWindow); er steht hier als Rueckfall, damit eine von Hand
// gesetzte Fassung nicht ins Leere laeuft.
// ── Tx1..Tx6, wie in WSJT-X ──────────────────────────────────────────
//
// Betreiber 2026-10-18: „dachte das sind dann mehr optionen usw. wie
// ein richtiges ft8 programm." Die sechs Standardsaetze sind der
// haeufigste Griff im QSO -- bisher musste man sie tippen.
//
// Gebaut werden sie NICHT hier, sondern von Ft8Ablauf::nachrichtFuer():
// derselbe Code, den die Automatik benutzt. Hier steht nur, welcher
// Knopf welchen Zustand meint.
QString Ft8Applet::txKnopfNachricht(int nummer, const QString& partner,
                                    const QString& meinRuf,
                                    const QString& meinLocator,
                                    int reportDb)
{
    using Z = Ft8Ablauf::Zustand;
    if (meinRuf.isEmpty()) { return {}; }
    // Tx1 bis Tx5 sprechen jemanden an. Ohne Gegenueber gibt es nichts
    // zu senden -- und einen Platzhalter ins Feld zu legen waere
    // schlimmer, denn der ginge auf Knopfdruck hinaus.
    if (nummer >= 1 && nummer <= 5 && partner.isEmpty()) { return {}; }

    switch (nummer) {
        case 1: return Ft8Ablauf::nachrichtFuer(Z::RufeAn, partner, meinRuf,
                                                meinLocator, reportDb);
        case 2: return Ft8Ablauf::nachrichtFuer(Z::SendeReport, partner,
                                                meinRuf, meinLocator, reportDb);
        case 3: return Ft8Ablauf::nachrichtFuer(Z::SendeRReport, partner,
                                                meinRuf, meinLocator, reportDb);
        case 4: return Ft8Ablauf::nachrichtFuer(Z::SendeRR73, partner, meinRuf,
                                                meinLocator, reportDb);
        case 5: return Ft8Ablauf::nachrichtFuer(Z::Sende73, partner, meinRuf,
                                                meinLocator, reportDb);
        case 6: return Ft8Ablauf::nachrichtFuer(Z::RufeCq, partner, meinRuf,
                                                meinLocator, reportDb);
        default: return {};
    }
}

// Das Gegenueber: der laufende Ablauf, sonst das erste Wort im
// Textfeld -- in jedem Standardsatz steht dort das Rufzeichen der
// Gegenstation, und ein Doppelklick auf eine Zeile hat es genau
// dorthin gelegt. Kein zusaetzliches Feld, keine zweite Wahrheit.
QString Ft8Applet::aktuellerPartner() const
{
    if (m_ablauf.laeuft() && !m_ablauf.partner().isEmpty()) {
        return m_ablauf.partner();
    }
    if (!m_textFeld) { return {}; }
    const QString erstes =
        m_textFeld->text().trimmed().section(QLatin1Char(' '), 0, 0).toUpper();
    return erstes == QLatin1String("CQ") ? QString{} : erstes;
}

// ── Das Blatt hinter dem ⚙ ──────────────────────────────────────────
//
// Zwei Felder, mehr nicht: ohne Rufzeichen kann der Dekoder kein CQ
// bauen, ohne Locator keinen Rapport mit Feld.
//
// Geschrieben werden BEIDE Schluesselpaare. `User/Callsign` ist der
// kanonische (den liest dieser Dekoder, den schreibt der Spot Hub),
// `StationCallsign` lesen vier andere Stellen -- QSO-Karte, KiwiSDR,
// der FlexRadio-Rundruf und das Logbuch. Wer hier eintraegt, soll
// nicht hinterher feststellen, dass die Haelfte des Programms sein
// Rufzeichen nicht kennt. Genau das war am 2026-10-21 der Fall:
// `User/Callsign` stand auf OE5SOS, das Setup-Feld war leer.
void Ft8Applet::zeigeOptionen()
{
    auto& s = AppSettings::instance();

    QDialog blatt(this);
    blatt.setWindowTitle(tr("FT8 Decoder"));
    auto* form = new QFormLayout(&blatt);

    auto* ruf = new QLineEdit(eigenesRufzeichen(), &blatt);
    ruf->setPlaceholderText(QStringLiteral("OE5SOS"));
    form->addRow(tr("Callsign"), ruf);

    auto* loc = new QLineEdit(eigenerLocator(), &blatt);
    loc->setPlaceholderText(QStringLiteral("JN67VV"));
    loc->setToolTip(tr("Four or six characters. The decoder sends the "
                       "first four in CQ calls."));
    form->addRow(tr("Grid square"), loc);

    auto* knoepfe = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &blatt);
    form->addRow(knoepfe);
    connect(knoepfe, &QDialogButtonBox::accepted, &blatt, &QDialog::accept);
    connect(knoepfe, &QDialogButtonBox::rejected, &blatt, &QDialog::reject);

    if (blatt.exec() != QDialog::Accepted) { return; }

    const QString neuerRuf = ruf->text().trimmed().toUpper();
    const QString neuerLoc = loc->text().trimmed().toUpper();
    s.setValue(QStringLiteral("User/Callsign"), neuerRuf);
    s.setValue(QStringLiteral("StationCallsign"), neuerRuf);
    s.setValue(QStringLiteral("User/GridSquare"), neuerLoc);
    s.setValue(QStringLiteral("StationGridSquare"), neuerLoc);
}

// Beide lesen ueber die gemeinsame Rueckfallkette der Station
// (AppSettings::stationCallsign / stationGridSquare). Hier stand bis
// zum 2026-10-28 eine eigene, kuerzere Kette — und die kannte nur die
// zwei kanonischen Schluessel, also genau die beiden, die in Martins
// Einstellungen leer waren. Siehe AppSettings.cpp, dort steht der
// Hergang.
QString Ft8Applet::eigenesRufzeichen()
{
    return AppSettings::stationCallsign().toUpper();
}

QString Ft8Applet::eigenerLocator()
{
    return AppSettings::stationGridSquare().toUpper();
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

            // Ein Doppelklick nimmt das QSO an — und ab hier laeuft es
            // bis 73 von selbst. Betreiber am 2026-10-22: „wichtig ist
            // auch, dass nach der annahme des qso das qso automatisch
            // anlaeuft bis 73."
            //
            // Davor stand hier der Schalter „Auto", und er stand aus:
            // der Doppelklick legte die Antwort nur ins Feld, gesendet
            // wurde nichts. Derselbe Fehlgriff wie beim CQ-Knopf
            // (#315). Angehalten wird mit „Stop".
            if (!m_sendet) {
                const QString nachricht = nachrichtAusZeile(c.selectedText());
                const QString sauber = c.selectedText().simplified();
                const int snr = sauber.section(QLatin1Char(' '), 1, 1).toInt();
                m_qsoGemeldet = false;
                m_ablauf.starteAlsAntworter(senderCallsign(nachricht),
                                            eigenesRufzeichen(),
                                            eigenerLocator(), snr,
                                            locatorAusNachricht(nachricht));
                ablaufSchritt();
            }
        }
        return true;
    }
    return AppletWidget::eventFilter(obj, ev);
}

void Ft8Applet::sendeNachricht(const QString& text)
{
    if (m_sendet) {
        qCInfo(lcFt8Senden) << "Senden abgelehnt: es laeuft schon eine Sendung";
        return;
    }
    const QString sauber = text.trimmed().toUpper();
    if (sauber.isEmpty()) {
        qCInfo(lcFt8Senden) << "Senden abgelehnt: das Textfeld ist leer"
                            << "(Rufzeichen gesetzt:"
                            << !eigenesRufzeichen().isEmpty() << ")";
        if (m_status) { m_status->setText(tr("nothing to send")); }
        return;
    }

    // ── Der Mikrofon-Router ist NICHT mehr Bedingung (2026-10-28) ────
    //
    // Hier stand `|| !m_model->micRouter()`. Der Router gehoert zum
    // eigenen WDSP-Sendekanal; die Flex hat keinen, sie rechnet
    // selbst. Also brach jeder Sendeversuch an der Flex hier ab —
    // stumm, weil die Statuszeile sofort ueberschrieben wird.
    //
    // Gebraucht wird nur die Tonquelle. Wer den Ton abholt, ist eine
    // Frage des Funkgeraets: beim eigenen der Router, beim Flex die
    // Sendepumpe in MainWindow_Flex.cpp.
    if (!m_model || !m_model->ft8TxSource()) {
        qCWarning(lcFt8Senden)
            << "Senden abgelehnt: keine Tonquelle (Modell"
            << (m_model != nullptr) << ", Ft8TxSource"
            << (m_model && m_model->ft8TxSource() != nullptr) << ")";
        if (m_status) { m_status->setText(tr("no transmit path")); }
        return;
    }

    Ft8TxSource* quelle = m_model->ft8TxSource();
    if (!quelle->vorbereiten(sauber, float(m_sendeTonHz), protocol())) {
        qCWarning(lcFt8Senden).noquote()
            << "Senden abgelehnt: ft8_lib kann \"" + sauber + "\" nicht packen";
        if (m_status) {
            m_status->setText(tr("cannot encode “%1”").arg(sauber));
        }
        return;
    }
    qCInfo(lcFt8Senden).noquote()
        << "Senden vorbereitet:" << sauber << "auf" << m_sendeTonHz << "Hz";

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
        // ── Genau, nicht grob (2026-10-29) ───────────────────────────
        //
        // Qt gibt jedem Taktgeber ab 20 ms von sich aus
        // `Qt::CoarseTimer` und erlaubt sich dabei, bis zu FUENF
        // PROZENT des Intervalls zu frueh zu wecken — damit mehrere
        // Taktgeber gemeinsam aufwachen koennen. Bei FT8 wird hier auf
        // die naechste Fenstergrenze gewartet, und das sind bis zu
        // 17,8 s. Fuenf Prozent davon sind 0,89 s.
        //
        // Am Geraet gemessen (Protokoll 2026-10-29, 07:02 bis 07:04,
        // fuenf Sendungen): MOX ging bei :29,21 an statt bei :30,00 —
        // 0,779 / 0,787 / 0,786 / 0,786 / 0,786 s zu frueh, jedes Mal.
        // Die Rechnung in `Ft8Tx::bisZumNaechsten` war dabei richtig;
        // der Taktgeber hat sie verworfen. Betreiber: „die empfangs
        // und sendezeit stimmt nicht."
        //
        // Die Systemuhr war es nicht: `sntp time.apple.com` meldete
        // im selben Augenblick +37 ms.
        m_sendeTimer->setTimerType(Qt::PreciseTimer);
    }
    m_sendeTimer->disconnect();
    const QString gesendet = sauber;
    connect(m_sendeTimer, &QTimer::timeout, this, [this, gesendet]() {
        if (!m_sendet || !m_model) { return; }
        Ft8TxSource* q = m_model->ft8TxSource();
        if (!q) {
            qCWarning(lcFt8Senden) << "Tonquelle zwischendurch verschwunden";
            abbrechen();
            return;
        }

        // Erst den Weg legen, dann tasten: andersherum ginge der
        // Sender mit dem Mikrofon auf, nicht mit dem Ton. Ohne Router
        // (Flex) entfaellt das Umlegen — dort holt die Sendepumpe die
        // Proben direkt aus der Quelle.
        if (auto* router = m_model->micRouter()) {
            m_quelleVorher = static_cast<int>(router->activeSource());
            router->setActiveSource(MicSource::Ft8);
        }
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
        zeigeEigeneSendung(gesendet);

        // Nur so lange tasten, wie das SIGNAL dauert -- nicht das
        // ganze Fenster.
        //
        // Gemessen am 2026-10-22 am Geraet: der Sender blieb 14,9 s an
        // (`mox=true` 06:17:30,295 bis 06:17:45,170), obwohl ein
        // FT8-Signal 12,64 s lang ist. Betreiber: „hat zu lange
        // gedauert." Die zwei Sekunden danach gehen nicht nur
        // verloren -- in ihnen laeuft der Empfang nicht, und bei FT8
        // beginnt das naechste Fenster unmittelbar. Wer zu spaet
        // zurueckkommt, hoert den Anfang der Antwort nicht.
        //
        // Der Zuschlag von 300 ms bleibt: er sorgt dafuer, dass der
        // letzte Ton wirklich draussen ist, bevor der Sender
        // abschaltet.
        // Auch hier genau: ein Grobtimer duerfte bis zu 0,65 s zu
        // frueh abschalten und damit den Schluss des Signals
        // abschneiden (siehe `setTimerType` oben).
        QTimer* schluss = new QTimer(this);
        schluss->setSingleShot(true);
        schluss->setTimerType(Qt::PreciseTimer);
        connect(schluss, &QTimer::timeout, this, [this, schluss]() {
            schluss->deleteLater();
            fensterZuEnde();
        });
        schluss->start(Ft8Tx::signalMs(protocol()) + 300);
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
        // Spalten wie in WSJT-X „Band Activity", dahinter das Land.
        const QString zeile = zeilentext(
            uhrzeit,
            static_cast<int>(std::lround(d.snrDb)),
            d.dtSeconds,
            static_cast<int>(std::lround(d.freqHz)),
            d.text,
            landFuer(senderCallsign(d.text)));
        zeigeZeile(zeile, d.text,
                   static_cast<int>(std::lround(d.freqHz)));

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
        if (!ruf.isEmpty()) {
            m_letzterRapport.insert(ruf,
                                    static_cast<int>(std::lround(d.snrDb)));
        }
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


// ── Eine Zeile in die Liste, gefiltert und gefaerbt ─────────────────
//
// Drei Sorten, nach Dringlichkeit:
//
//   1. Die Nachricht enthaelt das EIGENE Rufzeichen -- jemand ruft
//      dich. Gruen und fett; das ist die einzige Zeile, die man nicht
//      uebersehen darf.
//   2. Die Nachricht faengt mit „CQ" an -- jemand ist rufbar. Bernstein.
//   3. Alles andere -- ein fremdes QSO. Die gewoehnliche Textfarbe.
//
// Eingefaerbt wird ueber einen Cursor mit Zeichenformat, nicht ueber
// appendHtml(): die Liste steht in fester Schrittweite, und HTML
// faltet mehrfache Leerzeichen zusammen -- die Spalten (Zeit, dB, DT,
// Hz) stuenden danach nicht mehr untereinander.
Ft8Applet::Zeilenart Ft8Applet::zeilenart(bool anMich, bool neuesLand,
                                         bool istCq)
{
    if (anMich)    { return Zeilenart::AnMich; }
    if (neuesLand) { return Zeilenart::NeuesLand; }
    if (istCq)     { return Zeilenart::Cq; }
    return Zeilenart::Gewoehnlich;
}

// Siehe Ft8Applet.h.
QString Ft8Applet::zeilentext(const QString& uhrzeit, int snrDb,
                              double dtSekunden, int hz,
                              const QString& nachricht,
                              const QString& land)
{
    // Breite der Nachrichtenspalte. 18 ist gemessen, nicht geraten:
    // die laengste uebliche FT8-Nachricht ist ein Ruf mit Praefix und
    // Suffix samt Feld („ON6LEO F5OYA/P JN13" = 19), die allermeisten
    // bleiben darunter. Wer laenger ist, schiebt die Landspalte --
    // das faellt auf und schneidet nichts ab.
    constexpr int kNachrichtBreite = 19;

    const QString spalten = QStringLiteral("%1 %2 %3 %4  %5")
                                .arg(uhrzeit)
                                .arg(snrDb, 4)
                                .arg(dtSekunden, 5, 'f', 1)
                                .arg(hz, 5)
                                .arg(nachricht);
    if (land.isEmpty()) { return spalten; }

    // Nur auffuellen, wenn die Nachricht kuerzer ist -- `leftJustified`
    // mit `truncate=false` laesst Laengeres stehen.
    const QString breit = nachricht.leftJustified(kNachrichtBreite, QLatin1Char(' '));
    return QStringLiteral("%1 %2 %3 %4  %5  %6")
        .arg(uhrzeit)
        .arg(snrDb, 4)
        .arg(dtSekunden, 5, 'f', 1)
        .arg(hz, 5)
        .arg(breit, land);
}

QString Ft8Applet::landFuer(const QString& rufzeichen) const
{
    if (rufzeichen.isEmpty() || !m_model) { return {}; }
    auto* dxcc = m_model->dxccColorProvider();
    if (!dxcc) { return {}; }
    // Absichtlich NICHT an `isEnabled()` gebunden: der Schalter gehoert
    // der Faerbung. Den Namen des Landes zu lesen, ist keine Farbe --
    // wer die Farben abschaltet, will deswegen nicht wissen, wen er
    // hoert, und umgekehrt.
    const QString praefix = dxcc->ctyDat().resolvePrimaryPrefix(rufzeichen);
    if (praefix.isEmpty()) { return {}; }
    const DxccEntity* e = dxcc->ctyDat().entityByPrefix(praefix);
    return e ? e->name : QString();
}

// ── Was ICH gesendet habe, in die rechte Spalte ──────────────────────
//
// Betreiber am 2026-10-22: „sehe ich auch rechts, wenn ein qso zu
// stande kommt." Die Antworten an ihn standen dort schon -- jede
// Zeile mit dem eigenen Rufzeichen geht nach rechts, egal auf welcher
// Tonlage. Was fehlte, war die eigene Haelfte des Gespraechs: ein QSO
// laeuft jetzt von selbst ab, und ohne die eigenen Zuege steht rechts
// nur, was der andere sagt.
//
// Wie in WSJT-X: in die rechte Spalte, nicht in die linke -- links
// steht, was auf dem BAND los ist, und das eigene Signal gehoert
// nicht dazu. Die Spalten sind dieselben, nur steht „Tx" an der
// Stelle des Rapports und die Laufzeit bleibt leer; beides hat bei
// einer eigenen Sendung keinen Wert.
//
// Die Farbe ist das Messing der Sendetaste (kTxKeyBorder), nicht
// Rot: „kein kraeftiges Rot als Zustandsfarbe" gilt hier wie dort.
void Ft8Applet::zeigeEigeneSendung(const QString& nachricht)
{
    if (m_listRx == nullptr || nachricht.trimmed().isEmpty()) { return; }

    const QString zeile = QStringLiteral("%1 %2 %3 %4  %5")
                              .arg(QDateTime::currentDateTimeUtc()
                                       .toString(QStringLiteral("hhmmss")))
                              .arg(tr("Tx"), 4)
                              .arg(QString(), 5)
                              .arg(m_sendeTonHz, 5)
                              .arg(nachricht.trimmed());

    QTextCharFormat f;
    f.setForeground(QColor(QString::fromLatin1(Style::kTxKeyBorder)));
    f.setFontWeight(QFont::Bold);

    QTextCursor c(m_listRx->document());
    c.movePosition(QTextCursor::End);
    if (!m_listRx->document()->isEmpty()) { c.insertBlock(); }
    c.insertText(zeile, f);
}

void Ft8Applet::zeigeZeile(const QString& zeile, const QString& nachricht,
                           int tonHz)
{
    if (!m_list) { return; }

    const QString ruf = eigenesRufzeichen();
    // Wortgrenzen, nicht contains(): „OE5SOS" steckt auch in
    // „OE5SOSX", und das ist eine andere Station.
    const bool anMich =
        !ruf.isEmpty()
        && nachricht.toUpper().split(QLatin1Char(' '),
                                     Qt::SkipEmptyParts).contains(ruf);
    const bool istCq = nachricht.trimmed().toUpper().startsWith(
        QStringLiteral("CQ"));

    // Ein Land, das noch nie im Logbuch stand? Der Auflaeser steht
    // schon im Modell -- derselbe, der die Funde im Panadapter
    // einfaerbt. Also DIESELBE Palette, keine zweite erfunden.
    //
    // Steht seit dem 2026-10-27 VOR den Filtern: „New DXCC only"
    // braucht die Antwort, um ueberhaupt filtern zu koennen. Vorher
    // stieg der CQ-Filter davor aus, und das Land war an dieser Stelle
    // noch unbekannt.
    bool neuesLand = false;
    QColor landFarbe;
    const QString senderRuf = senderCallsign(nachricht);
    if (m_model && m_model->dxccColorProvider() && !senderRuf.isEmpty()) {
        auto* dxcc = m_model->dxccColorProvider();
        if (dxcc->isEnabled()) {
            const double mhz = (m_slice ? m_slice->frequency() : 0.0) / 1.0e6;
            const QString modus = protocol() == Ft8Decoder::Protocol::Ft4
                                      ? QStringLiteral("FT4")
                                      : QStringLiteral("FT8");
            neuesLand = dxcc->statusForSpot(senderRuf, mhz, modus)
                        == DxccStatus::NewDxcc;
            if (neuesLand) {
                landFarbe = dxcc->colorForSpot(senderRuf, mhz, modus);
            }
        }
    }

    // ── Die Ansichtsfilter (2026-10-27) ──────────────────────────────
    //
    // Beide gelten NUR fuer die Ansicht. Der Ablauf bekommt weiter
    // jeden Fund (siehe m_nurCq in Ft8Applet.h): sonst verschwaende
    // ein laufendes QSO, sobald ein Haken sitzt.
    //
    // Eine Zeile, die an MICH geht, kommt durch jeden Filter. Wer
    // gerufen wird und es nicht sieht, weil ein Haken sitzt, verliert
    // ein QSO -- das ist kein Filter mehr, sondern ein Fehler.
    if (m_nurCq && !istCq && !anMich) { return; }
    if (m_nurNeueLaender && !neuesLand && !anMich) { return; }

    QTextCharFormat f;
    const Zeilenart art = zeilenart(anMich, neuesLand, istCq);
    if (art == Zeilenart::AnMich) {
        f.setForeground(QColor(QString::fromLatin1(Style::kGreenText)));
        f.setFontWeight(QFont::Bold);
    } else if (art == Zeilenart::NeuesLand) {
        // Fett wie „an mich": ein neues Land uebersieht man sonst in
        // zwanzig Zeilen je Fenster.
        f.setForeground(landFarbe.isValid()
                            ? landFarbe
                            : QColor(QString::fromLatin1(Style::kAmberText)));
        f.setFontWeight(QFont::Bold);
    } else if (art == Zeilenart::Cq) {
        f.setForeground(QColor(QString::fromLatin1(Style::kAmberText)));
    } else {
        f.setForeground(QColor(QString::fromLatin1(Style::kTextPrimary)));
    }

    const auto anhaengen = [&f, &zeile](QPlainTextEdit* liste) {
        QTextCursor c(liste->document());
        c.movePosition(QTextCursor::End);
        if (!liste->document()->isEmpty()) { c.insertBlock(); }
        c.insertText(zeile, f);
        // Ans Ende der Zeile zu schreiben schiebt die Ansicht mit --
        // dann steht die Uhrzeit links ausserhalb des Fensters, und
        // man sieht die Spalte, die man am wenigsten braucht. Gesehen
        // im gerenderten Blatt am 2026-10-21.
    };

    anhaengen(m_list);
    ++m_lineCount;

    // Rechte Spalte („Rx Frequency"): was auf der eigenen Stelle liegt
    // -- oder an mich geht, egal wo. Das Fenster ist die Signalbreite
    // von FT8/FT4 (50 Hz), nicht ein gegriffener Wert: schmaler, und
    // ein Fund am Rand des eigenen Signals fehlt; breiter, und die
    // Spalte fuellt sich mit Nachbarn.
    if (m_listRx != nullptr) {
        const int abstand = std::abs(tonHz - m_sendeTonHz);
        if (anMich || abstand <= kRxFensterHz) {
            anhaengen(m_listRx);
        }
    }
}

} // namespace Longpath
