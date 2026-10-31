#pragma once
// =================================================================
// src/gui/applets/Ft8PlaetzeApplet.h  (Longpath)
// =================================================================
//
// Was auf den Plaetzen laeuft — Multi Answering zum Zusehen.
//
// ── Warum es das geben muss ─────────────────────────────────────────
//
// `Ft8Mehrfach::belegung()` liefert je Platz Partner, Zustand und
// Tonlage. Am 2026-10-30 nachgesehen: NIEMAND liest es. Mit
// `slots 3` gehen drei Signale gleichzeitig hinaus, und der Betreiber
// sieht keines davon. Dieselbe Art Luecke wie Multi Answering selbst,
// das Monate lang gebaut und unerreichbar dalag.
//
// Betreiber: „ich will sehen, sobald ein qso oder mehrere vielleicht
// seitlich im eigenen fenster zu sehen sind." Aus drei
// Entwurfsblaettern (werkzeug/ft8-plaetze-2026-10-30) hat er A
// gewaehlt: eine Kachel je Platz, darunter die Warteschlange.
//
// ── Warum ein eigenes Applet ────────────────────────────────────────
//
// Im FT8-Fenster ist kein Platz: die Arbeitsleiste unten traegt genau
// eine Zeile, und sechs davon wuerden die Fundliste erdruecken. Als
// eigenes Panel laesst es sich danebenstellen, in der Hoehe ziehen und
// bei `slots 1` einfach zumachen.
//
// ── Was hier NICHT passiert ─────────────────────────────────────────
//
// Es fuehrt keinen Ablauf, kennt kein Protokoll und uebersetzt keine
// Zustaende. Es bekommt einen fertigen `Ft8PlaetzeStand` und malt ihn.
// Zwei Fenster, die beide in `Ft8Mehrfach` hineingreifen, haetten
// jedes seine eigene Meinung darueber, welcher Zug gerade dran ist.
//
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "AppletWidget.h"
#include "Ft8PlatzStand.h"

class QLabel;

namespace Longpath {

/// Die Malfläche. Getrennt vom Applet, damit ein Prueflauf sie ohne
/// RadioModel aufziehen und abgreifen kann.
class Ft8PlaetzeFlaeche : public QWidget
{
    Q_OBJECT
public:
    explicit Ft8PlaetzeFlaeche(QWidget* parent = nullptr);

    void setzeStand(const Ft8PlaetzeStand& s);
    const Ft8PlaetzeStand& stand() const { return m_stand; }

    /// Die Hoehe, die der aktuelle Stand braucht. Das Panel waechst
    /// mit der Zahl der Plaetze statt eine feste Hoehe zu behaupten.
    int noetigeHoehe() const;

    QSize sizeHint() const override;

    // ── Die Masze aus dem gewaehlten Blatt ─────────────────────
    static constexpr int kKachelH   = 92;
    static constexpr int kKachelLuft = 8;
    static constexpr int kRandX     = 10;
    static constexpr int kStufen    = 5;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    /// Gibt die Hoehe zurueck, die sie gebraucht hat.
    int  maleKopfzeile(QPainter& p);
    void maleKachel(QPainter& p, int y, int nummer, const Ft8PlatzStand& z);
    void maleWartende(QPainter& p, int y);

    Ft8PlaetzeStand m_stand;
};

class Ft8PlaetzeApplet : public AppletWidget
{
    Q_OBJECT
public:
    explicit Ft8PlaetzeApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("Ft8Slots"); }
    QString appletTitle() const override { return QStringLiteral("FT8 Slots"); }
    void    syncFromModel() override {}

public slots:
    /// Vom `Ft8Applet` her. Einziger Weg herein.
    void zeigeStand(const Longpath::Ft8PlaetzeStand& s);

    /// Wie hoch das Fenster sein muss, wenn es abgeloest wird.
    ///
    /// `AppletFloatingWindow::applyDefaultSize()` fragt `sizeHint()`
    /// -- NICHT `minimumHeight()`. Und der Vorgabe-sizeHint eines
    /// Applets ist die Summe seiner Anordnung, in der ein
    /// VERSTECKTES Widget gar nicht mitzaehlt: im leeren Zustand ist
    /// die Flaeche versteckt, also blieb nur das Hinweisschild, und
    /// der Betreiber bekam am 2026-10-30 eine Titelleiste von
    /// zwanzig Punkten Hoehe ohne Inhalt.
    ///
    /// Darum die Hoehe hier selbst sagen, statt sie ausrechnen zu
    /// lassen.
    QSize sizeHint() const override;

public:
    Ft8PlaetzeFlaeche* flaecheForTest() const { return m_flaeche; }

private:
    Ft8PlaetzeFlaeche* m_flaeche{nullptr};
    QLabel*            m_leer{nullptr};
};

}  // namespace Longpath
