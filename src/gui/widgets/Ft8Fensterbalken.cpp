// =================================================================
// src/gui/widgets/Ft8Fensterbalken.cpp  (Longpath)
// =================================================================
//
// Siehe Ft8Fensterbalken.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "Ft8Fensterbalken.h"

#include "gui/StyleConstants.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace Longpath {

Ft8Fensterbalken::Ft8Fensterbalken(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void Ft8Fensterbalken::setzeStand(double anteil, bool sendet)
{
    const double a = std::clamp(anteil, 0.0, 1.0);
    // Nur neu zeichnen, wenn sich wirklich etwas bewegt hat: der
    // Aufrufer darf im Sekundentakt melden, ohne dass es kostet.
    if (qFuzzyCompare(a + 1.0, m_anteil + 1.0) && sendet == m_sendet) {
        return;
    }
    m_anteil = a;
    m_sendet = sendet;
    update();
}

void Ft8Fensterbalken::setzeSignalstrecke(double von, double bis)
{
    m_von = std::clamp(von, 0.0, 1.0);
    m_bis = std::clamp(bis, m_von, 1.0);
    update();
}

void Ft8Fensterbalken::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect r = rect().adjusted(0, 0, -1, -1);
    if (r.width() < 4 || r.height() < 3) { return; }

    // Die Mulde: schwarz mit feinem Rand, wie jedes versenkte Feld.
    p.fillRect(r, QColor(QString::fromLatin1(Style::kInsetBg)));
    p.setPen(QColor(QString::fromLatin1(Style::kInsetBorder)));
    p.drawRect(r);

    const QRect innen = r.adjusted(1, 1, -1, -1);
    if (innen.width() < 2) { return; }

    // Zwei feine Striche: wo das Signal anfaengt und aufhoert. Eine
    // gefuellte Flaeche dahinter wuerde mit dem Fortschritt streiten
    // (im Entwurfsblatt vom 2026-10-30 ausprobiert und verworfen).
    p.setPen(QColor(QString::fromLatin1(Style::kMarkeImGlas)));
    for (const double a : {m_von, m_bis}) {
        const int x = innen.left()
                      + int(a * double(innen.width()) + 0.5);
        if (x > innen.left() && x < innen.right()) {
            p.drawLine(x, innen.top(), x, innen.bottom());
        }
    }

    const int voll = int(m_anteil * double(innen.width()) + 0.5);
    if (voll > 0) {
        // Messing beim Hoeren, das gedaempfte Rot der Sendetaste beim
        // Senden. Kein kraeftiges Rot: das bleibt der Warnung.
        p.fillRect(QRect(innen.left(), innen.top(), voll, innen.height()),
                   QColor(QString::fromLatin1(
                       m_sendet ? Style::kRedBorder : Style::kTxKeyBorder)));
    }
}

} // namespace Longpath
