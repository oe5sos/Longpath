// =================================================================
// src/models/bandstack/BandStackController.h  (Longpath)
// =================================================================
//
// Verbindet die Ablage des Bandstapels (BandStackStore) mit den
// Scheiben: ruft Eintraege ab, merkt sich je Scheibe und Band die
// zuletzt benutzte Stelle, fuehrt Verweilen und Verfall aus und laedt
// und speichert das Dokument unter hardware/<mac>/bandstack/Document.
//
// no-port-check: Longpath-Fassung der Upstream-Steuerung. Die aus Thetis
// uebernommenen Regeln stehen in BandStackStore und BandStackSeeds.
//
// Nach Upstream NereusSDR src/models/bandstack/BandStackController
// (PR #748, J.J. Boyd KG4VCF). Was hier fehlt, weil Longpath es nicht
// hat: der Core mit mehreren Geraeten, das Netz-Protokoll (parseRecall,
// publishedDocument, lastRecall), die Frage vor dem Verschieben eines
// geteilten Empfaengers. Was hier anders ist:
//   * Das Bandgedaechtnis je Band (SliceModel::saveToSettings) liegt
//     unter dem Band aus bandFromFrequency, wie bei jedem anderen
//     Bandwechsel in Longpath; der Stapel selbst ordnet nach
//     BandDefaults::bandGridBandFromFrequency (mit den Rundfunkbaendern).
//   * Eine Stelle ausserhalb des Bandes der eingestellten Region wird
//     beim Verlassen nicht in den Eintrag geschrieben (Betreiber am
//     2026-10-29: „bandwechsel sollte immer in das amateurfunkband
//     springen, nicht daneben").
//   * Nach dem Abruf richtet sich das Sprechseitenband nach dem Band
//     (Betreiber am 2026-10-23: 160, 80 und 40 m immer LSB).
//   * Ein Regionswechsel ersetzt die nicht gesperrten Eintraege durch
//     die Startbelegung der neuen Region (Thetis setzt den ganzen Stapel
//     zurueck, clsBandStackManager.cs RegionReset; Upstream fragt).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 - Neu, nach Upstream NereusSDR (2026-11-05, J.J. Boyd
//                 KG4VCF). Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Anthropic Claude Code.
// =================================================================

#pragma once

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>
#include <optional>

#include "models/Band.h"
#include "models/bandstack/BandStackStore.h"

class QTimer;

namespace Longpath {

class RadioModel;
class SliceModel;

/// Wie der Panadapter einer Scheibe beim Druck aussieht. Wird nur in
/// einen Eintrag gelegt; fehlt er, behaelt der Eintrag seine Werte.
struct BandStackView {
    std::optional<qint64> centerHz;
    std::optional<qint64> spanHz;
};

/// Einer pro RadioModel, im Hauptfaden. Jede Methode, die bool liefert,
/// gibt bei false den Grund in `refusal` (wenn nicht null) und aendert
/// nichts.
class BandStackController : public QObject {
    Q_OBJECT

public:
    explicit BandStackController(RadioModel* model);
    ~BandStackController() override;

    /// Ein Dokument ist geladen: ein Geraet ist verbunden.
    bool isReady() const;

    /// Warum der Stapel gerade nicht geht, leer wenn er geht.
    QString unavailableReason() const;

    /// Ein Druck auf die Bandtaste. Auf einem anderen Band: verlassen
    /// (speichern) und dorthin, wo die Rueckkehrart des Bandes hinzeigt.
    /// Auf diesem Band: speichern und zum naechsten (Next) oder vorigen
    /// (Previous) Eintrag, mit Umlauf.
    bool press(int sliceId, Band band, BandStackDirection direction,
               const BandStackView& view, QString* refusal);

    /// Naechster oder voriger Eintrag auf dem Band der Scheibe.
    bool step(int sliceId, BandStackDirection direction, const BandStackView& view,
              QString* refusal);

    /// Zu einem bestimmten Eintrag (Zeile, Strich im Spektrum).
    bool goTo(int sliceId, const QString& entryId, const BandStackView& view,
              QString* refusal);

    /// Die aktuelle Stelle als neuen Eintrag ablegen; er wird die Stelle
    /// der Scheibe.
    bool add(int sliceId, const BandStackView& view, QString* refusal);

    /// Die aktuelle Stelle bewusst in `entryId` schreiben.
    bool saveHere(int sliceId, const QString& entryId, const BandStackView& view,
                  QString* refusal);

    bool edit(const QString& id, const std::optional<QString>& name, std::optional<bool> locked,
              QString* refusal);
    bool remove(const QString& id, QString* refusal);
    bool reorder(Band band, const QStringList& ids, QString* refusal);
    bool setReturn(Band band, BandStackReturnMode mode, const QString& entryId, QString* refusal);
    bool clear(std::optional<Band> band, QString* refusal);

    /// Ersetzt nicht gesperrte und Start-Eintraege durch die Startbelegung
    /// der eingestellten Region. Keine Scheibe bewegt sich.
    bool reseed(QString* refusal);

    bool options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                 std::optional<int> dwellSeconds, QString* refusal);

    /// Holt den letzten Verfall zurueck, 5 Sekunden lang.
    bool restoreExpired(const QString& batchId, QString* refusal);

    const BandStackStore& store() const { return m_store; }

    /// Das Band, auf dem der Stapel die Scheibe sieht.
    Band stackBandFor(int sliceId) const;

    /// Die Stelle der Scheibe auf `band`, so wie die Anzeige sie zeigt:
    /// leer, wenn die Scheibe auf das Band gestimmt statt abgerufen wurde
    /// (sie steht dann neben ihrer Stelle, siehe onSliceTuned).
    QString shownPlace(int sliceId, Band band) const;

    /// „40m 2/5", „40m -/5" ohne Stelle, leer ohne Stapel.
    QString positionText(int sliceId) const;

    QString lastExpiryBatchId() const { return m_lastExpiryBatchId; }
    int lastExpiryCount() const { return m_lastExpiryCount; }

    /// Schreibt eine noch wartende Aenderung sofort.
    void flush();

    // Pruefstand: eigene Uhr, Zeitgeber nicht abwarten.
    void setClockForTest(std::function<qint64()> clock) { m_clockForTest = std::move(clock); }
    void runExpiryForTest() { runExpiry(); }
    void runDwellForTest(int sliceId) { runDwell(sliceId); }
    bool dwellArmedForTest(int sliceId) const;
    void reloadForTest() { m_mac.clear(); loadDocument(); }

signals:
    void changed();
    /// Ein Abruf ist durch. `report` etwa „40m: 7.185 LSB, 5 of 6".
    void recalled(int sliceId, const QString& report);

private:
    enum class Kind { Press, Step, GoTo };
    struct Request {
        Kind kind{Kind::Press};
        int sliceId{-1};
        Band band{Band::GEN};
        BandStackDirection direction{BandStackDirection::Next};
        QString entryId;
        BandStackView view;
    };
    struct Resolution;
    struct RecalledBand {
        Band band{Band::GEN};
        Band gridBand{Band::GEN};
    };

    bool performRecall(const Request& request, QString* refusal);
    Resolution resolve(const Request& request) const;
    Band stackBandForSlice(const SliceModel* slice) const;
    QSet<QChar> slicesOnBand(Band band) const;
    BandStackSpot currentSpot(const SliceModel* slice, const BandStackView& view,
                              const BandStackEntry* existing) const;
    std::optional<BandStackSpot> lastSpot(const SliceModel* slice, Band band) const;
    QList<BandStackEntry> seedEntries() const;
    qint64 nowMs() const;

    void wireSlices();
    void onSliceTuned(SliceModel* slice);
    void armDwell(const SliceModel* slice);
    void stopDwell(int sliceId);
    void runDwell(int sliceId);
    void runExpiry();

    void loadDocument();
    void startFresh();
    void adoptSavedSpots(const SliceModel* slice);
    void followRegion();
    void markChanged();
    void writeDocument();
    QString documentKey() const;

    RadioModel* m_model{nullptr};
    BandStackStore m_store;
    QString m_mac;
    bool m_dirty{false};
    QTimer* m_saveTimer{nullptr};
    QTimer* m_expiryTimer{nullptr};
    std::function<qint64()> m_clockForTest;

    bool m_inRecall{false};
    QSet<const SliceModel*> m_wiredSlices;
    QHash<int, QTimer*> m_dwellTimers;
    // Die zuletzt benutzte Stelle je Scheibe und Band, frischer als das
    // Bandgedaechtnis.
    QMap<QChar, QMap<Band, BandStackSpot>> m_lastUsed;
    // Ein Start-Eintrag kann ausserhalb des Bereichs seines Bandes liegen
    // (eine GEN-Zeile im Bereich von 22 m); eine dorthin abgerufene
    // Scheibe steht trotzdem auf diesem Band.
    QMap<QChar, RecalledBand> m_recalledBand;
    // Das Band, auf dem jede Scheibe zuletzt gesehen wurde, und die
    // Scheiben, die anders als durch einen Abruf dorthin kamen: sie
    // stehen neben ihrer Stelle, und ein Verlassen schreibt nichts
    // (Upstream, J.J. Boyds Entscheidung vom 2026-11-06). Nicht im
    // Dokument; jeder Start beginnt ohne.
    QMap<QChar, Band> m_onBand;
    QSet<QChar> m_detached;
    // Beim frischen Dokument: Scheiben, die spaeter aufgehen, bringen ihre
    // abgelegten Stellen ebenfalls mit.
    bool m_adoptOpeningSlices{false};
    QSet<QChar> m_adopted;

    QString m_lastExpiryBatchId;
    int m_lastExpiryCount{0};
    qint64 m_lastExpiryMs{0};
    QList<BandStackEntry> m_lastExpired;
    QMap<Band, QString> m_lastExpiredSpecific;
};

} // namespace Longpath
