#pragma once

// no-port-check: Longpath-original.

// =================================================================
// src/core/CatRigBruecke.h  (Longpath)
// =================================================================
//
// Haelt die Scheibe und ein per CAT gefuehrtes Geraet gleich. Nur im
// Betrieb ohne SDR (ProtocolVersion::CatRig): dort ist das Geraet die
// Quelle der Wahrheit, und die Scheibe ist seine Anzeige und sein
// Bedienfeld.
//
// Neben einem SDR gilt das Gegenteil, und dort bleibt es dabei: das
// Begleitgeraet treibt nur die Marke, siehe CompanionRigController.h.
//
// ── Kein Kreis ──────────────────────────────────────────────────
//
// Eine Meldung vom Geraet stellt die Scheibe, und das darf nicht als
// Befehl zurueck ans Geraet gehen. Umgekehrt meldet das Geraet nach
// einem Befehl noch einen Moment die alte Frequenz, weil rigctld im
// Takt abgefragt wird. Diese Meldung darf die Scheibe nicht zurueckziehen:
// sonst springt der VFO beim Drehen. Darum gilt nach jedem Befehl eine
// kurze Frist, in der Meldungen vom Geraet nicht zaehlen.
//
// Getastet wird hier nichts.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>

#include "core/WdspTypes.h"

namespace Longpath {

class CompanionRigController;
class SliceModel;

class CatRigBruecke : public QObject {
    Q_OBJECT
public:
    /// So lange zaehlen Meldungen vom Geraet nach einem Befehl nicht.
    /// Ein Befehl braucht einen Umlauf durch rigctld und die naechste
    /// Abfrage. Eine kuerzere Frist liesse die alte Frequenz durch.
    static constexpr int kFristNachBefehlMs = 400;

    CatRigBruecke(CompanionRigController* rig, SliceModel* scheibe,
                  QObject* parent = nullptr);

    /// Rig-Modus nach Scheibe. false, wenn es keine Entsprechung gibt.
    static bool modusAusGeraet(const QString& rigModus, DSPMode& modus);

    /// Scheibe nach Rig-Modus. Leer, wenn das Geraet ihn nicht kennt.
    static QString modusFuerGeraet(DSPMode modus);

    /// Fuer den Pruefstand: die Frist sofort als abgelaufen behandeln.
    void fristAufhebenForTest() { m_befehlGesendet = false; }

private:
    void vomGeraetFrequenz(qint64 hz);
    void vomGeraetModus(const QString& modus);
    void vonScheibeFrequenz(double hz);
    void vonScheibeModus(DSPMode modus);
    void vomGeraetFilter();
    void vonScheibeFilter(int low, int high);
    bool fristLaeuft() const;

    QPointer<CompanionRigController> m_rig;
    QPointer<SliceModel>             m_scheibe;
    QElapsedTimer m_seitBefehl;
    bool m_befehlGesendet{false};
    bool m_uebernimmtVomGeraet{false};
};

} // namespace Longpath
