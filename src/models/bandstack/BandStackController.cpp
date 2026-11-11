// =================================================================
// src/models/bandstack/BandStackController.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-Fassung der Upstream-Steuerung; siehe den Kopf
// von BandStackController.h. Die Thetis-Regeln stehen in BandStackStore.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 - Neu, nach Upstream NereusSDR (2026-11-05, J.J. Boyd
//                 KG4VCF). Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#include "models/bandstack/BandStackController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <utility>

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/safety/RegionSetting.h"
#include "models/BandDefaults.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackSeeds.h"

namespace Longpath {

namespace {

// Hoechstens einmal je Sekunde nach einer Aenderung geschrieben.
constexpr int kSaveCoalesceMs = 1000;
// Ein Verfallsdurchgang je Minute.
constexpr int kExpiryPassMs = 60000;
// Ein Verfall laesst sich 5 Sekunden lang zuruecknehmen.
constexpr qint64 kExpiryUndoMs = 5000;
// Das Duplikatfenster der Ablage: innerhalb 100 Hz ist dieselbe Stelle.
constexpr qint64 kSameSpotHz = 100;

const QString kNoEntry = QStringLiteral("That band stack entry no longer exists.");

void setRefusal(QString* refusal, const QString& text)
{
    if (refusal != nullptr) {
        *refusal = text;
    }
}

// SliceModel::modeFromName antwortet USB auf einen Namen, den es nicht
// kennt; ein Name zaehlt nur, wenn er unveraendert zurueckkommt.
std::optional<DSPMode> knownMode(const QString& name)
{
    const DSPMode mode = SliceModel::modeFromName(name);
    if (SliceModel::modeName(mode) != name) {
        return std::nullopt;
    }
    return mode;
}

// Die Region aus Setup → General → Region. Ohne Eintrag zeigt die Seite
// „United States" (GeneralOptionsPage, wie Thetis), also gilt das auch hier.
safety::Region currentRegion()
{
    const safety::RegionChoice choice = safety::configuredRegion();
    return choice.configured ? choice.region : safety::Region::UnitedStates;
}

// Ein Amateurband (das Bandgedaechtnis kennt nur diese, GEN und WWV).
bool isHamBand(Band band)
{
    double low = 0.0;
    double high = 0.0;
    return bandRangeHz(band, low, high);
}

} // namespace

// Was ein Abruf taete, auf einer Kopie der Ablage ausgerechnet: ein Plan
// aendert nichts, und ein abgelehnter Abruf laesst die echte Ablage stehen.
struct BandStackController::Resolution {
    bool valid{false};
    QString refusal;
    SliceModel* slice{nullptr};
    Band band{Band::GEN};
    qint64 frequencyHz{0};
    Band leftBand{Band::GEN};
    BandStackSpot leftSpot;
    QString mode;
    int filterLow{0};
    int filterHigh{0};
    QString entryId;
    QString report;
    BandStackStore after;
};

BandStackController::BandStackController(RadioModel* model)
    : QObject(model)
    , m_model(model)
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kSaveCoalesceMs);
    connect(m_saveTimer, &QTimer::timeout, this, &BandStackController::writeDocument);

    m_expiryTimer = new QTimer(this);
    m_expiryTimer->setInterval(kExpiryPassMs);
    connect(m_expiryTimer, &QTimer::timeout, this, &BandStackController::runExpiry);
    m_expiryTimer->start();

    connect(m_model, &RadioModel::sliceAdded, this, [this](int) { wireSlices(); });
    connect(m_model, &RadioModel::currentRadioChanged, this,
            [this](const RadioInfo&) { loadDocument(); });
    connect(m_model, &RadioModel::connectionStateChanged, this,
            [this](ConnectionState) {
        loadDocument();
        emit changed();
    });
    // Eine Scheibe beim Verbinden dorthin zurueckzustellen, wo sie war,
    // ist kein Stimmen auf ein Band.
    connect(m_model, &RadioModel::sliceStateRestored, this, [this](int sliceId) {
        const SliceModel* slice = m_model->sliceById(sliceId);
        if (slice == nullptr) {
            return;
        }
        m_onBand[slice->sliceLetter()] = stackBandForSlice(slice);
        if (m_detached.remove(slice->sliceLetter())) {
            emit changed();
        }
    });
    if (QCoreApplication::instance() != nullptr) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
                &BandStackController::flush);
    }

    wireSlices();
    loadDocument();
}

BandStackController::~BandStackController()
{
    writeDocument();
}

bool BandStackController::isReady() const
{
    return !m_mac.isEmpty() && m_model->currentRadioMac() == m_mac;
}

QString BandStackController::unavailableReason() const
{
    if (isReady()) {
        return QString();
    }
    return QStringLiteral("Connect to a radio to use band stacks.");
}

bool BandStackController::press(int sliceId, Band band, BandStackDirection direction,
                                const BandStackView& view, QString* refusal)
{
    Request request;
    request.kind = Kind::Press;
    request.sliceId = sliceId;
    request.band = band;
    request.direction = direction;
    request.view = view;
    return performRecall(request, refusal);
}

bool BandStackController::step(int sliceId, BandStackDirection direction,
                               const BandStackView& view, QString* refusal)
{
    Request request;
    request.kind = Kind::Step;
    request.sliceId = sliceId;
    request.direction = direction;
    request.view = view;
    return performRecall(request, refusal);
}

bool BandStackController::goTo(int sliceId, const QString& entryId, const BandStackView& view,
                               QString* refusal)
{
    Request request;
    request.kind = Kind::GoTo;
    request.sliceId = sliceId;
    request.entryId = entryId;
    request.view = view;
    return performRecall(request, refusal);
}

Band BandStackController::stackBandFor(int sliceId) const
{
    const SliceModel* slice = m_model->sliceById(sliceId);
    return slice != nullptr ? stackBandForSlice(slice) : Band::GEN;
}

// Das Band der Scheibe fuer den Stapel: das Band ihres letzten Abrufs,
// solange sie im Bereich dieser Frequenz bleibt; dann eine Stelle, die
// genau diese Frequenz haelt (dasselbe nach einem Neustart); dann das
// Band, in das ihre Frequenz faellt.
Band BandStackController::stackBandForSlice(const SliceModel* slice) const
{
    const double hz = slice->frequency();
    const Band grid = BandDefaults::bandGridBandFromFrequency(hz);
    const QChar letter = slice->sliceLetter();

    const auto recalled = m_recalledBand.constFind(letter);
    if (recalled != m_recalledBand.constEnd() && recalled->gridBand == grid) {
        return recalled->band;
    }

    const auto holdsThisFrequency = [&](Band band) {
        const BandStackEntry* entry = m_store.entry(m_store.place(letter, band));
        return entry != nullptr && qAbs(entry->frequencyHz - qRound64(hz)) < kSameSpotHz;
    };
    if (holdsThisFrequency(grid)) {
        return grid;
    }
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band band = static_cast<Band>(i);
        if (holdsThisFrequency(band)) {
            return band;
        }
    }
    return grid;
}

QString BandStackController::shownPlace(int sliceId, Band band) const
{
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr) {
        return QString();
    }
    const QChar letter = slice->sliceLetter();
    if (m_detached.contains(letter) && m_onBand.value(letter, Band::GEN) == band) {
        return QString();
    }
    return m_store.place(letter, band);
}

QString BandStackController::positionText(int sliceId) const
{
    if (!isReady()) {
        return QString();
    }
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr) {
        return QString();
    }
    const Band band = stackBandForSlice(slice);
    const QList<BandStackEntry> entries = m_store.entries(band);
    const QString place = shownPlace(sliceId, band);
    qsizetype position = -1;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        if (entries.at(i).id == place) {
            position = i;
            break;
        }
    }
    const QString number = position >= 0 ? QString::number(position + 1) : QStringLiteral("-");
    return QStringLiteral("%1 %2/%3").arg(bandLabel(band), number).arg(entries.size());
}

QSet<QChar> BandStackController::slicesOnBand(Band band) const
{
    QSet<QChar> letters;
    for (const SliceModel* slice : m_model->slices()) {
        // Eine danebenstehende Scheibe ist auf dem Band, aber nicht auf
        // ihrer Stelle dort.
        if (slice != nullptr && stackBandForSlice(slice) == band
            && !m_detached.contains(slice->sliceLetter())) {
            letters.insert(slice->sliceLetter());
        }
    }
    return letters;
}

// Fehlt die Ansicht, behaelt der Eintrag seine Werte.
BandStackSpot BandStackController::currentSpot(const SliceModel* slice, const BandStackView& view,
                                               const BandStackEntry* existing) const
{
    const qint64 hz = qRound64(slice->frequency());
    BandStackSpot spot;
    spot.frequencyHz = hz;
    spot.mode = SliceModel::modeName(slice->dspMode());
    spot.filterLowHz = slice->filterLow();
    spot.filterHighHz = slice->filterHigh();
    spot.centerHz = view.centerHz.value_or(existing != nullptr ? existing->centerHz : hz);
    spot.spanHz = view.spanHz.value_or(existing != nullptr ? existing->spanHz : 0);
    return spot;
}

std::optional<BandStackSpot> BandStackController::lastSpot(const SliceModel* slice, Band band) const
{
    const auto bands = m_lastUsed.constFind(slice->sliceLetter());
    if (bands != m_lastUsed.constEnd()) {
        const auto spot = bands->constFind(band);
        if (spot != bands->constEnd()) {
            return *spot;
        }
    }
    const std::optional<SliceModel::SavedBandSpot> saved = slice->savedBandSpot(band);
    if (!saved.has_value()) {
        return std::nullopt;
    }
    BandStackSpot spot;
    spot.frequencyHz = qRound64(saved->frequencyHz);
    spot.mode = SliceModel::modeName(saved->mode);
    spot.filterLowHz = saved->filterLow;
    spot.filterHighHz = saved->filterHigh;
    spot.centerHz = spot.frequencyHz;
    return spot;
}

BandStackController::Resolution BandStackController::resolve(const Request& request) const
{
    Resolution r;
    if (!isReady()) {
        r.refusal = unavailableReason();
        return r;
    }
    SliceModel* slice = m_model->sliceById(request.sliceId);
    if (slice == nullptr) {
        r.refusal = QStringLiteral("That slice is not open.");
        return r;
    }
    r.slice = slice;
    const QChar letter = slice->sliceLetter();
    const Band current = stackBandForSlice(slice);

    Band band = request.band;
    if (request.kind == Kind::Step) {
        band = current;
    } else if (request.kind == Kind::GoTo) {
        const BandStackEntry* entry = m_store.entry(request.entryId);
        if (entry == nullptr) {
            r.refusal = kNoEntry;
            return r;
        }
        band = entry->band;
    }
    r.band = band;

    if (slice->locked()) {
        r.refusal = QStringLiteral("Band %1 ignored: the slice is locked. Unlock it to change bands.")
                        .arg(bandLabel(band));
        return r;
    }
    if (m_model->txBoundSlice() == slice && (m_model->mox() || m_model->tune())) {
        r.refusal = QStringLiteral("Band %1 ignored: Slice %2 is transmitting.")
                        .arg(bandLabel(band), QString(letter));
        return r;
    }
    if (band == Band::XVTR) {
        // In den Worten von RadioModel::onBandButtonClicked.
        r.refusal = QStringLiteral("Band %1 ignored: transverter config not yet supported")
                        .arg(bandLabel(band));
        return r;
    }

    // Erst speichern, damit ein Druck, der auf der eben verlassenen Stelle
    // landet (ein Eintrag, Umlauf), dorthin geht, was eben gespeichert wurde.
    BandStackStore after = m_store;
    const QString placeId = after.place(letter, current);
    const BandStackEntry* leaving = after.entry(placeId);
    r.leftBand = current;
    r.leftSpot = currentSpot(slice, request.view, leaving);
    // Zur eigenen Stelle gehen heisst zu ihr zurueck: es gibt nichts zu
    // speichern, sonst ersetzte die Stelle, zu der die Scheibe gewandert
    // ist, die verlangte.
    const bool goingBack = request.kind == Kind::GoTo && request.entryId == placeId;
    // Upstream, J.J. Boyd am 2026-11-06: eine Scheibe, die auf dieses Band
    // gestimmt hat, kam nie auf ihre Stelle hier; ein Verlassen schreibt
    // nichts hinein.
    const bool detached = m_detached.contains(letter);
    // Longpath (Betreiber am 2026-10-29): eine Stelle ausserhalb des Bandes
    // der eingestellten Region kommt nicht in den Stapel -- sonst holte der
    // naechste Druck eine Frequenz zurueck, auf der nicht gesendet werden
    // darf.
    const Band hamBand = bandFromFrequency(slice->frequency());
    const bool outsideBand =
        isHamBand(hamBand)
        && BandDefaults::frequenzImBand(hamBand, slice->frequency()) != slice->frequency();
    if (!goingBack && !detached && !outsideBand) {
        after.saveOnLeave(letter, current, r.leftSpot, nowMs(), slicesOnBand(current));
    }

    BandStackTarget target;
    switch (request.kind) {
    case Kind::Press:
        // Daneben stehend fuehrt ein Druck auf dieses Band zur Stelle,
        // nicht einen Schritt darueber hinaus.
        target = detached && band == current
            ? after.planArrive(letter, band)
            : after.planPress(letter, current, band, request.direction);
        break;
    case Kind::Step:
        target = after.planStep(letter, band, request.direction);
        break;
    case Kind::GoTo:
        target = {BandStackTarget::Entry, request.entryId, band};
        break;
    }

    std::optional<BandStackSpot> last;
    if (target.kind == BandStackTarget::LastUsed) {
        last = lastSpot(slice, band);
        if (!last.has_value()) {
            target = after.planCurrent(letter, band);
        }
    }

    switch (target.kind) {
    case BandStackTarget::Entry: {
        const BandStackEntry* entry = after.entry(target.entryId);
        if (entry == nullptr) {
            r.refusal = kNoEntry;
            return r;
        }
        if (!knownMode(entry->mode).has_value()) {
            r.refusal = QStringLiteral("That band stack entry has an unknown mode.");
            return r;
        }
        r.frequencyHz = entry->frequencyHz;
        r.mode = entry->mode;
        r.filterLow = entry->filterLowHz;
        r.filterHigh = entry->filterHighHz;
        r.entryId = entry->id;
        const QList<BandStackEntry> onBand = after.entries(band);
        qsizetype position = 0;
        while (position < onBand.size() && onBand.at(position).id != entry->id) {
            ++position;
        }
        r.report = QStringLiteral("%1: %2 %3, %4 of %5")
                       .arg(bandLabel(band),
                            BandStackStore::displayFrequency(entry->frequencyHz), entry->mode)
                       .arg(position + 1)
                       .arg(onBand.size());
        const QString id = entry->id;
        after.setPlace(letter, band, id);
        after.markUsed(id, nowMs());
        break;
    }
    case BandStackTarget::LastUsed:
        r.frequencyHz = last->frequencyHz;
        r.mode = last->mode;
        r.filterLow = last->filterLowHz;
        r.filterHigh = last->filterHighHz;
        r.report = QStringLiteral("%1: %2 %3")
                       .arg(bandLabel(band), BandStackStore::displayFrequency(last->frequencyHz),
                            last->mode);
        // Kein Eintrag, also keine Stelle auf diesem Band.
        after.clearPlace(letter, band);
        break;
    case BandStackTarget::EmptyBand: {
        const std::optional<double> middle = BandDefaults::bandMiddleHz(band);
        if (!middle.has_value()) {
            r.refusal = QStringLiteral("Band %1 ignored: it has no starting frequency")
                            .arg(bandLabel(band));
            return r;
        }
        // Die Mitte der Region-2-Tabelle kann in der eingestellten Region
        // ausserhalb liegen (60 m); dann der Anfang des Bandes.
        r.frequencyHz = qRound64(BandDefaults::frequenzImBand(band, *middle));
        const BandSeed seed = BandDefaults::seedFor(band);
        if (seed.valid) {
            r.mode = SliceModel::modeName(seed.mode);
        }
        r.report = QStringLiteral("%1: empty stack, tuned to the middle of the band")
                       .arg(bandLabel(band));
        break;
    }
    case BandStackTarget::None:
        r.refusal = QStringLiteral("%1 has no band stack entries to step to.").arg(bandLabel(band));
        return r;
    }

    r.after = std::move(after);
    r.valid = true;
    return r;
}

bool BandStackController::performRecall(const Request& request, QString* refusal)
{
    followRegion();
    Resolution r = resolve(request);
    if (!r.valid) {
        setRefusal(refusal, r.refusal);
        return false;
    }

    SliceModel* slice = r.slice;
    const double targetHz = static_cast<double>(r.frequencyHz);

    m_inRecall = true;
    const auto endRecall = qScopeGuard([this] { m_inRecall = false; });
    stopDwell(slice->sliceIndex());

    // Das Bandgedaechtnis: wie jeder Bandwechsel in Longpath unter dem
    // Band aus bandFromFrequency (RadioModel::onBandButtonClicked).
    const Band leftMemory = bandFromFrequency(slice->frequency());
    const Band targetMemory = bandFromFrequency(targetHz);
    slice->saveToSettings(leftMemory);
    const bool memoryChanges = targetMemory != leftMemory;
    if (memoryChanges) {
        // Alles ausser Frequenz, Betriebsart und Filter; die bringt der
        // Eintrag.
        slice->restoreFromSettings(targetMemory, false);
    }

    slice->setFrequency(targetHz);
    if (!r.mode.isEmpty()) {
        if (const std::optional<DSPMode> mode = knownMode(r.mode)) {
            // Betreiber am 2026-10-23: 160, 80 und 40 m bei der Bandauswahl
            // immer LSB. Kippt das Seitenband, bringt setDspMode die
            // Vorgabebreite der neuen Seite; der Filter des Eintrags gehoert
            // zur alten.
            const DSPMode gerichtet = isHamBand(targetMemory)
                ? BandDefaults::korrigiereSeitenband(targetMemory, *mode)
                : *mode;
            slice->setDspMode(gerichtet);
            if (gerichtet == *mode && (r.filterLow != 0 || r.filterHigh != 0)) {
                slice->setFilter(r.filterLow, r.filterHigh);
            }
        }
    } else if (r.filterLow != 0 || r.filterHigh != 0) {
        slice->setFilter(r.filterLow, r.filterHigh);
    }
    if (memoryChanges) {
        // Wie onBandButtonClicked: die wiederhergestellte Abtastrate auch
        // am Geraet, nicht nur im Menue.
        m_model->applyRestoredSampleRate(slice);
    }

    // Hat die Scheibe die Frequenz nicht angenommen, wird nichts gespeichert.
    if (qRound64(slice->frequency()) != r.frequencyHz) {
        setRefusal(refusal, QStringLiteral("Slice %1 stayed on %2.")
                                .arg(QString(slice->sliceLetter()),
                                     BandStackStore::displayFrequency(qRound64(slice->frequency()))));
        return false;
    }
    slice->saveToSettings(targetMemory);

    const QChar letter = slice->sliceLetter();
    m_store = std::move(r.after);
    // Das Verlassen kann den Eintrag weit verschoben haben (am Geraet am
    // 2026-11-11: 14.010 CWU -> 14.254.300 USB stand danach als zweite
    // Zeile zwischen den CW-Eintraegen).
    nachFrequenzOrdnen(r.leftBand);
    m_lastUsed[letter][r.leftBand] = r.leftSpot;
    m_lastUsed[letter][r.band] = currentSpot(slice, {}, nullptr);
    m_recalledBand[letter] = {r.band, BandDefaults::bandGridBandFromFrequency(targetHz)};
    m_onBand[letter] = r.band;
    m_detached.remove(letter);

    markChanged();
    emit recalled(slice->sliceIndex(), r.report);
    return true;
}

bool BandStackController::add(int sliceId, const BandStackView& view, QString* refusal)
{
    if (!isReady()) {
        setRefusal(refusal, unavailableReason());
        return false;
    }
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr) {
        setRefusal(refusal, QStringLiteral("That slice is not open."));
        return false;
    }
    const QString id = m_store.add(slice->sliceLetter(), stackBandForSlice(slice),
                                   currentSpot(slice, view, nullptr), nowMs(), refusal);
    if (id.isEmpty()) {
        return false;
    }
    // Der neue Eintrag ist die Stelle der Scheibe, und sie steht darauf.
    m_detached.remove(slice->sliceLetter());
    markChanged();
    return true;
}

bool BandStackController::saveHere(int sliceId, const QString& entryId, const BandStackView& view,
                                   QString* refusal)
{
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr) {
        setRefusal(refusal, QStringLiteral("That slice is not open."));
        return false;
    }
    const BandStackEntry* entry = m_store.entry(entryId);
    if (entry == nullptr) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    if (entry->locked) {
        setRefusal(refusal, QStringLiteral("That entry is locked. Unlock it to save here."));
        return false;
    }
    if (entry->band != stackBandForSlice(slice)) {
        setRefusal(refusal, QStringLiteral("Slice %1 is not on %2.")
                                .arg(QString(slice->sliceLetter()), bandLabel(entry->band)));
        return false;
    }
    m_store.saveHere(slice->sliceLetter(), entryId, currentSpot(slice, view, entry), nowMs());
    nachFrequenzOrdnen(stackBandForSlice(slice));
    m_detached.remove(slice->sliceLetter());
    markChanged();
    return true;
}

bool BandStackController::edit(const QString& id, const std::optional<QString>& name,
                               std::optional<bool> locked, QString* refusal)
{
    if (!m_store.edit(id, name, locked)) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::remove(const QString& id, QString* refusal)
{
    if (!m_store.remove(id)) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::reorder(Band band, const QStringList& ids, QString* refusal)
{
    if (!m_store.reorder(band, ids)) {
        setRefusal(refusal,
                   QStringLiteral("That list does not match the entries on %1.").arg(bandLabel(band)));
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::setReturn(Band band, BandStackReturnMode mode, const QString& entryId,
                                    QString* refusal)
{
    if (!m_store.setReturn(band, mode, entryId)) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::clear(std::optional<Band> band, QString* /*refusal*/)
{
    if (m_store.clearUnlocked(band) > 0) {
        markChanged();
    }
    return true;
}

bool BandStackController::reseed(QString* /*refusal*/)
{
    m_store.replaceWithSeeds(seedEntries());
    m_store.setRegion(regionName(currentRegion()));
    markChanged();
    return true;
}

bool BandStackController::options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                                  std::optional<int> dwellSeconds, QString* refusal)
{
    if (expiryMinutes.has_value() && *expiryMinutes != 0 && *expiryMinutes != 5
        && *expiryMinutes != 15 && *expiryMinutes != 30 && *expiryMinutes != 60) {
        setRefusal(refusal, QStringLiteral("Auto-expiry can be off, or 5, 15, 30 or 60 minutes."));
        return false;
    }
    if (dwellSeconds.has_value() && *dwellSeconds != 0 && *dwellSeconds != 10
        && *dwellSeconds != 30 && *dwellSeconds != 60) {
        setRefusal(refusal, QStringLiteral("Auto-save can be off, or 10, 30 or 60 seconds."));
        return false;
    }

    BandStackOptions next = m_store.options();
    next.dupeProtection = dupes.value_or(next.dupeProtection);
    next.expiryMinutes = expiryMinutes.value_or(next.expiryMinutes);
    next.dwellSeconds = dwellSeconds.value_or(next.dwellSeconds);
    m_store.setOptions(next);
    if (next.dwellSeconds == 0) {
        for (QTimer* timer : std::as_const(m_dwellTimers)) {
            timer->stop();
        }
    }
    markChanged();
    return true;
}

bool BandStackController::restoreExpired(const QString& batchId, QString* refusal)
{
    if (batchId.isEmpty() || batchId != m_lastExpiryBatchId
        || nowMs() - m_lastExpiryMs > kExpiryUndoMs) {
        setRefusal(refusal, QStringLiteral("Those entries can no longer be brought back."));
        return false;
    }

    // Ihre Zeit beginnt neu, damit der naechste Durchgang sie nicht gleich
    // wieder nimmt.
    QList<BandStackEntry> entries = m_lastExpired;
    const qint64 now = nowMs();
    for (BandStackEntry& entry : entries) {
        entry.lastUsedMs = now;
    }
    m_store.restore(entries);
    for (auto it = m_lastExpiredSpecific.constBegin(); it != m_lastExpiredSpecific.constEnd(); ++it) {
        m_store.setReturn(it.key(), BandStackReturnMode::Specific, it.value());
    }

    m_lastExpired.clear();
    m_lastExpiredSpecific.clear();
    m_lastExpiryBatchId.clear();
    m_lastExpiryCount = 0;
    markChanged();
    return true;
}

bool BandStackController::dwellArmedForTest(int sliceId) const
{
    const QTimer* timer = m_dwellTimers.value(sliceId, nullptr);
    return timer != nullptr && timer->isActive();
}

QList<BandStackEntry> BandStackController::seedEntries() const
{
    const FilterPresetStore* presets = m_model->filterPresetStore();
    QList<BandStackEntry> entries;
    const QList<SeedRow> rows = seedsForRegion(currentRegion());
    entries.reserve(rows.size());
    for (const SeedRow& row : rows) {
        const std::optional<DSPMode> mode = knownMode(row.mode);
        if (!mode.has_value()) {
            qCWarning(lcMemories) << "Band stack seed row not loaded, unknown mode:" << row.mode
                                  << row.frequencyHz;
            continue;
        }

        // FN ist der N-te Platz der Filtervorgaben der Betriebsart: erst
        // die des Bedieners, dann die eingebaute, wenn jene keinen hat.
        const int slot = row.filterNumber - 1;
        std::optional<std::pair<int, int>> filter;
        if (presets != nullptr) {
            const QList<FilterPreset> table = presets->presetsForMode(*mode);
            if (slot >= 0 && slot < table.size()) {
                filter = std::make_pair(table.at(slot).low, table.at(slot).high);
            }
        }
        if (!filter.has_value()) {
            const QList<std::pair<int, int>> table = SliceModel::presetsForMode(*mode);
            if (slot >= 0 && slot < table.size()) {
                filter = table.at(slot);
            }
        }
        if (!filter.has_value()) {
            qCWarning(lcMemories) << "Band stack seed row not loaded, no filter:" << row.mode
                                  << row.filterNumber << row.frequencyHz;
            continue;
        }

        BandStackEntry entry;
        entry.band = row.band;
        entry.frequencyHz = row.frequencyHz;
        entry.mode = row.mode;
        entry.filterLowHz = filter->first;
        entry.filterHighHz = filter->second;
        entry.centerHz = row.frequencyHz;
        entry.locked = row.locked;
        entries.append(entry);
    }
    return entries;
}

qint64 BandStackController::nowMs() const
{
    return m_clockForTest ? m_clockForTest() : QDateTime::currentMSecsSinceEpoch();
}

void BandStackController::wireSlices()
{
    for (SliceModel* slice : m_model->slices()) {
        if (slice == nullptr || m_wiredSlices.contains(slice)) {
            continue;
        }
        m_wiredSlices.insert(slice);
        const auto tuned = [this, slice] { onSliceTuned(slice); };
        connect(slice, &SliceModel::frequencyChanged, this, tuned);
        connect(slice, &SliceModel::dspModeChanged, this, tuned);
        connect(slice, &SliceModel::filterChanged, this, tuned);
        const int sliceId = slice->sliceIndex();
        connect(slice, &QObject::destroyed, this, [this, slice, sliceId] {
            m_wiredSlices.remove(slice);
            stopDwell(sliceId);
        });
        if (m_adoptOpeningSlices) {
            adoptSavedSpots(slice);
            markChanged();
        }
        // Ein Buchstabe kann als neue Scheibe wiederkommen, die noch nichts
        // ueberquert hat.
        m_onBand[slice->sliceLetter()] = stackBandForSlice(slice);
        m_detached.remove(slice->sliceLetter());
    }
}

// Stimmen, ueber eine Bandkante oder nicht, merkt nur die zuletzt benutzte
// Stelle dieses Bandes und zieht das Verweilen auf. Es speichert keinen
// Eintrag und bewegt keine Stelle. Wer so auf ein anderes Band kommt,
// steht dort neben seiner Stelle.
void BandStackController::onSliceTuned(SliceModel* slice)
{
    if (m_inRecall) {
        return;
    }
    const QChar letter = slice->sliceLetter();
    const auto recalled = m_recalledBand.constFind(letter);
    if (recalled != m_recalledBand.constEnd()
        && recalled->gridBand != BandDefaults::bandGridBandFromFrequency(slice->frequency())) {
        m_recalledBand.remove(letter);
    }
    const Band band = stackBandForSlice(slice);
    m_lastUsed[letter][band] = currentSpot(slice, {}, nullptr);
    armDwell(slice);

    if (m_onBand.value(letter, band) != band) {
        m_onBand[letter] = band;
        m_detached.insert(letter);
        // Was die Anzeige zeigt, hat sich geaendert; das Dokument nicht.
        emit changed();
    }
}

void BandStackController::armDwell(const SliceModel* slice)
{
    const int seconds = m_store.options().dwellSeconds;
    if (seconds <= 0) {
        return;
    }
    const int sliceId = slice->sliceIndex();
    QTimer* timer = m_dwellTimers.value(sliceId, nullptr);
    if (timer == nullptr) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, sliceId] { runDwell(sliceId); });
        m_dwellTimers.insert(sliceId, timer);
    }
    timer->start(seconds * 1000);
}

void BandStackController::stopDwell(int sliceId)
{
    QTimer* timer = m_dwellTimers.value(sliceId, nullptr);
    if (timer != nullptr) {
        timer->stop();
    }
}

void BandStackController::runDwell(int sliceId)
{
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr || m_store.options().dwellSeconds <= 0 || !isReady()) {
        return;
    }
    if (m_store.addDwell(stackBandForSlice(slice), currentSpot(slice, {}, nullptr), nowMs())) {
        markChanged();
    }
}

void BandStackController::runExpiry()
{
    if (!isReady()) {
        return;
    }
    QMap<Band, QString> specific;
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band band = static_cast<Band>(i);
        const QString id = m_store.specificId(band);
        if (!id.isEmpty()) {
            specific.insert(band, id);
        }
    }

    const QList<BandStackEntry> removed = m_store.expire(nowMs());
    if (removed.isEmpty()) {
        return;
    }

    // Ein Eintrag, der geht, nimmt das „Always return here" seines Bandes
    // mit; das Zuruecknehmen muss es wiederbringen.
    m_lastExpiredSpecific.clear();
    for (const BandStackEntry& entry : removed) {
        if (specific.value(entry.band) == entry.id) {
            m_lastExpiredSpecific.insert(entry.band, entry.id);
        }
    }
    m_lastExpired = removed;
    m_lastExpiryBatchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_lastExpiryCount = static_cast<int>(removed.size());
    m_lastExpiryMs = nowMs();
    markChanged();
}

QString BandStackController::documentKey() const
{
    return QStringLiteral("hardware/%1/bandstack/Document").arg(m_mac);
}

void BandStackController::loadDocument()
{
    const QString mac = m_model->currentRadioMac();
    if (mac.isEmpty() || mac == m_mac) {
        return;
    }

    // Das gehende Geraet behaelt sein Dokument, und ein Wiederverbinden
    // liest zurueck, was geschrieben wurde.
    writeDocument();
    m_saveTimer->stop();
    m_mac = mac;
    m_lastUsed.clear();
    m_recalledBand.clear();
    m_detached.clear();
    m_adoptOpeningSlices = false;
    m_adopted.clear();
    m_lastExpired.clear();
    m_lastExpiredSpecific.clear();
    m_lastExpiryBatchId.clear();
    m_lastExpiryCount = 0;
    for (QTimer* timer : std::as_const(m_dwellTimers)) {
        timer->stop();
    }

    auto& settings = AppSettings::instance();
    const QString raw = settings.value(documentKey()).toString();
    if (raw.isEmpty()) {
        startFresh();
    } else {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(raw.toUtf8(), &error);
        BandStackStore loaded;
        if (error.error == QJsonParseError::NoError && document.isObject()
            && loaded.fromJson(document.object())) {
            m_store = std::move(loaded);
        } else {
            qCWarning(lcMemories) << "Band stack: the saved document for" << m_mac
                                  << "could not be read. It was kept as Document.bad and the "
                                     "stack starts again from the region's seeds.";
            settings.setValue(documentKey() + QStringLiteral(".bad"), raw);
            startFresh();
        }
    }

    followRegion();
    if (m_dirty) {
        m_saveTimer->start();
    }
    // Auf diesem Dokument hat noch keine Scheibe ein Band ueberquert.
    m_onBand.clear();
    for (const SliceModel* slice : m_model->slices()) {
        if (slice != nullptr) {
            m_onBand[slice->sliceLetter()] = stackBandForSlice(slice);
        }
    }
    emit changed();
}

// Die Startbelegung der Region, dann die abgelegten Stellen jeder Scheibe.
// Keine Scheibe bewegt sich.
void BandStackController::startFresh()
{
    m_store = BandStackStore();
    m_store.loadSeeds(seedEntries());
    m_store.setRegion(regionName(currentRegion()));
    m_adoptOpeningSlices = true;
    for (const SliceModel* slice : m_model->slices()) {
        if (slice != nullptr) {
            adoptSavedSpots(slice);
        }
    }
    m_dirty = true;
}

void BandStackController::adoptSavedSpots(const SliceModel* slice)
{
    const QChar letter = slice->sliceLetter();
    if (m_adopted.contains(letter)) {
        return;
    }
    m_adopted.insert(letter);

    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band band = static_cast<Band>(i);
        if (band == Band::XVTR || !slice->hasSettingsFor(band)) {
            continue;
        }
        const std::optional<SliceModel::SavedBandSpot> saved = slice->savedBandSpot(band);
        if (!saved.has_value()) {
            continue;
        }
        const qint64 hz = qRound64(saved->frequencyHz);
        // Das Gedaechtnis liegt unter bandFromFrequency; der Stapel ordnet
        // nach dem Band, auf dem er die Frequenz sieht.
        const Band stackBand = BandDefaults::bandGridBandFromFrequency(saved->frequencyHz);
        // Eine Stelle ausserhalb des Bandes der Region kommt nicht hinein
        // (siehe resolve).
        if (isHamBand(band) && BandDefaults::frequenzImBand(band, saved->frequencyHz)
                                   != saved->frequencyHz) {
            continue;
        }
        const QString existing = m_store.entryNear(stackBand, hz);
        if (!existing.isEmpty()) {
            m_store.setPlace(letter, stackBand, existing);
            continue;
        }
        const BandStackSpot spot{hz, SliceModel::modeName(saved->mode), saved->filterLow,
                                 saved->filterHigh, hz, 0};
        m_store.add(letter, stackBand, spot, nowMs(), nullptr);
    }
}

// Longpath: ein Regionswechsel ersetzt die nicht gesperrten Eintraege und
// die unveraenderten Start-Eintraege durch die Startbelegung der neuen
// Region (die Einstellungsseite warnt: „Changing this setting will reset
// your band stack entries"). Von Hand gesperrte Eintraege bleiben.
void BandStackController::followRegion()
{
    if (m_mac.isEmpty()) {
        return;
    }
    const QString region = regionName(currentRegion());
    if (m_store.region() == region) {
        return;
    }
    if (!m_store.region().isEmpty()) {
        qCInfo(lcMemories).noquote() << "Band stack: region changed from" << m_store.region()
                                     << "to" << region << "- loading its seeds";
        m_store.replaceWithSeeds(seedEntries());
    }
    m_store.setRegion(region);
    markChanged();
}

void BandStackController::nachFrequenzOrdnen(Band band)
{
    QList<BandStackEntry> eintraege = m_store.entries(band);
    std::stable_sort(eintraege.begin(), eintraege.end(),
                     [](const BandStackEntry& a, const BandStackEntry& b) {
                         return a.frequencyHz < b.frequencyHz;
                     });
    QStringList ids;
    ids.reserve(eintraege.size());
    for (const BandStackEntry& e : eintraege) {
        ids.append(e.id);
    }
    QStringList jetzt;
    for (const BandStackEntry& e : m_store.entries(band)) {
        jetzt.append(e.id);
    }
    if (ids != jetzt) {
        m_store.reorder(band, ids);
    }
}

void BandStackController::markChanged()
{
    m_dirty = true;
    // Nicht neu gestartet: geschrieben wird binnen einer Sekunde nach der
    // ersten Aenderung, wie viele auch folgen.
    if (!m_saveTimer->isActive()) {
        m_saveTimer->start();
    }
    emit changed();
}

void BandStackController::flush()
{
    m_saveTimer->stop();
    writeDocument();
}

void BandStackController::writeDocument()
{
    if (!m_dirty || m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(
        documentKey(),
        QString::fromUtf8(QJsonDocument(m_store.toJson()).toJson(QJsonDocument::Compact)));
    m_dirty = false;
}

} // namespace Longpath
