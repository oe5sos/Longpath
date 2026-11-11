// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/Station3dView.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Die WebEngine-Fassung der 3D-Flaeche: ein QWebEngineView mit der
// Seite resources/flug3d/flug3d.html (CesiumJS, festgenagelt, vom CDN,
// SRI-geprueft) auf Googles Photorealistic 3D Tiles. Nur gebaut, wenn
// Qt WebEngine beim Konfigurieren gefunden wurde (LONGPATH_HAS_WEBENGINE);
// eigene Bibliothek, damit die Pruefstaende WebEngine nicht mitlinken
// (siehe Station3dSurface.h).
//
// Die Seite meldet sich ueber die Konsole ("longpath:{json}"): ready,
// arrived, failed. Ein Flug, der kommt, bevor die Seite bereit ist,
// wartet -- nur der letzte, ein Bediener, der drei Rufzeichen schnell
// nacheinander nachschlaegt, will zum dritten.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include "gui/widgets/Station3dSurface.h"

#include <optional>

class QTimer;
class QWebEngineView;

namespace Longpath {

class Station3dView : public Station3dSurface {
    Q_OBJECT
public:
    explicit Station3dView(QWidget* parent = nullptr,
                           const QString& cesiumBase = Station3dPage::cesiumBaseUrl());

    void fly(const Station3dFlight& flight) override;

    /// Beim Programmstart: Station3dSurface::create() baut ab dann diese.
    static void registerFactory();

    bool isReadyForTest() const { return m_ready; }
    /// Fuer den Pruefstand: die Startfrist (sonst 30 s).
    void setStartTimeoutMsForTest(int ms);

    /// Eine Zeile aus der Konsole der Seite, wie die Seite sie schickt.
    /// Oeffentlich, damit der Pruefstand die Deutung ohne Netz fahren kann.
    void handlePageMessage(const QString& json);

signals:
    void ready();

protected:
    // Die Startfrist zaehlt nur, solange die Ansicht zu sehen ist: eine
    // verborgene QWebEngineView laedt und zeichnet nicht (Live-Befund
    // 2026-11-11). Nach einem Fehlstart versucht das Sichtbarwerden neu.
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void startPage();
    void reportFailure(const QString& reason, bool keyProblem);

    QWebEngineView* m_view{nullptr};
    QString m_cesiumBase;
    QTimer* m_startTimer{nullptr};
    bool m_ready{false};
    bool m_dead{false};
    std::optional<Station3dFlight> m_pending;
};

} // namespace Longpath
