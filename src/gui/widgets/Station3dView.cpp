// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/Station3dView.cpp  (Longpath)
// =================================================================
// Longpath-original. No Thetis port. Siehe Station3dView.h.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-10 — Absagen von Cesium ion und Google je Quelle gedeutet.
// =================================================================

#include "gui/widgets/Station3dView.h"

#include "core/GoogleMapTiles.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace Longpath {

namespace {

Q_LOGGING_CATEGORY(lcFlug3d, "longpath.flug3d")

constexpr int kStartTimeoutMs = 30000;

// Die Seite meldet ueber console.log("longpath:{json}"); alles andere
// aus der Konsole geht ins Protokoll, damit ein Fehler in CesiumJS
// nicht still bleibt.
class ConsolePage : public QWebEnginePage {
public:
    std::function<void(const QString&)> onMessage;

    explicit ConsolePage(QObject* parent) : QWebEnginePage(parent) {}

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString& message, int line,
                                  const QString& source) override
    {
        static const QString kPrefix = QStringLiteral("longpath:");
        if (message.startsWith(kPrefix)) {
            if (onMessage) { onMessage(message.mid(kPrefix.size())); }
            return;
        }
        if (level == ErrorMessageLevel) {
            qCWarning(lcFlug3d) << "page:" << message << source << line;
        }
    }

    // Ein Klick auf einen Verweis in der Seite (Googles Nutzungs-
    // bedingungen in der Quellenzeile) gehoert in den Browser des
    // Bedieners, nicht in die Kartenflaeche.
    bool acceptNavigationRequest(const QUrl& url, NavigationType type,
                                 bool isMainFrame) override
    {
        if (type == NavigationTypeLinkClicked) {
            QDesktopServices::openUrl(url);
            return false;
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

} // namespace

void Station3dView::registerFactory()
{
    Station3dSurface::setFactory([](QWidget* parent) -> Station3dSurface* {
        return new Station3dView(parent);
    });
}

Station3dView::Station3dView(QWidget* parent, const QString& cesiumBase)
    : Station3dSurface(parent)
    , m_view(new QWebEngineView(this))
    , m_startTimer(new QTimer(this))
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_view);

    auto* page = new ConsolePage(m_view);
    page->onMessage = [this](const QString& json) { handlePageMessage(json); };
    m_view->setPage(page);
    QWebEngineSettings* s = page->settings();
    s->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    s->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    // Hintergrund schwarz wie der Weltraum, bevor die Seite steht --
    // sonst blitzt beim ersten Flug ein weisses Rechteck auf.
    page->setBackgroundColor(Qt::black);

    m_startTimer->setSingleShot(true);
    m_startTimer->setInterval(kStartTimeoutMs);
    connect(m_startTimer, &QTimer::timeout, this, [this]() {
        if (!m_ready) {
            reportFailure(QStringLiteral("the 3D view did not start within %1 s")
                              .arg(kStartTimeoutMs / 1000), false);
        }
    });
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (!ok && !m_ready) {
            reportFailure(QStringLiteral("the 3D page could not be loaded"), false);
        }
    });

    const QString html = Station3dPage::html(cesiumBase);
    if (html.isEmpty()) {
        // Die Ressource fehlt -- ein Baufehler, kein Netzproblem.
        QTimer::singleShot(0, this, [this]() {
            reportFailure(QStringLiteral("the 3D page is missing from this build"), false);
        });
        return;
    }
    // Eine https-Basis gibt der Seite einen sicheren Ursprung; geladen
    // wird von dort nichts (.invalid ist reserviert, RFC 2606).
    m_view->setHtml(html, QUrl(QStringLiteral("https://longpath.invalid/flug3d/")));
    m_startTimer->start();
}

void Station3dView::fly(const Station3dFlight& flight)
{
    if (m_dead) {
        // Die Seite ist nicht hochgekommen; der Aufrufer faellt zurueck.
        QTimer::singleShot(0, this, [this]() {
            emit failed(QStringLiteral("the 3D view is not running"), false);
        });
        return;
    }
    if (!m_ready) {
        m_pending = flight;
        return;
    }
    m_view->page()->runJavaScript(Station3dPage::flightScript(flight));
}

void Station3dView::reportFailure(const QString& reason, bool keyProblem)
{
    m_startTimer->stop();
    if (!m_ready) { m_dead = true; }
    m_pending.reset();
    qCWarning(lcFlug3d) << "3D flight failed:" << reason;
    emit failed(reason, keyProblem);
}

void Station3dView::handlePageMessage(const QString& json)
{
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    const QString ev = o.value(QStringLiteral("event")).toString();

    if (ev == QLatin1String("ready")) {
        m_startTimer->stop();
        m_ready = true;
        m_dead = false;
        emit ready();
        if (m_pending) {
            const Station3dFlight f = *m_pending;
            m_pending.reset();
            fly(f);
        }
        return;
    }
    if (ev == QLatin1String("arrived")) {
        emit arrived();
        return;
    }
    if (ev == QLatin1String("failed")) {
        const QString kind = o.value(QStringLiteral("kind")).toString();
        const QString message = o.value(QStringLiteral("message")).toString();
        if (kind == QLatin1String("cesium")) {
            reportFailure(QStringLiteral("CesiumJS could not be loaded — no internet?"), false);
            return;
        }
        if (kind == QLatin1String("webgl")) {
            reportFailure(QStringLiteral("this computer's graphics cannot show the 3D "
                                         "view (WebGL: %1)").arg(message), false);
            return;
        }
        // Die Absage des Dienstes: dieselbe Deutung wie der Test im
        // Dialog, damit der Bediener an beiden Stellen denselben Satz liest.
        const Map3dAccess::Source src =
            o.value(QStringLiteral("source")).toString() == QLatin1String("ion")
                ? Map3dAccess::Source::CesiumIon : Map3dAccess::Source::Google;
        const int status = o.value(QStringLiteral("status")).toInt();
        const QByteArray body = o.value(QStringLiteral("body")).toString().toUtf8();
        if (status == 0) {
            reportFailure(QStringLiteral("%1 could not be reached — %2")
                              .arg(src == Map3dAccess::Source::CesiumIon
                                       ? QStringLiteral("Cesium ion")
                                       : QStringLiteral("Google's 3D tiles"),
                                   message), false);
            return;
        }
        QString detail;
        const GoogleTilesProbe::Verdict v = src == Map3dAccess::Source::CesiumIon
            ? GoogleTilesProbe::interpretIon(status, body, &detail)
            : GoogleTilesProbe::interpret(status, body, &detail);
        const bool keyProblem = GoogleTilesProbe::isAccessProblem(v);
        // Nach einem gelungenen Start bleibt die Seite benutzbar; nur der
        // Flug faellt zurueck.
        m_startTimer->stop();
        m_pending.reset();
        qCWarning(lcFlug3d) << Map3dAccess::sourceName(src) << "refused the 3D tiles:"
                            << status << detail;
        emit failed(GoogleTilesProbe::sentenceFor(src, v, detail), keyProblem);
        return;
    }
    if (ev == QLatin1String("tileFailed") || ev == QLatin1String("jsError")) {
        qCWarning(lcFlug3d) << ev << o.value(QStringLiteral("message")).toString();
    }
}

} // namespace Longpath
