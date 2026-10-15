// =================================================================
// src/gui/widgets/SliceFlag.cpp  (Longpath)
// =================================================================
// Longpath-original. Siehe SliceFlag.h fuer die Herkunft.
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "gui/widgets/SliceFlag.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace Longpath {
namespace {

constexpr int kKopfHoch   = 14;   // Griff, Kennung, Schliessen
constexpr int kPilleHoch  = 15;
constexpr int kRand       = 10;

QColor rolle(const char* name, const char* rueckfall)
{
    return QColor(Style::role(name, rueckfall));
}

} // namespace

SliceFlag::SliceFlag(int sliceIndex, QWidget* parent)
    : QWidget(parent), m_sliceIndex(sliceIndex)
{
    setFixedSize(kWidth, kHeight);
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_NoMousePropagation, true);
    setToolTip(tr("Slice flag — drag it where you want it; the line keeps "
                  "pointing at its own carrier."));

    // Die Leitlinie: ein Punkt breit, volle Deckung. Siehe die
    // Begruendung im Kopf der Datei — ein durchscheinendes Kind ueber
    // dem GPU-Panadapter ist heikel, ein schmales undurchsichtiges
    // nicht.
    if (parent) {
        m_leitlinie = new QWidget(parent);
        m_leitlinie->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_leitlinie->hide();
    }
}

void SliceFlag::setFrequency(double hz)
{
    // Punkt zwischen MHz und kHz, zwei Nachkommastellen: dieselbe
    // Schreibweise wie im Frequenzfeld, damit nicht zwei Lesarten
    // nebeneinander stehen.
    const double mhz = hz / 1e6;
    const QString neu = QString::number(mhz, 'f', 5);
    if (neu == m_freq) { return; }
    m_freq = neu;
    update();
}

void SliceFlag::setBand(const QString& band)
{
    if (band == m_band) { return; }
    m_band = band;
    update();
}

void SliceFlag::setMode(const QString& mode)
{
    if (mode == m_mode) { return; }
    m_mode = mode;
    update();
}

void SliceFlag::setFront(bool front)
{
    if (front == m_front) { return; }
    m_front = front;
    if (front) { raise(); }
    update();
    legeLeitlinie();
}

void SliceFlag::setManualOffset(const QPoint& o)
{
    m_offset = o;
    m_moved  = !o.isNull();
}

void SliceFlag::anchorTo(int carrierX, int spectrumTop, int spectrumBottom)
{
    m_carrierX       = carrierX;
    m_spectrumBottom = spectrumBottom;

    // Ueber dem Traeger, mittig, mit etwas Luft zum oberen Rand.
    const int x = carrierX - kWidth / 2;
    const int y = spectrumTop + 8;
    m_ankerLage = QPoint(x, y);

    // Von Hand verschoben? Dann gilt die Verschiebung, nicht der Anker.
    const QPoint ziel = m_moved ? m_ankerLage + m_offset : m_ankerLage;

    // In der Flaeche halten: eine Flagge, die halb hinausragt, ist
    // genauso unbrauchbar wie keine.
    int zx = ziel.x();
    int zy = ziel.y();
    if (parentWidget()) {
        zx = qBound(0, zx, qMax(0, parentWidget()->width()  - kWidth));
        zy = qBound(0, zy, qMax(0, parentWidget()->height() - kHeight));
    }
    move(zx, zy);
    legeLeitlinie();
}

void SliceFlag::legeLeitlinie()
{
    if (!m_leitlinie || m_carrierX < 0) { return; }
    const int oben = y() + height();
    const int unten = m_spectrumBottom;
    if (unten <= oben) { m_leitlinie->hide(); return; }
    m_leitlinie->setGeometry(m_carrierX, oben, 1, unten - oben);
    QPalette p = m_leitlinie->palette();
    p.setColor(QPalette::Window,
               m_front ? rolle("accent", Style::kAccent)
                       : rolle("border", Style::kBorder));
    m_leitlinie->setAutoFillBackground(true);
    m_leitlinie->setPalette(p);
    m_leitlinie->show();
    m_leitlinie->raise();
    raise();   // die Flagge bleibt ueber ihrer eigenen Linie
}

void SliceFlag::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor rahmen = m_front ? rolle("accent", Style::kAccent)
                                  : rolle("border", Style::kBorder);
    const QColor grund  = rolle("panel", Style::kPanelBg);
    const QColor text   = m_front ? rolle("measured", Style::kAmberText)
                                  : rolle("text-secondary", Style::kTextSecondary);

    p.setPen(QPen(rahmen, 1));
    p.setBrush(grund);
    p.drawRoundedRect(QRectF(0.5, 0.5, kWidth - 1, kHeight - 1), 4, 4);

    // Kopf: Griff links, Kennung, Schliessen rechts.
    p.setPen(rolle("text-scale", Style::kTextScale));
    for (int i = 0; i < 3; ++i) {
        p.fillRect(6 + i * 5, 5, 2, 2,
                   rolle("text-scale", Style::kTextScale));
    }
    QFont klein = font();
    klein.setPixelSize(8);
    p.setFont(klein);
    p.drawText(QRect(26, 1, 120, kKopfHoch), Qt::AlignVCenter,
               tr("SLICE %1").arg(QChar('A' + m_sliceIndex)));
    p.drawText(QRect(kWidth - 20, 1, 14, kKopfHoch),
               Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("✕"));

    // Die Frequenz — der Grund, warum es die Flagge gibt.
    QFont gross = font();
    gross.setPixelSize(21);
    p.setFont(gross);
    p.setPen(text);
    p.drawText(QRect(kRand, kKopfHoch - 2, kWidth - 2 * kRand, 24),
               Qt::AlignVCenter | Qt::AlignLeft, m_freq);

    // Band und Betriebsart als zwei Pillen.
    QFont pille = font();
    pille.setPixelSize(9);
    p.setFont(pille);
    int px = kRand;
    for (int i = 0; i < 2; ++i) {
        const QString t = (i == 0) ? m_band : m_mode;
        if (t.isEmpty()) { continue; }
        const bool hervor = (i == 1) && m_front;
        const QRect r(px, kHeight - kPilleHoch - 6, 52, kPilleHoch);
        p.setPen(QPen(hervor ? rahmen : rolle("border", Style::kBorder), 1));
        p.setBrush(hervor ? rolle("sel-bg", Style::kGlassSelTop)
                          : rolle("button", Style::kButtonBg));
        p.drawRoundedRect(r, 3, 3);
        p.setPen(hervor ? rolle("text", Style::kTextPrimary)
                        : rolle("text-secondary", Style::kTextSecondary));
        p.drawText(r, Qt::AlignCenter, t);
        px += 58;
    }
}

void SliceFlag::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }

    // Das Kreuz oben rechts.
    if (e->position().y() < kKopfHoch && e->position().x() > kWidth - 22) {
        emit closeRequested(m_sliceIndex);
        return;
    }
    emit picked(m_sliceIndex);
    m_dragging = true;
    m_dragStart = e->globalPosition().toPoint();
    m_posStart  = pos();
    setCursor(Qt::ClosedHandCursor);
}

void SliceFlag::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) { QWidget::mouseMoveEvent(e); return; }
    const QPoint delta = e->globalPosition().toPoint() - m_dragStart;
    QPoint ziel = m_posStart + delta;
    if (parentWidget()) {
        ziel.setX(qBound(0, ziel.x(),
                         qMax(0, parentWidget()->width()  - kWidth)));
        ziel.setY(qBound(0, ziel.y(),
                         qMax(0, parentWidget()->height() - kHeight)));
    }
    move(ziel);
    legeLeitlinie();
}

void SliceFlag::mouseReleaseEvent(QMouseEvent* e)
{
    if (!m_dragging) { QWidget::mouseReleaseEvent(e); return; }
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
    // Ab jetzt gilt die Verschiebung. Die Leitlinie zeigt weiter auf
    // den Traeger — das ist der ganze Zweck des dritten Entwurfs.
    m_offset = pos() - m_ankerLage;
    m_moved  = true;
    emit moved(m_sliceIndex, m_offset);
}

} // namespace Longpath
