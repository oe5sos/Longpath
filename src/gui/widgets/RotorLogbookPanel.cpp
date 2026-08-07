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
// =================================================================

#include "RotorLogbookPanel.h"
#include "GlobeWidget.h"
#include "RotorDialWidget.h"

#include "core/AppSettings.h"
#include "core/CtyDatParser.h"
#include "core/DxccColorProvider.h"
#include "core/DxccFlag.h"
#include "core/Maidenhead.h"
#include "core/QrzClient.h"
#include "core/QrzLogbookUploader.h"
#include "core/SolarTimes.h"
#include "gui/StyleConstants.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QAction>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
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

#include <algorithm>

namespace NereusSDR {

namespace {

const QString kMyGridKey    = QStringLiteral("StationGridSquare");
const QString kWorldImgKey  = QStringLiteral("GlobeWorldImagePath");
const QString kShowPhotoKey = QStringLiteral("RotorLogShowQrzPhoto");

// NASA Blue Marble. Works of the US government are public domain, so
// this can be fetched and kept without anyone agreeing to anything —
// which is why it is offered as a download instead of being bundled:
// the file is larger than the rest of the application's assets put
// together, and the globe is one optional view.
constexpr const char* kNasaSmallUrl =
    "https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57752/"
    "land_shallow_topo_2048.jpg";
constexpr const char* kNasaLargeUrl =
    "https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73909/"
    "world.topo.bathy.200412x5400x2700.jpg";

// Portraits live beside the log, not in the system cache: they are small,
// they belong to contacts the operator made, and a cache the OS may clear
// would silently start costing a request per lookup again.
QString photoCacheDir()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/qrz-photos");
    QDir().mkpath(dir);
    return dir;
}

QString photoCachePath(const QString& url)
{
    const QByteArray h =
        QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1);
    return photoCacheDir() + QLatin1Char('/') + QString::fromLatin1(h.toHex())
           + QStringLiteral(".img");
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
    col->addLayout(callRow);

    // Station card: portrait, flag, name line. The portrait is on the
    // left and fixed-width so the text below never reflows as images of
    // different shapes arrive.
    auto* cardRow = new QHBoxLayout;
    cardRow->setSpacing(8);

    m_photo = new QLabel(this);
    m_photo->setFixedSize(78, 78);
    m_photo->setAlignment(Qt::AlignCenter);
    m_photo->setScaledContents(false);
    m_photo->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2; border-radius: 3px; }")
        .arg(QString::fromLatin1(Style::kInsetBg),
             QString::fromLatin1(Style::kBorderSubtle)));
    m_photo->setVisible(false);
    // A portrait is one network request per station, so it has to be
    // switchable — and a setting nobody can find is not a switch.
    m_photo->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_photo, &QLabel::customContextMenuRequested, this,
            [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* off = menu.addAction(QStringLiteral("Don't show QRZ photos"));
        if (menu.exec(m_photo->mapToGlobal(pos)) == off) {
            AppSettings::instance().setValue(kShowPhotoKey, false);
            m_photo->clear();
            m_photo->setVisible(false);
            setStatus(QStringLiteral("QRZ photos off — re-enable in settings "
                                     "(%1)").arg(kShowPhotoKey));
        }
    });
    cardRow->addWidget(m_photo, 0, Qt::AlignTop);

    auto* cardCol = new QVBoxLayout;
    cardCol->setSpacing(2);

    m_flag = new QLabel(QString{}, this);
    QFont ff = m_flag->font();
    ff.setPixelSize(20);
    m_flag->setFont(ff);
    m_flag->setVisible(false);
    cardCol->addWidget(m_flag);

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
    cardCol->addStretch(1);

    cardRow->addLayout(cardCol, 1);
    col->addLayout(cardRow);

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
    col->addLayout(gridRow);

    // Dial and globe share one slot rather than stacking vertically: in a
    // dock this narrow, two 260 px squares would push the log off screen,
    // and they answer the same question in two ways.
    m_dial  = new RotorDialWidget(this);
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
            downloadWorldImage(QString::fromLatin1(kNasaSmallUrl),
                               QStringLiteral("2048 × 1024"));
        } else if (chosen == large) {
            downloadWorldImage(QString::fromLatin1(kNasaLargeUrl),
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
    auto* longBtn   = new QPushButton(QStringLiteral("Long path"), this);
    for (QPushButton* b : {rotateBtn, stopBtn, longBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        btnRow->addWidget(b);
    }
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
    col->addLayout(btnRow);

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
    col->addLayout(rstRow);

    for (QLineEdit* e : {m_myGrid, m_dxGrid, m_rstSent, m_rstRcvd, m_comment}) {
        e->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    }

    auto* logBtn = new QPushButton(QStringLiteral("Log QSO"), this);
    logBtn->setStyleSheet(Style::buttonBaseStyle()
                          + Style::greenCheckedStyle());
    col->addWidget(logBtn);

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
        "  border-radius: 3px; gridline-color: %3; font-size: 11px; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        "  border-bottom: 1px solid %3; padding: 2px 5px; font-size: 9px; }"
    ).arg(QString::fromLatin1(Style::kInsetBg),
          QString::fromLatin1(Style::kTextPrimary),
          QString::fromLatin1(Style::kBorderSubtle),
          QString::fromLatin1(Style::kButtonBg),
          QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(m_recent);

    // ── Wiring ───────────────────────────────────────────────────────
    connect(m_callEdit, &QLineEdit::textChanged,
            this, &RotorLogbookPanel::onCallsignEdited);
    connect(m_callEdit, &QLineEdit::returnPressed, this, [this]() {
        onLookupRequested();
        if (m_dial->hasTarget()) {
            emit m_dial->rotateRequested(m_dial->targetBearing());
        }
    });
    connect(m_lookupBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLookupRequested);
    connect(m_myGrid, &QLineEdit::textChanged,
            this, &RotorLogbookPanel::applyLocators);
    connect(m_dxGrid, &QLineEdit::textChanged,
            this, &RotorLogbookPanel::applyLocators);
    connect(logBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLogQso);

    // Simulated movement until a rotator protocol exists, so the
    // Turning and OnTarget states stay reachable.
    auto* timer = new QTimer(this);
    timer->setInterval(40);
    auto beginTurn = [this, timer]() {
        if (!m_dial->hasTarget()) { return; }
        m_dial->setState(RotorDialWidget::State::Turning);
        timer->start();
        // Swing the globe round to the heading as the mast turns: the
        // point of the view is watching which way the path goes.
        m_globe->lookAlongBearing(m_dial->targetBearing());
    };
    connect(rotateBtn, &QPushButton::clicked, this, beginTurn);
    connect(m_dial, &RotorDialWidget::rotateRequested, this,
            [beginTurn](double) { beginTurn(); });
    connect(stopBtn, &QPushButton::clicked, this, [this, timer]() {
        timer->stop();
        m_dial->setState(RotorDialWidget::State::Targeted);
    });
    connect(longBtn, &QPushButton::clicked, this, [this]() {
        if (m_dial->hasTarget()) {
            m_dial->setTargetBearing(m_dial->targetBearing() + 180.0);
        }
    });
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        const double togo = m_dial->travelDegrees();
        if (qAbs(togo) < 1.0) {
            timer->stop();
            m_dial->setState(RotorDialWidget::State::OnTarget);
            return;
        }
        m_dial->setActualBearing(m_dial->actualBearing()
                                 + (togo > 0 ? 1.0 : -1.0));
    });
}

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
        if (Callsigns::normalized(m_callEdit->text()) != call) { return; }
        m_lastInfo = info;

        if (isValidGridSquare(info.grid)) {
            QSignalBlocker block(m_dxGrid);
            m_dxGrid->setText(info.grid);
            applyLocators();
        }
        QStringList bits;
        if (!info.displayName().isEmpty()) { bits << info.displayName(); }
        if (!info.city.isEmpty())          { bits << info.city; }
        if (!info.country.isEmpty())       { bits << info.country; }
        m_stationLine->setText(bits.join(QStringLiteral(" · ")));
        showStationVisuals(info);
        setStatus(QString{});
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

// ── Bearings ────────────────────────────────────────────────────────

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
        "QLabel { color: %1; font-size: 10px; }")
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

void RotorLogbookPanel::downloadWorldImage(const QString& url,
                                           const QString& label)
{
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
            [this, reply, dest, url]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // Name the address. If NASA has moved the file, the operator
            // can find its replacement and use "Choose a file…" — an
            // unexplained failure would just look like a broken feature.
            setStatus(QStringLiteral("Download failed: %1 — you can still "
                                     "load an image manually (%2)")
                          .arg(reply->errorString(), url), true);
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

        // Load before remembering the path: a truncated or redirected
        // download must not become the remembered setting.
        if (!m_globe->loadTexture(dest)) {
            setStatus(QStringLiteral("That download wasn't a readable image"),
                      true);
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
    if (call.isEmpty()) {
        m_stationLine->clear();
        m_solarLine->clear();
        m_flag->clear();
        m_flag->setVisible(false);
        m_photo->clear();
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
    const QString flag = dxccFlagEmoji(prefix);
    m_flag->setText(flag);
    m_flag->setVisible(!flag.isEmpty());

    const DxccEntity* ent = prefix.isEmpty() ? nullptr
                                             : cty.entityByPrefix(prefix);
    if (!ent || !ent->hasLatLon) { return; }

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
    m_stationLine->setText(QStringLiteral("%1 · %2 km (from prefix)")
        .arg(ent->name)
        .arg(calculateDistanceKm(mine, entGrid), 0, 'f', 0));
}

// ── Station card ────────────────────────────────────────────────────

void RotorLogbookPanel::showStationVisuals(const CallsignInfo& info)
{
    // Flag from the DXCC prefix rather than from QRZ's free-text country
    // field: the prefix is what cty.dat resolves deterministically, while
    // "country" arrives spelled a dozen different ways.
    QString flag;
    if (m_radio) {
        if (DxccColorProvider* dxcc = m_radio->dxccColorProvider()) {
            const QString prefix =
                dxcc->ctyDat().resolvePrimaryPrefix(info.call);
            flag = dxccFlagEmoji(prefix);
        }
    }
    m_flag->setText(flag);
    m_flag->setVisible(!flag.isEmpty());

    if (AppSettings::instance().value(kShowPhotoKey, true).toBool()
        && !info.imageUrl.isEmpty()) {
        loadStationPhoto(info.imageUrl);
    } else {
        m_photo->clear();
        m_photo->setVisible(false);
    }
}

void RotorLogbookPanel::loadStationPhoto(const QString& url)
{
    const QUrl u(url);
    // QRZ portraits are ordinary https URLs; anything else is not a
    // portrait and should not be fetched just because a field said so.
    if (!u.isValid() || u.scheme() != QLatin1String("https")) { return; }

    auto show = [this](const QByteArray& bytes) {
        QPixmap pm;
        if (!pm.loadFromData(bytes)) { return false; }
        m_photo->setPixmap(pm.scaled(m_photo->size() - QSize(2, 2),
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
        m_photo->setVisible(true);
        return true;
    };

    const QString cached = photoCachePath(url);
    QFile cf(cached);
    if (cf.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = cf.readAll();
        cf.close();
        if (show(bytes)) { return; }
    }

    if (!m_net) { m_net = new QNetworkAccessManager(this); }
    QNetworkRequest req(u);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(req);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url, cached, show]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) { return; }

        const QByteArray bytes = reply->readAll();
        // Cap what gets written: a redirect to something huge should not
        // fill the config directory with a file nobody asked for.
        if (bytes.isEmpty() || bytes.size() > 4 * 1024 * 1024) { return; }
        if (!show(bytes)) { return; }

        QFile out(cached);
        if (out.open(QIODevice::WriteOnly)) { out.write(bytes); }
    });
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
    if (!m_qrz || !m_qrz->hasCredentials()) {
        setStatus(QStringLiteral("Add your QRZ account in Tools first"), true);
        return;
    }
    setStatus(QStringLiteral("Looking up %1…").arg(call));
    m_qrz->lookup(call);
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
        m_uploader->upload(e);
    }
    setStatus(msg);

    refreshRecentList();
    emit qsoLogged(e);

    // Clear only what belongs to the contact just made. The locators
    // and the reports stay — the next station is usually worked with
    // the same defaults, and retyping "59" every time is friction.
    m_callEdit->clear();
    m_comment->clear();
    m_stationLine->clear();
    m_lastInfo = CallsignInfo{};
    m_callEdit->setFocus();
}

void RotorLogbookPanel::refreshRecentList()
{
    m_recent->setRowCount(0);

    QFile f(logbookPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return; }
    const QString text = QString::fromUtf8(f.readAll());
    f.close();

    // Read back what was written rather than remembering it: the list
    // then shows the file's truth, including contacts from earlier
    // sessions, and a write that silently failed would be visible.
    static const QRegularExpression fieldRe(
        QStringLiteral("<([A-Za-z_]+):(\\d+)(?::[A-Za-z])?>"));

    struct Row { QString time, call, band, mode; };
    QList<Row> rows;
    Row cur;

    int pos = 0;
    while (pos < text.size()) {
        const int lt = text.indexOf(QLatin1Char('<'), pos);
        if (lt < 0) { break; }
        const int gt = text.indexOf(QLatin1Char('>'), lt);
        if (gt < 0) { break; }
        const QString spec = text.mid(lt + 1, gt - lt - 1);
        pos = gt + 1;

        if (spec.compare(QStringLiteral("EOR"), Qt::CaseInsensitive) == 0) {
            if (!cur.call.isEmpty()) { rows.append(cur); }
            cur = Row{};
            continue;
        }
        const QStringList parts = spec.split(QLatin1Char(':'));
        if (parts.size() < 2) { continue; }
        bool ok = false;
        const int len = parts.at(1).toInt(&ok);
        // ADIF is length-prefixed; a value may legally contain '<', so
        // walking lengths is the only correct way to read it.
        if (!ok || len < 0 || pos + len > text.size()) { continue; }
        const QString value = text.mid(pos, len);
        pos += len;

        const QString name = parts.at(0).toUpper();
        if      (name == QLatin1String("CALL"))    { cur.call = value; }
        else if (name == QLatin1String("BAND"))    { cur.band = value; }
        else if (name == QLatin1String("MODE"))    { cur.mode = value; }
        else if (name == QLatin1String("TIME_ON")) {
            cur.time = value.left(4);
            cur.time.insert(2, QLatin1Char(':'));
        }
    }

    const int shown = std::min<int>(rows.size(), 12);
    m_recent->setRowCount(shown);
    for (int i = 0; i < shown; ++i) {
        const Row& r = rows.at(rows.size() - 1 - i);   // newest first
        m_recent->setItem(i, 0, new QTableWidgetItem(r.time));
        m_recent->setItem(i, 1, new QTableWidgetItem(r.call));
        m_recent->setItem(i, 2, new QTableWidgetItem(r.band));
        m_recent->setItem(i, 3, new QTableWidgetItem(r.mode));
    }
    m_recent->resizeColumnsToContents();
}

void RotorLogbookPanel::setStatus(const QString& text, bool warn)
{
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(QString::fromLatin1(warn ? Style::kAmberText
                                   : Style::kTextSecondary)));
}

} // namespace NereusSDR
