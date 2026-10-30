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
#include "core/Ft8Ablauf.h"
#include "core/Ft8Tx.h"
#include "gui/widgets/Ft8Fensterbalken.h"
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

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
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

// ── Die drei Zeiten einer Sendung ─────────────────────────
//
// Begruendung und Rechnung: `Ft8Tx::sendeplan`.

/// Wie lange nach der Fenstergrenze auf den Dekoder gewartet wird,
/// bevor der Zug festgelegt und getastet wird.
///
/// Am Geraet gemessen (2026-10-30): das Fenster endete um :30,000,
/// der Dekoder meldete um :30,039 — 39 ms mit zwei Runden
/// Subtraktion. 250 ms lassen Luft fuer mehr Runden und einen
/// beschaeftigten Rechner, ohne dass etwas davon verlorengeht: die
/// Frist wird aus der fuehrenden Stille der Wellenform genommen,
/// nicht hinten angehaengt.
constexpr qint64 kDekoderFristMs = 250;

/// Wo das Signal nach der Fenstergrenze beginnen soll.
///
/// WSJT-X setzt es auf eine halbe Sekunde; das ist der Wert, gegen
/// den die ganze Betriebsart eingestellt ist. Von vorn abgespielt
/// laege es bei 1180 ms (ft8_lib zentriert) — dekodierbar, aber
/// 0,68 s neben der Norm.
constexpr qint64 kZielVersatzMs = 500;

/// Nachlauf, damit der letzte Ton wirklich draussen ist, bevor der
/// Sender abschaltet.
constexpr qint64 kSenderNachlaufMs = 300;

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

    // ── Hound: eine DX-Expedition rufen (2026-10-30) ──────────
    //
    // Im Fox-Betrieb laeuft ein QSO anders: den Schluss macht die
    // Expedition, und wer unter 1000 Hz sendet, wird gar nicht erst
    // beantwortet. Der Ablauf dafuer steht in `Ft8Ablauf::
    // starteAlsHound`; hier wird nur gesagt, dass er gelten soll.
    //
    // Ein Schalter, keine Erkennung: ob eine Station im Fox-Betrieb
    // laeuft, steht erst in ihrem 0.1-Satz -- und den hoert man erst,
    // wenn sie jemanden AUFRUFT. Bis dahin sieht ein Fox-CQ wie jedes
    // andere aus. Der Betreiber weiss es vorher, die Maschine nicht.
    m_houndBtn = blueToggle(tr("Hound"), 58);
    m_houndBtn->setToolTip(
        tr("Call a DXpedition running in Fox mode: the Fox closes the "
           "QSO, and a Hound transmitting below 1000 Hz is not "
           "answered — the transmit tone is raised if needed."));
    connect(m_houndBtn, &QPushButton::clicked, this, [this](bool an) {
        m_houndBetrieb = an;
        AppSettings::instance().setValue(
            QStringLiteral("Ft8/Hound"),
            an ? QStringLiteral("True") : QStringLiteral("False"));
        if (!m_tonFeld) { return; }
        if (an) {
            if (m_tonFeld->value() < Ft8Ablauf::kHoundMinTonHz) {
                // Lieber hochziehen als warnen: eine Warnung, die man
                // wegklickt, laesst einen stundenlang ins Leere rufen.
                m_tonVorHound = m_tonFeld->value();
                m_tonFeld->setValue(Ft8Ablauf::kHoundMinTonHz + 500);
                if (m_status) {
                    m_status->setText(tr("tone %1 → %2 Hz (Hounds below "
                                         "1000 are not answered)")
                                          .arg(m_tonVorHound)
                                          .arg(m_tonFeld->value()));
                }
            }
        } else if (m_tonVorHound > 0) {
            // ── Und zurueck (2026-10-30) ───────────────────
            //
            // Am Geraet gesehen: der Betreiber hat Hound ein- und
            // wieder ausgeschaltet, und seine 837 Hz waren weg -- 1500
            // blieben stehen. Eine Einstellung, die ich fuer eine
            // Betriebsart hochziehe, gebe ich zurueck, wenn die
            // Betriebsart endet. Sie gehoert ihm, nicht mir.
            const int zurueck = m_tonVorHound;
            m_tonVorHound = 0;
            m_tonFeld->setValue(zurueck);
            if (m_status) {
                m_status->setText(tr("tone back to %1 Hz").arg(zurueck));
            }
        }
    });
    m_houndBetrieb = AppSettings::instance()
                         .value(QStringLiteral("Ft8/Hound"),
                                QStringLiteral("False")).toString()
                     == QStringLiteral("True");
    m_houndBtn->setChecked(m_houndBetrieb);
    head->addWidget(m_houndBtn);

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
    // Fund (siehe `inSicht`): sonst verschwaende ein
    // laufendes QSO, sobald der Haken sitzt.
    // ── Die zwei Haken sind Pillen geworden (2026-10-30) ──────
    //
    // „CQ only" und „New DXCC only" standen hier als zwei Haken in der
    // Kopfzeile, waehrend unten vier Pillen dasselbe taten. Zwei
    // Filtersysteme nebeneinander sind eines zu viel: zwei Haken
    // gleichzeitig ergaben die Schnittmenge, ohne dass das irgendwo
    // stand, und die Kopfzeile wurde so breit, dass das Panel sich bei
    // 460 Punkten nicht mehr zeichnen liesz (im gerenderten Blatt
    // gemessen: 706 statt 460).
    //
    // Beides steckt jetzt in den Pillen unter dem Kopf. Die Zugaenge
    // `nurCqButtonForTest` / `nurNeueLaenderButtonForTest` zeigen
    // dorthin — derselbe Zweck, dieselbe Pruefung.

    m_wasserfallBtn = blueToggle(tr("Waterfall"), 72);
    m_wasserfallBtn->setToolTip(tr("Show the waterfall above the list."));
    connect(m_wasserfallBtn, &QPushButton::clicked, this, [this](bool an) {
        m_wasserfallAn = an;
        if (m_wasserfall) { m_wasserfall->setVisible(an); }
        AppSettings::instance().setValue(
            QStringLiteral("Ft8/Wasserfall"),
            an ? QStringLiteral("True") : QStringLiteral("False"));
    });
    head->addWidget(m_wasserfallBtn);
    // Der Knopf zeigt, was gilt — die Wahl steht in AppSettings.

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

    // ── Der Wasserfall ist abschaltbar (Entwurf C, 2026-10-30) ───
    //
    // Er nahm ein Viertel der Hoehe und stand im gerenderten Blatt
    // leer da — er lebt erst, wenn Ton hereinkommt. Wer ihn braucht,
    // schaltet ihn ein; wer die Liste braucht, bekommt den Platz.
    // Die Wahl bleibt erhalten.
    m_wasserfallAn = AppSettings::instance()
                         .value(QStringLiteral("Ft8/Wasserfall"),
                                QStringLiteral("True")).toString()
                     == QStringLiteral("True");
    m_wasserfall->setVisible(m_wasserfallAn);
    if (m_wasserfallBtn) { m_wasserfallBtn->setChecked(m_wasserfallAn); }

    // ── Der Fuellstand sitzt jetzt in der Arbeitsleiste (30.10.) ──
    //
    // Hier stand ein vier Pixel hoher Fortschrittsbalken ueber der
    // Liste, der dasselbe zeigte wie der Fensterbalken unten -- nur
    // ohne die Marken fuer Signalanfang und -ende, und an einer
    // Stelle, an der niemand hinsieht. Am laufenden Geraet standen
    // beide uebereinander im Bild.
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

    // ── Eine Liste statt zweier (Entwurf C, 2026-10-30) ───────
    //
    // Bis dahin standen „Band Activity" und „Rx Frequency"
    // nebeneinander, wie in WSJT-X. Im Panel sind das zwei Spalten zu
    // je 214 Punkten, und dort passt die Zeile nicht hinein: im
    // gerenderten Blatt vom 2026-10-30 stand „Austr…" und „Fed." statt
    // des Landes, und rechts fehlte das Ende der Nachricht. Dazu zeigte
    // die rechte Spalte meistens dieselben Zeilen noch einmal.
    //
    // Jetzt eine Liste ueber die ganze Breite, und welche Zeilen zu
    // sehen sind, sagen vier Pillen. „Meine Frequenz" ist die alte
    // rechte Spalte — dieselbe Regel (eigene Tonlage ± 50 Hz, oder an
    // mich gerichtet), nur als Sicht statt als zweites Fenster.
    //
    // Betreiber am 2026-10-30, nach drei Entwurfsblaettern: „c bitte".
    auto* sichtReihe = new QHBoxLayout();
    sichtReihe->setSpacing(3);
    const auto sichtKnopf = [this, sichtReihe](const QString& text,
                                               Sicht welche, int breite) {
        QPushButton* b = blueToggle(text, breite);
        b->setCheckable(true);
        connect(b, &QPushButton::clicked, this, [this, welche]() {
            m_sicht = welche;
            zeichneListe();
        });
        sichtReihe->addWidget(b);
        return b;
    };
    m_sichtAlles  = sichtKnopf(tr("All"),       Sicht::Alles, 46);
    m_sichtAnMich = sichtKnopf(tr("To me"),     Sicht::AnMich, 54);
    m_sichtCq     = sichtKnopf(tr("CQ"),        Sicht::Cq, 38);
    m_sichtNeuesLand = sichtKnopf(tr("New DXCC"), Sicht::NeuesLand, 72);
    m_sichtFreq   = sichtKnopf(tr("My freq"),   Sicht::MeineFrequenz, 62);
    m_sichtAlles->setChecked(true);
    m_sichtAnMich->setToolTip(tr("Only messages addressed to you."));
    m_sichtCq->setToolTip(tr("Only CQ calls."));
    m_sichtNeuesLand->setToolTip(tr("Only countries that are not in the "
                                    "log yet."));
    m_sichtFreq->setToolTip(tr("Decodes within 50 Hz of your transmit "
                               "tone, plus anything addressed to you — "
                               "what the second column used to show."));
    sichtReihe->addStretch(1);
    m_fundZahl = new QLabel(QString(), this);
    m_fundZahl->setFont(Style::monoFont(font(), Style::kFontMicro));
    m_fundZahl->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                  .arg(Style::kTextInactive));
    sichtReihe->addWidget(m_fundZahl);

    m_clearBtn = styledButton(tr("Clear"), 54);
    m_clearBtn->setCheckable(false);   // leert die Liste, rastet nicht ein
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        // Auch den Speicher: sonst kaeme beim naechsten Pillendruck
        // alles zurueck, was gerade geloescht wurde.
        m_zeilen.clear();
        zeichneListe();
    });
    sichtReihe->addWidget(m_clearBtn);
    root->addLayout(sichtReihe);

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
    // (der „Manual"-Knopf kommt weiter unten dazu)

    // ── CQ rufen ────────────────────────────────────────────────────
    //
    // Ft8Ablauf kann seit dem QSO-Ablauf auch die ANRUFER-Rolle
    // (starteAlsAnrufer, dreizehn Prueffaelle) — nur rief sie
    // niemand auf. Antworten ging per Doppelklick, selbst CQ rufen
    // gar nicht. Dieser Knopf schliesst das, mit demselben Verhalten
    // wie der Doppelklick: er faengt den Ablauf an und laesst ihn
    // bis 73 laufen.
    // ── Die Arbeitsleiste (Entwurf C, 2026-10-30) ────────────
    //
    // Was beim automatischen Durchlauf zaehlt, steht unten, wo die
    // Hand ist: mit wem, welcher Zug, was danach kommt, wie weit das
    // Fenster. Vorher stand davon nichts im Panel — der Zustand war
    // nur aus der Statuszeile zu erraten, und der Fensterfortschritt
    // stand grau im Panelkopf, an der unauffaelligsten Stelle fuer das
    // einzige Ding, das sich dauernd bewegt.
    m_arbeit = new QWidget(this);
    // Ein Messingstrich links: dieselbe Rolle wie der Panelakzent im
    // Kopf — hier faengt etwas an, das gemessen wird.
    //
    // Radius 6, nicht 3. HAUSSTIL.md: „Nie Radius 3. Das ist der
    // Qt-Standardwert und laesst alles nach Qt aussehen." Der
    // Prueflauf tst_hausstil_radius faengt das -- und hat es bei
    // dieser Leiste auch getan (2026-10-30).
    m_arbeit->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border: 1px solid %2;"
        " border-left: 3px solid %3; border-radius: 6px; }")
        .arg(Style::kPanelBg, Style::kBorder, Style::kTxKeyBottom));
    auto* arbeit = new QGridLayout(m_arbeit);
    arbeit->setContentsMargins(10, 6, 10, 7);
    arbeit->setHorizontalSpacing(8);
    arbeit->setVerticalSpacing(4);

    const auto schild = [this](const QString& t, const char* farbe,
                               int groesse, bool fett = false) {
        auto* l = new QLabel(t, m_arbeit);
        QFont f = font();
        f.setPointSize(groesse);
        f.setBold(fett);
        l->setFont(f);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; border: none; }")
                             .arg(QString::fromLatin1(farbe)));
        return l;
    };

    auto* kopfReihe = new QHBoxLayout();
    kopfReihe->setSpacing(7);
    m_arbeitZustand = schild(tr("idle"), Style::kTextSecondary,
                             Style::kFontSmall);
    m_arbeitPartner = schild(QString(), Style::kTextPrimary,
                             Style::kFontSub, true);
    m_arbeitLand    = schild(QString(), Style::kTextInactive,
                             Style::kFontSmall);
    kopfReihe->addWidget(m_arbeitZustand);
    kopfReihe->addWidget(m_arbeitPartner);
    kopfReihe->addWidget(m_arbeitLand);
    kopfReihe->addStretch(1);
    m_arbeitUhr = schild(QString(), Style::kTextInactive, Style::kFontSmall);
    m_arbeitUhr->setFont(Style::monoFont(font(), Style::kFontSmall));
    kopfReihe->addWidget(m_arbeitUhr);
    arbeit->addLayout(kopfReihe, 0, 0, 1, 2);

    // Der naechste Zug im Klartext — dieselbe Schrift wie die Funde,
    // damit man ihn als FT8-Satz liest und nicht als Beschriftung.
    m_arbeitZug = new QLabel(QString(), m_arbeit);
    m_arbeitZug->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_arbeitZug->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " padding: 3px 6px; border-radius: 2px; }")
        .arg(Style::kInsetBg, Style::kTxKeyBorder, Style::kInsetBorder));
    arbeit->addWidget(m_arbeitZug, 1, 0);

    m_arbeitDanach = schild(QString(), Style::kTextSecondary,
                            Style::kFontCaption);
    m_arbeitDanach->setMinimumWidth(118);
    m_arbeitDanach->setWordWrap(true);
    arbeit->addWidget(m_arbeitDanach, 1, 1, 2, 1, Qt::AlignTop);

    m_arbeitBalken = new Ft8Fensterbalken(m_arbeit);
    m_arbeitBalken->setFixedHeight(10);
    arbeit->addWidget(m_arbeitBalken, 2, 0);
    arbeit->setColumnStretch(0, 1);

    root->addWidget(m_arbeit);

    // ── Tx1..Tx6 hinter einem Knopf ─────────────────────
    //
    // Sechs gleich aussehende Knoepfe in einer Reihe sind Handbetrieb,
    // und Handbetrieb ist beim automatischen Durchlauf die Ausnahme.
    // Sie legen den Satz weiterhin nur ins Feld — gesendet wird mit
    // „Send".
    m_handBtn = styledButton(tr("Manual"), 86);
    m_handBtn->setCheckable(false);
    m_handBtn->setToolTip(tr("The six standard messages Tx1…Tx6."));
    auto* handMenu = new QMenu(m_handBtn);
    for (int n = 1; n <= 6; ++n) {
        QAction* a = handMenu->addAction(QStringLiteral("Tx%1").arg(n));
        connect(a, &QAction::triggered, this, [this, n]() {
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
        m_txAktionen[n - 1] = a;
    }
    m_handBtn->setMenu(handMenu);
    sendRow->insertWidget(0, m_handBtn);

    // `blueToggle` statt `styledButton`: nur der erste haengt die
    // Blaufuellung des eingerasteten Knopfes an (Style::blueCheckedStyle).
    // Ohne sie aendert `setChecked` nichts Sichtbares — genau das meldete
    // der Betreiber am 2026-10-29: „der button sollte sich auch farblich
    // aendern, wenn er geklickt ist." Dieselbe Fuellung tragen FT8/FT4
    // darueber und die Bandwahl in der Kopfleiste.
    m_cqRufenBtn = blueToggle(tr("CQ"), 40);
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
        m_cqAutomatik = true;
        starteCqRuf();
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
    // ── Breit genug fuer zwei Stellen (2026-10-30) ───────────
    //
    // Hier stand `setFixedWidth(56)`. Am Geraet stand darin „x1" —
    // die zweite Stelle war weg, obwohl 16 eingestellt waren.
    // Betreiber: „neben dem CQ ist ein Fenster - anzahl der
    // wiederholungen. dies kann man nicht sehen!"
    //
    // Keine feste Zahl mehr: der Bereich geht bis 30, das Praefix ist
    // ein Zeichen, und wie breit das in der jeweiligen Schrift ist,
    // weiss die Schrift besser als ich. `sizeHint()` rechnet es aus
    // dem groeszten Wert samt Praefix und Pfeilen — eine feste Breite
    // ueberschreibt genau diese Rechnung.
    m_cqVersucheSpin->setPrefix(tr("x"));
    m_cqVersucheSpin->setMinimumWidth(m_cqVersucheSpin->sizeHint().width());
    m_cqVersucheSpin->setToolTip(tr(
        "How many periods a CQ call repeats by itself.\nIt stops "
        "earlier when somebody answers, or on Stop."));

    // ── Multi Answering: wie viele gleichzeitig (2026-10-30) ─────
    //
    // MSHVs Kernmerkmal. Die Technik lag seit #323/#333/#335 fertig
    // und geprueft im Baum und wurde von niemandem gerufen — hier ist
    // der Anschluss.
    //
    // Vorgabe 1 = aus. Die Sendeleistung wird GETEILT: zwei Plaetze
    // sind −6 dB je Station, drei −9,5, sechs −15,6 (MSHV
    // `gen_ft8.cpp:303`, und die Teilung ist kein Geschmack — der
    // Sender hat eine Hoechstamplitude). Bei 5 W und drei Plaetzen
    // bleiben 1,7 W je Station.
    m_plaetzeSpin = new QSpinBox(this);
    m_plaetzeSpin->setRange(1, Ft8Mehrfach::kMaxPlaetze);
    m_plaetzeSpin->setValue(AppSettings::instance()
                                .value(QStringLiteral("Ft8/Plaetze"),
                                       QStringLiteral("1"))
                                .toInt());
    m_plaetzeSpin->setPrefix(tr("slots "));
    m_plaetzeSpin->setMinimumWidth(m_plaetzeSpin->sizeHint().width());
    m_plaetzeSpin->setToolTip(tr(
        "Work several callers at once (MSHV\u2019s Multi Answering).\n"
        "The transmit power is SHARED: 2 slots are \u22126 dB each, "
        "3 are \u22129.5, 6 are \u221215.6.\n1 means off."));
    m_plaetze = qBound(1, m_plaetzeSpin->value(), Ft8Mehrfach::kMaxPlaetze);
    m_mehrfach.setPlaetze(m_plaetze);
    m_freieStelleBtn = styledButton(tr("Free"), 46);
    m_freieStelleBtn->setCheckable(false);
    m_freieStelleBtn->setToolTip(tr(
        "Move the transmit tone to a spot where nobody was heard.\n"
        "Longpath knows this from its own decodes \u2014 WSJT-X and MSHV "
        "do not."));
    connect(m_freieStelleBtn, &QPushButton::clicked, this, [this]() {
        sucheFreieStelle(false);
    });

    connect(m_plaetzeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) {
        m_plaetze = v;
        m_mehrfach.setPlaetze(v);
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QString::number(v));
        if (m_status && v > 1) {
            // Die Zahl, die man wissen muss, bevor man sendet.
            const double dB = -10.0 * std::log10(double(v));
            m_status->setText(tr("%1 slots \u2014 %2 dB each")
                                  .arg(v).arg(dB, 0, 'f', 1));
        }
    });
    connect(m_cqVersucheSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("Ft8/CqVersuche"),
                                         QString::number(v));
    });
    sendRow->addWidget(m_cqVersucheSpin);
    sendRow->addWidget(m_plaetzeSpin);
    sendRow->addWidget(m_freieStelleBtn);

    m_cqBtn = blueToggle(tr("Send"), 56);
    m_cqBtn->setToolTip(tr("Send this message once, in the next own "
                           "period. No auto-sequencing."));
    connect(m_cqBtn, &QPushButton::clicked, this, [this]() {
        sendeNachricht(m_textFeld ? m_textFeld->text() : QString());
    });
    sendRow->addWidget(m_cqBtn);

    m_stopBtn = styledButton(tr("Stop"), 48);
    m_stopBtn->setCheckable(false);    // haelt an, rastet nicht ein
    m_stopBtn->setToolTip(tr("Stop transmitting and drop the QSO "
                             "sequence."));
    m_stopBtn->setEnabled(false);
    // „Stop" heisst Stop. abbrechen() allein nimmt nur das laufende
    // Fenster zurueck — der Ablauf blieb stehen und waere beim
    // naechsten passenden Fund von selbst weitergelaufen. Seit der
    // Doppelklick den Ablauf immer startet, ist das der einzige Weg
    // hinaus, und er muss halten.
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        // Stop heiszt Stop: auch der Wiederanlauf nach einem QSO --
        // und auch die Plaetze.
        //
        // `m_mehrfach.abbrechen()` fehlte (Durchsicht 2026-10-30).
        // Ohne das liefen die Plaetze weiter: `fensterZuEnde` sah
        // `aktive() > 0`, sendete und kam gar nicht erst zu dem
        // angehaltenen Einzelablauf. Ein Stop, der nicht stoppt, ist
        // schlimmer als kein Stop -- man drueckt ihn und glaubt, es
        // sei vorbei.
        m_cqAutomatik = false;
        m_mehrfach.abbrechen();
        m_ablauf.abbrechen();
        m_cqVersucheOffen = 0;
        abbrechen();
        zeigeAblauf();
    });
    sendRow->addWidget(m_stopBtn);

    root->addLayout(sendRow);

    // ── Keine Fusszeile mehr (30.10.) ──────────────────
    //
    // Dort stand „last period: 23", waehrend rechts neben den Pillen
    // „23 decodes" steht -- dieselbe Zahl, zweimal, am laufenden
    // Geraet nebeneinander im Bild. „Clear" gehoert ohnehin neben die
    // Liste, die es leert.
}

void Ft8Applet::setProtocol(Ft8Decoder::Protocol p)
{
    // ── Die gehoerten Tonlagen gelten nicht mehr (2026-10-30) ───
    //
    // Der Wechsel FT8 <-> FT4 zieht die Scheibe auf eine andere
    // Frequenz (7.074 -> 7.0475 MHz auf 40 m). Eine Tonlage ist aber
    // relativ zur Scheibe: was eben noch bei 1500 Hz lag, liegt jetzt
    // ganz woanders. Die Stellensuche wuerde sonst auf Zahlen
    // arbeiten, die nichts mehr bedeuten.
    m_toeneJePeriode.clear();

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

    // ── Mehrere Plaetze: jeder bekommt sein Fenster ──────────
    //
    // Dieser Block fehlte, bis er bei der Durchsicht am 2026-10-30
    // auffiel: eine Ersetzung war still fehlgeschlagen, und damit
    // drehte Multi Answering seine Plaetze NIE weiter und schrieb NIE
    // ein QSO ins Logbuch. Gesendet haette es, aber kein Zug waere je
    // vorangekommen.
    //
    // `fensterVerstrichen` reicht es an alle laufenden Ablaeufe weiter
    // und raeumt danach auf; `nimmFertige` liefert jedes fertige QSO
    // GENAU EINMAL -- darum geht es von hier direkt ins Logbuch und
    // nicht ueber `meldeFertigesQso`, das fuer den einzelnen Ablauf
    // gedacht ist und mit `m_qsoGemeldet` gegen Doppelmeldung sichert.
    if (mehrfachAn()) {
        m_mehrfach.fensterVerstrichen();
        for (const Ft8Ablauf& fertig : m_mehrfach.nimmFertige()) {
            emit qsoFertig(logEintrag(
                fertig, m_slice ? m_slice->frequency() : 0.0,
                m_slice && m_slice->dspMode() == DSPMode::DIGL,
                protocol(), eigenerLocator(), m_sendeTonHz));
            qCInfo(lcFt8Senden).noquote()
                << "QSO fertig (Platz):" << fertig.partner();
        }
        if (m_mehrfach.aktive() > 0) {
            // Den CQ-Ruf MITDREHEN, nicht einfrieren: er geht als
            // eigener Platz mit hinaus (siehe `sendeMehrfach`), also
            // muss auch sein Zaehler laufen. Sonst stuende er nach
            // zehn abgearbeiteten Anrufern immer noch bei Versuch 1
            // und riefe bis zum Abend.
            if (m_ablauf.laeuft()
                && m_ablauf.zustand() == Ft8Ablauf::Zustand::RufeCq) {
                if (m_cqVersucheOffen > 0) { --m_cqVersucheOffen; }
                m_ablauf.fensterVerstrichen();
                if (m_cqVersucheOffen == 0) { m_ablauf.abbrechen(); }
            }
            sendeMehrfach();
            return;
        }
        // Kein Platz mehr belegt: zurueck in den gewoehnlichen Weg,
        // damit ein laufender CQ-Ruf weitergeht.
    }
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
// ── Einen CQ-Ruf anfangen ───────────────────────────────
//
// Eine Stelle, nicht zwei: der Knopf ruft sie, und nach einem
// fertigen QSO ruft `zeigeAblauf` sie wieder. Als es zwei Kopien gab,
// hat eine davon `setzeCqWiederholungen` vergessen -- und der Ruf
// brach nach fuenf statt nach den eingestellten sechzehn ab
// (2026-10-29).
void Ft8Applet::starteCqRuf()
{
    // ── Nicht in ein laufendes QSO hineinrufen (2026-10-30) ─────
    //
    // Nur beim ANFANG eines Rufs, und nur wenn die Stelle wirklich
    // belegt ist. Mitten im QSO zu wechseln waere schlimmer als
    // jede belegte Stelle: wer antwortet, sucht einen dort, wo er
    // zuletzt war.
    sucheFreieStelle(true);

    m_qsoGemeldet = false;
    m_ablauf.starteAlsAnrufer(eigenesRufzeichen(), eigenerLocator());
    m_cqVersucheOffen = m_cqVersucheSpin ? m_cqVersucheSpin->value() : 1;

    // Dieselbe Zahl auch dem Ablauf sagen. Ohne diese Zeile zaehlte
    // er fuer sich bis fuenf und brach ab, waehrend hier noch elf
    // Versuche offen standen (gemessen am 2026-10-29, 12:47:12).
    m_ablauf.setzeCqWiederholungen(m_cqVersucheOffen);
    ablaufSchritt();
    zeigeKnopfzustand();
}

void Ft8Applet::ablaufSchritt()
{
    // Mit mehreren Plaetzen entscheidet `Ft8Mehrfach`, was hinausgeht
    // -- der einzelne Ablauf ist dann nur noch einer von mehreren.
    if (mehrfachAn() && m_mehrfach.aktive() > 0) {
        sendeMehrfach();
        return;
    }
    const QString was = m_ablauf.naechsteNachricht();
    zeigeAblauf();
    if (was.isEmpty() || m_sendet) { return; }
    if (m_textFeld) { m_textFeld->setText(was); }
    sendeNachricht(was);
}

// ── Wo gerade jemand liegt ───────────────────────────────────────────
QVector<int> Ft8Applet::belegteToene() const
{
    QVector<int> alle;
    for (const QVector<int>& periode : m_toeneJePeriode) {
        alle += periode;
    }
    return alle;
}

int Ft8Applet::tonUntergrenze() const
{
    // Ein Hound unter 1000 Hz wird nicht beantwortet -- dann darf die
    // Stellensuche ihn auch nicht dorthin schicken.
    return m_houndBetrieb ? Ft8Ablauf::kHoundMinTonHz : kSendeTonMinHz;
}

// ── Eine freie Sendestelle suchen ────────────────────────────────────
//
// Weder WSJT-X noch MSHV tun das. Longpath weiss aus den eigenen
// Funden, WO Stationen senden -- das ist eine bessere Auskunft als
// Energie im Wasserfall, denn sie sagt „hier sendet jemand" und nicht
// „hier ist Rauschen".
bool Ft8Applet::sucheFreieStelle(bool nurWennBelegt)
{
    if (!m_tonFeld) { return false; }
    const QVector<int> belegt = belegteToene();
    const int von  = tonUntergrenze();
    const int bis  = kSendeTonMaxHz;
    const int alt  = m_tonFeld->value();

    if (belegt.isEmpty()) {
        // Nichts gehoert, nichts zu wissen. Abwesenheit ist keine
        // Aussage: wer hier verschoebe, zoege eine Station von einer
        // vielleicht guten Stelle weg, weil der Empfang gerade
        // schweigt.
        if (m_status && !nurWennBelegt) {
            m_status->setText(tr("nothing heard yet — tone unchanged"));
        }
        return false;
    }

    const int abstand = int(Ft8TxSource::kPlatzAbstandHz);
    if (nurWennBelegt && !stelleBelegt(belegt, alt, abstand)) {
        return false;
    }

    const int neu = freieTonlage(belegt, von, bis, abstand, alt);
    if (neu == alt) {
        if (m_status) {
            m_status->setText(stelleBelegt(belegt, alt, abstand)
                                  ? tr("no free slot — tone unchanged")
                                  : tr("%1 Hz is clear").arg(alt));
        }
        return false;
    }

    m_tonFeld->setValue(neu);
    qCInfo(lcFt8Senden).nospace()
        << "Freie Stelle gesucht: " << alt << " war belegt, jetzt "
        << neu << " Hz (" << belegt.size() << " Stationen gehoert)";
    if (m_status) {
        m_status->setText(tr("tone %1 \u2192 %2 Hz (was busy)")
                              .arg(alt).arg(neu));
    }
    return true;
}

// ── Ein Fenster, mehrere Stationen ─────────────────────
void Ft8Applet::sendeMehrfach()
{
    if (m_sendet) { return; }
    m_mehrfach.setEigeneDaten(eigenesRufzeichen(), eigenerLocator());
    m_mehrfach.setGrundtonHz(float(m_sendeTonHz));
    QVector<Ft8TxSource::Platz> plaetze = m_mehrfach.naechsteNachrichten();

    // ── Der CQ-Ruf ist ein Platz, kein zweiter Ablauf ─────────
    //
    // Bei der Durchsicht am 2026-10-30 gefunden: mit Plaetzen liefen
    // ZWEI Zustandsmaschinen nebeneinander. Der CQ-Knopf startet
    // `m_ablauf` als Anrufer; antwortet dann jemand, nimmt ihn
    // `m_mehrfach` auf -- und von da an bediente `fensterZuEnde` nur
    // noch die Plaetze. Rufen UND Abarbeiten gleichzeitig ist aber
    // genau der Sinn der Sache; MSHV haelt dafuer einen Platz frei.
    //
    // Solange der Ruf laeuft und ein Platz frei ist, geht er als
    // eigener Platz mit hinaus. Ist alles belegt, schweigt das CQ --
    // wer sechs Stationen gleichzeitig abarbeitet, braucht keine
    // siebte.
    if (m_ablauf.laeuft()
        && m_ablauf.zustand() == Ft8Ablauf::Zustand::RufeCq
        && plaetze.size() < m_plaetze) {
        const QString cq = m_ablauf.naechsteNachricht();
        if (!cq.isEmpty()) {
            // Die erste Tonlage der Leiter, die KEIN Platz benutzt.
            //
            // Nicht `toene[plaetze.size()]`: `Ft8Mehrfach` vergibt
            // seine Toene nach dem Index im eigenen Feld, und das
            // enthaelt nur die aktiven Ablaeufe. Faellt einer von
            // ihnen mit leerer Nachricht heraus, stimmt die Zahl der
            // Eintraege nicht mehr mit den vergebenen Indizes
            // ueberein -- und das CQ laege auf derselben Tonlage wie
            // eine laufende Station. Zwei Nachrichten auf einem Ton
            // ueberlagern sich zu Matsch, und beide sind weg.
            const QVector<float> toene =
                Ft8TxSource::tonleiter(float(m_sendeTonHz),
                                       Ft8Mehrfach::kMaxPlaetze);
            float frei = 0.0f;
            for (const float t : toene) {
                bool belegtVonPlatz = false;
                for (const Ft8TxSource::Platz& p : plaetze) {
                    if (qFuzzyCompare(p.toneHz + 1.0f, t + 1.0f)) {
                        belegtVonPlatz = true;
                        break;
                    }
                }
                if (!belegtVonPlatz) { frei = t; break; }
            }
            if (frei > 0.0f) {
                Ft8TxSource::Platz p;
                p.text   = cq;
                p.toneHz = frei;
                plaetze.push_back(p);
            }
        }
    }

    if (plaetze.isEmpty()) { return; }

    QStringList zeilen;
    for (const Ft8TxSource::Platz& p : plaetze) { zeilen << p.text; }
    const QString anzeige = zeilen.join(QStringLiteral(" | "));
    if (m_textFeld) { m_textFeld->setText(anzeige); }
    zeigeArbeit();
    sendeNachricht(anzeige, plaetze);
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
    zeigeKnopfzustand();
    zeigeArbeit();
    if (!m_status) { return; }
    switch (m_ablauf.zustand()) {
        case Ft8Ablauf::Zustand::Fertig: {
            const QString wer = m_ablauf.partner();
            m_status->setText(tr("QSO with %1 complete").arg(wer));
            meldeFertigesQso();

            // ── Und weiter, ohne Klick ─────────────────────
            //
            // Wer den CQ-Knopf gedrueckt hat, will rufen -- nicht ein
            // QSO fuehren und dann warten. Bis zum 2026-10-30 war hier
            // Schluss: nach dem RR73 blieb es still, bis der Betreiber
            // wieder drueckte. Betreiber: „Der Durchlauf muss
            // automatisch ablaufen, so schnell wie moeglich ohne mein
            // klicken bei einem QSO."
            //
            // Nur wenn der Ruf vom Knopf kam. Ein Doppelklick in die
            // Liste ist eine Einzelentscheidung: danach wird nicht von
            // selbst CQ gerufen.
            if (m_cqAutomatik && !m_sendet) {
                qCInfo(lcFt8Senden).noquote()
                    << "QSO mit" << wer << "fertig — rufe weiter CQ";
                starteCqRuf();
            }
            break;
        }
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
                // Eine Einzelentscheidung: danach nicht von selbst
                // weiter CQ rufen.
                m_cqAutomatik = false;
                if (m_houndBetrieb) {
                    // Im Fox-Betrieb ruft man die Expedition; den
                    // Schluss macht sie. Siehe `starteAlsHound`.
                    m_ablauf.starteAlsHound(senderCallsign(nachricht),
                                            eigenesRufzeichen(),
                                            eigenerLocator());
                    ablaufSchritt();
                    return true;
                }
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
    sendeNachricht(text, {});
}

void Ft8Applet::sendeNachricht(const QString& text,
                               const QVector<Ft8TxSource::Platz>& plaetze)
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

    // ── Ein Fenster, mehrere Stationen (Multi Answering) ───────
    //
    // Mit mehreren Plaetzen geht nicht EIN Satz hinaus, sondern die
    // Summe mehrerer -- jeder auf seiner eigenen Tonlage. Der Rest
    // des Sendewegs (Sendeplan, Taktgeber, Tasten) ist identisch:
    // darum steht hier nur das Packen anders, nicht der ganze Weg
    // noch einmal.
    if (!plaetze.isEmpty()) {
        if (!quelle->vorbereitenMehrere(plaetze, protocol())) {
            qCWarning(lcFt8Senden)
                << "Senden abgelehnt: ft8_lib kann die" << plaetze.size()
                << "Plaetze nicht packen";
            if (m_status) { m_status->setText(tr("cannot encode slots")); }
            return;
        }
        QStringList was;
        for (const Ft8TxSource::Platz& p : plaetze) {
            was << QStringLiteral("%1@%2").arg(p.text)
                       .arg(int(p.toneHz));
        }
        qCInfo(lcFt8Senden).noquote()
            << "Senden vorbereitet (" << plaetze.size() << "Plaetze):"
            << was.join(QStringLiteral(" | "));
    } else {
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
    }

    // Wann faengt mein naechstes Fenster an? Die Gegenperiode zu der,
    // in der gehoert wird — wer in derselben sendet, hoert sich selbst
    // und niemand sonst.
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    const Ft8Tx::Sendeplan plan =
        Ft8Tx::sendeplan(protocol(), kDekoderFristMs,
                         kZielVersatzMs, kSenderNachlaufMs);
    const qint64 wartet = Ft8Tx::bisZumNaechsten(jetzt, protocol(),
                                                 m_periode)
                          + plan.tastenNachMs;
    m_sendet = true;
    if (m_cqBtn)   { m_cqBtn->setEnabled(false); }
    if (m_stopBtn) { m_stopBtn->setEnabled(true); }
    zeigeKnopfzustand();
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
    QString gesendet = sauber;
    const bool ausAblauf = m_ablauf.laeuft();
    connect(m_sendeTimer, &QTimer::timeout, this,
            [this, gesendet, ausAblauf, plan]() mutable {
        if (!m_sendet || !m_model) { return; }
        Ft8TxSource* q = m_model->ft8TxSource();
        if (!q) {
            qCWarning(lcFt8Senden) << "Tonquelle zwischendurch verschwunden";
            abbrechen();
            return;
        }

        // ── Den Zug nachsehen, bevor er hinausgeht ─────────────
        //
        // Die Nachricht wurde beim Ende der VORIGEN Sendung gepackt,
        // also knapp 13 s vor diesem Augenblick. In der Zwischenzeit
        // hat die Gegenstation geantwortet und der Dekoder ist fertig
        // geworden — nur kam das bisher zu spaet: `ablaufSchritt()`
        // kehrt bei laufender Sendung um, und der neue Zug musste ein
        // ganzes Fenster warten. Jeder Zug kostete 30 s statt 15.
        //
        // Darum wird hier, nach der Dekoderfrist, nochmal gefragt.
        // Nur bei einer Sendung aus dem Ablauf: wer Tx1…Tx6 oder
        // „Send" drueckt, will genau das senden, was dort steht.
        if (ausAblauf && m_ablauf.laeuft()) {
            const QString neu = m_ablauf.naechsteNachricht();
            if (!neu.isEmpty() && neu != gesendet) {
                if (q->vorbereiten(neu, float(m_sendeTonHz), protocol())) {
                    qCInfo(lcFt8Senden).noquote()
                        << "Zug in der Frist gewechselt:" << gesendet
                        << "->" << neu;
                    gesendet = neu;
                    if (m_textFeld) { m_textFeld->setText(neu); }
                } else {
                    qCWarning(lcFt8Senden).noquote()
                        << "Neuer Zug \"" + neu + "\" liess sich nicht "
                           "packen — es geht der alte hinaus";
                }
            }
        }

        // Erst den Weg legen, dann tasten: andersherum ginge der
        // Sender mit dem Mikrofon auf, nicht mit dem Ton. Ohne Router
        // (Flex) entfaellt das Umlegen — dort holt die Sendepumpe die
        // Proben direkt aus der Quelle.
        if (auto* router = m_model->micRouter()) {
            m_quelleVorher = static_cast<int>(router->activeSource());
            router->setActiveSource(MicSource::Ft8);
        }
        // Ab der uebersprungenen Stille: so bleibt der Zeitpunkt des
        // ersten Tons derselbe, obwohl wir spaeter tasten.
        q->starten(plan.probenAbMs);

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
        // ── So lange, wie das Signal WIRKLICH braucht ─────────
        //
        // Hier stand `signalMs + 300`, gerechnet ab der
        // Fenstergrenze. Das Signal beginnt aber erst nach der
        // fuehrenden Stille der Wellenform. Am Geraet gemessen
        // (2026-10-30, 07:09:30,003 getastet, erster Ton 07:09:31,185,
        // geloest 07:09:42,945): die letzten 880 ms fehlten, also 5,5
        // von 79 Symbolen. `sendeplan()` rechnet die Dauer jetzt vom
        // Zielversatz aus — der Nachlauf von 300 ms bleibt, damit der
        // letzte Ton wirklich draussen ist.
        schluss->start(static_cast<int>(plan.tastdauerMs));
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
    zeigeKnopfzustand();
    updateStatus();
}

// ── Was die Knoepfe zeigen ───────────────────────────────────────────
//
// Betreiber am 2026-10-29: „der button sollte sich auch farblich
// aendern, wenn er geklickt ist."
//
// Nicht „geklickt" ist hier das Richtige, sondern „laeuft". Ein
// einrastbarer Knopf kippt beim Klick von selbst um und bliebe danach
// stehen, auch wenn der Ruf laengst aufgegeben hat — er zeigte dann,
// was zuletzt gedrueckt wurde, statt was gerade geschieht. Also wird
// der Zustand hier aus dem Ablauf gelesen und gesetzt.
//
// Blau ist im Hausstil das Anfassbare und das Aktive; die Knoepfe
// nehmen dieselbe Fuellung wie FT8/FT4 oder die Bandwahl darueber.
void Ft8Applet::zeigeKnopfzustand()
{
    const bool ruftCq = m_ablauf.laeuft();
    if (m_cqRufenBtn) { m_cqRufenBtn->setChecked(ruftCq); }
    // „Send" leuchtet nur beim Einzelschuss: waehrend eines Ablaufs
    // ist er ohnehin gesperrt, und zwei leuchtende Knoepfe waeren eine
    // Frage statt einer Antwort.
    if (m_cqBtn) { m_cqBtn->setChecked(m_sendet && !ruftCq); }
}

void Ft8Applet::updateStatus()
{
    if (!m_status || !m_stream) { return; }

    const int imFenster = m_stream->bufferedSamples();
    const double sekunden = static_cast<double>(imFenster)
                            / Ft8Decoder::kSampleRate;
    const double voll = m_stream->slotMs() / 1000.0;

    // ── Die Uhr steht unten, nicht oben (2026-10-30) ────────
    //
    // Hier stand jede Sekunde „0.0 / 15.0 s" — und ueberschrieb
    // damit jede Meldung, die der Ablauf gerade hinterlassen hatte.
    // Seit Entwurf C traegt die Arbeitsleiste die Uhr; oben bleibt
    // stehen, WAS gerade los ist. Ohne laufenden Ablauf (und damit
    // ohne Arbeitsleiste) bleibt es bei der Uhr: eine leere Zeile
    // waere schlechter als eine, die wenigstens den Takt zeigt.
    // Die Uhr steht in der Arbeitsleiste, und nur dort. Am laufenden
    // Geraet stand sie zweimal im Bild: oben im Panelkopf und unten.
    // Oben bleibt, WAS zuletzt los war -- im Leerlauf, wie viele Funde
    // die letzte Periode brachte.
    if (!m_ablauf.laeuft() && !m_sendet) {
        m_status->setText(tr("listening"));
    }

    // Die Arbeitsleiste laeuft mit derselben Uhr: sie zeigt denselben
    // Stand, nur dort, wo man hinsieht.
    if (m_arbeitBalken) {
        m_arbeitBalken->setzeStand(voll > 0.0 ? sekunden / voll : 0.0,
                                   m_sendet);
    }
    if (m_arbeitUhr) {
        m_arbeitUhr->setText(QStringLiteral("%1 / %2 s")
                                 .arg(sekunden, 0, 'f', 1)
                                 .arg(voll, 0, 'f', 1));
    }
}

// ── Die Arbeitsleiste nachziehen ─────────────────────────────────────
//
// Sie sagt dasselbe wie die Statuszeile, aber als Satz, den man im
// Vorbeigehen liest: mit wem, welcher Zug, was danach kommt. Gerufen
// aus `zeigeAblauf()` -- also bei jedem Zustandswechsel des Ablaufs.
void Ft8Applet::zeigeArbeit()
{
    if (!m_arbeit) { return; }

    using Z = Ft8Ablauf::Zustand;
    const Z z        = m_ablauf.zustand();
    const QString wer = m_ablauf.partner();
    const bool laeuft = m_ablauf.laeuft();

    QString zustand;
    QString danach;
    switch (z) {
    case Z::Ruht:
        zustand = tr("idle");
        danach  = tr("press CQ, or double-click a line");
        break;
    case Z::RufeCq:
        zustand = m_cqVersucheOffen > 0
                      ? tr("calling CQ · %1 left").arg(m_cqVersucheOffen)
                      : tr("calling CQ");
        danach  = tr("waiting for an answer");
        break;
    case Z::RufeAn:
        zustand = tr("calling");
        danach  = tr("waiting for his report");
        break;
    case Z::SendeReport:
        zustand = tr("QSO with");
        danach  = tr("then his confirmation");
        break;
    case Z::SendeRReport:
        zustand = tr("QSO with");
        danach  = tr("then his RR73");
        break;
    case Z::SendeRR73:
    case Z::Sende73:
        zustand = tr("QSO with");
        danach  = m_cqAutomatik ? tr("then the log, then CQ again")
                                : tr("then into the log");
        break;
    case Z::Fertig:
        zustand = tr("complete");
        danach  = m_cqAutomatik ? tr("logged · calling CQ again")
                                : tr("logged");
        break;
    case Z::Abgebrochen:
        zustand = wer.isEmpty() ? tr("stopped") : tr("gave up");
        danach  = QString();
        break;
    }

    if (m_arbeitZustand) { m_arbeitZustand->setText(zustand); }
    if (m_arbeitPartner) { m_arbeitPartner->setText(wer); }
    if (m_arbeitLand) {
        m_arbeitLand->setText(wer.isEmpty() ? QString() : landFuer(wer));
    }
    if (m_arbeitDanach) { m_arbeitDanach->setText(danach); }
    if (m_arbeitZug) {
        // Was wirklich hinausgeht: bei laufender Sendung steht es
        // schon im Feld, sonst fragt der Ablauf.
        const QString zug = laeuft ? m_ablauf.naechsteNachricht()
                                   : QString();
        m_arbeitZug->setText(zug.isEmpty() ? tr("—") : zug);
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
    zeigeFundZahl();
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
        const int snr = static_cast<int>(std::lround(d.snrDb));

        // ── Multi Answering (2026-10-30 angeschlossen) ─────────
        //
        // Mit mehreren Plaetzen fuehrt `Ft8Mehrfach` die Ablaeufe, und
        // jeder gehoerte Satz geht an ALLE -- jeder prueft selbst, ob
        // er ihn angeht. Ruft mich jemand, der noch nicht dabei ist,
        // bekommt er einen freien Platz oder stellt sich an.
        if (mehrfachAn()) {
            m_mehrfach.setEigeneDaten(eigenesRufzeichen(), eigenerLocator());
            bool bewegt = m_mehrfach.verarbeite(d.text, snr) > 0;
            const QString anrufer = senderCallsign(d.text);
            const QString meiner  = eigenesRufzeichen();
            const bool gehtAnMich =
                !meiner.isEmpty()
                && d.text.toUpper()
                       .split(QLatin1Char(' '), Qt::SkipEmptyParts)
                       .contains(meiner);
            if (gehtAnMich && !anrufer.isEmpty()
                && !m_mehrfach.kennt(anrufer)) {
                bewegt = m_mehrfach.nimmAnrufer(
                             anrufer, snr,
                             locatorAusNachricht(d.text)) || bewegt;
            }
            if (bewegt) { ablaufSchritt(); }
        } else if (m_ablauf.laeuft() && m_ablauf.verarbeite(d.text, snr)) {
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

    // Wo in dieser Periode jemand lag -- fuer die Stellensuche.
    QVector<int> dieserPeriode;
    dieserPeriode.reserve(funde.size());
    for (const Ft8Decode& d : funde) {
        dieserPeriode.append(static_cast<int>(std::lround(d.freqHz)));
    }
    m_toeneJePeriode.append(dieserPeriode);
    while (m_toeneJePeriode.size() > kPeriodenImGedaechtnis) {
        m_toeneJePeriode.removeFirst();
    }
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
    if (nachricht.trimmed().isEmpty()) { return; }

    Fundzeile z;
    z.text = QStringLiteral("%1 %2 %3 %4  %5")
                 .arg(QDateTime::currentDateTimeUtc()
                          .toString(QStringLiteral("hhmmss")))
                 .arg(tr("Tx"), 4)
                 .arg(QString(), 5)
                 .arg(m_sendeTonHz, 5)
                 .arg(nachricht.trimmed());
    z.nachricht     = nachricht.trimmed();
    z.tonHz         = m_sendeTonHz;
    z.eigeneSendung = true;
    merkeZeile(z);
}

// ── Eine Zeile merken und, wenn sie in die Sicht gehoert, zeigen ────
void Ft8Applet::merkeZeile(const Fundzeile& z)
{
    m_zeilen.append(z);
    if (m_zeilen.size() > kMaxZeilen) {
        m_zeilen.remove(0, m_zeilen.size() - kMaxZeilen);
        // Beim Abschneiden muss die ganze Liste neu, sonst stuenden
        // Zeilen darin, die es im Speicher nicht mehr gibt.
        zeichneListe();
        return;
    }
    if (inSicht(z)) {
        haengeAn(z);
        ++m_lineCount;
        if (m_list) {
            m_list->moveCursor(QTextCursor::End);
            m_list->ensureCursorVisible();
        }
    }
    zeigeFundZahl();
}

bool Ft8Applet::inSicht(const Fundzeile& z) const
{
    // Die eigene Sendung steht ueberall: sie IST der Zusammenhang,
    // in dem die Antwort gelesen wird.
    if (z.eigeneSendung) { return true; }
    switch (m_sicht) {
    case Sicht::Alles:  break;
    case Sicht::AnMich: return z.anMich;
    case Sicht::Cq:     return z.istCq || z.anMich;
    case Sicht::NeuesLand: return z.neuesLand || z.anMich;
    case Sicht::MeineFrequenz:
        // Dieselbe Regel wie frueher die rechte Spalte: das Fenster
        // ist die Signalbreite von FT8/FT4 (50 Hz), kein gegriffener
        // Wert. Schmaler, und ein Fund am Rand des eigenen Signals
        // fehlt; breiter, und es fuellt sich mit Nachbarn.
        return z.anMich || std::abs(z.tonHz - m_sendeTonHz) <= kRxFensterHz;
    }
    // „Alles" heiszt alles. Bis zum 2026-10-30 lagen hier noch zwei
    // Haken aus der Kopfzeile darueber, und zwei Filtersysteme
    // nebeneinander ergaben Faelle, die niemand im Kopf behaelt.
    //
    // Die Regel, die bleibt: eine Zeile, die an MICH geht, kommt
    // durch jede Sicht. Wer gerufen wird und es nicht sieht, weil ein
    // Filter sitzt, verliert ein QSO -- das ist kein Filter mehr,
    // sondern ein Fehler. Darum steht `|| z.anMich` in jedem Fall
    // oben.
    return true;
}

void Ft8Applet::haengeAn(const Fundzeile& z)
{
    if (!m_list) { return; }
    QTextCharFormat f;
    if (z.eigeneSendung) {
        // Die Farbe ist das Messing der Sendetaste, nicht Rot: „kein
        // kraeftiges Rot als Zustandsfarbe" gilt hier wie dort.
        f.setForeground(QColor(QString::fromLatin1(Style::kTxKeyBorder)));
        f.setFontWeight(QFont::Bold);
    } else {
        const Zeilenart art = zeilenart(z.anMich, z.neuesLand, z.istCq);
        if (art == Zeilenart::AnMich) {
            f.setForeground(QColor(QString::fromLatin1(Style::kGreenText)));
            f.setFontWeight(QFont::Bold);
        } else if (art == Zeilenart::NeuesLand) {
            // Fett wie „an mich": ein neues Land uebersieht man sonst
            // in zwanzig Zeilen je Fenster.
            f.setForeground(z.landFarbe.isValid()
                                ? z.landFarbe
                                : QColor(QString::fromLatin1(
                                      Style::kAmberText)));
            f.setFontWeight(QFont::Bold);
        } else if (art == Zeilenart::Cq) {
            f.setForeground(QColor(QString::fromLatin1(Style::kAmberText)));
        } else {
            f.setForeground(QColor(QString::fromLatin1(Style::kTextPrimary)));
        }
    }
    QTextCursor c(m_list->document());
    c.movePosition(QTextCursor::End);
    if (!m_list->document()->isEmpty()) { c.insertBlock(); }
    c.insertText(z.text, f);
    // Ans Ende der Zeile zu schreiben schiebt die Ansicht mit -- dann
    // steht die Uhrzeit links ausserhalb des Fensters, und man sieht
    // die Spalte, die man am wenigsten braucht. Gesehen im
    // gerenderten Blatt am 2026-10-21.
}

// Die ganze Liste aus dem Speicher. Laeuft bei jedem Pillendruck --
// vierhundert Zeilen sind dafuer schnell genug, und alles andere
// hiesse, zwei Wahrheiten zu fuehren.
void Ft8Applet::zeichneListe()
{
    if (m_sichtAlles)  { m_sichtAlles->setChecked(m_sicht == Sicht::Alles); }
    if (m_sichtAnMich) { m_sichtAnMich->setChecked(m_sicht == Sicht::AnMich); }
    if (m_sichtCq)     { m_sichtCq->setChecked(m_sicht == Sicht::Cq); }
    if (m_sichtNeuesLand) { m_sichtNeuesLand->setChecked(
                                m_sicht == Sicht::NeuesLand); }
    if (m_sichtFreq)   { m_sichtFreq->setChecked(m_sicht
                                                 == Sicht::MeineFrequenz); }
    if (!m_list) { return; }
    m_list->clear();
    m_lineCount = 0;
    for (const Fundzeile& z : m_zeilen) {
        if (!inSicht(z)) { continue; }
        haengeAn(z);
        ++m_lineCount;
    }
    m_list->moveCursor(QTextCursor::End);
    m_list->ensureCursorVisible();
    zeigeFundZahl();
}

void Ft8Applet::zeigeFundZahl()
{
    if (!m_fundZahl) { return; }
    // Zwei Zahlen, und sie sind NICHT dasselbe: wie viel in der Liste
    // steht, und was die letzte Periode gebracht hat. Beim Aufraeumen
    // am 2026-10-30 habe ich sie erst fuer eine Dopplung gehalten --
    // sie standen im Bild des Betreibers nur zufaellig gleich, weil
    // die Liste frisch war. Jetzt stehen sie nebeneinander, statt an
    // zwei Enden des Panels.
    const QString liste = m_sicht == Sicht::Alles
                              ? tr("%1 decodes").arg(m_zeilen.size())
                              : tr("%1 of %2").arg(m_lineCount)
                                    .arg(m_zeilen.size());
    m_fundZahl->setText(m_lastPeriodCount < 0
                            ? liste
                            : QStringLiteral("%1 \u00b7 %2")
                                  .arg(liste,
                                       tr("%1 last").arg(m_lastPeriodCount)));
}

void Ft8Applet::zeigeZeile(const QString& zeile, const QString& nachricht,
                           int tonHz)
{
    Fundzeile z;
    z.text      = zeile;
    z.nachricht = nachricht;
    z.tonHz     = tonHz;

    const QString ruf = eigenesRufzeichen();
    // Wortgrenzen, nicht contains(): „OE5SOS" steckt auch in
    // „OE5SOSX", und das ist eine andere Station.
    z.anMich =
        !ruf.isEmpty()
        && nachricht.toUpper().split(QLatin1Char(' '),
                                     Qt::SkipEmptyParts).contains(ruf);
    z.istCq = nachricht.trimmed().toUpper().startsWith(
        QStringLiteral("CQ"));

    // Ein Land, das noch nie im Logbuch stand? Der Aufloeser steht
    // schon im Modell -- derselbe, der die Funde im Panadapter
    // einfaerbt. Also DIESELBE Palette, keine zweite erfunden.
    //
    // Steht hier und nicht in `inSicht`: die Abfrage kostet, und mit
    // dem Zeilenspeicher wuerde sie sonst bei jedem Pillendruck fuer
    // vierhundert Zeilen neu laufen.
    const QString senderRuf = senderCallsign(nachricht);
    if (m_model && m_model->dxccColorProvider() && !senderRuf.isEmpty()) {
        auto* dxcc = m_model->dxccColorProvider();
        if (dxcc->isEnabled()) {
            const double mhz = (m_slice ? m_slice->frequency() : 0.0) / 1.0e6;
            const QString modus = protocol() == Ft8Decoder::Protocol::Ft4
                                      ? QStringLiteral("FT4")
                                      : QStringLiteral("FT8");
            z.neuesLand = dxcc->statusForSpot(senderRuf, mhz, modus)
                          == DxccStatus::NewDxcc;
            if (z.neuesLand) {
                z.landFarbe = dxcc->colorForSpot(senderRuf, mhz, modus);
            }
        }
    }

    // Gemerkt wird JEDE Zeile, gezeigt nur die, die in die Sicht
    // gehoert. Bis zum 2026-10-30 wurde hier gefiltert und verworfen
    // -- wer „CQ only" einschaltete, sah das Band unveraendert, bis
    // die naechste Periode durch war, und beim Ausschalten kam das
    // Verworfene nie zurueck.
    merkeZeile(z);
}

} // namespace Longpath
