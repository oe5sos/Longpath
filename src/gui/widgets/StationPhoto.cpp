// =================================================================
// src/gui/widgets/StationPhoto.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StationPhoto.h for why the fetching rules
// live in one place instead of two.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/StationPhoto.h"

#include "gui/StyleConstants.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QStandardPaths>
#include <QUrl>

namespace Longpath {

QString StationPhoto::cacheDir()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/qrz-photos");
    QDir().mkpath(dir);
    return dir;
}

QString StationPhoto::cachePath(const QString& url)
{
    const QByteArray h =
        QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1);
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(h.toHex())
           + QStringLiteral(".img");
}

StationPhoto::StationPhoto(QWidget* parent)
    : QWidget(parent)
{
    // Small enough that the 78-pixel frame in the rotor panel does not
    // have to fight it. The pane that wants a large one says so.
    setMinimumSize(60, 45);
    m_placeholder = QStringLiteral("No photo");
}

QSize StationPhoto::sizeHint() const
{
    // Roughly the shape QRZ portraits come in. Not a constraint — the
    // pane that owns this decides the real size — but a sane starting
    // point for anything that lays out on the hint.
    return QSize(220, 165);
}

void StationPhoto::showPlaceholder(const QString& reason)
{
    m_pendingUrl.clear();
    m_pixmap = QPixmap{};
    m_havePhoto = false;
    m_placeholder = reason;
    update();
}

bool StationPhoto::showBytes(const QByteArray& bytes)
{
    QPixmap pm;
    // loadFromData sniffs the format. An HTML error page served with a
    // 200 — which is what several CDNs do for a missing file — fails
    // here, and that is the point: it must not be cached as a portrait.
    if (bytes.isEmpty() || !pm.loadFromData(bytes)) { return false; }
    m_pixmap = pm;
    m_havePhoto = true;
    update();
    emit photoShown();
    return true;
}

void StationPhoto::setUrl(const QString& url)
{
    if (url.trimmed().isEmpty()) {
        showPlaceholder(QStringLiteral("No photo"));
        return;
    }

    const QUrl u(url);
    // A portrait is an https URL. `imageUrl` arrived over the network in
    // somebody else's XML, so it is not trusted to be one — a field
    // holding file:// or a plain-http address to a machine on the local
    // network is not a portrait, and fetching it because a field said so
    // would turn a lookup into a request the operator never made.
    if (!u.isValid() || u.scheme() != QLatin1String("https")) {
        showPlaceholder(QStringLiteral("Photo address is not https — "
                                       "not fetched"));
        return;
    }

    m_pendingUrl = url;

    const QString cached = cachePath(url);
    QFile cf(cached);
    if (cf.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = cf.readAll();
        cf.close();
        if (showBytes(bytes)) { return; }
        // A cached file that will not decode is a cached file that is
        // wrong. Drop it rather than failing the same way forever.
        QFile::remove(cached);
    }

    m_placeholder = QStringLiteral("Loading…");
    update();

    if (!m_net) { m_net = new QNetworkAccessManager(this); }
    QNetworkRequest req(u);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(req);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, cached, url]() {
        reply->deleteLater();

        // The operator has clicked another contact since. Whatever this
        // is a portrait of, it is not the one on screen.
        if (m_pendingUrl != url) { return; }

        if (reply->error() != QNetworkReply::NoError) {
            showPlaceholder(QStringLiteral("Couldn't fetch the photo"));
            return;
        }

        const QByteArray bytes = reply->readAll();
        if (bytes.size() > kMaxBytes) {
            showPlaceholder(QStringLiteral("Photo is too large — skipped"));
            return;
        }
        if (!showBytes(bytes)) {
            showPlaceholder(QStringLiteral("That wasn't an image"));
            return;
        }

        QFile out(cached);
        if (out.open(QIODevice::WriteOnly)) { out.write(bytes); }
    });
}

void StationPhoto::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(QColor(Style::kInsetBorder), 1.0));
    p.setBrush(QColor(Style::kInsetBg));
    p.drawRoundedRect(r, 4.0, 4.0);

    if (m_havePhoto && !m_pixmap.isNull()) {
        // Fit inside, never crop. A portrait cropped to fill loses the
        // face about half the time, which is the only part anybody is
        // looking at.
        const QSize target = m_pixmap.size().scaled(
            size() - QSize(4, 4), Qt::KeepAspectRatio);
        const QRect dst(QPoint((width()  - target.width())  / 2,
                               (height() - target.height()) / 2),
                        target);
        p.drawPixmap(dst, m_pixmap);
        return;
    }

    p.setPen(QColor(Style::kTextScale));
    QFont f = p.font();
    f.setPixelSize(11);
    p.setFont(f);
    p.drawText(rect().adjusted(8, 8, -8, -8),
               Qt::AlignCenter | Qt::TextWordWrap, m_placeholder);
}

} // namespace Longpath
