#pragma once

// =================================================================
// src/gui/widgets/SliceFlag.h  (Longpath)
// =================================================================
//
// Longpath-original. Kein Port — aber die Idee ist alt:
//
// Bis zum 2026-08-18 gab es die VFO-Flagge (VfoWidget, 3274 Zeilen,
// aus AetherSDR nach Thetis-Vorbild portiert). Der Betreiber hat sie
// an diesem Tag streichen lassen: „Sie kommt ersatzlos weg, nicht
// ausblendbar." Alles darin zog in eigene Fenster um — Frequenz ins
// FrequencyApplet, Lautstaerke und Rauschen ins RxApplet, der
// S-Meter-Balken ins Zeigerinstrument. Der letzte Stand der alten
// Flagge steht in Commit 300e8d48.
//
// Am 2026-10-15 wollte er sie zurueck — aber nicht die alte: „wie es
// eigentlich bei aether und nereus war ... welches wir dann umgebaut
// haben auf lauter eigene windows". Aus drei Entwuerfen hat er den
// dritten gewaehlt.
//
// ── Was dieser dritte Entwurf ist ───────────────────────────────────
//
// Eine SCHMALE Flagge je Scheibe, 236 x 56 Punkte, mit genau dem, was
// er genannt hat: Frequenz, Band, Betriebsart. Dazu eine Leitlinie
// hinunter zum eigenen Traeger, damit bei mehreren Scheiben klar
// bleibt, welche Flagge zu welchem Signal gehoert. Die vordere
// Scheibe hell, die hinteren gedaempft.
//
// Alles andere — Filter, AGC, Antennen, Rauschminderung — bleibt, wo
// es seit August ist. Die Flagge holt die alte Flagge NICHT zurueck;
// sie holt den Weg zurueck, die Frequenz dort zu lesen und zu
// greifen, wo das Signal steht.
//
// ── Die Leitlinie ist ein eigenes Widget ────────────────────────────
//
// Der Panadapter ist ein QRhiWidget; seine Flaeche malt die GPU. Ein
// durchscheinendes Kind darueber ist heikel. Ein EIN PUNKT breites
// Kind mit voller Deckung ist es nicht — darum ist die Leitlinie ein
// eigenes schmales QWidget und kein gemalter Strich.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;

namespace Longpath {

class SliceFlag : public QWidget {
    Q_OBJECT
public:
    /// Breite und Hoehe stehen fest: der Entwurf ist auf 236 x 56
    /// gezeichnet und geprueft. Eine Flagge, die mit ihrem Inhalt
    /// waechst, verdeckt irgendwann das, wofuer sie da ist.
    static constexpr int kWidth  = 236;
    static constexpr int kHeight = 56;

    explicit SliceFlag(int sliceIndex, QWidget* parent);

    int sliceIndex() const { return m_sliceIndex; }

    /// Die drei Dinge, die der Betreiber genannt hat.
    void setFrequency(double hz);
    void setBand(const QString& band);
    void setMode(const QString& mode);

    /// Vordere Scheibe hell, hintere gedaempft.
    void setFront(bool front);
    bool isFront() const { return m_front; }

    /// Der Punkt auf dem Panadapter, an dem der Traeger dieser Scheibe
    /// steht, und die Oberkante des Spektrums. Die Flagge setzt sich
    /// selbst darueber und zieht ihre Leitlinie dorthin.
    void anchorTo(int carrierX, int spectrumTop, int spectrumBottom);

    /// Von Hand verschoben? Dann folgt die Flagge der Frequenz nicht
    /// mehr waagerecht, sondern bleibt, wo sie hingelegt wurde — die
    /// Leitlinie zeigt weiter auf den Traeger.
    bool wasMoved() const { return m_moved; }
    QPoint manualOffset() const { return m_offset; }
    void setManualOffset(const QPoint& o);

signals:
    void closeRequested(int sliceIndex);
    void moved(int sliceIndex, const QPoint& offset);
    void picked(int sliceIndex);     ///< angeklickt -> nach vorn

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    void legeLeitlinie();

    int      m_sliceIndex{0};
    QString  m_freq{QStringLiteral("—")};
    QString  m_band;
    QString  m_mode;
    bool     m_front{false};
    bool     m_moved{false};
    QPoint   m_offset;            ///< von Hand, relativ zur Ankerlage
    QPoint   m_ankerLage;         ///< wohin anchorTo() sie legen wuerde
    int      m_carrierX{-1};
    int      m_spectrumBottom{0};
    bool     m_dragging{false};
    QPoint   m_dragStart;
    QPoint   m_posStart;
    QPointer<QWidget> m_leitlinie;
};

} // namespace Longpath
