#pragma once
// =================================================================
// src/gui/applets/Ft8PlatzStand.h  (Longpath)
// =================================================================
//
// Was auf den Plaetzen steht — als reine Daten, ohne Logik.
//
// Zwischen zwei Applets: `Ft8Applet` fuehrt die Ablaeufe und baut
// diesen Stand, `Ft8PlaetzeApplet` zeigt ihn. Die Alternative waere,
// das Plaetze-Panel in `Ft8Mehrfach` und `Ft8Warteschlange`
// hineingreifen zu lassen — dann haetten zwei Fenster denselben
// Zustand ausgelesen und jedes seine eigene Meinung dazu gehabt,
// welcher Zug gerade dran ist.
//
// `Ft8Ablauf::Zustand` steht hier NICHT drin, und zwar mit Absicht:
// das Panel soll keine Protokollzustaende uebersetzen muessen. Es
// bekommt den fertigen Text („RR73", „Tx2", „CQ") und die Stufe.
//
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QVector>

namespace Longpath {

/// Ein belegter oder rufender Platz.
struct Ft8PlatzStand {
    /// Leer heisst: hier laeuft der CQ-Ruf, kein QSO.
    QString partner;
    QString land;
    int     tonHz{0};
    /// Der naechste Zug als Text, wie ihn das Fenster zeigt.
    QString zug;
    /// kKeinReport (-99), solange keiner feststeht.
    int     gesendet{-99};
    int     empfangen{-99};
    /// 0..5 — wie weit das QSO ist. Fuenf Striche, einer je Zug.
    int     stufe{0};
};

/// Einer, der gerufen hat und wartet.
struct Ft8WartenderStand {
    QString partner;
    QString land;
    int     snr{0};
};

/// Der ganze Stand in einem Stueck: so kann das Panel nicht halb
/// aktuell sein.
struct Ft8PlaetzeStand {
    QVector<Ft8PlatzStand>     plaetze;
    QVector<Ft8WartenderStand> wartende;
    /// Wie viele Plaetze eingestellt sind (der Nenner in „2 of 3").
    int  plaetzeGesamt{1};
    /// Geht in diesem Augenblick etwas hinaus? Die EINE warme Stelle
    /// im Panel haengt daran.
    bool sendet{false};
};

}  // namespace Longpath
