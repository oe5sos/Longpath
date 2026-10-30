#pragma once

// =================================================================
// src/gui/widgets/Ft8Fensterbalken.h  (Longpath)
// =================================================================
//
// Wie weit das FT8-Fenster ist, als feiner Balken.
//
// Longpath-original. Die Zahlen kommen aus `Ft8Tx` (Fensterlaenge,
// Signallaenge, Zielversatz) -- hier wird nichts neu erfunden: die
// zwei Marken im Balken stehen dort, wo das Signal anfaengt und
// aufhoert, und das sagt `Ft8Tx::sendeplan`.
//
// ── Warum ein eigenes Bauteil ───────────────────────────────────────
//
// Es gab vorher nur „0.0 / 15.0 s" als graue Schrift ganz oben rechts
// im Panelkopf -- die unauffaelligste Stelle fuer das einzige Ding,
// das sich dauernd bewegt. Beim automatischen Durchlauf ist das die
// Frage, die man im Vorbeigehen beantwortet haben will: sendet er
// gerade, und wie lange noch?
//
// Farbe: Messing (`kTxKeyBorder`) beim Hoeren, das gedaempfte Rot der
// Sendetaste beim Senden. Kein kraeftiges Rot -- das bleibt der
// Warnung (CLAUDE.local.md, Betreiberregel).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QWidget>

namespace Longpath {

class Ft8Fensterbalken : public QWidget
{
    Q_OBJECT

public:
    explicit Ft8Fensterbalken(QWidget* parent = nullptr);

    /// `anteil` 0..1 im laufenden Fenster, `sendet` faerbt um.
    void setzeStand(double anteil, bool sendet);

    /// Wo das Signal im Fenster liegt, als Anteil 0..1. Beide Marken
    /// kommen aus `Ft8Tx::sendeplan`; der Aufrufer rechnet sie, damit
    /// dieses Bauteil nichts ueber FT8 wissen muss.
    void setzeSignalstrecke(double von, double bis);

    QSize sizeHint() const override { return QSize(180, 10); }
    QSize minimumSizeHint() const override { return QSize(60, 8); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    double m_anteil{0.0};
    bool   m_sendet{false};
    double m_von{0.0};
    double m_bis{1.0};
};

} // namespace Longpath
