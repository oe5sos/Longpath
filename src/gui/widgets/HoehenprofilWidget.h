// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/gui/widgets/HoehenprofilWidget.h  (Longpath)
// =================================================================
//
// Ein UKW-Weg als Bild (2026-11-11): Gelaende mit Erdkruemmung, Sicht-
// linie, erste Fresnel-Zone, in Bernstein was in die inneren 60 % der
// Zone ragt, und ohne Sichtweg die Horizontstrahlen beider Stationen
// mit dem gemeinsamen Volumen. Aussehen wie das gewaehlte Entwurfsblatt
// (werkzeug/logbuch-2026-11-09/blaetter/5-hoehenprofil-2m.png): Grund
// kPanelBg, Bernstein kAmberText, die uebrigen Toene als kProfil*.
// Die Rechnung steht in core/Hoehenprofil.h; hier wird nur gezeichnet.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include "core/Hoehenprofil.h"

#include <QString>
#include <QWidget>

namespace Longpath {

class HoehenprofilWidget : public QWidget {
    Q_OBJECT
public:
    struct Beschriftung {
        QString titel;         ///< "SN7L  JO70SS  ·  344 km  ·  SSB  ·  2026-07-04"
        QString linksUnten;    ///< eigenes Ende: "OE/OO-106 Gmundnerberg  884 m"
        QString rechtsUnten;   ///< Gegenstation: "JO70SS  1352 m"
        QString genauigkeit;   ///< der Gegenstation, fuer das Urteil
    };

    explicit HoehenprofilWidget(QWidget* parent = nullptr);

    void setProfil(const Hoehenprofil::Eingabe& ein,
                   const Hoehenprofil::Ergebnis& erg,
                   const Beschriftung& text);
    /// Kein Profil, sondern ein Satz (laedt, Fehler, fehlender Locator).
    void setHinweis(const QString& titel, const QString& satz);

    bool hatProfil() const { return m_erg.gueltig; }
    const Hoehenprofil::Ergebnis& ergebnis() const { return m_erg; }
    QString urteilZeile() const;

    QSize sizeHint() const override { return {960, 340}; }
    QSize minimumSizeHint() const override { return {420, 220}; }

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    Hoehenprofil::Eingabe  m_ein;
    Hoehenprofil::Ergebnis m_erg;
    Beschriftung m_text;
    QString m_hinweisTitel;
    QString m_hinweis;
};

} // namespace Longpath
