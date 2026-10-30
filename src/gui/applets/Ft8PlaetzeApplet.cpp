// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/applets/Ft8PlaetzeApplet.cpp  (Longpath)
// =================================================================
// Siehe Ft8PlaetzeApplet.h fuer den Anlass und die Wahl des Blattes.
//
// Gemalt wird nach Entwurf A vom 2026-10-30
// (werkzeug/ft8-plaetze-2026-10-30/runde3-A-kacheln.png). Die eine
// warme Stelle im ganzen Panel ist der Punkt „on air" im Kopf: bei
// Multi Answering senden ALLE Plaetze gleichzeitig, „sendet" ist
// darum kein Unterschied zwischen ihnen, sondern ein Zustand des
// Ganzen. Zwoelf warme Marken waeren zwoelfmal dieselbe Aussage und
// ein klarer Bruch der Zwei-Prozent-Regel des Hausstils.
// =================================================================

#include "Ft8PlaetzeApplet.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"

#include <QFontMetrics>
#include <QPalette>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

#include <algorithm>

namespace Longpath {

namespace {

/// Jede Farbe geht durch `hexRole()`. Das ist der zweite Weg der
/// Themes neben der QSS-Tabelle: wer im Malcode die Konstante direkt
/// nimmt, baut ein Panel, das in jedem Thema gleich aussieht --
/// und darum in den meisten falsch.
QColor farbe(const char* token)
{
    return QColor(Style::hexRole(token));
}

/// Der Rapport, wie er geschrieben wird: immer mit Vorzeichen und
/// zwei Stellen. Dieselbe Form wie in der Nachricht selbst -- so
/// stehen die Zahlen untereinander, auch wenn das Vorzeichen
/// wechselt.
QString rapport(int db)
{
    if (db == -99) { return {}; }
    return QStringLiteral("%1%2")
        .arg(db < 0 ? QLatin1Char('-') : QLatin1Char('+'))
        .arg(qAbs(db), 2, 10, QLatin1Char('0'));
}

}  // namespace

// ══ Die Malflaeche ═══════════════════════════════════════════════

Ft8PlaetzeFlaeche::Ft8PlaetzeFlaeche(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void Ft8PlaetzeFlaeche::setzeStand(const Ft8PlaetzeStand& s)
{
    m_stand = s;
    updateGeometry();
    update();
}

int Ft8PlaetzeFlaeche::noetigeHoehe() const
{
    int h = 10 + 22;   // 22 fuer die Kopfzeile
    h += m_stand.plaetze.size() * (kKachelH + kKachelLuft);
    if (!m_stand.wartende.isEmpty()) {
        h += 28 + m_stand.wartende.size() * 22 + 10;
    }
    return qMax(h, 60);
}

QSize Ft8PlaetzeFlaeche::sizeHint() const
{
    return QSize(320, noetigeHoehe());
}

void Ft8PlaetzeFlaeche::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Den Grund selbst malen. Eingebettet in die Spalte erbt das
    // Widget ihn vom Panel und man merkt nichts; als ABGEDOCKTES
    // Fenster -- und genau darum ging die Bitte („seitlich im eigenen
    // fenster") -- stuende sonst die Vorgabe des Systems dahinter,
    // und das ist auf diesem Rechner Weisz. Im Abgriff vom
    // 2026-10-30 war genau das zu sehen: dunkle Schrift auf hellem
    // Grund, die Haelfte unlesbar.
    p.fillRect(rect(), farbe(Style::kPanelBg));

    int y = maleKopfzeile(p);
    for (int i = 0; i < m_stand.plaetze.size(); ++i) {
        maleKachel(p, y, i + 1, m_stand.plaetze.at(i));
        y += kKachelH + kKachelLuft;
    }
    if (!m_stand.wartende.isEmpty()) {
        maleWartende(p, y);
    }
}

// ── Die EINE warme Stelle ────────────────────────────────────────────
//
// Die Titelleiste gehoert dem Container, nicht uns; „on air" und
// „2 of 3" aus dem Blatt haetten dort keinen Platz. Also traegt die
// Flaeche eine eigene schmale Zeile.
//
// Warum der Punkt nicht je Kachel sitzt: bei Multi Answering senden
// ALLE Plaetze gleichzeitig -- in derselben Tastung, 60 Hz
// auseinander. „Sendet" unterscheidet sie also nicht, es ist ein
// Zustand des Ganzen. Zwoelf warme Marken waeren zwoelfmal dieselbe
// Aussage und ein klarer Bruch der Zwei-Prozent-Regel.
int Ft8PlaetzeFlaeche::maleKopfzeile(QPainter& p)
{
    const int w = width() - 2 * kRandX;
    if (w < 80) { return 2; }

    QFont klein = font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);

    const int belegt = int(std::count_if(
        m_stand.plaetze.cbegin(), m_stand.plaetze.cend(),
        [](const Ft8PlatzStand& z) { return !z.partner.isEmpty(); }));
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRect(kRandX + w - 60, 4, 60, 14),
               Qt::AlignRight | Qt::AlignVCenter,
               tr("%1 of %2").arg(belegt).arg(m_stand.plaetzeGesamt));

    if (m_stand.sendet) {
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(Style::kTxKeyBorder));
        p.drawEllipse(QRectF(kRandX + w - 112, 7.5, 6, 6));
        p.setPen(farbe(Style::kTxKeyBorder));
        p.drawText(QRect(kRandX + w - 102, 4, 46, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, tr("on air"));
    }
    return 22;
}

void Ft8PlaetzeFlaeche::maleKachel(QPainter& p, int y, int nummer,
                                   const Ft8PlatzStand& z)
{
    const int w = width() - 2 * kRandX;
    if (w < 80) { return; }
    const QRect r(kRandX, y, w, kKachelH);

    // Panel-Radius 8 (HAUSSTIL: nie Radius 3).
    p.setPen(farbe(Style::kBorderSubtle));
    p.setBrush(farbe(Style::kTileActiveBg));
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    // Die Platznummer, klein und links -- eine Spurnummer, kein Titel.
    QFont klein = font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextInactive));
    p.drawText(QRect(r.left() + 8, r.top() + 10, 14, 14),
               Qt::AlignLeft | Qt::AlignVCenter, QString::number(nummer));

    const int tx = r.left() + 28;

    // Rufzeichen -- oder der CQ-Ruf, der keines hat.
    QFont gross = font();
    gross.setPointSizeF(z.partner.isEmpty() ? 14.0 : 16.0);
    p.setFont(gross);
    p.setPen(farbe(z.partner.isEmpty() ? Style::kTextScale
                                       : Style::kTextPrimary));
    p.drawText(QRect(tx, r.top() + 6, w - 100, 24),
               Qt::AlignLeft | Qt::AlignVCenter,
               z.partner.isEmpty() ? tr("calling CQ") : z.partner);

    // Land, oder was der CQ-Ruf stattdessen sagt.
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(z.partner.isEmpty() ? Style::kTextInactive
                                       : Style::kTextScale));
    p.drawText(QRect(tx, r.top() + 30, w - 100, 14),
               Qt::AlignLeft | Qt::AlignVCenter,
               z.partner.isEmpty() ? tr("waiting for an answer") : z.land);

    // Die Tonlage rechts oben. Grau, nicht warm: sie ist eine
    // Einstellung, keine Messung.
    // `Style::monoFont` statt `setFamily("Menlo")`: Menlo gibt es nur
    // auf dem Mac. Qt nimmt anderswo klaglos die naechstbeste Schrift,
    // und das ist nicht zwingend eine dicktengleiche -- dann stehen
    // die Zahlen nicht mehr untereinander, und genau dafuer ist die
    // Mono-Schrift hier da. `tst_schriftfamilien` liest den Quelltext
    // und faengt es (2026-10-30, auf dem Linux-Shard).
    p.setFont(Style::monoFont(font(), 13));
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(QRect(r.right() - 76, r.top() + 8, 68, 14),
               Qt::AlignRight | Qt::AlignVCenter,
               tr("%1 Hz").arg(z.tonHz));

    // Der naechste Zug, als Pille (Radius 6).
    if (!z.zug.isEmpty()) {
        const QRect pille(r.right() - 56, r.top() + 28, 48, 17);
        p.setPen(farbe(Style::kBorder));
        p.setBrush(farbe(Style::kPanelBg));
        p.drawRoundedRect(QRectF(pille).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        p.setFont(klein);
        p.setPen(farbe(Style::kTextPrimary));
        p.drawText(pille, Qt::AlignCenter, z.zug);
    }

    // Die beiden Rapporte in einer Zeile, in Mono -- damit sie
    // untereinander stehen, auch wenn das Vorzeichen wechselt.
    QStringList teile;
    if (z.gesendet  != -99) { teile << tr("sent %1").arg(rapport(z.gesendet)); }
    if (z.empfangen != -99) { teile << tr("rcvd %1").arg(rapport(z.empfangen)); }
    if (!teile.isEmpty()) {
        p.setFont(Style::monoFont(font(), 12));
        p.setPen(farbe(Style::kLabelMid));
        p.drawText(QRect(tx, r.top() + 50, w - 40, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   teile.join(QStringLiteral("   ")));
    }

    // Der Fortschritt: fuenf Striche, einer je Zug im QSO. Kein
    // durchgehender Balken -- ein QSO hat Stufen, keinen Verlauf.
    const int sy = r.top() + 76;
    for (int k = 0; k < kStufen; ++k) {
        const int x0 = tx + k * 13;
        // Erreicht gegen offen: im ersten Abgriff lagen beide so dicht
        // beieinander, dass die Stufe nicht abzulesen war. Jetzt
        // Zweitfarbe gegen die gedaempfte Rahmenfarbe.
        p.setPen(QPen(farbe(k < z.stufe ? Style::kTextSecondary
                                        : Style::kBorder), 2));
        p.drawLine(x0, sy, x0 + 9, sy);
    }
}

void Ft8PlaetzeFlaeche::maleWartende(QPainter& p, int y)
{
    const int w = width() - 2 * kRandX;
    p.setPen(farbe(Style::kBorderSubtle));
    p.drawLine(kRandX, y, kRandX + w, y);

    QFont klein = font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRect(kRandX + 4, y + 8, 160, 14),
               Qt::AlignLeft | Qt::AlignVCenter, tr("WAITING"));

    int yy = y + 26;
    for (const Ft8WartenderStand& wart : m_stand.wartende) {
        QFont f = font();
        f.setPointSizeF(10.5);
        p.setFont(f);
        p.setPen(farbe(Style::kLabelMid));
        p.drawText(QRect(kRandX + 14, yy, 110, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, wart.partner);

        p.setFont(klein);
        p.setPen(farbe(Style::kTextInactive));
        p.drawText(QRect(kRandX + 100, yy, w - 160, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, wart.land);

        p.setFont(Style::monoFont(font(), 12));
        p.setPen(farbe(Style::kTextScale));
        p.drawText(QRect(kRandX + w - 50, yy, 44, 16),
                   Qt::AlignRight | Qt::AlignVCenter, rapport(wart.snr));
        yy += 22;
    }
}

// ══ Das Applet ═══════════════════════════════════════════════════

Ft8PlaetzeApplet::Ft8PlaetzeApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    // Derselbe Grund wie in der Flaeche -- sonst bleibt unter ihr ein
    // heller Streifen stehen, wo der Dehnraum sitzt. Im Abgriff vom
    // 2026-10-30 war er deutlich zu sehen.
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(Style::hexRole(Style::kPanelBg)));
    setPalette(pal);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ── Der leere Zustand sagt die Wahrheit (2026-10-30) ───────
    //
    // Er stand zuerst da als „Set slots to 2 or more" -- und das war
    // irrefuehrend. Betreiber: „fuer was benoetige ich den ft8 slot".
    // Das Panel fuellt sich AUCH mit einem Platz: mit dem laufenden
    // Partner und mit der Warteschlange. Leer ist es nur, solange
    // ueberhaupt nichts laeuft.
    m_leer = new QLabel(tr("Nothing running.\n\n"
                           "While a QSO is going, this shows who you are "
                           "working and who is waiting for you.\n\n"
                           "Set “slots” in the FT8 window to 2 or more "
                           "to work several callers at the same time."), this);
    m_leer->setWordWrap(true);
    m_leer->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_leer->setContentsMargins(14, 14, 14, 14);
    m_leer->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;")
                              .arg(QString::fromLatin1(Style::kTextScale)));
    lay->addWidget(m_leer);

    m_flaeche = new Ft8PlaetzeFlaeche(this);
    m_flaeche->hide();
    lay->addWidget(m_flaeche);
    lay->addStretch(1);
}

void Ft8PlaetzeApplet::zeigeStand(const Longpath::Ft8PlaetzeStand& s)
{
    const bool etwas = hatInhalt(s);
    m_leer->setVisible(!etwas);
    m_flaeche->setVisible(etwas);
    m_flaeche->setzeStand(s);
    m_flaeche->setMinimumHeight(m_flaeche->noetigeHoehe());
}

}  // namespace Longpath
