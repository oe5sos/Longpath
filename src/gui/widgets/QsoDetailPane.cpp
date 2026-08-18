// =================================================================
// src/gui/widgets/QsoDetailPane.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See QsoDetailPane.h for why the network is only
// touched on request.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/QsoDetailPane.h"

#include "core/AppSettings.h"
#include "core/BeamHeading.h"
#include "core/CallsignCache.h"
#include "core/QrzClient.h"
#include "core/QsoConfirmation.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/StationPhoto.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {
namespace {

// ── On by default, which reverses the first decision here ────────────
//
// It shipped off, on the argument that arrow-keying down a log would be
// one QRZ request per row. That argument was already answered by the
// two things sitting either side of it: the delay below means a row
// scrolled past is never looked up at all, and the cache on disk means
// a row looked up once is never looked up again.
//
// What was left of the caution was a pane that showed nothing until you
// found a button — which reads as broken, because a detail pane that
// needs to be asked for details is not doing its job. Off is still
// available for a metered subscription; it is no longer the default.
const QString kAutoLookupKey =
    QStringLiteral("LogbookDetailAutoQrzLookup");

// Long enough that arrow-keying through the log is not a lookup per
// row, short enough that stopping on a contact feels like it answered
// rather than like it was asked.
constexpr int kLookupDelayMs = 400;

QLabel* caption(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    QFont f = l->font();
    f.setPixelSize(9);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                         .arg(QLatin1String(Style::kTextScale)));
    return l;
}

QFrame* rule(QWidget* parent)
{
    auto* f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet(QStringLiteral("QFrame { color: %1; }")
                         .arg(QLatin1String(Style::kInsetBorder)));
    return f;
}

// A confirmation chip. Green when confirmed, amber when somebody asked
// or refused, grey when nothing is recorded — which is NOT the same as
// refused, and the colours have to keep saying so.
void styleBadge(QLabel* l, const QString& text,
                QsoConfirmation::State state)
{
    const char* fg = Style::kTextInactive;
    const char* bg = Style::kInsetBg;
    switch (state) {
    case QsoConfirmation::State::Confirmed:
        fg = Style::kGreenText; bg = Style::kGreenBg;  break;
    case QsoConfirmation::State::Requested:
        fg = Style::kAmberText; bg = Style::kAmberBg;  break;
    case QsoConfirmation::State::No:
    case QsoConfirmation::State::Ignored:
        fg = Style::kTextTertiary; bg = Style::kButtonBg; break;
    case QsoConfirmation::State::Unknown:
        break;
    }
    l->setText(text);
    l->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: %2; border-radius: 6px; "
        "padding: 1px 6px; font-size: 11px; }")
            .arg(QLatin1String(fg), QLatin1String(bg)));
}

QString stateWord(QsoConfirmation::State s)
{
    switch (s) {
    case QsoConfirmation::State::Confirmed: return QStringLiteral("confirmed");
    case QsoConfirmation::State::Requested: return QStringLiteral("requested");
    case QsoConfirmation::State::No:        return QStringLiteral("declined");
    case QsoConfirmation::State::Ignored:   return QStringLiteral("ignored");
    case QsoConfirmation::State::Unknown:   break;
    }
    return QStringLiteral("nothing recorded");
}

QString compassPoint(double deg)
{
    static const char* kPoints[] = {"N", "NNE", "NE", "ENE", "E", "ESE",
                                    "SE", "SSE", "S", "SSW", "SW", "WSW",
                                    "W", "WNW", "NW", "NNW"};
    const int i = int(std::lround(BeamHeading::wrap360(deg) / 22.5)) % 16;
    return QString::fromLatin1(kPoints[i]);
}

} // namespace

QsoDetailPane::QsoDetailPane(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    clearEntry();
}

bool QsoDetailPane::autoLookupEnabled()
{
    return AppSettings::instance().value(kAutoLookupKey, true).toBool();
}

void QsoDetailPane::buildUi()
{
    // By object name, not by class name. A Qt stylesheet type selector
    // matches the metaobject name, which for a class inside a namespace
    // is "NereusSDR--QsoDetailPane" — write the plain class name and
    // the rule silently matches nothing, which is the kind of bug that
    // looks like a theming preference.
    setObjectName(QStringLiteral("qsoDetailPane"));
    setStyleSheet(QStringLiteral("#qsoDetailPane { background: %1; }")
                      .arg(QLatin1String(Style::kPanelBg)));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // ── Portrait ─────────────────────────────────────────────────────
    m_photo = new StationPhoto(this);
    m_photo->setFixedHeight(150);
    m_photo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    col->addWidget(m_photo);

    // ── Who ──────────────────────────────────────────────────────────
    m_call = new QLabel(this);
    {
        QFont f = m_call->font();
        f.setPixelSize(22);
        f.setFamily(QStringLiteral("Menlo"));
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        m_call->setFont(f);
    }
    m_call->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                              .arg(QLatin1String(Style::kAccent)));
    m_call->setTextInteractionFlags(Qt::TextSelectableByMouse);
    col->addWidget(m_call);

    m_name = new QLabel(this);
    m_name->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; }")
                              .arg(QLatin1String(Style::kTextPrimary)));
    m_name->setWordWrap(true);
    col->addWidget(m_name);

    m_where = new QLabel(this);
    m_where->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                               .arg(QLatin1String(Style::kTextScale)));
    m_where->setWordWrap(true);
    col->addWidget(m_where);

    m_stale = new QLabel(this);
    m_stale->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                               .arg(QLatin1String(Style::kAmberWarn)));
    m_stale->setWordWrap(true);
    m_stale->setVisible(false);
    col->addWidget(m_stale);

    // ── Confirmations ────────────────────────────────────────────────
    m_badges = new QWidget(this);
    auto* brow = new QHBoxLayout(m_badges);
    brow->setContentsMargins(0, 0, 0, 0);
    brow->setSpacing(5);
    m_qslLotw = new QLabel(m_badges);
    m_qslCard = new QLabel(m_badges);
    m_qslEqsl = new QLabel(m_badges);
    brow->addWidget(m_qslLotw);
    brow->addWidget(m_qslCard);
    brow->addWidget(m_qslEqsl);
    brow->addStretch(1);
    col->addWidget(m_badges);

    col->addWidget(rule(this));

    // ── Where to point ───────────────────────────────────────────────
    col->addWidget(caption(QStringLiteral("BEAM"), this));

    auto* beam = new QGridLayout;
    beam->setContentsMargins(0, 0, 0, 0);
    beam->setHorizontalSpacing(8);
    beam->setVerticalSpacing(2);
    auto valueLabel = [this]() {
        auto* l = new QLabel(this);
        QFont f = l->font();
        f.setFamily(QStringLiteral("Menlo"));
        f.setPixelSize(11);
        l->setFont(f);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                             .arg(QLatin1String(Style::kTextPrimary)));
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };
    auto keyLabel = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                             .arg(QLatin1String(Style::kTextScale)));
        return l;
    };
    m_shortPath = valueLabel();
    m_longPath  = valueLabel();
    m_distance  = valueLabel();
    beam->addWidget(keyLabel(QStringLiteral("short path")), 0, 0);
    beam->addWidget(m_shortPath, 0, 1);
    beam->addWidget(keyLabel(QStringLiteral("long path")),  1, 0);
    beam->addWidget(m_longPath,  1, 1);
    beam->addWidget(keyLabel(QStringLiteral("distance")),   2, 0);
    beam->addWidget(m_distance,  2, 1);
    beam->setColumnStretch(0, 1);
    col->addLayout(beam);

    auto* turnRow = new QHBoxLayout;
    turnRow->setContentsMargins(0, 0, 0, 0);
    turnRow->setSpacing(5);
    m_turnShort = new QPushButton(this);
    m_turnLong  = new QPushButton(QStringLiteral("long path"), this);
    m_turnLong->setToolTip(QStringLiteral(
        "The other way round the world. On the low bands and at grey "
        "line this is often the stronger signal."));
    turnRow->addWidget(m_turnShort, 1);
    turnRow->addWidget(m_turnLong, 1);
    col->addLayout(turnRow);

    m_travel = new QLabel(this);
    m_travel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                .arg(QLatin1String(Style::kAmberWarn)));
    m_travel->setWordWrap(true);
    col->addWidget(m_travel);

    connect(m_turnShort, &QPushButton::clicked, this, [this]() {
        if (!m_haveEntry) { return; }
        emit turnRotorRequested(BeamHeading::wrap360(m_entry.bearingDeg),
                                m_entry.call);
    });
    connect(m_turnLong, &QPushButton::clicked, this, [this]() {
        if (!m_haveEntry) { return; }
        emit turnRotorRequested(
            BeamHeading::longPath(BeamHeading::wrap360(m_entry.bearingDeg)),
            m_entry.call);
    });

    col->addWidget(rule(this));

    // ── The fields no column shows ───────────────────────────────────
    col->addWidget(caption(QStringLiteral("ALSO IN THE FILE"), this));

    m_extrasBox = new QWidget(this);
    m_extras = new QFormLayout(m_extrasBox);
    m_extras->setContentsMargins(0, 0, 0, 0);
    m_extras->setHorizontalSpacing(8);
    m_extras->setVerticalSpacing(1);
    m_extras->setLabelAlignment(Qt::AlignLeft);
    m_extras->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    col->addWidget(m_extrasBox);

    m_extrasEmpty = new QLabel(this);
    m_extrasEmpty->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }")
            .arg(QLatin1String(Style::kTextInactive)));
    m_extrasEmpty->setWordWrap(true);
    col->addWidget(m_extrasEmpty);

    col->addStretch(1);

    // ── Actions ──────────────────────────────────────────────────────
    m_lookupBtn = new QPushButton(QStringLiteral("Look up on QRZ"), this);
    connect(m_lookupBtn, &QPushButton::clicked,
            this, &QsoDetailPane::requestLookup);
    col->addWidget(m_lookupBtn);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(5);
    m_editBtn    = new QPushButton(QStringLiteral("Edit"), this);
    m_qrzPageBtn = new QPushButton(QStringLiteral("On qrz.com"), this);
    actions->addWidget(m_editBtn, 1);
    actions->addWidget(m_qrzPageBtn, 1);
    col->addLayout(actions);

    connect(m_editBtn, &QPushButton::clicked,
            this, &QsoDetailPane::editRequested);
    connect(m_qrzPageBtn, &QPushButton::clicked, this, [this]() {
        if (!m_haveEntry) { return; }
        // Built here from the callsign, deliberately NOT taken from
        // info.url. That field arrived in somebody else's XML, and a
        // button that opens whatever address a lookup handed back is a
        // button that can be pointed anywhere.
        const QString call = Callsigns::normalized(m_entry.call);
        if (call.isEmpty()) { return; }
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://www.qrz.com/db/")
                 + QString::fromLatin1(QUrl::toPercentEncoding(call))));
    });

    m_lookupDelay = new QTimer(this);
    m_lookupDelay->setSingleShot(true);
    m_lookupDelay->setInterval(kLookupDelayMs);
    connect(m_lookupDelay, &QTimer::timeout,
            this, &QsoDetailPane::requestLookup);
}

void QsoDetailPane::setQrzClient(QrzClient* qrz)
{
    if (m_qrz == qrz) { return; }
    m_qrz = qrz;
    wireQrz();
    refresh();
}

void QsoDetailPane::setCache(CallsignCache* cache)
{
    m_cache = cache;
    refresh();
}

void QsoDetailPane::wireQrz()
{
    if (!m_qrz) { return; }

    connect(m_qrz, &QrzClient::lookupSucceeded, this,
            [this](const QString& call, const CallsignInfo& info) {
        // Remember it whatever is on screen — the next contact with
        // this station, in this session or a year from now, is already
        // answered.
        if (m_cache) {
            m_cache->put(call, info);
            m_cache->save();
        }
        // A reply for a row the operator has already clicked past must
        // not overwrite the one they are looking at.
        if (!m_haveEntry
            || Callsigns::normalized(m_entry.call) != call) { return; }
        applyInfo(info, false);
    });

    connect(m_qrz, &QrzClient::lookupFailed, this,
            [this](const QString& call, QrzClient::Error err,
                   const QString& msg) {
        if (!m_haveEntry
            || Callsigns::normalized(m_entry.call) != call) { return; }
        // Put the button back. Leaving it reading "Looking up…" after a
        // failure is a button that looks busy forever.
        m_lookupBtn->setEnabled(true);
        m_lookupBtn->setText(QStringLiteral("Try QRZ again"));
        switch (err) {
        case QrzClient::Error::NotFound:
            m_photo->showPlaceholder(
                QStringLiteral("%1 isn't in the QRZ database").arg(call));
            break;
        case QrzClient::Error::AuthFailed:
            m_photo->showPlaceholder(
                QStringLiteral("QRZ rejected the login — check the "
                               "account in Tools"));
            break;
        case QrzClient::Error::Network:
            m_photo->showPlaceholder(
                QStringLiteral("Couldn't reach QRZ"));
            break;
        case QrzClient::Error::Provider:
            m_photo->showPlaceholder(
                msg.isEmpty() ? QStringLiteral("QRZ returned no data") : msg);
            break;
        }
    });
}

void QsoDetailPane::setRotorBearing(double deg)
{
    m_haveRotor = !std::isnan(deg);
    m_rotorDeg  = m_haveRotor ? BeamHeading::wrap360(deg) : 0.0;
    refreshBeam();
}

void QsoDetailPane::clearEntry()
{
    m_lookupDelay->stop();
    m_haveEntry = false;
    m_notLogged = false;
    m_entry = LogEntry{};
    m_info  = CallsignInfo{};
    refresh();
}

void QsoDetailPane::setEntry(const LogEntry& entry)
{
    m_lookupDelay->stop();

    if (entry.call.trimmed().isEmpty()) { clearEntry(); return; }

    m_entry = entry;
    m_haveEntry = true;
    m_notLogged = false;
    m_info = CallsignInfo{};

    const QString call = Callsigns::normalized(entry.call);

    bool stale = true;
    if (m_cache && m_cache->contains(call)) {
        m_info = m_cache->get(call);
        stale  = m_cache->isStale(m_info);
    }

    refresh();

    // A hit that is fresh needs nothing. A hit that is stale is shown
    // as it stands and refreshed only if the operator asked for that;
    // a miss waits for the button unless they asked for that too.
    if (m_info.isValid() && !stale) { return; }
    if (autoLookupEnabled()) { m_lookupDelay->start(); }
}

void QsoDetailPane::lookUpNow()
{
    m_lookupDelay->stop();
    // A cached answer needs no request. The photo and the name are
    // already on screen from setEntry; asking again would spend a
    // request to be told the same thing.
    if (m_info.isValid() && m_cache && !m_cache->isStale(m_info)) { return; }
    requestLookup();
}

void QsoDetailPane::showCallsign(const QString& call)
{
    LogEntry bare;
    bare.call = Callsigns::normalized(call);
    if (bare.call.isEmpty()) { clearEntry(); return; }
    setEntry(bare);
    // setEntry clears the flag, so it is set afterwards and the pane
    // redrawn — this is the one path where the subject is somebody who
    // is not in the log, and three parts of the pane say something
    // different because of it.
    m_notLogged = true;
    refresh();
    lookUpNow();
}

void QsoDetailPane::requestLookup()
{
    if (!m_haveEntry) { return; }
    const QString call = Callsigns::normalized(m_entry.call);
    if (call.isEmpty()) { return; }

    if (!m_qrz || !m_qrz->hasCredentials()) {
        m_photo->showPlaceholder(
            QStringLiteral("Add your QRZ account in Tools to see photos"));
        return;
    }
    m_lookupBtn->setEnabled(false);
    m_lookupBtn->setText(QStringLiteral("Looking up…"));
    if (!m_info.isValid()) {
        m_photo->showPlaceholder(QStringLiteral("Looking up %1…").arg(call));
    }
    m_qrz->lookup(call);
}

void QsoDetailPane::applyInfo(const CallsignInfo& info, bool stale)
{
    m_info = info;

    m_lookupBtn->setEnabled(true);
    m_lookupBtn->setText(stale ? QStringLiteral("Refresh from QRZ")
                               : QStringLiteral("Look up on QRZ"));

    if (!info.imageUrl.isEmpty()) {
        m_photo->setUrl(info.imageUrl);
    } else if (info.isValid()) {
        // A lookup happened and there was no portrait. Which of the two
        // reasons applies is worth saying, because one of them is
        // something the operator can change and the other is not.
        m_photo->showPlaceholder(
            QStringLiteral("QRZ has no photo for %1 — or the XML "
                           "subscription that carries it").arg(info.call));
    }

    const QString shown =
        info.isValid() ? info.displayName() : m_entry.name.trimmed();
    m_name->setText(shown);
    m_name->setVisible(!shown.isEmpty());

    QStringList where;
    const QString city = info.isValid() && !info.city.trimmed().isEmpty()
                             ? info.city.trimmed() : m_entry.qth.trimmed();
    const QString land = info.isValid() && !info.country.trimmed().isEmpty()
                             ? info.country.trimmed()
                             : m_entry.country.trimmed();
    const QString grid = !m_entry.gridSquare.trimmed().isEmpty()
                             ? m_entry.gridSquare.trimmed()
                             : info.grid.trimmed();
    if (!city.isEmpty()) { where << city; }
    if (!land.isEmpty()) { where << land; }
    if (!grid.isEmpty()) { where << grid; }
    m_where->setText(where.join(QStringLiteral(" · ")));
    m_where->setVisible(!where.isEmpty());

    if (stale && info.fetchedUtc > 0) {
        const QDateTime when =
            QDateTime::fromSecsSinceEpoch(info.fetchedUtc).toLocalTime();
        m_stale->setText(
            QStringLiteral("Looked up %1 — may be out of date")
                .arg(when.toString(QStringLiteral("d MMM yyyy"))));
        m_stale->setVisible(true);
    } else {
        m_stale->setVisible(false);
    }
}

void QsoDetailPane::refreshBeam()
{
    // A bearing needs both locators. Without them the entry carries a
    // bearing of zero, and offering to turn the antenna due north
    // because a grid square is missing is worse than offering nothing.
    const bool have = m_haveEntry
                   && m_entry.distanceKm > 0.0
                   && !m_entry.gridSquare.trimmed().isEmpty()
                   && !m_entry.myGridSquare.trimmed().isEmpty();

    m_turnShort->setEnabled(have);
    m_turnLong->setEnabled(have);

    if (!have) {
        const QString dash = QStringLiteral("—");
        m_shortPath->setText(dash);
        m_longPath->setText(dash);
        m_distance->setText(dash);
        m_turnShort->setText(QStringLiteral("Turn rotor"));
        if (!m_haveEntry) {
            m_travel->clear();
        } else if (m_notLogged) {
            // Not "your locator and theirs are missing" — there is no
            // contact here to have locators. Say the true thing.
            m_travel->setText(QStringLiteral(
                "Not in your log — no bearing until you work them."));
        } else {
            m_travel->setText(QStringLiteral(
                "No bearing — needs your locator and theirs."));
        }
        m_travel->setVisible(m_haveEntry);
        return;
    }

    const double sp = BeamHeading::wrap360(m_entry.bearingDeg);
    const double lp = BeamHeading::longPath(sp);
    m_shortPath->setText(QStringLiteral("%1° %2")
                             .arg(sp, 0, 'f', 0, QLatin1Char(' '))
                             .arg(compassPoint(sp)));
    m_longPath->setText(QStringLiteral("%1° %2")
                            .arg(lp, 0, 'f', 0, QLatin1Char(' '))
                            .arg(compassPoint(lp)));
    m_distance->setText(QStringLiteral("%1 km")
                            .arg(m_entry.distanceKm, 0, 'f', 0));
    m_turnShort->setText(QStringLiteral("Turn to %1°").arg(sp, 0, 'f', 0));

    if (m_haveRotor) {
        // How far it will actually turn, which on a rotor with an end
        // stop is a different number from the difference between the
        // two headings. BeamHeading::plan knows that; nothing else here
        // does.
        const BeamHeading::Move m =
            BeamHeading::plan(m_rotorDeg, sp, BeamHeading::Stop::None);
        m_travel->setText(QStringLiteral("Rotor at %1° — %2")
                              .arg(m_rotorDeg, 0, 'f', 0)
                              .arg(BeamHeading::advice(m)));
        m_travel->setVisible(true);
    } else {
        m_travel->setVisible(false);
    }
}

void QsoDetailPane::refreshExtras()
{
    // Rebuilt rather than updated: the set of fields differs from one
    // contact to the next, so there is no stable row to update.
    // Deleted here and now, not deleteLater(). A deferred delete leaves
    // the old labels parented and visible while the new rows are added
    // on top of them, and the pane draws one contact's fields over
    // another's for a frame. Nothing being deleted is on the call
    // stack, so the immediate delete is safe.
    while (QLayoutItem* it = m_extras->takeAt(0)) {
        delete it->widget();
        delete it;
    }

    if (!m_haveEntry) {
        m_extrasBox->setVisible(false);
        m_extrasEmpty->setVisible(false);
        return;
    }

    int shown = 0;
    for (const auto& kv : m_entry.extras) {
        // APP_NEREUS_QRZUP is ours and already has a column. Showing it
        // here as well would be the same fact twice, in two notations.
        if (LogEntry::modelsAdifField(kv.first.toUpper())) { continue; }
        if (kv.second.trimmed().isEmpty()) { continue; }

        auto* k = new QLabel(kv.first, m_extrasBox);
        k->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                             .arg(QLatin1String(Style::kTextScale)));
        auto* v = new QLabel(kv.second, m_extrasBox);
        QFont f = v->font();
        f.setFamily(QStringLiteral("Menlo"));
        f.setPixelSize(11);
        v->setFont(f);
        v->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                             .arg(QLatin1String(Style::kTextPrimary)));
        v->setWordWrap(true);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_extras->addRow(k, v);
        ++shown;
    }

    m_extrasBox->setVisible(shown > 0);
    m_extrasEmpty->setVisible(shown == 0);
    if (shown == 0) {
        m_extrasEmpty->setText(m_notLogged
            ? QStringLiteral("No contact with this station in the log.")
            : QStringLiteral("Nothing beyond the columns. Contacts "
                             "imported from another logger usually carry "
                             "DXCC, zones and QSL state here."));
    }
}

void QsoDetailPane::refresh()
{
    if (!m_haveEntry) {
        m_call->setText(QStringLiteral("—"));
        m_name->setVisible(false);
        m_where->setVisible(false);
        m_stale->setVisible(false);
        m_badges->setVisible(false);
        m_photo->showPlaceholder(QStringLiteral("Select a contact"));
        m_lookupBtn->setEnabled(false);
        m_lookupBtn->setText(QStringLiteral("Look up on QRZ"));
        m_editBtn->setEnabled(false);
        m_qrzPageBtn->setEnabled(false);
        refreshBeam();
        refreshExtras();
        return;
    }

    m_call->setText(m_entry.call);
    // Nothing to confirm when there is no contact. Three grey chips
    // reading "nothing recorded" would be technically true and would
    // look like a QSO that nobody ever confirmed.
    m_badges->setVisible(!m_notLogged);
    m_editBtn->setEnabled(!m_notLogged);
    m_qrzPageBtn->setEnabled(true);
    m_lookupBtn->setEnabled(true);

    const auto lotw = QsoConfirmation::lotw(m_entry);
    const auto card = QsoConfirmation::qslCard(m_entry);
    const auto eqsl = QsoConfirmation::eqsl(m_entry);
    styleBadge(m_qslLotw, QStringLiteral("LoTW"), lotw);
    styleBadge(m_qslCard, QStringLiteral("card"),  card);
    styleBadge(m_qslEqsl, QStringLiteral("eQSL"),  eqsl);
    m_qslLotw->setToolTip(QStringLiteral("LOTW_QSL_RCVD: %1")
                              .arg(stateWord(lotw)));
    m_qslCard->setToolTip(QStringLiteral("QSL_RCVD: %1")
                              .arg(stateWord(card)));
    m_qslEqsl->setToolTip(QStringLiteral("EQSL_QSL_RCVD: %1")
                              .arg(stateWord(eqsl)));
    m_badges->setToolTip(QsoConfirmation::describe(m_entry));

    // Whatever the cache had, or nothing — either way the pane reads
    // the log entry for everything the lookup does not supply, so a
    // contact with no QRZ answer still shows its name and QTH.
    applyInfo(m_info, m_info.isValid() && m_cache
                          && m_cache->isStale(m_info));

    if (!m_info.isValid()) {
        m_photo->showPlaceholder(
            m_qrz && m_qrz->hasCredentials()
                ? QStringLiteral("No lookup yet for %1").arg(m_entry.call)
                : QStringLiteral("Add your QRZ account in Tools to see "
                                 "photos"));
    }

    refreshBeam();
    refreshExtras();
}

} // namespace NereusSDR
