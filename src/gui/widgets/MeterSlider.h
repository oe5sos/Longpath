// =================================================================
// src/gui/widgets/MeterSlider.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/MeterSlider.h
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. Longpath is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per AetherSDR convention.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-19 — Ported/adapted in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code. Rewrapped in the Longpath
//                 namespace; logic/visuals preserved verbatim from the
//                 AetherSDR source. Dependency of VaxApplet (Phase 3O
//                 Sub-Phase 9, Task 9.2).
// =================================================================

#pragma once

#include <QWidget>
#include "gui/StyleConstants.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

namespace Longpath {

// Combined horizontal level meter + gain slider.
// Background shows RMS level, draggable thumb controls gain.
class MeterSlider : public QWidget {
    Q_OBJECT

public:
    explicit MeterSlider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(16);
        setMinimumWidth(60);
        setCursor(Qt::PointingHandCursor);
        // Performance: paintEvent fills rect() opaquely (line ~82) as
        // the first draw, so Qt does not need to composite our parent
        // under us.  Saves one IOSurface memmove per paint — meaningful
        // because MeterSlider sits in TX applets that update at 10-20 Hz
        // while transmitting.
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

    float gain() const { return m_gain; }
    float level() const { return m_level; }

    void setGain(float g) {
        g = std::clamp(g, 0.0f, 1.0f);
        if (g != m_gain) {
            m_gain = g;
            update();
        }
    }

    void setLevel(float l) {
        l = std::clamp(l, 0.0f, 1.0f);
        if (l != m_level) {
            m_level = l;
            update();
        }
    }

signals:
    void gainChanged(float gain);  // 0.0–1.0

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int w = width();
        const int h = height();
        const int margin = 1;
        const int barH = h - 2 * margin;
        const int barW = w - 2 * margin;

        // Hausstil (2026-09-17, Applet-Blaetter): "Blau ist anfassbar,
        // Warm ist gemessen." Hier stand alles im abgeschafften Tuerkis
        // #00b4d8 auf blaustichigem Grund — der Pegel (gemessen) und der
        // Griff (anfassbar) in derselben Farbe. Jetzt: versenkte Rinne,
        // Pegel in Bernstein (ab 90 % die Warnfarbe), Verstaerkung und
        // Griff in Akzentblau.
        p.fillRect(rect(), QColor(Style::kInsetBg));
        p.setPen(QColor(Style::kBorder));
        p.drawRect(rect().adjusted(0, 0, -1, -1));

        // Level meter fill (behind the slider) — gemessen: Bernstein
        if (m_level > 0.0f) {
            int fillW = static_cast<int>(m_level * barW);
            QColor fillColor = m_level < 0.7f ? QColor(Style::kAmberDim)
                             : m_level < 0.9f ? QColor(Style::kAmberText)
                                              : QColor(Style::kGaugeDanger);
            fillColor.setAlpha(150);
            p.fillRect(margin, margin, fillW, barH, fillColor);
        }

        // Gain thumb position
        int thumbX = margin + static_cast<int>(m_gain * barW);
        thumbX = std::clamp(thumbX, margin, margin + barW);

        // Gain fill (solid, up to thumb) — anfassbar: Akzent
        if (m_gain > 0.0f) {
            int gainW = static_cast<int>(m_gain * barW);
            QColor gainFill(Style::kAccent);
            gainFill.setAlpha(50);
            p.fillRect(margin, margin, gainW, barH, gainFill);
        }

        // Thumb line
        p.setPen(QPen(QColor(Style::kAccent), 2));
        p.drawLine(thumbX, margin, thumbX, margin + barH);

        // Thumb triangle (top)
        QPolygon tri;
        tri << QPoint(thumbX - 3, margin)
            << QPoint(thumbX + 3, margin)
            << QPoint(thumbX, margin + 4);
        p.setBrush(QColor(Style::kAccent));
        p.setPen(Qt::NoPen);
        p.drawPolygon(tri);
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = true;
            updateGainFromMouse(e->pos().x());
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (m_dragging) {
            updateGainFromMouse(e->pos().x());
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = false;
        }
    }

private:
    void updateGainFromMouse(int x) {
        float g = static_cast<float>(x - 1) / static_cast<float>(width() - 2);
        g = std::clamp(g, 0.0f, 1.0f);
        if (g != m_gain) {
            m_gain = g;
            emit gainChanged(m_gain);
            update();
        }
    }

    float m_gain{0.5f};
    float m_level{0.0f};
    bool  m_dragging{false};
};

} // namespace Longpath
