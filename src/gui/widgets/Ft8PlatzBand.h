#pragma once

// =================================================================
// src/gui/widgets/Ft8PlatzBand.h  (Longpath)
// =================================================================
//
// Longpath-original. Das Band der laufenden QSOs im FT8-Fenster.
//
// ── Wozu ────────────────────────────────────────────────────────────
//
// Mit Multi Answering (Ft8Mehrfach) laufen bis zu sechs Gespraeche
// nebeneinander. Ohne eine Flaeche dafuer sieht der Bediener nur die
// Fundliste und weiss nicht, mit wem er gerade spricht, wie weit das
// jeweilige QSO ist und auf welcher Tonlage es hinausgeht.
//
// ── Warum keine Tabelle, keine Kaesten ──────────────────────────────
//
// Entwurfsrunde 2026-10-24, Massstab des Betreibers: „uebersichtlich,
// professionell, einfach, high end". Drei Blaetter gezeichnet
// (tests/tst_ft8_entwurfsblaetter.cpp); der Weg dorthin war WENIGER.
// Eine Zeile je Platz, links das Rufzeichen, rechts der naechste Zug,
// darunter Tonlage und ein Fortschritt als Haarlinie. Sechs Kaesten
// auf einer Flaeche, die schon Rahmen hat, waeren sechs Rahmen zu
// viel.
//
// Das Band zeigt NUR an. Es kennt kein Ft8Mehrfach und keinen Ablauf
// -- es bekommt eine Liste und malt sie. Dieselbe Trennung wie
// ueberall sonst im FT8-Weg.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-24 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QVector>
#include <QWidget>

namespace Longpath {

class Ft8PlatzBand : public QWidget
{
    Q_OBJECT

public:
    /// Ein Platz, so wie er gezeigt wird. Bewusst reiner Text: das
    /// Band soll nicht wissen, welche Zustaende es gibt.
    struct Platz {
        QString ruf;          ///< Gegenstation
        QString naechstes;    ///< was als naechstes hinausgeht ("R-12")
        int     hz{0};        ///< Tonlage dieses Platzes
        int     schritt{0};   ///< 0..kSchritteBisQso
        bool    wartet{false};///< steht in der Schlange, sendet nicht
    };

    /// Anruf, Rapport, R-Rapport, RR73, 73 -- fuenf Zuege bis zum
    /// vollstaendigen QSO. Der Fortschritt wird daran gemessen.
    static constexpr int kSchritteBisQso = 5;

    explicit Ft8PlatzBand(QWidget* parent = nullptr);

    /// Was gerade laeuft, und wie viele Plaetze es insgesamt gibt.
    /// Leere Liste -> das Band macht sich unsichtbar, damit es im
    /// gewoehnlichen Einzelbetrieb keine Zeile kostet.
    void setzePlaetze(const QVector<Platz>& plaetze, int vonWievielen);

    const QVector<Platz>& plaetzeForTest() const { return m_plaetze; }
    int vonWievielenForTest() const { return m_vonWievielen; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Platz> m_plaetze;
    int            m_vonWievielen{0};
};

}  // namespace Longpath
