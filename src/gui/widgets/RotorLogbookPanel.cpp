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
#include "RotorDialWidget.h"

#include "core/AppSettings.h"
#include "core/CtyDatParser.h"
#include "core/DxccColorProvider.h"
#include "core/Maidenhead.h"
#include "core/QrzClient.h"
#include "core/QrzLogbookUploader.h"
#include "gui/StyleConstants.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QTimer>
#include <QRegularExpression>

#include <algorithm>

namespace NereusSDR {

namespace {

const QString kMyGridKey = QStringLiteral("StationGridSquare");

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

    m_stationLine = new QLabel(QString{}, this);
    m_stationLine->setWordWrap(true);
    m_stationLine->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextPrimary));
    col->addWidget(m_stationLine);

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

    m_dial = new RotorDialWidget(this);
    col->addWidget(m_dial, 1);

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
    connect(rotateBtn, &QPushButton::clicked, this, [this, timer]() {
        if (m_dial->hasTarget()) {
            m_dial->setState(RotorDialWidget::State::Turning);
            timer->start();
        }
    });
    connect(m_dial, &RotorDialWidget::rotateRequested, this,
            [this, timer](double) {
        if (m_dial->hasTarget()) {
            m_dial->setState(RotorDialWidget::State::Turning);
            timer->start();
        }
    });
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
    setStatus(QStringLiteral("%1 km · %2° %3")
                  .arg(km, 0, 'f', 0).arg(deg, 0, 'f', 0)
                  .arg(compassPoint(deg)));
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
        m_lastInfo = CallsignInfo{};
        return;
    }

    // Country estimate from cty.dat — no network, so it keeps up with
    // typing. QRZ replaces it on demand.
    const QString mine = m_myGrid->text().trimmed().toUpper();
    if (!isValidGridSquare(mine) || !m_radio) { return; }
    DxccColorProvider* dxcc = m_radio->dxccColorProvider();
    if (!dxcc) { return; }

    const CtyDatParser& cty = dxcc->ctyDat();
    const QString prefix = cty.resolvePrimaryPrefix(call);
    const DxccEntity* ent = prefix.isEmpty() ? nullptr
                                             : cty.entityByPrefix(prefix);
    if (!ent || !ent->hasLatLon) { return; }

    const QString entGrid = gridSquareFromLatLon(ent->latitude, ent->longitude);
    m_dial->setTargetBearing(calculateBearingInDegrees(mine, entGrid));
    m_stationLine->setText(QStringLiteral("%1 · %2 km (from prefix)")
        .arg(ent->name)
        .arg(calculateDistanceKm(mine, entGrid), 0, 'f', 0));
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
