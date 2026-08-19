// =================================================================
// src/gui/widgets/RotorLogbookPanel.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see RotorLogbookPanel.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — workSpot() entry point for "Turn rotor to <call>" from
//                 the Spot Hub. Enter in the callsign field now only
//                 looks the station up (it used to also start the
//                 rotator — moving the mast is not what Enter means).
//                 The QRZ locator is shown on the station card instead
//                 of only being written silently into the DX field.
//                 AI-assisted via Anthropic Claude (Cowork), operator
//                 Martin Fischer.
// =================================================================

#include "RotorLogbookPanel.h"
#include "GlobeWidget.h"
#include "RotorDialWidget.h"

#include "core/AdifLog.h"
#include "core/AppSettings.h"
#include "core/CtyDatParser.h"
#include "core/DxccColorProvider.h"
#include "core/DxccFlag.h"
#include "core/HamlibInstaller.h"
#include "core/Maidenhead.h"
#include "core/QrzClient.h"
#include "core/QrzLogbookUploader.h"
#include "core/RotctldClient.h"
#include "core/RotctldProcess.h"
#include "core/RotorModels.h"
#include "core/SolarTimes.h"
#include "gui/LogbookWindow.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/StationPhoto.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QAction>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMargins>
#include <QResizeEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QMenu>
#include <cmath>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QClipboard>
#include <QGuiApplication>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QTimer>
#include <QRegularExpression>
#include <QUrl>
#ifdef HAVE_SERIALPORT
#include <QSerialPortInfo>
#endif

#include <algorithm>

namespace NereusSDR {

namespace {

const QString kMyGridKey    = QStringLiteral("StationGridSquare");
const QString kWorldImgKey  = QStringLiteral("GlobeWorldImagePath");
const QString kShowPhotoKey = QStringLiteral("RotorLogShowQrzPhoto");
const QString kRotorHostKey = QStringLiteral("RotorRotctldHost");
const QString kRotorPortKey = QStringLiteral("RotorRotctldPort");
// Teachable presets + park (2026-08-11). %1 is the slot number 1..4.
// Empty Deg value = slot not taught yet. Client-authoritative, so
// AppSettings per the settings policy.
const QString kPresetLabelKey = QStringLiteral("RotorPreset%1Label");
const QString kPresetDegKey   = QStringLiteral("RotorPreset%1Deg");
const QString kParkDegKey     = QStringLiteral("RotorParkDeg");
const QString kRotorUseSerialKey = QStringLiteral("RotorUseLocalSerial");
const QString kRotorModelKey     = QStringLiteral("RotorHamlibModel");
const QString kRotorDeviceKey    = QStringLiteral("RotorSerialDevice");
const QString kRotorBaudKey      = QStringLiteral("RotorSerialBaud");
// Mechanical end stop, degrees; negative = turns freely. Drives the
// dial's travel-path maths so the shown direction is the one the mast
// can actually take.
const QString kRotorEndStopKey   = QStringLiteral("RotorEndStopDeg");
const QString kAutoLookupKey = QStringLiteral("QrzAutoLookupWhileTyping");

// Long enough that typing a callsign is one request, short enough that
// the answer is there before the operator reaches for the log button.
constexpr int kAutoLookupDelayMs = 700;

// Below this, a partial callsign matches the pattern often enough to
// waste a lookup: "OE1" is a legal prefix and not a station.
constexpr int kMinAutoLookupChars = 4;

// NASA Blue Marble. Works of the US government are public domain, so
// this can be fetched and kept without anyone agreeing to anything —
// which is why it is offered as a download instead of being bundled:
// the file is larger than the rest of the application's assets put
// together, and the globe is one optional view.
//
// Each entry is a list of candidates tried in order. NASA reorganised
// its image sites and the deep paths do move; one dead link should be
// a pause of a second, not a feature that does not work. My first
// attempt at the large one 404'd because the Blue Marble Next
// Generation naming carries a band count — "200412.3x5400x2700", not
// "200412x5400x2700" — which is exactly the kind of detail a fallback
// list exists to absorb.
const QStringList& smallCandidates()
{
    static const QStringList l = {
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "57000/57752/land_shallow_topo_2048.jpg"),
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "57000/57752/land_ocean_ice_2048.jpg"),
    };
    return l;
}

const QStringList& largeCandidates()
{
    static const QStringList l = {
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "73000/73909/world.topo.bathy.200412.3x5400x2700.jpg"),
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "73000/73776/world.topo.bathy.200408.3x5400x2700.jpg"),
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "74000/74117/world.200412.3x5400x2700.jpg"),
    };
    return l;
}

// The portrait cache used to be described here. It moved to
// StationPhoto::cacheDir() along with the rest of the fetching rules
// (2026-08-10) — the logbook detail pane needs the same directory, and
// two functions deciding where the cache lives is one too many.

// A row that can be hidden when the panel is dragged small.
//
// QLayout has no setVisible(), so every row that might be shed needs a
// widget of its own to hide. Zero margins so wrapping changes nothing
// about how the row looks while it is up. (2026-08-10)
QWidget* addShedRow(QVBoxLayout* col, QLayout* row, QWidget* parent)
{
    auto* w = new QWidget(parent);
    row->setContentsMargins(0, 0, 0, 0);
    w->setLayout(row);
    col->addWidget(w);
    return w;
}

QLabel* caption(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    QFont f = l->font();
    f.setPixelSize(9);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                         .arg(Style::kTextScale));
    return l;
}

QString compassPoint(double deg)
{
    static const char* kPoints[] = {"N", "NNE", "NE", "ENE", "E", "ESE",
                                    "SE", "SSE", "S", "SSW", "SW", "WSW",
                                    "W", "WNW", "NW", "NNW"};
    int idx = static_cast<int>(deg / 22.5 + 0.5) % 16;
    if (idx < 0) { idx += 16; }
    return QString::fromLatin1(kPoints[idx]);
}

} // namespace

RotorLogbookPanel::RotorLogbookPanel(RadioModel* radio, QrzClient* qrz,
                                     QrzLogbookUploader* uploader,
                                     QWidget* parent)
    : QWidget(parent), m_radio(radio), m_qrz(qrz), m_uploader(uploader)
{
    buildUi();
    wireQrz();
    applyLocators();
    refreshRecentList();

    // The sun keeps moving while you are in QSO. A minute is finer than
    // the model's own accuracy needs, but coarse enough to be free.
    auto* solarTick = new QTimer(this);
    solarTick->setInterval(60 * 1000);
    connect(solarTick, &QTimer::timeout,
            this, &RotorLogbookPanel::updateSolarLine);
    solarTick->start();
}

QString RotorLogbookPanel::logbookPath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/logbook.adi");
}

// ── UI ──────────────────────────────────────────────────────────────

void RotorLogbookPanel::buildUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("background: %1;").arg(Style::kAppBg));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 8, 10, 8);
    col->setSpacing(7);

    // Callsign
    auto* callRow = new QHBoxLayout;
    callRow->setSpacing(6);
    m_callEdit = new QLineEdit(this);
    m_callEdit->setPlaceholderText(QStringLiteral("callsign"));
    QFont cf = m_callEdit->font();
    cf.setPixelSize(16);
    cf.setBold(true);
    m_callEdit->setFont(cf);
    m_callEdit->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    callRow->addWidget(m_callEdit, 1);

    m_lookupBtn = new QPushButton(QStringLiteral("QRZ"), this);
    m_lookupBtn->setStyleSheet(Style::buttonBaseStyle());
    callRow->addWidget(m_lookupBtn);

    // A second Log button, here in the callsign row. The one further
    // down is where it belongs when you are filling in reports and a
    // comment; this one is for the other case, which is most of them:
    // type a callsign, watch the name and locator arrive, click. Two
    // buttons for one action is usually a smell, but the distance
    // between the callsign field and the bottom of the panel is the
    // whole cost of logging a contact when you are working a run.
    auto* quickLogBtn = new QPushButton(QStringLiteral("LOG"), this);
    quickLogBtn->setStyleSheet(Style::buttonBaseStyle()
                               + Style::greenCheckedStyle());
    quickLogBtn->setToolTip(QStringLiteral(
        "Log this contact now, with whatever is filled in"));
    callRow->addWidget(quickLogBtn);
    connect(quickLogBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLogQso);

    m_rowCall = addShedRow(col, callRow, this);

    // Station card: portrait, flag, name line. The portrait is on the
    // left and fixed-width so the text below never reflows as images of
    // different shapes arrive.
    auto* cardRow = new QHBoxLayout;
    cardRow->setSpacing(8);

    // The fetching rules — https only, no downgrade on redirect, a size
    // cap, a cache beside the log — used to live in this file. The
    // logbook detail pane needs the same ones, and two copies of a
    // security decision is one copy too many, so they moved into
    // StationPhoto and both callers use it. (2026-08-10)
    m_photo = new StationPhoto(this);
    m_photo->setFixedSize(78, 78);
    m_photo->setVisible(false);
    // A portrait is one network request per station, so it has to be
    // switchable — and a setting nobody can find is not a switch.
    m_photo->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_photo, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* off = menu.addAction(QStringLiteral("Don't show QRZ photos"));
        if (menu.exec(m_photo->mapToGlobal(pos)) == off) {
            AppSettings::instance().setValue(kShowPhotoKey, false);
            m_photo->showPlaceholder(QString{});
            m_photo->setVisible(false);
            setStatus(QStringLiteral("QRZ photos off — re-enable in settings "
                                     "(%1)").arg(kShowPhotoKey));
        }
    });
    // The panel's frame is small and beside a callsign that already
    // says who this is, so it stays hidden until there is a picture
    // rather than explaining itself in 78 pixels.
    connect(m_photo, &StationPhoto::photoShown, this,
            [this]() { m_photo->setVisible(true); });
    cardRow->addWidget(m_photo, 0, Qt::AlignTop);

    auto* cardCol = new QVBoxLayout;
    cardCol->setSpacing(2);

    // The flag lives on the same line as the station text rather than in
    // a label of its own. A separate row is one more thing that can end
    // up zero-height or laid out off-screen, and then a missing flag
    // looks like a missing lookup.
    m_stationLine = new QLabel(QString{}, this);
    m_stationLine->setWordWrap(true);
    m_stationLine->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextPrimary));
    cardCol->addWidget(m_stationLine);

    // Grey line at the far end. On the low bands this decides whether
    // the path is open at all, so it belongs next to the callsign, not
    // buried in a propagation window.
    m_solarLine = new QLabel(QString{}, this);
    m_solarLine->setWordWrap(true);
    cardCol->addWidget(m_solarLine);

    // Have I had this one before, and does it count. The question gets
    // asked in the two seconds before deciding to call, so it belongs
    // beside the callsign and not in a window.
    m_workedLine = new QLabel(QString{}, this);
    m_workedLine->setWordWrap(true);
    cardCol->addWidget(m_workedLine);
    cardCol->addStretch(1);

    cardRow->addLayout(cardCol, 1);
    m_rowCard = addShedRow(col, cardRow, this);

    // Locators
    auto* gridRow = new QHBoxLayout;
    gridRow->setSpacing(6);
    gridRow->addWidget(caption(QStringLiteral("MY"), this));
    m_myGrid = new QLineEdit(
        AppSettings::instance().value(kMyGridKey, QString{}).toString(), this);
    m_myGrid->setPlaceholderText(QStringLiteral("your locator"));
    gridRow->addWidget(m_myGrid, 1);
    gridRow->addWidget(caption(QStringLiteral("DX"), this));
    m_dxGrid = new QLineEdit(this);
    m_dxGrid->setPlaceholderText(QStringLiteral("their locator"));
    gridRow->addWidget(m_dxGrid, 1);
    m_rowGrid = addShedRow(col, gridRow, this);

    // Dial and globe share one slot rather than stacking vertically: in a
    // dock this narrow, two 260 px squares would push the log off screen,
    // and they answer the same question in two ways.
    m_dial  = new RotorDialWidget(this);
    m_dial->setEndStop(AppSettings::instance()
                           .value(kRotorEndStopKey, -1.0).toDouble());
    m_globe = new GlobeWidget(this);

    auto* globePage = new QWidget(this);
    auto* globeCol  = new QVBoxLayout(globePage);
    globeCol->setContentsMargins(0, 0, 0, 0);
    globeCol->setSpacing(4);
    globeCol->addWidget(m_globe, 1);
    auto* worldBtn = new QPushButton(QStringLiteral("World image…"), globePage);
    worldBtn->setStyleSheet(Style::buttonBaseStyle());
    worldBtn->setToolTip(QStringLiteral(
        "Paint the globe with an equirectangular world map (2:1). "
        "Without one it still shows the path, just unpainted."));
    globeCol->addWidget(worldBtn);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_dial);
    m_viewStack->addWidget(globePage);
    col->addWidget(m_viewStack, 1);

    const QString saved =
        AppSettings::instance().value(kWorldImgKey, QString{}).toString();
    if (!saved.isEmpty()) { m_globe->loadTexture(saved); }

    connect(worldBtn, &QPushButton::clicked, this, [this, worldBtn]() {
        QMenu menu(this);
        QAction* small = menu.addAction(
            QStringLiteral("Download from NASA — 2048 × 1024 (about 1 MB)"));
        QAction* large = menu.addAction(
            QStringLiteral("Download from NASA — 5400 × 2700 (about 7 MB)"));
        menu.addSeparator();
        QAction* pick = menu.addAction(QStringLiteral("Choose a file…"));

        QAction* chosen = menu.exec(worldBtn->mapToGlobal(
            QPoint(0, worldBtn->height())));
        if (chosen == small) {
            downloadWorldImage(smallCandidates(), 0,
                               QStringLiteral("2048 × 1024"));
        } else if (chosen == large) {
            downloadWorldImage(largeCandidates(), 0,
                               QStringLiteral("5400 × 2700"));
        } else if (chosen == pick) {
            chooseWorldImage();
        }
    });

    // Rotor buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    auto* rotateBtn = new QPushButton(QStringLiteral("Rotate"), this);
    auto* stopBtn   = new QPushButton(QStringLiteral("Stop"), this);
    // The wide "Long path" button that used to sit here was retired on
    // 2026-08-11 (bench finding #4): it duplicated the preset row's
    // "LP", minus LP's empty-aim hint. One control, one behaviour.
    for (QPushButton* b : {rotateBtn, stopBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        btnRow->addWidget(b);
    }
    auto* setupBtn = new QPushButton(QStringLiteral("Rotor…"), this);
    setupBtn->setStyleSheet(Style::buttonBaseStyle());
    setupBtn->setToolTip(QStringLiteral(
        "Connect to a rotator through Hamlib's rotctld"));
    btnRow->addWidget(setupBtn);
    connect(setupBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::openRotorSetupDialog);

    // Compass presets (2026-08-10): one click aims at a cardinal
    // course; the turn itself stays behind Rotate, same as a click on
    // the rose — a preset must never move the mast by itself.
    auto* presetRow = new QHBoxLayout;
    presetRow->setSpacing(4);
    struct Preset { const char* label; double deg; };
    static constexpr Preset kPresets[] = {
        {"N", 0}, {"NE", 45}, {"E", 90}, {"SE", 135},
        {"S", 180}, {"SW", 225}, {"W", 270}, {"NW", 315},
    };
    for (const Preset& pre : kPresets) {
        auto* b = new QPushButton(QString::fromLatin1(pre.label), this);
        b->setStyleSheet(Style::buttonBaseStyle());
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(QStringLiteral("Aim %1° — then Rotate to turn")
                          .arg(pre.deg, 0, 'f', 0));
        const double deg = pre.deg;
        connect(b, &QPushButton::clicked, this, [this, deg]() {
            m_dial->setTargetBearing(deg);
        });
        presetRow->addWidget(b);
    }
    presetRow->addStretch(1);
    m_rowPreset = addShedRow(col, presetRow, this);

    // ── Teachable presets, park, long path (2026-08-11) ──────────────
    //
    // The row above aims at compass points; this one aims at the
    // operator's own targets — "EU", "JA", the repeater — taught by
    // right-click from whatever the dial is currently aiming at (or
    // the rotator's fresh reading when nothing is aimed). Same
    // contract as every preset: aiming never moves the mast; the turn
    // stays behind Rotate.
    auto* userRow = new QHBoxLayout;
    userRow->setSpacing(4);
    for (int i = 0; i < kUserPresetSlots; ++i) {
        auto* b = new QPushButton(this);
        b->setStyleSheet(Style::buttonBaseStyle());
        b->setFocusPolicy(Qt::NoFocus);
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        m_userPreset[i] = b;
        connect(b, &QPushButton::clicked, this, [this, i]() {
            const QString deg = AppSettings::instance()
                .value(kPresetDegKey.arg(i + 1), QString{}).toString();
            if (deg.isEmpty()) {
                setStatus(QStringLiteral(
                    "Empty preset — right-click it to teach the current "
                    "aim"), true);
                return;
            }
            m_dial->setTargetBearing(deg.toDouble());
        });
        connect(b, &QPushButton::customContextMenuRequested, this,
                [this, i](const QPoint& pos) { userPresetMenu(i, pos); });
        userRow->addWidget(b);
        refreshUserPresetButton(i);
    }

    m_parkBtn = new QPushButton(QStringLiteral("Park"), this);
    m_parkBtn->setStyleSheet(Style::buttonBaseStyle());
    m_parkBtn->setFocusPolicy(Qt::NoFocus);
    m_parkBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_parkBtn, &QPushButton::clicked, this, [this]() {
        const QString deg = AppSettings::instance()
            .value(kParkDegKey, QString{}).toString();
        if (deg.isEmpty()) {
            setStatus(QStringLiteral(
                "No park position yet — right-click Park to teach it"),
                true);
            return;
        }
        m_dial->setTargetBearing(deg.toDouble());
    });
    connect(m_parkBtn, &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { parkMenu(pos); });
    userRow->addWidget(m_parkBtn);
    refreshParkButton();

    // Long path: the same station, the other way round the planet.
    // Flips the AIM only — deliberately symmetric with everything else
    // on these rows.
    m_lpBtn = new QPushButton(QStringLiteral("LP"), this);
    m_lpBtn->setStyleSheet(Style::buttonBaseStyle());
    m_lpBtn->setFocusPolicy(Qt::NoFocus);
    m_lpBtn->setToolTip(QStringLiteral(
        "Flip the aim to the long path (±180°). Aim first, then LP, "
        "then Rotate."));
    connect(m_lpBtn, &QPushButton::clicked, this, [this]() {
        if (!m_dial->hasTarget()) {
            setStatus(QStringLiteral(
                "Nothing aimed yet — click the rose or a preset first, "
                "then LP flips it"), true);
            return;
        }
        m_dial->setTargetBearing(
            std::fmod(m_dial->targetBearing() + 180.0, 360.0));
    });
    userRow->addWidget(m_lpBtn);
    userRow->addStretch(1);
    m_rowUserPreset = addShedRow(col, userRow, this);

    m_globeBtn = new QPushButton(QStringLiteral("Globe"), this);
    m_globeBtn->setCheckable(true);
    m_globeBtn->setStyleSheet(Style::buttonBaseStyle());
    m_globeBtn->setToolTip(QStringLiteral("Swap the dial for the globe view"));
    btnRow->addWidget(m_globeBtn);
    connect(m_globeBtn, &QPushButton::toggled, this, [this](bool on) {
        m_viewStack->setCurrentIndex(on ? 1 : 0);
        // Idle spin only while the globe is the thing being looked at —
        // motion in a hidden page is wasted, and in a visible corner it
        // is a distraction while operating.
        m_globe->setAutoRotate(false);
        if (on) { updateGlobeFromLocators(); }
    });
    m_rowBtn = addShedRow(col, btnRow, this);

    // Report + log
    auto* rstRow = new QHBoxLayout;
    rstRow->setSpacing(6);
    rstRow->addWidget(caption(QStringLiteral("SENT"), this));
    m_rstSent = new QLineEdit(QStringLiteral("59"), this);
    m_rstSent->setFixedWidth(46);
    m_rstSent->setAlignment(Qt::AlignCenter);
    rstRow->addWidget(m_rstSent);
    rstRow->addWidget(caption(QStringLiteral("RCVD"), this));
    m_rstRcvd = new QLineEdit(QStringLiteral("59"), this);
    m_rstRcvd->setFixedWidth(46);
    m_rstRcvd->setAlignment(Qt::AlignCenter);
    rstRow->addWidget(m_rstRcvd);
    m_comment = new QLineEdit(this);
    m_comment->setPlaceholderText(QStringLiteral("comment"));
    rstRow->addWidget(m_comment, 1);
    m_rowRst = addShedRow(col, rstRow, this);

    for (QLineEdit* e : {m_myGrid, m_dxGrid, m_rstSent, m_rstRcvd, m_comment}) {
        e->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    }

    auto* logRow = new QHBoxLayout;
    logRow->setSpacing(6);
    auto* logBtn = new QPushButton(QStringLiteral("Log QSO"), this);
    logBtn->setStyleSheet(Style::buttonBaseStyle()
                          + Style::greenCheckedStyle());
    logRow->addWidget(logBtn, 1);
    auto* bookBtn = new QPushButton(QStringLiteral("Logbook…"), this);
    bookBtn->setStyleSheet(Style::buttonBaseStyle());
    bookBtn->setToolTip(QStringLiteral(
        "Search, correct and export the whole log"));
    logRow->addWidget(bookBtn);
    m_rowLog = addShedRow(col, logRow, this);
    connect(bookBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::openLogbookWindow);

    m_status = new QLabel(QString{}, this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextSecondary));
    col->addWidget(m_status);

    // Recent contacts — the log is a file, and a file you cannot see is
    // a file you do not trust. Newest first.
    m_recent = new QTableWidget(0, 4, this);
    m_recent->setHorizontalHeaderLabels({QStringLiteral("UTC"),
                                         QStringLiteral("Call"),
                                         QStringLiteral("Band"),
                                         QStringLiteral("Mode")});
    m_recent->verticalHeader()->setVisible(false);
    m_recent->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recent->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recent->horizontalHeader()->setStretchLastSection(true);
    m_recent->setMaximumHeight(130);
    m_recent->setStyleSheet(QStringLiteral(
        "QTableWidget { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; gridline-color: %3; font-size: 11px; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        "  border-bottom: 1px solid %3; padding: 2px 5px; font-size: 9px; }"
    ).arg(QString::fromLatin1(Style::kInsetBg),
          QString::fromLatin1(Style::kTextPrimary),
          QString::fromLatin1(Style::kBorderSubtle),
          QString::fromLatin1(Style::kButtonBg),
          QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(m_recent);

    // ── Shrinking down to the compass (2026-08-10) ───────────────────
    //
    // Everything above competes for the same column, and a panel with
    // ten rows in it has a minimum height of all ten. Dragged narrow in
    // a dock, the dial was the thing that got squeezed — which is
    // backwards, because the dial is the one part that is useful at a
    // glance from across the room.
    //
    // So the rows are shed from the bottom of this list upwards as the
    // panel loses height, and the dial keeps whatever is left. Order is
    // least useful first. The recent-contacts table goes early because
    // it is by far the tallest and the logbook window shows the same
    // thing properly. The status line goes late because it is one line
    // and it is where the panel says what went wrong. The rotate and
    // stop buttons go last of all, and even then the dial itself can
    // still be double-clicked to turn.
    // The taught presets outrank the cardinal row: cardinals are an
    // orientation aid, the taught ones are the operator's actual
    // targets — so the cardinal row sheds first.
    m_shedOrder = {m_recent, m_rowLog, m_rowRst, m_rowCard, m_rowPreset,
                   m_rowUserPreset, m_rowGrid, m_rowCall, m_status,
                   m_rowBtn};
    m_shedHeights.resize(m_shedOrder.size());
    m_column = col;

    // ── Wiring ───────────────────────────────────────────────────────
    connect(m_callEdit, &QLineEdit::textChanged,
            this, &RotorLogbookPanel::onCallsignEdited);
    // Enter looks the station up, full stop (2026-08-10). It used to
    // also start the rotator turning, which meant confirming a callsign
    // moved the mast — an action nobody expects from the Enter key. The
    // turn stays on the Rotate button and the dial's double-click.
    connect(m_callEdit, &QLineEdit::returnPressed,
            this, &RotorLogbookPanel::onLookupRequested);
    connect(m_lookupBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLookupRequested);
    connect(m_myGrid, &QLineEdit::textChanged,
            this, &RotorLogbookPanel::applyLocators);
    connect(m_dxGrid, &QLineEdit::textChanged, this, [this]() {
        // Only a human typing here counts as manual. Writing a QRZ
        // answer into the field must not make the field look defended
        // against the next QRZ answer.
        if (!m_adoptingGrid) { m_dxGridIsManual = true; }
        applyLocators();
    });

    // Automatic QRZ lookup while typing. Debounced rather than
    // per-keystroke: OE, OE1, OE1W, OE1WY would otherwise be four
    // requests for one station, and the replies can land out of order.
    m_lookupTimer = new QTimer(this);
    m_lookupTimer->setSingleShot(true);
    m_lookupTimer->setInterval(kAutoLookupDelayMs);
    connect(m_lookupTimer, &QTimer::timeout,
            this, &RotorLogbookPanel::onLookupRequested);
    connect(logBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLogQso);

    // Stand-in movement for when no rotator is connected, so the dial
    // is demonstrable and the Turning / OnTarget states stay reachable.
    // A real rotator takes this timer's place entirely.
    m_simTimer = new QTimer(this);
    m_simTimer->setInterval(40);
    connect(m_simTimer, &QTimer::timeout, this, [this]() {
        const double togo = m_dial->travelDegrees();
        if (qAbs(togo) < 1.0) {
            m_simTimer->stop();
            m_dial->setState(RotorDialWidget::State::OnTarget);
            return;
        }
        m_dial->setActualBearing(m_dial->actualBearing()
                                 + (togo > 0 ? 1.0 : -1.0));
    });

    connect(rotateBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::beginTurn);
    connect(m_dial, &RotorDialWidget::rotateRequested, this,
            [this](double) { beginTurn(); });
    connect(stopBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::haltTurn);
}

// ── Shrinking down to the compass ───────────────────────────────────

void RotorLogbookPanel::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    updateCompactness();
}

void RotorLogbookPanel::updateCompactness()
{
    if (m_shedOrder.isEmpty() || !m_column) { return; }

    // Remember how tall each row is while it is up. A hidden widget's
    // sizeHint is still valid in Qt, but a row that has never been laid
    // out reports its hint before its children have theirs, so the
    // first honest measurement is cached and reused.
    for (int i = 0; i < m_shedOrder.size(); ++i) {
        QWidget* w = m_shedOrder.at(i);
        if (!w || !w->isVisible()) { continue; }
        const int h = w->sizeHint().height();
        if (h > 0) { m_shedHeights[i] = h; }
    }

    const QMargins m = m_column->contentsMargins();
    const int spacing = m_column->spacing();
    int avail = height() - m.top() - m.bottom();

    // What the dial insists on keeping. Below this there is no compass
    // left to protect and shedding another row buys nothing.
    const int dialFloor =
        m_viewStack ? m_viewStack->minimumSizeHint().height() : 96;

    int needed = dialFloor;
    for (int i = 0; i < m_shedOrder.size(); ++i) {
        if (m_shedHeights.at(i) > 0) {
            needed += m_shedHeights.at(i) + spacing;
        }
    }

    // Shed from the front of the list — least useful first — until what
    // is left fits. Purely a function of the height we were handed, so
    // it cannot oscillate: hiding a row does not change `avail`.
    int shed = 0;
    while (needed > avail && shed < m_shedOrder.size()) {
        if (m_shedHeights.at(shed) > 0) {
            needed -= m_shedHeights.at(shed) + spacing;
        }
        ++shed;
    }

    for (int i = 0; i < m_shedOrder.size(); ++i) {
        QWidget* w = m_shedOrder.at(i);
        if (!w) { continue; }
        const bool show = i >= shed;
        if (w->isVisible() != show) { w->setVisible(show); }
    }

    // With the buttons gone there is nothing left saying how to turn,
    // so the dial has to say it itself.
    if (m_dial) {
        m_dial->setHint(shed >= m_shedOrder.size()
            ? QStringLiteral("Click to aim, double-click to turn. Drag the "
                             "panel taller to get the controls back.")
            : QString{});
    }
}

// ── Rotator ─────────────────────────────────────────────────────────

// ── Teachable presets + park (2026-08-11) ───────────────────────────

void RotorLogbookPanel::refreshUserPresetButton(int slot)
{
    if (slot < 0 || slot >= kUserPresetSlots || !m_userPreset[slot]) {
        return;
    }
    AppSettings& s = AppSettings::instance();
    const QString label =
        s.value(kPresetLabelKey.arg(slot + 1), QString{}).toString();
    const QString deg =
        s.value(kPresetDegKey.arg(slot + 1), QString{}).toString();

    QPushButton* b = m_userPreset[slot];
    if (deg.isEmpty()) {
        // Untaught. A number would suggest a bearing; a dot suggests a
        // free slot, which is what it is.
        b->setText(QStringLiteral("·"));
        b->setToolTip(QStringLiteral(
            "Free preset — right-click to store the current aim under a "
            "name of your own"));
        return;
    }
    b->setText(label.isEmpty()
                   ? QStringLiteral("%1°").arg(deg.toDouble(), 0, 'f', 0)
                   : label);
    b->setToolTip(QStringLiteral(
        "Aim %1° (%2) — then Rotate to turn. Right-click to re-teach, "
        "rename or clear.")
                      .arg(deg.toDouble(), 0, 'f', 0)
                      .arg(label.isEmpty() ? QStringLiteral("unnamed")
                                           : label));
}

void RotorLogbookPanel::refreshParkButton()
{
    if (!m_parkBtn) { return; }
    const QString deg =
        AppSettings::instance().value(kParkDegKey, QString{}).toString();
    m_parkBtn->setToolTip(deg.isEmpty()
        ? QStringLiteral("No park position stored — right-click to teach "
                         "the current aim as park")
        : QStringLiteral("Aim the park position (%1°) — then Rotate. "
                         "Right-click to re-teach or clear.")
              .arg(deg.toDouble(), 0, 'f', 0));
}

bool RotorLogbookPanel::teachableBearing(double* outDeg) const
{
    if (m_dial && m_dial->hasTarget()) {
        *outDeg = m_dial->targetBearing();
        return true;
    }
    // No aim set: fall back to where the mast actually points — but
    // only a FRESH reading. Teaching from a stale needle stores a lie.
    if (m_rotor && m_rotor->isConnected() && m_rotor->hasFreshPosition()) {
        *outDeg = m_rotor->azimuth();
        return true;
    }
    return false;
}

void RotorLogbookPanel::userPresetMenu(int slot, const QPoint& posInButton)
{
    if (slot < 0 || slot >= kUserPresetSlots || !m_userPreset[slot]) {
        return;
    }
    AppSettings& s = AppSettings::instance();
    const QString degKey   = kPresetDegKey.arg(slot + 1);
    const QString labelKey = kPresetLabelKey.arg(slot + 1);
    const bool taught = !s.value(degKey, QString{}).toString().isEmpty();

    QMenu menu(this);
    double teach = 0.0;
    const bool canTeach = teachableBearing(&teach);
    QAction* save = menu.addAction(canTeach
        ? QStringLiteral("Store current aim here (%1°)")
              .arg(teach, 0, 'f', 0)
        : QStringLiteral("Store current aim here — aim something first"));
    save->setEnabled(canTeach);
    QAction* rename = taught
        ? menu.addAction(QStringLiteral("Rename…")) : nullptr;
    QAction* clear = taught
        ? menu.addAction(QStringLiteral("Clear")) : nullptr;

    QAction* chosen =
        menu.exec(m_userPreset[slot]->mapToGlobal(posInButton));
    if (!chosen) { return; }

    if (chosen == save) {
        s.setValue(degKey, QString::number(teach, 'f', 1));
        // First teach without a name: ask for one right away — a row of
        // bare numbers is exactly the unhelpfulness this row replaces.
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, QStringLiteral("Preset name"),
            QStringLiteral("Name for %1° (blank keeps the number):")
                .arg(teach, 0, 'f', 0),
            QLineEdit::Normal,
            s.value(labelKey, QString{}).toString(), &ok);
        if (ok) { s.setValue(labelKey, name.trimmed()); }
        s.save();
    } else if (rename && chosen == rename) {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, QStringLiteral("Preset name"),
            QStringLiteral("New name:"), QLineEdit::Normal,
            s.value(labelKey, QString{}).toString(), &ok);
        if (ok) {
            s.setValue(labelKey, name.trimmed());
            s.save();
        }
    } else if (clear && chosen == clear) {
        s.setValue(degKey, QString{});
        s.setValue(labelKey, QString{});
        s.save();
    }
    refreshUserPresetButton(slot);
}

void RotorLogbookPanel::parkMenu(const QPoint& posInButton)
{
    if (!m_parkBtn) { return; }
    AppSettings& s = AppSettings::instance();
    const bool taught = !s.value(kParkDegKey, QString{}).toString().isEmpty();

    QMenu menu(this);
    double teach = 0.0;
    const bool canTeach = teachableBearing(&teach);
    QAction* save = menu.addAction(canTeach
        ? QStringLiteral("Store current aim as park (%1°)")
              .arg(teach, 0, 'f', 0)
        : QStringLiteral("Store current aim as park — aim something "
                         "first"));
    save->setEnabled(canTeach);
    QAction* clear = taught
        ? menu.addAction(QStringLiteral("Clear park position")) : nullptr;

    QAction* chosen = menu.exec(m_parkBtn->mapToGlobal(posInButton));
    if (!chosen) { return; }
    if (chosen == save) {
        s.setValue(kParkDegKey, QString::number(teach, 'f', 1));
        s.save();
    } else if (clear && chosen == clear) {
        s.setValue(kParkDegKey, QString{});
        s.save();
    }
    refreshParkButton();
}

void RotorLogbookPanel::beginTurn()
{
    if (!m_dial->hasTarget()) { return; }
    const double bearing = m_dial->targetBearing();

    // Swing the globe round to the heading as the mast turns: the point
    // of that view is watching which way the path goes.
    m_globe->lookAlongBearing(bearing);
    m_dial->setState(RotorDialWidget::State::Turning);

    if (m_rotor && m_rotor->isConnected()) {
        m_dial->setSimulated(false);
        m_rotor->moveTo(bearing);
        return;
    }
    // No rotator. Say so every time rather than once at startup — the
    // needle moving is otherwise indistinguishable from the mast
    // moving, and that is a mistake an operator makes exactly once
    // before they stop trusting the display.
    //
    // The status line is not enough on its own: it scrolls away, and
    // what is left afterwards is a dial that looks like a reading. So
    // the needle itself goes dashed and dim and the rose is labelled
    // for as long as the reading is invented. (2026-08-10)
    m_dial->setSimulated(true);
    setStatus(QStringLiteral("No rotator connected — needle simulated"), true);
    m_simTimer->start();
}

void RotorLogbookPanel::haltTurn()
{
    m_simTimer->stop();
    if (m_rotor && m_rotor->isConnected()) { m_rotor->stop(); }
    m_dial->setState(RotorDialWidget::State::Targeted);
}

void RotorLogbookPanel::ensureRotor()
{
    if (m_rotor) { return; }
    m_rotor = new RotctldClient(this);

    AppSettings& s = AppSettings::instance();
    m_rotor->setTarget(
        s.value(kRotorHostKey, QString{}).toString(),
        static_cast<quint16>(s.value(kRotorPortKey, 4533).toInt()));

    // The rotator's own reading drives the second needle. Nothing else
    // may write it while a rotator is connected, or the dial would show
    // where the software thinks the mast is rather than where it is.
    connect(m_rotor, &RotorController::positionChanged, this,
            [this](double az) {
        // A real reading has arrived, so the needle stops being a
        // pretence — and the stand-in timer must stop too, or it would
        // keep nudging the needle away from what the mast reports.
        m_simTimer->stop();
        m_dial->setSimulated(false);
        m_dial->setActualBearing(az);
    });

    // Az/el rotators: the reported elevation appears on the dial. The
    // signal only fires when the value actually changes, so an
    // azimuth-only rotator never triggers the readout.
    connect(m_rotor, &RotctldClient::elevationChanged, this,
            [this](double el) { m_dial->setElevation(el); });

    connect(m_rotor, &RotorController::stateChanged, this,
            [this](RotorController::State st) {
        switch (st) {
        case RotorController::State::Idle:
            m_dial->setState(m_dial->hasTarget()
                                 ? RotorDialWidget::State::OnTarget
                                 : RotorDialWidget::State::Idle);
            setStatus(QStringLiteral("Rotator connected — %1")
                          .arg(m_rotor->description()));
            break;
        case RotorController::State::Moving:
            m_dial->setState(RotorDialWidget::State::Turning);
            break;
        case RotorController::State::Connecting:
            setStatus(QStringLiteral("Connecting to %1…")
                          .arg(m_rotor->description()));
            break;
        case RotorController::State::Disconnected:
            setStatus(QStringLiteral("Rotator disconnected"), true);
            break;
        case RotorController::State::Error:
            break;   // errorOccurred carries the detail
        }
    });

    connect(m_rotor, &RotorController::errorOccurred, this,
            [this](const QString& msg) {
        setStatus(QStringLiteral("Rotator: %1").arg(msg), true);
    });
}

void RotorLogbookPanel::showRotorSetup()
{
    openRotorSetupDialog();
}

// 2026-08-10: entry point for "Turn rotor to <call>" from the Spot Hub.
// Everything after setText is the panel's normal typing pipeline —
// onCallsignEdited resolves the prefix and sets the dial target when an
// own locator is present; a QRZ answer refines it later. If a bearing
// came out of that, the turn starts at once; if not (no own locator),
// the status line already says what is missing, which beats turning to
// a bearing that does not exist.
void RotorLogbookPanel::workSpot(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty()) { return; }
    m_callEdit->setText(c);
    if (m_dial->hasTarget()) { beginTurn(); }
}

void RotorLogbookPanel::takeSpot(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty()) { return; }

    // Das Setzen des Feldes zieht alles nach: onCallsignEdited holt Land
    // und Flagge aus cty.dat, stellt den Zeiger auf die Zielpeilung der
    // DXCC-Einheit und schreibt die Entfernung in die Statuszeile — ohne
    // Netz. Kommt die QRZ-Antwort, ersetzt der genaue Locator die
    // Schaetzung aus dem Prefix.
    m_callEdit->setText(c);
    m_callEdit->setFocus();

    // Kein beginTurn(): siehe Begruendung am Kopf der Deklaration. Wer
    // drehen will, hat den Rotate-Knopf daneben und sieht vorher, wohin.
    if (!m_dial->hasTarget()) {
        setStatus(QStringLiteral("%1 — no bearing yet, waiting for a locator")
                      .arg(c));
    }
}

void RotorLogbookPanel::openRotorSetupDialog()
{
    ensureRotor();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Rotator"));

    auto* col = new QVBoxLayout(&dlg);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(10);

    AppSettings& s = AppSettings::instance();

    // Two ways in. Most operators want the first and should not have to
    // know the second exists; anyone already running rotctld their own
    // way, or with the controller on another machine, needs the second
    // and would be blocked without it.
    auto* modeBox = new QComboBox(&dlg);
    modeBox->addItem(QStringLiteral("Controller on this computer"));
    modeBox->addItem(QStringLiteral("rotctld already running (network)"));
    modeBox->setCurrentIndex(
        s.value(kRotorUseSerialKey, true).toBool() ? 0 : 1);
    col->addWidget(modeBox);

    // Mechanical end stop (2026-08-10). The dial computes its travel
    // path around this, so the sector it draws is the way the mast will
    // actually turn instead of the way a free compass would.
    auto* stopRow = new QHBoxLayout;
    auto* stopLabel = new QLabel(QStringLiteral("End stop"), &dlg);
    stopLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    stopRow->addWidget(stopLabel);
    auto* stopCombo = new QComboBox(&dlg);
    stopCombo->addItem(QStringLiteral("none — turns freely"), -1.0);
    stopCombo->addItem(QStringLiteral("North (0°)"), 0.0);
    stopCombo->addItem(QStringLiteral("East (90°)"), 90.0);
    stopCombo->addItem(QStringLiteral("South (180°)"), 180.0);
    stopCombo->addItem(QStringLiteral("West (270°)"), 270.0);
    {
        const double savedStop =
            s.value(kRotorEndStopKey, -1.0).toDouble();
        int at = 0;
        for (int i = 0; i < stopCombo->count(); ++i) {
            if (qFuzzyCompare(stopCombo->itemData(i).toDouble() + 1.0,
                              savedStop + 1.0)) { at = i; break; }
        }
        stopCombo->setCurrentIndex(at);
    }
    connect(stopCombo, &QComboBox::currentIndexChanged, &dlg,
            [this, stopCombo](int) {
        const double deg = stopCombo->currentData().toDouble();
        AppSettings::instance().setValue(kRotorEndStopKey, deg);
        m_dial->setEndStop(deg);
    });
    stopRow->addWidget(stopCombo, 1);
    col->addLayout(stopRow);

    // ── Local: model + port + baud ───────────────────────────────────
    auto* localBox = new QWidget(&dlg);
    auto* localForm = new QFormLayout(localBox);
    localForm->setContentsMargins(0, 0, 0, 0);

    auto* modelCombo = new QComboBox(localBox);
    const QVector<RotorModel> models = commonRotorModels();
    for (const RotorModel& m : models) {
        modelCombo->addItem(QStringLiteral("%1  (%2)").arg(m.name)
                                .arg(m.hamlibId), m.hamlibId);
    }
    // Anything Hamlib knows that is not in the short list.
    modelCombo->addItem(QStringLiteral("Other — enter a Hamlib number"), -1);
    const int savedModel = s.value(kRotorModelKey, 601).toInt();
    const int at = modelCombo->findData(savedModel);
    modelCombo->setCurrentIndex(at >= 0 ? at : 0);
    localForm->addRow(QStringLiteral("Controller"), modelCombo);

    auto* otherEdit = new QLineEdit(QString::number(savedModel), localBox);
    otherEdit->setPlaceholderText(QStringLiteral("rotctl --list shows them all"));
    otherEdit->setVisible(at < 0);
    localForm->addRow(QStringLiteral("Model number"), otherEdit);

    auto* noteLabel = new QLabel(localBox);
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                 .arg(QString::fromLatin1(Style::kTextSecondary)));
    localForm->addRow(QString{}, noteLabel);

    auto* portCombo = new QComboBox(localBox);
    portCombo->setEditable(true);   // a port that is not plugged in yet
    localForm->addRow(QStringLiteral("Serial port"), portCombo);

    auto* baudCombo = new QComboBox(localBox);
    for (int b : commonRotorBauds()) { baudCombo->addItem(QString::number(b), b); }
    const int savedBaud = s.value(kRotorBaudKey, 9600).toInt();
    const int bat = baudCombo->findData(savedBaud);
    baudCombo->setCurrentIndex(bat >= 0 ? bat : baudCombo->findData(9600));
    localForm->addRow(QStringLiteral("Speed"), baudCombo);
    col->addWidget(localBox);

    // ── Hamlib itself ────────────────────────────────────────────────
    //
    // Everything above is useless without rotctld, and rotctld comes
    // from Hamlib, which is not part of this program. Telling the
    // operator to open a terminal is where the feature stopped being
    // used, so the install happens here.
    //
    // Both queries below shell out to rotctl, so they are read once and
    // kept. Running them from refreshUi() would spawn two processes
    // every time the operator touched a combo box.
    auto* hamlibBox = new QWidget(&dlg);
    auto* hamlibCol = new QVBoxLayout(hamlibBox);
    hamlibCol->setContentsMargins(0, 0, 0, 0);
    hamlibCol->setSpacing(6);

    auto* hamlibLabel = new QLabel(hamlibBox);
    hamlibLabel->setWordWrap(true);
    hamlibLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                   .arg(QString::fromLatin1(Style::kTextSecondary)));
    hamlibCol->addWidget(hamlibLabel);

    auto* hamlibBtns = new QHBoxLayout;
    auto* installBtn = new QPushButton(hamlibBox);
    installBtn->setStyleSheet(Style::buttonBaseStyle());
    hamlibBtns->addWidget(installBtn);
    auto* copyCmdBtn = new QPushButton(QStringLiteral("Copy command"), hamlibBox);
    copyCmdBtn->setStyleSheet(Style::buttonBaseStyle());
    hamlibBtns->addWidget(copyCmdBtn);
    hamlibBtns->addStretch(1);
    hamlibCol->addLayout(hamlibBtns);

    // Homebrew takes minutes and says a great deal while it works. A
    // dialog that sits silent for that long looks hung, and an operator
    // who cannot see the output has nothing to paste into a question.
    auto* installLog = new QPlainTextEdit(hamlibBox);
    installLog->setReadOnly(true);
    installLog->setMaximumHeight(150);
    installLog->setVisible(false);
    installLog->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: %1; color: %2; "
                       "font-family: Menlo, monospace; font-size: 11px; "
                       "border: 1px solid %3; }")
            .arg(QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kTextSecondary),
                 QString::fromLatin1(Style::kInsetBorder)));
    hamlibCol->addWidget(installLog);
    col->addWidget(hamlibBox);

    // No parent: this lives on the stack beside the dialog, and giving
    // it a parent that outlives nothing would only invite a double
    // delete the day the declarations are reordered.
    HamlibInstaller installer;
    QString hamlibVersion;
    QVector<HamlibRotorEntry> hamlibKnows;
    auto rescanHamlib = [&]() {
        hamlibVersion = HamlibInstaller::installedVersion();
        // Only worth asking when there is something to ask. Each of
        // these runs rotctl, and the answer is kept rather than fetched
        // again on every keystroke.
        hamlibKnows = hamlibVersion.isEmpty()
            ? QVector<HamlibRotorEntry>{} : HamlibInstaller::supportedModels();
    };
    rescanHamlib();

    auto refreshPorts = [portCombo, &s]() {
        const QString keep = portCombo->currentText();
        portCombo->clear();
#ifdef HAVE_SERIALPORT
        for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
            // Name the device beside the path. "/dev/tty.usbserial-1410"
            // tells nobody which box it is; "FT232R USB UART" does.
            const QString desc = info.description().isEmpty()
                ? info.manufacturer() : info.description();
            portCombo->addItem(desc.isEmpty()
                ? info.systemLocation()
                : QStringLiteral("%1  —  %2").arg(info.systemLocation(), desc),
                info.systemLocation());
        }
#endif
        if (portCombo->count() == 0) {
            portCombo->addItem(QStringLiteral("no serial ports found"),
                               QString{});
        }
        const QString saved = keep.isEmpty()
            ? s.value(kRotorDeviceKey, QString{}).toString() : keep;
        if (!saved.isEmpty()) {
            const int i = portCombo->findData(saved);
            if (i >= 0) { portCombo->setCurrentIndex(i); }
            else { portCombo->setEditText(saved); }
        }
    };
    refreshPorts();

    // ── Network: host + port ─────────────────────────────────────────
    auto* netBox = new QWidget(&dlg);
    auto* netForm = new QFormLayout(netBox);
    netForm->setContentsMargins(0, 0, 0, 0);
    auto* hostEdit = new QLineEdit(m_rotor->host(), netBox);
    hostEdit->setPlaceholderText(
        QStringLiteral("address running rotctld, e.g. 192.168.1.20"));
    netForm->addRow(QStringLiteral("Host"), hostEdit);
    auto* portEdit = new QLineEdit(QString::number(m_rotor->port()), netBox);
    portEdit->setPlaceholderText(QStringLiteral("4533"));
    netForm->addRow(QStringLiteral("Port"), portEdit);
    col->addWidget(netBox);

    auto* status = new QLabel(&dlg);
    status->setWordWrap(true);
    status->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                              .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(status);

    auto* row = new QHBoxLayout;
    auto* connectBtn = new QPushButton(&dlg);
    connectBtn->setStyleSheet(Style::buttonBaseStyle());
    row->addWidget(connectBtn);
    row->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), &dlg);
    closeBtn->setStyleSheet(Style::buttonBaseStyle());
    row->addWidget(closeBtn);
    col->addLayout(row);

    auto currentModel = [modelCombo, otherEdit]() {
        const int id = modelCombo->currentData().toInt();
        return id > 0 ? id : otherEdit->text().trimmed().toInt();
    };

    auto refreshUi = [&]() {
        const bool local = modeBox->currentIndex() == 0;
        localBox->setVisible(local);
        netBox->setVisible(!local);
        otherEdit->setVisible(local
                              && modelCombo->currentData().toInt() < 0);

        const int id = currentModel();
        QString note;
        for (const RotorModel& m : models) {
            if (m.hamlibId == id) { note = m.note; break; }
        }
        noteLabel->setText(note);
        noteLabel->setVisible(!note.isEmpty());

        // ── What to say about Hamlib ─────────────────────────────────
        const bool installed = !hamlibVersion.isEmpty();
        const bool installing = installer.isRunning();
        hamlibBox->setVisible(local);

        if (!installed) {
            hamlibLabel->setText(QStringLiteral(
                "Hamlib isn't installed. It provides rotctld, which is "
                "the piece that talks to the controller — without it "
                "nothing below can connect."));
            installBtn->setVisible(HamlibInstaller::canInstallAutomatically());
            installBtn->setEnabled(!installing);
            installBtn->setText(installing
                ? QStringLiteral("Installing…")
                : QStringLiteral("Install Hamlib and connect"));
            copyCmdBtn->setVisible(true);
            copyCmdBtn->setText(
                HamlibInstaller::canInstallAutomatically()
                    ? QStringLiteral("Copy command")
                    : QStringLiteral("Copy instructions"));
            if (!HamlibInstaller::canInstallAutomatically()) {
                hamlibLabel->setText(hamlibLabel->text()
                    + QStringLiteral("\n\n")
                    + HamlibInstaller::manualInstructions());
            }
        } else {
            QString line = QStringLiteral("Hamlib %1 · %2")
                               .arg(hamlibVersion,
                                    RotctldProcess::findBinary());

            // The number matters more than the name. Hamlib gains
            // drivers between releases, so a controller in the picker
            // can be missing from the copy that is actually installed —
            // and rotctld's way of saying so is to exit, which reads as
            // a cable fault rather than as an out-of-date Hamlib.
            if (!hamlibKnows.isEmpty() && id > 0) {
                bool has = false;
                for (const HamlibRotorEntry& e : hamlibKnows) {
                    if (e.model == id) { has = true; break; }
                }
                if (!has) {
                    line += QStringLiteral(
                        "\n⚠ This Hamlib doesn't have model %1. Update "
                        "Hamlib, or pick one of the emulations listed "
                        "under the controller.").arg(id);
                }
            }
            hamlibLabel->setText(line);
            installBtn->setVisible(false);
            copyCmdBtn->setVisible(false);
        }

        connectBtn->setText(m_rotor->isConnected()
            ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
        // Offering Connect with no rotctld to run only produces a
        // puzzling failure; the install button above is the thing to
        // press.
        connectBtn->setEnabled(!local || installed);
        status->setText(m_rotor->isConnected()
            ? QStringLiteral("Connected — %1").arg(m_rotor->description())
            : QStringLiteral("Not connected"));
    };
    refreshUi();

    connect(modeBox, &QComboBox::currentIndexChanged, &dlg,
            [&](int) { refreshUi(); });
    connect(modelCombo, &QComboBox::currentIndexChanged, &dlg,
            [&](int) { refreshUi(); });

    auto reportConn = connect(m_rotor, &RotorController::errorOccurred,
                              &dlg, [status](const QString& m) {
        status->setText(m);
    });
    auto reportState = connect(m_rotor, &RotorController::stateChanged,
                               &dlg, [&](auto) { refreshUi(); });

    connect(connectBtn, &QPushButton::clicked, &dlg, [&]() {
        if (m_rotor->isConnected()) {
            m_rotor->disconnectFromRotor();
            m_rotorProc.stop();
            refreshUi();
            return;
        }

        const bool local = modeBox->currentIndex() == 0;
        s.setValue(kRotorUseSerialKey, local);

        if (local) {
            const int model = currentModel();
            const QString device = portCombo->currentData().isValid()
                && !portCombo->currentData().toString().isEmpty()
                    ? portCombo->currentData().toString()
                    : portCombo->currentText().trimmed();
            const int baud = baudCombo->currentData().toInt();

            s.setValue(kRotorModelKey, model);
            s.setValue(kRotorDeviceKey, device);
            s.setValue(kRotorBaudKey, baud);

            QString err;
            if (!m_rotorProc.start(model, device, baud, 4533, &err)) {
                status->setText(err);
                return;
            }
            m_rotor->setTarget(QStringLiteral("127.0.0.1"), 4533);
            s.setValue(kRotorHostKey, QStringLiteral("127.0.0.1"));
            s.setValue(kRotorPortKey, 4533);
        } else {
            const QString host = hostEdit->text().trimmed();
            const quint16 port =
                static_cast<quint16>(portEdit->text().trimmed().toUInt());
            m_rotor->setTarget(host, port ? port : 4533);
            s.setValue(kRotorHostKey, host);
            s.setValue(kRotorPortKey, port ? port : 4533);
        }
        m_rotor->connectToRotor();
    });
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    // ── Installing Hamlib, then connecting ───────────────────────────

    connect(copyCmdBtn, &QPushButton::clicked, &dlg, [&]() {
        const QStringList cmd = HamlibInstaller::installCommand();
        QGuiApplication::clipboard()->setText(
            cmd.isEmpty() ? HamlibInstaller::manualInstructions()
                          : cmd.join(QLatin1Char(' ')));
        status->setText(QStringLiteral("Copied. Paste it into a terminal, "
                                       "then press Connect."));
    });

    connect(&installer, &HamlibInstaller::output, &dlg,
            [installLog](const QString& line) {
        installLog->appendPlainText(line);
    });

    connect(&installer, &HamlibInstaller::finished, &dlg,
            [&](bool ok, const QString& message) {
        rescanHamlib();
        status->setText(message);
        refreshUi();
        // "Install and connect" means both. Stopping at "installed" and
        // making the operator find the next button is the small gap the
        // whole exercise was meant to close.
        if (ok && !m_rotor->isConnected()) { connectBtn->click(); }
    });

    connect(installBtn, &QPushButton::clicked, &dlg, [&]() {
        installLog->clear();
        installLog->setVisible(true);
        status->setText(QStringLiteral(
            "Installing Hamlib. This takes a few minutes the first time "
            "— Homebrew's output is below."));
        refreshUi();

        QString err;
        if (!installer.install(&err)) {
            status->setText(err);
            refreshUi();
        }
    });

    dlg.exec();
    disconnect(reportConn);
    disconnect(reportState);
}

void RotorLogbookPanel::updateSolarLine()
{
    if (!m_hasDxPos) {
        m_solarLine->clear();
        return;
    }

    const SolarInfo s =
        solarInfo(QDateTime::currentDateTimeUtc(), m_dxLat, m_dxLon);

    QString text;
    if (s.alwaysUp) {
        text = QStringLiteral("DX: midnight sun");
    } else if (s.alwaysDown) {
        text = QStringLiteral("DX: polar night");
    } else {
        text = QStringLiteral("DX: SR %1z · SS %2z")
                   .arg(s.riseUtc.toString(QStringLiteral("HH:mm")),
                        s.setUtc.toString(QStringLiteral("HH:mm")));
    }
    // Elevation with an explicit sign: "-9" reads as night at a glance
    // in a way that "9 degrees below the horizon" does not.
    text += QStringLiteral(" · sun %1%2°")
                .arg(s.elevationDeg >= 0 ? QStringLiteral("+")
                                         : QStringLiteral("-"))
                .arg(std::abs(s.elevationDeg), 0, 'f', 0);
    if (s.greyline) { text += QStringLiteral(" · GREY LINE"); }

    m_solarLine->setText(text);
    m_solarLine->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(s.greyline ? Style::kAmberText
                                            : Style::kTextSecondary)));
}

void RotorLogbookPanel::chooseWorldImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose an equirectangular world image"),
        AppSettings::instance().value(kWorldImgKey, QString{}).toString(),
        QStringLiteral("Images (*.jpg *.jpeg *.png *.tif *.tiff *.webp)"));
    if (path.isEmpty()) { return; }

    if (!m_globe->loadTexture(path)) {
        setStatus(QStringLiteral("Couldn't read that image"), true);
        return;
    }
    AppSettings::instance().setValue(kWorldImgKey, path);
    setStatus(QStringLiteral("World image loaded"));
}

void RotorLogbookPanel::downloadWorldImage(const QStringList& candidates,
                                           int index, const QString& label)
{
    if (index < 0 || index >= candidates.size()) { return; }
    const QString url = candidates.at(index);

    if (!m_net) { m_net = new QNetworkAccessManager(this); }

    const QString dest =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/world.jpg");

    setStatus(QStringLiteral("Downloading world image (%1)…").arg(label));

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, label](qint64 got, qint64 total) {
        if (total <= 0) { return; }
        setStatus(QStringLiteral("Downloading world image (%1) — %2%")
                      .arg(label).arg(got * 100 / total));
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, dest, candidates, index, label]() {
        reply->deleteLater();

        auto tryNext = [this, candidates, index, label](const QString& why) {
            if (index + 1 < candidates.size()) {
                downloadWorldImage(candidates, index + 1, label);
                return;
            }
            // Out of candidates. Name the last address: if NASA has moved
            // the file for good, the operator can find its replacement and
            // use "Choose a file…". An unexplained failure would just look
            // like a feature that does not work.
            setStatus(QStringLiteral("Download failed: %1 — you can still "
                                     "load an image manually (%2)")
                          .arg(why, candidates.last()), true);
        };

        if (reply->error() != QNetworkReply::NoError) {
            tryNext(reply->errorString());
            return;
        }

        const QByteArray bytes = reply->readAll();
        QFile out(dest);
        if (!out.open(QIODevice::WriteOnly)) {
            setStatus(QStringLiteral("Couldn't save the image: %1")
                          .arg(out.errorString()), true);
            return;
        }
        out.write(bytes);
        out.close();

        // Load before remembering the path: a truncated download, or an
        // error page served with status 200, must not become the
        // remembered setting.
        if (!m_globe->loadTexture(dest)) {
            tryNext(QStringLiteral("not a readable image"));
            return;
        }
        AppSettings::instance().setValue(kWorldImgKey, dest);
        setStatus(QStringLiteral("World image loaded (%1 KB)")
                      .arg(bytes.size() / 1024));
    });
}

void RotorLogbookPanel::onCallsignEdited(const QString& raw)
{
    const QString call = raw.trimmed().toUpper();
    if (call != raw) {
        QSignalBlocker block(m_callEdit);
        m_callEdit->setText(call);
    }
    updateWorkedLine(call);
    scheduleAutoLookup(call);

    if (call.isEmpty()) {
        m_flagEmoji.clear();
        m_stationLine->clear();
        m_solarLine->clear();
        m_workedLine->clear();
        m_photo->showPlaceholder(QString{});
        m_photo->setVisible(false);
        m_hasDxPos = false;
        m_lastInfo = CallsignInfo{};
        return;
    }

    // Country estimate from cty.dat — no network, so it keeps up with
    // typing. QRZ replaces it on demand.
    if (!m_radio) { return; }
    DxccColorProvider* dxcc = m_radio->dxccColorProvider();
    if (!dxcc) { return; }

    const CtyDatParser& cty = dxcc->ctyDat();
    const QString prefix = cty.resolvePrimaryPrefix(call);

    // The flag comes from the prefix alone, so it appears as the callsign
    // is typed — no locator and no lookup needed.
    updateFlagFor(call);

    const DxccEntity* ent = prefix.isEmpty() ? nullptr
                                             : cty.entityByPrefix(prefix);
    if (!ent || !ent->hasLatLon) {
        // Even with no coordinates the entity name is worth showing, and
        // it proves the prefix resolved — which is what a missing flag
        // would otherwise leave ambiguous.
        setStationLine(ent ? ent->name : QString{});
        return;
    }

    // The entity centre is enough for the grey line even with no
    // locator of our own — sunrise over Japan does not depend on where
    // the listener is standing.
    m_dxLat = ent->latitude; m_dxLon = ent->longitude; m_hasDxPos = true;
    m_globe->setTarget(ent->latitude, ent->longitude);
    updateSolarLine();

    const QString mine = m_myGrid->text().trimmed().toUpper();
    if (!isValidGridSquare(mine)) { return; }

    const QString entGrid = gridSquareFromLatLon(ent->latitude, ent->longitude);
    m_dial->setTargetBearing(calculateBearingInDegrees(mine, entGrid));
    setStationLine(QStringLiteral("%1 · %2 km (from prefix)")
        .arg(ent->name)
        .arg(calculateDistanceKm(mine, entGrid), 0, 'f', 0));
}

// ── Station card ────────────────────────────────────────────────────

void RotorLogbookPanel::currentBandMode(QString& band, QString& mode) const
{
    band.clear();
    mode.clear();
    SliceModel* s = m_radio ? m_radio->activeSlice() : nullptr;
    if (!s) { return; }

    band = bandLabel(bandFromFrequency(s->frequency()));

    // Same collapsing as buildEntry: ADIF has no LSB/USB mode, and the
    // worked-before answer has to be about the mode the contact will be
    // logged as, not the one the radio calls it.
    const QString m = SliceModel::modeName(s->dspMode());
    if (m == QLatin1String("LSB") || m == QLatin1String("USB")) {
        mode = QStringLiteral("SSB");
    } else if (m == QLatin1String("CWL") || m == QLatin1String("CWU")) {
        mode = QStringLiteral("CW");
    } else {
        mode = m;
    }
}

void RotorLogbookPanel::updateWorkedLine(const QString& call)
{
    if (call.trimmed().isEmpty()) {
        m_workedLine->clear();
        return;
    }

    WorkedBefore::PrefixResolver resolver;
    if (m_radio) {
        if (DxccColorProvider* dxcc = m_radio->dxccColorProvider()) {
            resolver = [dxcc](const QString& c) {
                return dxcc->ctyDat().resolvePrimaryPrefix(c);
            };
        }
    }

    QString band, mode;
    currentBandMode(band, mode);
    const WorkedSummary s = m_worked.lookup(call, band, mode, resolver);

    QStringList bits;
    QString colour = QString::fromLatin1(Style::kTextSecondary);

    if (s.knownEntity && s.newEntity) {
        bits << QStringLiteral("NEW DXCC");
        colour = QString::fromLatin1(Style::kGreenText);
    } else if (s.knownEntity && (s.newBand || s.newMode)) {
        QStringList what;
        if (s.newBand) { what << QStringLiteral("band"); }
        if (s.newMode) { what << QStringLiteral("mode"); }
        bits << QStringLiteral("new %1 for %2")
                    .arg(what.join(QStringLiteral(" and ")), s.entity);
        colour = QString::fromLatin1(Style::kAmberText);
    }

    if (s.timesWorked > 0) {
        QString had = s.timesWorked == 1
            ? QStringLiteral("worked once")
            : QStringLiteral("worked %1×").arg(s.timesWorked);
        if (s.lastWorked.isValid()) {
            had += QStringLiteral(", last %1")
                       .arg(s.lastWorked.toString(QStringLiteral("yyyy-MM-dd")));
        }
        bits << had;
    } else if (bits.isEmpty()) {
        bits << QStringLiteral("not in your log");
    }

    m_workedLine->setText(bits.join(QStringLiteral(" · ")));
    m_workedLine->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(colour));
}

void RotorLogbookPanel::updateFlagFor(const QString& call)
{
    // Flag from the DXCC prefix rather than from QRZ's free-text country
    // field: the prefix is what cty.dat resolves deterministically, while
    // "country" arrives spelled a dozen different ways.
    m_flagEmoji.clear();
    if (!m_radio) { return; }
    DxccColorProvider* dxcc = m_radio->dxccColorProvider();
    if (!dxcc) { return; }
    m_flagEmoji = dxccFlagEmoji(dxcc->ctyDat().resolvePrimaryPrefix(call));
}

// 2026-08-10, operator wish: "more data after Enter". Everything the
// lookup answer carries that an operator in QSO actually uses, on two
// readable lines instead of one thin one.
QString RotorLogbookPanel::stationText(const CallsignInfo& info) const
{
    QStringList top;
    if (!info.displayName().isEmpty()) { top << info.displayName(); }
    if (!info.city.trimmed().isEmpty())    { top << info.city.trimmed(); }
    if (!info.state.trimmed().isEmpty())   { top << info.state.trimmed(); }
    if (!info.country.trimmed().isEmpty()) { top << info.country.trimmed(); }
    if (isValidGridSquare(info.grid)) {
        top << info.grid.trimmed().toUpper();
    }

    QStringList more;
    if (!info.county.trimmed().isEmpty()) { more << info.county.trimmed(); }
    if (!info.licenseClass.trimmed().isEmpty()) {
        more << QStringLiteral("class %1").arg(info.licenseClass.trimmed());
    }
    // Which QSL routes the station answers on — the question that
    // decides whether working them will ever count for an award.
    QStringList qsl;
    if (info.lotw)    { qsl << QStringLiteral("LoTW"); }
    if (info.eqsl)    { qsl << QStringLiteral("eQSL"); }
    if (info.mailQsl) { qsl << QStringLiteral("card"); }
    if (!qsl.isEmpty()) {
        more << QStringLiteral("QSL: %1").arg(qsl.join(QLatin1Char(' ')));
    }
    const QString mine = m_myGrid->text().trimmed().toUpper();
    if (isValidGridSquare(mine) && isValidGridSquare(info.grid)) {
        more << QStringLiteral("%1 km · %2°")
                    .arg(calculateDistanceKm(mine, info.grid), 0, 'f', 0)
                    .arg(calculateBearingInDegrees(mine, info.grid),
                         0, 'f', 0);
    }

    QString text = top.join(QStringLiteral(" · "));
    if (!more.isEmpty()) {
        text += QLatin1Char('\n') + more.join(QStringLiteral(" · "));
    }
    return text;
}

void RotorLogbookPanel::setStationLine(const QString& text)
{
    if (m_flagEmoji.isEmpty()) {
        m_stationLine->setText(text);
    } else if (text.isEmpty()) {
        m_stationLine->setText(m_flagEmoji);
    } else {
        m_stationLine->setText(m_flagEmoji + QLatin1Char(' ') + text);
    }
}

void RotorLogbookPanel::showStationVisuals(const CallsignInfo& info)
{
    if (AppSettings::instance().value(kShowPhotoKey, true).toBool()
        && !info.imageUrl.isEmpty()) {
        // StationPhoto owns the rules now — https only, no downgrade on
        // redirect, a size cap, the cache. It shows itself once a
        // picture actually decodes; see the photoShown connection in
        // buildUi. (2026-08-10)
        m_photo->setUrl(info.imageUrl);
    } else {
        m_photo->showPlaceholder(QString{});
        m_photo->setVisible(false);
    }
}

void RotorLogbookPanel::onLookupRequested()
{
    const QString call = Callsigns::normalized(m_callEdit->text());
    if (call.isEmpty()) { return; }
    if (!Callsigns::isLikelyCallsign(call)) {
        setStatus(QStringLiteral("%1 doesn't look like a callsign").arg(call),
                  true);
        return;
    }

    // Already answered once this session. A contest is three hundred
    // callsigns and some of them repeat; a metered subscription should
    // not pay twice for the same answer.
    if (m_qrzCache.contains(call)) {
        const CallsignInfo cached = m_qrzCache.value(call);
        m_lastInfo = cached;
        adoptGridFromQrz(cached);
        updateFlagFor(cached.call);
        setStationLine(stationText(cached));
        showStationVisuals(cached);
        return;
    }

    if (!m_qrz || !m_qrz->hasCredentials()) {
        setStatus(QStringLiteral("Add your QRZ account in Tools first"), true);
        return;
    }
    setStatus(QStringLiteral("Looking up %1…").arg(call));
    m_qrz->lookup(call);
}

void RotorLogbookPanel::scheduleAutoLookup(const QString& call)
{
    if (!m_lookupTimer) { return; }
    m_lookupTimer->stop();

    if (!AppSettings::instance().value(kAutoLookupKey, true).toBool()) {
        return;
    }
    if (call.size() < kMinAutoLookupChars) { return; }
    if (!Callsigns::isLikelyCallsign(call)) { return; }
    if (!m_qrz || !m_qrz->hasCredentials()) { return; }

    // A cached answer needs no waiting.
    if (m_qrzCache.contains(call)) { onLookupRequested(); return; }

    m_lookupTimer->start();
}

void RotorLogbookPanel::adoptGridFromQrz(const CallsignInfo& info)
{
    if (!isValidGridSquare(info.grid)) { return; }
    // Never over a locator a person typed. QRZ is often right and
    // sometimes years out of date; the operator in front of the radio
    // has just been told the real one.
    if (m_dxGridIsManual && !m_dxGrid->text().trimmed().isEmpty()) { return; }

    m_adoptingGrid = true;
    m_dxGrid->setText(info.grid.trimmed().toUpper());
    m_adoptingGrid = false;
    applyLocators();
}

// ── Logging ─────────────────────────────────────────────────────────

LogEntry RotorLogbookPanel::buildEntry() const
{
    LogEntry e;
    e.call         = Callsigns::normalized(m_callEdit->text());
    e.timeOn       = QDateTime::currentDateTimeUtc();
    e.myGridSquare = m_myGrid->text().trimmed().toUpper();
    e.gridSquare   = m_dxGrid->text().trimmed().toUpper();
    e.rstSent      = m_rstSent->text().trimmed();
    e.rstRcvd      = m_rstRcvd->text().trimmed();
    e.comment      = m_comment->text().trimmed();

    if (SliceModel* s = m_radio ? m_radio->activeSlice() : nullptr) {
        e.freqMHz = s->frequency() / 1e6;
        e.band    = bandLabel(bandFromFrequency(s->frequency()));

        // ADIF has no LSB/USB mode — those are submodes of SSB, and a
        // record with MODE=LSB is rejected or silently rewritten.
        const QString m = SliceModel::modeName(s->dspMode());
        if (m == QLatin1String("LSB") || m == QLatin1String("USB")) {
            e.mode = QStringLiteral("SSB");
            e.submode = m;
        } else if (m == QLatin1String("CWL") || m == QLatin1String("CWU")) {
            e.mode = QStringLiteral("CW");
        } else {
            e.mode = m;
        }
    }

    // QRZ detail, but only when it belongs to THIS callsign — a card
    // left over from the previous station would put the wrong operator
    // in the log.
    if (m_lastInfo.isValid()
        && Callsigns::normalized(m_lastInfo.call) == e.call) {
        e.name    = m_lastInfo.displayName();
        e.qth     = m_lastInfo.city;
        e.country = m_lastInfo.country;
    }

    if (isValidGridSquare(e.myGridSquare) && isValidGridSquare(e.gridSquare)) {
        e.distanceKm = calculateDistanceKm(e.myGridSquare, e.gridSquare);
        e.bearingDeg = calculateBearingInDegrees(e.myGridSquare, e.gridSquare);
    }
    return e;
}

bool RotorLogbookPanel::appendToLogFile(const LogEntry& entry, QString* error)
{
    const QString path = logbookPath();
    const bool isNew = !QFile::exists(path);

    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text)) {
        if (error) { *error = f.errorString(); }
        return false;
    }
    QTextStream out(&f);
    if (isNew) {
        // Strict importers reject a file whose first token is a record
        // rather than a header terminated by <EOH>.
        out << "NereusSDR logbook\n"
            << "<ADIF_VER:5>3.1.4 <PROGRAMID:9>NereusSDR <EOH>\n";
    }
    out << entry.toAdifRecord() << "\n";
    out.flush();
    return true;
}

void RotorLogbookPanel::onLogQso()
{
    const LogEntry e = buildEntry();
    if (!e.isValid()) {
        setStatus(QStringLiteral("Enter a callsign to log"), true);
        return;
    }

    // Ask, do not refuse. Working the same station twice on one band
    // and mode inside a couple of minutes is unusual but legitimate —
    // a botched exchange repeated, a contest dupe logged deliberately.
    // The same rule the importer uses, so one idea of "the same QSO"
    // rather than two that disagree.
    if (m_worked.wouldDuplicate(e)) {
        if (QMessageBox::question(this, QStringLiteral("Duplicate"),
                QStringLiteral("You already have %1 on %2 at about this "
                               "time.\n\nLog it again?")
                    .arg(e.call,
                         e.band.isEmpty() ? QStringLiteral("this band")
                                          : e.band),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            setStatus(QStringLiteral("Not logged — %1 is already in the log")
                          .arg(e.call), true);
            return;
        }
    }

    QString err;
    if (!appendToLogFile(e, &err)) {
        setStatus(QStringLiteral("Couldn't write the log: %1").arg(err), true);
        return;
    }

    // Confirm the local write before the upload, so a failing upload
    // never reads as a lost contact.
    QString msg = QStringLiteral("Logged %1").arg(e.call);
    if (!e.band.isEmpty()) { msg += QStringLiteral(" on %1").arg(e.band); }
    if (m_uploader && m_uploader->isConfigured()) {
        msg += QStringLiteral(" · uploading…");
        m_lastLogged = e;
        m_uploader->upload(e);
    }
    setStatus(msg);

    refreshRecentList();
    emit qsoLogged(e);

    // Clear only what belongs to the contact just made. The locators
    // and the reports stay — the next station is usually worked with
    // the same defaults, and retyping "59" every time is friction.
    // Clear the DX locator too, and forget that it was hand-entered.
    // Carrying the last station's grid into the next contact is how a
    // whole run of contest QSOs ends up logged from one square.
    m_adoptingGrid = true;
    m_dxGrid->clear();
    m_adoptingGrid = false;
    m_dxGridIsManual = false;

    m_callEdit->clear();
    m_comment->clear();
    m_stationLine->clear();
    m_flagEmoji.clear();
    m_lastInfo = CallsignInfo{};
    m_callEdit->setFocus();
}

// 2026-08-10: WSJT-X (or any program speaking its UDP protocol) logged
// a contact. Same file, same duplicate rule, same uploader as manual
// logging — the only differences are that nothing here asks questions
// (an FT8 run must not stop for a dialog) and the status line says
// where the contact came from.
void RotorLogbookPanel::logExternalQso(const LogEntry& entry)
{
    if (!entry.isValid()) { return; }

    LogEntry e = entry;
    if (e.myGridSquare.trimmed().isEmpty()) {
        e.myGridSquare = m_myGrid->text().trimmed().toUpper();
    }
    if (isValidGridSquare(e.myGridSquare)
        && isValidGridSquare(e.gridSquare)) {
        e.distanceKm = calculateDistanceKm(e.myGridSquare, e.gridSquare);
        e.bearingDeg =
            calculateBearingInDegrees(e.myGridSquare, e.gridSquare);
    }

    // Silently skip duplicates instead of asking: WSJT-X re-sends its
    // message when its own log is edited, and a dialog popping up mid
    // FT8 sequence would cost the next transmit period.
    if (m_worked.wouldDuplicate(e)) {
        setStatus(QStringLiteral("%1 already logged — WSJT-X duplicate "
                                 "skipped").arg(e.call));
        return;
    }

    QString err;
    if (!appendToLogFile(e, &err)) {
        setStatus(QStringLiteral("Couldn't write WSJT-X contact: %1")
                      .arg(err), true);
        return;
    }

    QString msg = QStringLiteral("Logged %1 from WSJT-X").arg(e.call);
    if (!e.band.isEmpty()) { msg += QStringLiteral(" on %1").arg(e.band); }
    if (m_uploader && m_uploader->isConfigured()) {
        msg += QStringLiteral(" · uploading…");
        m_lastLogged = e;
        m_uploader->upload(e);
    }
    setStatus(msg);
    refreshRecentList();
    emit qsoLogged(e);
}

void RotorLogbookPanel::markUploaded(const QString& call)
{
    // Only the contact just logged, not every past QSO with the same
    // station. Marking by callsign alone would claim a station's whole
    // history had been uploaded because one of its contacts was.
    if (!m_lastLogged.isValid()) { return; }
    if (Callsigns::normalized(m_lastLogged.call)
        != Callsigns::normalized(call)) { return; }

    QVector<LogEntry> all = AdifLog::read(logbookPath());
    bool changed = false;
    for (LogEntry& e : all) {
        if (!e.uploadedToQrz && AdifLog::isSameQso(e, m_lastLogged)) {
            e.uploadedToQrz = true;
            changed = true;
            break;
        }
    }
    if (!changed) { return; }

    QString err;
    if (!AdifLog::write(logbookPath(), all, &err)) {
        // Not worth interrupting for: the contact is logged and it is
        // uploaded. Only the note saying so failed, and the worst
        // consequence is that it gets offered for upload again, where
        // QRZ will call it a duplicate.
        qWarning("Couldn't record the QRZ upload: %s", qPrintable(err));
    }
    m_lastLogged = LogEntry{};
}

void RotorLogbookPanel::refreshRecentList()
{
    m_recent->setRowCount(0);

    // Read the file back rather than remembering what was written: the
    // list then shows the file's truth, including contacts from earlier
    // sessions, and a write that silently failed would be visible.
    //
    // Shares AdifLog with the logbook window, so the dock and the window
    // can never disagree about what a record says.
    QVector<LogEntry> all = AdifLog::read(logbookPath());

    // Rebuild the worked-before index from the same read. Doing it here
    // rather than on a timer means the answer beside a callsign is
    // never older than the last contact written — including one
    // written by the logbook window or pulled in by an import.
    {
        WorkedBefore::PrefixResolver resolver;
        if (m_radio) {
            if (DxccColorProvider* dxcc = m_radio->dxccColorProvider()) {
                resolver = [dxcc](const QString& c) {
                    return dxcc->ctyDat().resolvePrimaryPrefix(c);
                };
            }
        }
        m_worked.rebuild(all, resolver);
    }

    // Sort rather than trusting file order. The file is written oldest
    // first, but an imported log need not be, and relying on position
    // would put the twelve oldest contacts here without any error.
    std::stable_sort(all.begin(), all.end(),
                     [](const LogEntry& a, const LogEntry& b) {
        if (a.timeOn.isValid() != b.timeOn.isValid()) {
            return a.timeOn.isValid();
        }
        return a.timeOn > b.timeOn;
    });

    const int shown = std::min<int>(all.size(), 12);
    m_recent->setRowCount(shown);
    for (int i = 0; i < shown; ++i) {
        const LogEntry& e = all.at(i);   // already newest first
        const QDateTime u = e.timeOn.toUTC();
        m_recent->setItem(i, 0, new QTableWidgetItem(
            u.isValid() ? u.toString(QStringLiteral("hh:mm")) : QString{}));
        m_recent->setItem(i, 1, new QTableWidgetItem(e.call));
        m_recent->setItem(i, 2, new QTableWidgetItem(e.band));
        m_recent->setItem(i, 3, new QTableWidgetItem(
            e.submode.isEmpty() ? e.mode : e.submode));
    }
    m_recent->resizeColumnsToContents();
}

void RotorLogbookPanel::setUploadTargets(const QVector<QsoUploader*>& targets)
{
    m_uploadTargets = targets;
    if (m_logWindow) { m_logWindow->setUploaders(m_uploadTargets); }
}

void RotorLogbookPanel::showLogbook()
{
    openLogbookWindow();
}

void RotorLogbookPanel::openLogbookWindow()
{
    // One window, reused. Opening a second copy of the same file in two
    // windows is how one of them ends up writing over the other's edit.
    if (!m_logWindow) {
        m_logWindow = new LogbookWindow(logbookPath(), this);
        m_logWindow->setUploaders(m_uploadTargets);
        // The same QRZ client the panel uses, so the detail pane can
        // put a name and a portrait against a selected contact. Shared
        // rather than a second client: one session key, one queue, and
        // one place the credentials live.
        m_logWindow->setQrzClient(m_qrz);
        connect(m_logWindow, &LogbookWindow::logChanged,
                this, &RotorLogbookPanel::refreshRecentList);

        // ── Point the beam at a logged contact ───────────────────────
        //
        // The logbook knows the bearing to every contact and this panel
        // owns the rotor; until now nobody had joined them, so working
        // a station again meant reading a number off one half of the
        // screen and typing it into the other.
        //
        // The dial follows too, not only the rotor: leaving it showing
        // the old heading while the antenna turns is the kind of small
        // lie that gets believed at three in the morning.
        //
        // ── One path, not two (2026-08-10) ───────────────────────────
        //
        // This used to refuse with a modal box when no rotator was
        // connected, while the panel's own Rotate button, one control
        // away, cheerfully turned the needle and said so in the status
        // line. Same request, two different programs.
        //
        // The box was the wrong half of that pair. It interrupted —
        // during a contact, which is when you look a station up — and
        // it withheld the part that needs no rotator at all: showing
        // where the station is. Setting the target and swinging the
        // globe is display, not motion.
        //
        // So the target goes on the dial and beginTurn() decides the
        // rest, exactly as it does for the button. Whatever it does, it
        // now does the same thing from both places.
        connect(m_logWindow, &LogbookWindow::turnRotorRequested, this,
                [this](double bearing, const QString& call) {
            Q_UNUSED(call);
            if (!m_dial) { return; }
            m_dial->setTargetBearing(bearing);
            beginTurn();   // same call workSpot() and the button make
        });

        // Hand cty.dat down to the map. Contacts logged before a QRZ
        // lookup filled in a locator — and most contacts in a log
        // imported from another program — have no GRIDSQUARE at all,
        // and without this the map draws nothing for them.
        if (m_radio) {
            if (DxccColorProvider* dxcc = m_radio->dxccColorProvider()) {
                m_logWindow->setPositionFallback(
                    [dxcc](const QString& call, double& lat, double& lon) {
                        const QString prefix =
                            dxcc->ctyDat().resolvePrimaryPrefix(call);
                        if (prefix.isEmpty()) { return false; }
                        const DxccEntity* ent = dxcc->ctyDat()
                                                    .entityByPrefix(prefix);
                        if (!ent || !ent->hasLatLon) { return false; }
                        lat = ent->latitude;
                        lon = ent->longitude;
                        return true;
                    });
            }
        }
    }
    m_logWindow->reload();
    m_logWindow->show();
    m_logWindow->raise();
    m_logWindow->activateWindow();
}

void RotorLogbookPanel::setStatus(const QString& text, bool warn)
{
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(QString::fromLatin1(warn ? Style::kAmberText
                                   : Style::kTextSecondary)));
}

// ── QRZ wiring and locator maths ────────────────────────────────────
//
// Lost the same way as the logbook's editing slots: a scripted edit
// dropped the tail of a region. Restored verbatim from ea7b31c2.

void RotorLogbookPanel::wireQrz()
{
    if (!m_qrz) {
        m_lookupBtn->setEnabled(false);
        m_lookupBtn->setToolTip(
            QStringLiteral("Add your QRZ account in Tools to enable lookups"));
        return;
    }

    connect(m_qrz, &QrzClient::lookupSucceeded, this,
            [this](const QString& call, const CallsignInfo& info) {
        // A queued reply for a callsign the operator has already typed
        // past must not overwrite the card.
        // Remember it whatever the operator has typed since — the next
        // time this callsign comes round, the answer is already here.
        m_qrzCache.insert(call, info);

        if (Callsigns::normalized(m_callEdit->text()) != call) { return; }
        m_lastInfo = info;

        adoptGridFromQrz(info);
        updateFlagFor(info.call);
        setStationLine(stationText(info));
        showStationVisuals(info);
        setStatus(QString{});

        // Show the answer as a picture too (2026-08-10, operator wish):
        // a successful lookup brings the globe up and swings it along
        // the path to the station. The dial is one click away on the
        // same button.
        if (m_dial->hasTarget()) {
            if (m_globeBtn && !m_globeBtn->isChecked()) {
                m_globeBtn->setChecked(true);   // switches the stack
            }
            m_globe->lookAlongBearing(m_dial->targetBearing());
        }
    });

    connect(m_qrz, &QrzClient::lookupFailed, this,
            [this](const QString& call, QrzClient::Error err,
                   const QString& msg) {
        if (Callsigns::normalized(m_callEdit->text()) != call) { return; }
        m_lastInfo = CallsignInfo{};
        switch (err) {
        case QrzClient::Error::NotFound:
            setStatus(QStringLiteral("%1 isn't in QRZ — country estimate kept")
                          .arg(call), true);
            break;
        case QrzClient::Error::AuthFailed:
            setStatus(QStringLiteral("QRZ rejected the login"), true);
            break;
        case QrzClient::Error::Network:
            setStatus(QStringLiteral("Couldn't reach QRZ — estimate kept"), true);
            break;
        case QrzClient::Error::Provider:
            setStatus(msg.isEmpty() ? QStringLiteral("QRZ returned no data")
                                    : msg, true);
            break;
        }
    });

    if (m_uploader) {
        connect(m_uploader, &QsoUploader::uploadFinished, this,
                [this](const QString& call, bool ok, bool duplicate,
                       const QString& message) {
            if (ok) {
                // Record that it got through. Without this the answer
                // is forgotten at the next restart, and the only safe
                // thing left is to send everything again.
                markUploaded(call);
            }
            setStatus(ok
                ? QStringLiteral("%1 — %2").arg(call,
                      duplicate ? QStringLiteral("already in your QRZ logbook")
                                : message)
                // The contact is in the local file regardless; say so,
                // or a failed upload reads as a lost QSO.
                : QStringLiteral("%1 logged locally, QRZ upload failed: %2")
                      .arg(call, message),
                !ok);
        });
    }
}

void RotorLogbookPanel::applyLocators()
{
    const QString mine = m_myGrid->text().trimmed().toUpper();
    const QString dx   = m_dxGrid->text().trimmed().toUpper();
    AppSettings::instance().setValue(kMyGridKey, mine);

    const QString needsInput = QString::fromLatin1(Style::kLineEditStyle)
        + QStringLiteral("QLineEdit { border: 1px solid %1; }")
              .arg(Style::kAmberBorder);
    const bool myOk = isValidGridSquare(mine);
    m_myGrid->setStyleSheet(myOk ? QString::fromLatin1(Style::kLineEditStyle)
                                 : needsInput);
    if (!myOk) {
        setStatus(mine.isEmpty()
            ? QStringLiteral("Enter your own locator to get bearings")
            : QStringLiteral("%1 is not a valid locator").arg(mine), true);
        return;
    }
    if (!isValidGridSquare(dx)) { return; }

    const double km  = calculateDistanceKm(mine, dx);
    const double deg = calculateBearingInDegrees(mine, dx);
    m_dial->setTargetBearing(deg);
    updateGlobeFromLocators();
    updateSolarLine();
    setStatus(QStringLiteral("%1 km · %2° %3")
                  .arg(km, 0, 'f', 0).arg(deg, 0, 'f', 0)
                  .arg(compassPoint(deg)));
}

void RotorLogbookPanel::updateGlobeFromLocators()
{
    if (!m_globe) { return; }
    const QString mine = m_myGrid->text().trimmed().toUpper();
    const QString dx   = m_dxGrid->text().trimmed().toUpper();

    if (isValidGridSquare(mine)) {
        double lat = 0.0, lon = 0.0;
        calculateLatLonFromGridSquare(mine, lat, lon);
        m_globe->setHome(lat, lon);
    }
    if (isValidGridSquare(dx)) {
        double lat = 0.0, lon = 0.0;
        calculateLatLonFromGridSquare(dx, lat, lon);
        m_globe->setTarget(lat, lon);
        m_dxLat = lat; m_dxLon = lon; m_hasDxPos = true;
    } else {
        m_globe->clearTarget();
    }
    // The sun moves; a terminator computed once at startup would be
    // visibly wrong by evening.
    m_globe->useCurrentSubsolarPoint();
}

} // namespace NereusSDR
