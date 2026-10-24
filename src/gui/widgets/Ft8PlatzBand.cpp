// =================================================================
// src/gui/widgets/Ft8PlatzBand.cpp  (Longpath)
// =================================================================
//
// Siehe Ft8PlatzBand.h fuer das Warum und die Entwurfsrunde.
//
// Modification history (Longpath):
//   2026-10-24 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "gui/widgets/Ft8PlatzBand.h"

#include <QPainter>

#include "gui/StyleConstants.h"

namespace Longpath {

namespace {
constexpr int kZeilenHoehe = 44;   // Platz fuer Ruf, Tonlage, Haarlinie
constexpr int kKopfHoehe   = 20;
constexpr int kRand        = 12;
}  // namespace

Ft8PlatzBand::Ft8PlatzBand(QWidget* parent)
    : QWidget(parent)
{
    setVisible(false);          // kostet im Einzelbetrieb keine Zeile
}

void Ft8PlatzBand::setzePlaetze(const QVector<Platz>& plaetze,
                                int vonWievielen)
{
    m_plaetze      = plaetze;
    m_vonWievielen = vonWievielen;
    setVisible(!plaetze.isEmpty());
    updateGeometry();
    update();
}

QSize Ft8PlatzBand::sizeHint() const
{
    if (m_plaetze.isEmpty()) { return QSize(0, 0); }
    return QSize(400, kKopfHoehe + kZeilenHoehe);
}

void Ft8PlatzBand::paintEvent(QPaintEvent*)
{
    if (m_plaetze.isEmpty()) { return; }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor rahmen(QString::fromLatin1(Style::kBorder));
    const QColor text  (QString::fromLatin1(Style::kTextPrimary));
    const QColor leise (QString::fromLatin1(Style::kTextTertiary));
    const QColor stumm (QString::fromLatin1(Style::kTextInactive));
    const QColor akzent(QString::fromLatin1(Style::kAccent));

    // Eine Haarlinie oben trennt vom Wasserfall -- kein Kasten.
    p.setPen(rahmen);
    p.drawLine(kRand, 0, width() - kRand, 0);

    // Ueberschrift links, Zaehler rechts.
    QFont klein = font();
    klein.setPixelSize(Style::kFontCaption);
    klein.setBold(true);
    klein.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(klein);
    p.setPen(leise);
    p.drawText(QRect(kRand, 4, 300, kKopfHoehe - 4),
               Qt::AlignVCenter | Qt::AlignLeft, tr("IN QSO"));

    klein.setBold(false);
    klein.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
    p.setFont(klein);
    p.setPen(stumm);
    p.drawText(QRect(width() - kRand - 220, 4, 220, kKopfHoehe - 4),
               Qt::AlignVCenter | Qt::AlignRight,
               tr("%1 of %2 slots").arg(m_plaetze.size()).arg(m_vonWievielen));

    // Die Zeilen nebeneinander, gleich breit.
    const int nutz   = width() - 2 * kRand;
    const int breite = m_plaetze.isEmpty() ? nutz : nutz / m_plaetze.size();
    QFont rufSchrift = Style::monoFont(font(), Style::kFontBody);
    QFont zugSchrift = Style::monoFont(font(), Style::kFontSmall);
    QFont hzSchrift  = Style::monoFont(font(), Style::kFontCaption);

    for (int i = 0; i < m_plaetze.size(); ++i) {
        const Platz& e = m_plaetze.at(i);
        const int x  = kRand + i * breite;
        const int w  = breite - 16;
        const int y  = kKopfHoehe + 4;

        p.setFont(rufSchrift);
        p.setPen(e.wartet ? stumm : text);
        p.drawText(QRect(x, y, w, 18), Qt::AlignVCenter | Qt::AlignLeft, e.ruf);

        p.setFont(zugSchrift);
        p.setPen(leise);
        p.drawText(QRect(x, y, w, 18), Qt::AlignVCenter | Qt::AlignRight,
                   e.wartet ? tr("waiting") : e.naechstes);

        p.setFont(hzSchrift);
        p.setPen(stumm);
        p.drawText(QRect(x, y + 17, w, 13), Qt::AlignVCenter | Qt::AlignLeft,
                   e.wartet ? QString() : tr("%1 Hz").arg(e.hz));

        // Fortschritt als Haarlinie: der zurueckgelegte Teil hell.
        // Ein Balken waere hier zu laut fuer das, was er sagt.
        const int ly = y + 32;
        p.setPen(rahmen);
        p.drawLine(x, ly, x + w, ly);
        if (!e.wartet && e.schritt > 0) {
            const int bis = x + w * std::min(e.schritt, kSchritteBisQso)
                                / kSchritteBisQso;
            p.setPen(QPen(akzent, 2));
            p.drawLine(x, ly, bis, ly);
        }
    }
}

}  // namespace Longpath
