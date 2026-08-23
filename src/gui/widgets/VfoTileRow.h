#pragma once

// =================================================================
// src/gui/widgets/VfoTileRow.h  (Longpath)
// =================================================================
//
// Longpath-original, nach einer Vorlage des Betreibers.
//
// ── Die Kachelreihe ueber der Frequenz ──────────────────────────────
//
// Der Betreiber am 2026-08-23, mit einem Bildschirmfoto von Zeus Link:
// "weiters sollte die frequenz auch ein eigenes widget sein, wie hier
// am foto."
//
// Auf seinem Bild steht ueber der grossen Ziffernanzeige eine Reihe
// kleiner Kacheln: je Empfaenger eine mit Nummer, Frequenz und Band,
// die sendende rot umrandet, dazu eine fuer den KiwiSDR mit AN/AUS.
//
// Der Nutzen ist nicht Zierde: mit mehreren Scheiben ist die grosse
// Zahl immer nur EINE davon, und ohne die Reihe muss man raten, was
// die anderen gerade tun. Die Kacheln beantworten das auf einen Blick
// und sind zugleich der kuerzeste Weg, die aktive zu wechseln.
//
// ── Was hier NICHT ist ──────────────────────────────────────────────
//
// Zeus hat zusaetzlich eine Kachel "MULTI RX". Was sie bei ihm genau
// schaltet, ist von aussen nicht zu erkennen, und eine Kachel zu
// bauen, deren Wirkung man raet, waere schlechter als keine. Sie
// fehlt darum, bis der Betreiber sie vorfuehrt.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QPointer>
#include <QWidget>

namespace Longpath {

class RadioModel;
class SliceModel;

// Eine Kachel. Vollstaendig in der Quelldatei erklaert; hier nur
// angekuendigt, damit die Reihe ihre Kacheln in einer Liste
// halten kann statt sie ueber findChildren zu suchen — das
// verlangte Q_OBJECT und damit moc in der Quelldatei.
class VfoTile;

class VfoTileRow : public QWidget {
    Q_OBJECT

public:
    explicit VfoTileRow(RadioModel* model, QWidget* parent = nullptr);

    /// Neu aufbauen — nach Zu- oder Abgang einer Scheibe.
    void rebuild();

    /// Nur die Beschriftungen auffrischen, ohne Umbau. Wird bei jeder
    /// Frequenzaenderung gerufen, also oft; ein Umbau waere dort das
    /// falsche Werkzeug (er nimmt dem Betreiber den Mauszeiger unter
    /// der Kachel weg).
    void refresh();

signals:
    /// Die KiwiSDR-Kachel wurde gedrueckt. Das Applet entscheidet, was
    /// daraus folgt — diese Reihe kennt den Panadapter nicht.
    void kiwiToggleRequested();

private:
    VfoTile* buildSliceTile(SliceModel* slice, int index);
    VfoTile* buildKiwiTile();

    QPointer<RadioModel> m_model;
    QList<VfoTile*> m_sliceTiles;
    VfoTile* m_kiwiTile{nullptr};
    QList<QMetaObject::Connection> m_conns;
    bool m_kiwiOn{false};

public:
    /// Zustand der KiwiSDR-Kachel. Setzt das Applet, weil nur es den
    /// Panadapter kennt.
    void setKiwiOn(bool on);
    bool kiwiOn() const { return m_kiwiOn; }
};

} // namespace Longpath
