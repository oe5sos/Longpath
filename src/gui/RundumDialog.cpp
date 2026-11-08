// no-port-check: Longpath-original, see RundumDialog.h.
//
// src/gui/RundumDialog.cpp  (Longpath)

#include "gui/RundumDialog.h"

#include "core/RotorController.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Longpath {

// ── Polarbild ───────────────────────────────────────────────────────

RundumBild::RundumBild(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(240, 240);
}

void RundumBild::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(Style::kGlassPanelBot));

    const QPointF m = QRectF(rect()).center();
    const qreal r = std::min(width(), height()) * 0.5 - 22.0;
    if (r < 40.0) { return; }

    // Ringe und Himmelsrichtungen. Norden oben, im Uhrzeigersinn.
    auto punkt = [&](double az, qreal radius) {
        const double w = (az - 90.0) * M_PI / 180.0;
        return QPointF(m.x() + radius * std::cos(w), m.y() + radius * std::sin(w));
    };
    p.setPen(QPen(QColor(Style::kBorder), 1.0));
    p.setBrush(Qt::NoBrush);
    for (qreal f : {0.33, 0.66, 1.0}) { p.drawEllipse(m, r * f, r * f); }
    p.setFont(Style::monoFont(p.font(), Style::kFontCaption));
    p.setPen(QColor(Style::kTextScale));
    const char* namen[4] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; ++i) {
        const QPointF q = punkt(i * 90.0, r + 11.0);
        p.drawText(QRectF(q.x() - 8, q.y() - 8, 16, 16), Qt::AlignCenter,
                   QString::fromLatin1(namen[i]));
    }

    if (m_karte) {
        const auto& s = m_karte->sektoren();
        float maxBel = 0.0f;
        float minBoden = 0.0f, maxBoden = 0.0f;
        bool erster = true;
        for (const auto& x : s) {
            if (x.proben == 0) { continue; }
            maxBel = std::max(maxBel, x.belegungMax);
            if (erster) { minBoden = maxBoden = x.boden(); erster = false; }
            minBoden = std::min(minBoden, x.boden());
            maxBoden = std::max(maxBoden, x.boden());
        }
        // Belegung: je Sektor ein Keil, Laenge relativ zum staerksten.
        QColor fuell(Style::kAmberText);
        fuell.setAlpha(150);
        p.setPen(Qt::NoPen);
        p.setBrush(fuell);
        const double halb = RundumKarte::kSektorGrad / 2.0;
        for (int i = 0; i < s.size(); ++i) {
            if (s[i].proben == 0 || maxBel <= 0.0f) { continue; }
            const qreal rr = r * (0.08 + 0.92 * s[i].belegungMax / maxBel);
            const double az = i * RundumKarte::kSektorGrad;
            QPainterPath keil(m);
            keil.lineTo(punkt(az - halb, rr));
            keil.lineTo(punkt(az + halb, rr));
            keil.closeSubpath();
            p.drawPath(keil);
        }
        // Rauschboden: eine Linie, innen der ruhigste Sektor, aussen
        // mindestens 10 dB darueber -- damit ein Unterschied von 1 dB
        // nicht wie ein Stoerer aussieht.
        const float spanne = std::max(10.0f, maxBoden - minBoden);
        QPainterPath linie;
        bool offen = false;
        for (int i = 0; i <= s.size(); ++i) {
            const auto& x = s[i % s.size()];
            if (x.proben == 0) { offen = false; continue; }
            const qreal rr = r * (0.25 + 0.75 * (x.boden() - minBoden) / spanne);
            const QPointF q = punkt(i * RundumKarte::kSektorGrad, rr);
            if (!offen) { linie.moveTo(q); offen = true; } else { linie.lineTo(q); }
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(Style::kTextSecondary), 1.5));
        p.drawPath(linie);
    }

    if (m_azGueltig) {
        p.setPen(QPen(QColor(Style::kTextPrimary), 1.5));
        p.drawLine(m, punkt(m_az, r));
    }
}

// ── Fenster ─────────────────────────────────────────────────────────

RundumDialog::RundumDialog(RadioModel* radio, SpectrumWidget* spektrum, QWidget* parent)
    : QDialog(parent)
    , m_radio(radio)
    , m_spektrum(spektrum)
{
    setWindowTitle(tr("Rotor sweep"));
    auto* lay = new QVBoxLayout(this);
    auto* text = new QLabel(tr("Turns the antenna once around in 30° steps and "
                               "records, per direction, the activity in the "
                               "panadapter (amber) and the noise floor (line). "
                               "It returns to the starting bearing."), this);
    text->setWordWrap(true);
    text->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    lay->addWidget(text);

    m_bild = new RundumBild(this);
    m_bild->setKarte(&m_lauf.karte());
    lay->addWidget(m_bild, 1);

    m_status = new QLabel(this);
    lay->addWidget(m_status);
    m_ergebnis = new QLabel(this);
    m_ergebnis->setWordWrap(true);
    lay->addWidget(m_ergebnis);

    auto* knoepfe = new QHBoxLayout();
    m_start = new QPushButton(tr("Start"), this);
    m_stopp = new QPushButton(tr("Stop"), this);
    m_stopp->setEnabled(false);
    knoepfe->addStretch(1);
    knoepfe->addWidget(m_start);
    knoepfe->addWidget(m_stopp);
    lay->addLayout(knoepfe);

    connect(m_start, &QPushButton::clicked, this, &RundumDialog::starte);
    connect(m_stopp, &QPushButton::clicked, &m_lauf, &RundumLauf::abbrechen);
    connect(&m_lauf, &RundumLauf::fortschritt, this, [this](int i, int n, double az) {
        m_status->setText(tr("Step %1 of %2 — %3°").arg(i).arg(n).arg(qRound(az)));
        m_bild->setRichtung(az, true);
        m_bild->update();
    });
    connect(&m_lauf, &RundumLauf::fertig, this, &RundumDialog::beende);
    connect(&m_lauf, &RundumLauf::fehler, this, [this](const QString& t) {
        m_status->setText(t);
    });
    if (m_radio && m_radio->rotor()) {
        connect(m_radio->rotor(), &RotorController::positionChanged, this, [this](double az) {
            m_bild->setRichtung(az, true);
        });
    }
    if (m_spektrum) {
        connect(m_spektrum, &SpectrumWidget::bandLage, &m_lauf, &RundumLauf::nimmBandLage);
        connect(m_spektrum, &SpectrumWidget::bandLage, m_bild, [this]() { m_bild->update(); });
    }
}

RundumDialog::~RundumDialog()
{
    if (m_lauf.phase() != RundumLauf::Phase::Ruhe) { m_lauf.abbrechen(); }
    if (m_spektrum) { m_spektrum->setBandLageMelden(false); }
}

void RundumDialog::starte()
{
    if (!m_spektrum) {
        m_status->setText(tr("No panadapter to measure with."));
        return;
    }
    m_lauf.setRotor(m_radio ? m_radio->rotor() : nullptr);
    m_ergebnis->clear();
    if (!m_lauf.start()) { return; }
    m_spektrum->setBandLageMelden(true);
    m_start->setEnabled(false);
    m_stopp->setEnabled(true);
}

void RundumDialog::beende(bool vollstaendig)
{
    if (m_spektrum) { m_spektrum->setBandLageMelden(false); }
    m_start->setEnabled(true);
    m_stopp->setEnabled(false);
    m_status->setText(vollstaendig ? tr("Done — back at the starting bearing.")
                                   : tr("Stopped — turning back to the starting bearing."));
    zeigeErgebnis();
}

void RundumDialog::zeigeErgebnis()
{
    const RundumKarte& k = m_lauf.karte();
    if (k.abgedeckt() == 0) {
        m_ergebnis->setText(tr("No measurements were taken."));
        return;
    }
    float laut = 0.0f;
    const int bel = k.meisteBelegung();
    const int boden = k.lautesterBoden(&laut);
    m_ergebnis->setText(tr("Most activity: %1° · Noisiest: %2° (+%3 dB over the "
                           "quietest) · %4 of %5 directions measured")
                            .arg(bel, 3, 10, QLatin1Char('0'))
                            .arg(boden, 3, 10, QLatin1Char('0'))
                            .arg(laut, 0, 'f', 1)
                            .arg(k.abgedeckt())
                            .arg(RundumKarte::kSektoren));
}

QString RundumDialog::statusTextForTest() const { return m_status->text(); }
QString RundumDialog::ergebnisTextForTest() const { return m_ergebnis->text(); }

} // namespace Longpath
