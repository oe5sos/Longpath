// =================================================================
// src/gui/applets/eq/StripEqSimpleBar.h  (Longpath)
// =================================================================
//
// Sieben benannte Regler fuer die Stimme -- die einfache Ansicht des
// Entzerrers.
//
// „mit dem eq bin ich als anwender nicht zufrieden. wie können wir
//  dies verbessern, vereinfachen ... ev generell 2 varianten, einen
//  leichtere und eine für profi im gleichen design?"
//
// Der Unterschied zur Profi-Ansicht ist AUSSCHLIESSLICH, was gezeigt
// wird. Es gibt keinen zweiten Entzerrer, keine zweite Kurve und
// keine Umrechnung: die sieben Regler sind die sieben freien Griffe
// des Bandsatzes (EqBandLayout.h), und sie stellen nur deren Pegel.
// Frequenz, Breite und Filterart bleiben unangetastet -- ein Wechsel
// der Ansicht aendert den Ton also nicht um ein Hundertstel Dezibel.
//
// Deshalb steht unter jedem Namen die WIRKLICHE Frequenz des Bandes
// und nicht die, die der Name verspricht. Wer in der Profi-Ansicht
// „Praesenz" auf 2,2 kHz geschoben hat, soll hier 2,2 kHz lesen und
// nicht 3 kHz.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#pragma once

#include <QVector>
#include <QWidget>
#include <array>

class QLabel;
class QSlider;

namespace Longpath {

class ClientEq;
class EqHost;

class StripEqSimpleBar : public QWidget {
    Q_OBJECT

public:
    // Welcher Griff hinter welchem Namen steht. Die Frequenzen sind
    // die des Bandsatzes -- hier wird benannt, nicht gesetzt.
    struct Slot {
        int         band;     // Index in ClientEq
        const char* name;     // Versalien ueber dem Regler
        float       freqHz;   // wo der Regler hingehoert
        float       q;        // wie breit er greift
        const char* what;     // ein Satz, als Hilfetext
    };
    static constexpr int kCount = 7;
    static const std::array<Slot, kCount>& bandSlots();

    explicit StripEqSimpleBar(QWidget* parent = nullptr);

    void setEq(ClientEq* eq);
    void setHost(EqHost* host);

    // Alle sieben Griffe auf ihre Frequenz und Breite setzen, die
    // Pegel behalten. Fuer den Fall, dass in der Profi-Ansicht
    // verschoben wurde und die Reihe nicht mehr der Reihe nach steht.
    void tidy();

    // Regler und Zahlen aus den Baendern nachziehen. Nach jeder
    // Aenderung von aussen noetig -- Profi-Ansicht, Zuruecksetzen,
    // Rueckgaengig, geladenes Profil.
    void reload();

    // ── Das Ziel ─────────────────────────────────────────────────────
    //
    // „Ich weiss nicht, welche Kurve richtig waere."
    //
    // Je Regler die Abweichung der GEMESSENEN Stimme vom Sprachziel,
    // in dB: positiv heisst zuviel, negativ heisst zuwenig. Leer
    // heisst „noch nichts gemessen" -- dann steht dort ein Strich
    // und keine Null.
    //
    // Es wird NICHTS verstellt. Der Unterschied zur verworfenen
    // Automatik ist genau der: die hat gedreht, das hier sagt nur,
    // wo du stehst. Die Hand bleibt deine.
    void setZiel(const QVector<double>& abweichungDb);

signals:
    // Ein Regler wurde bewegt; die Kurve darueber muss neu gezeichnet
    // werden.
    void changed();

private:
    void applyBand(int i, int sliderValue);

    ClientEq* m_eq{nullptr};
    EqHost*   m_host{nullptr};
    bool      m_reloading{false};

    std::array<QSlider*, kCount> m_sliders{};
    std::array<QLabel*,  kCount> m_freqLabels{};
    std::array<QLabel*,  kCount> m_valueLabels{};
    std::array<QLabel*,  kCount> m_zielLabels{};
};

} // namespace Longpath
