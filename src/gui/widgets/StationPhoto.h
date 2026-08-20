#pragma once

// =================================================================
// src/gui/widgets/StationPhoto.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// The portrait a callsign database has for a station, with the rules
// that make fetching one safe.
//
// ── Why this is a class and not four lines in a slot ─────────────────
//
// The rules were written once, inside RotorLogbookPanel, and they are
// the interesting part:
//
//   * https only. `imageUrl` is a string that arrived over the network.
//     A field that says file:///etc/passwd or http://192.168.1.1/ is
//     not a portrait, and fetching it because a field said so is how a
//     lookup becomes a request the operator never made.
//   * NoLessSafeRedirectPolicy, so a redirect cannot walk the request
//     back down to plain http.
//   * A size cap, so a redirect to something enormous cannot fill the
//     config directory with a file nobody asked for.
//   * A cache on disk beside the log, because the same station gets
//     looked at again and every restart should not cost QRZ a request.
//
// The detail pane in the logbook needs exactly the same rules. Copying
// them would give the project two versions of a security decision, and
// the second copy is the one that quietly loses a clause. So the rules
// live here and both callers use them.
//
// ── What it shows when there is no photo ─────────────────────────────
//
// Not nothing. An empty frame is indistinguishable from a broken one,
// and the operator cannot tell "this station has no portrait" from "you
// have no QRZ subscription" from "the lookup has not run yet". Each of
// those has a different thing to do about it, so each gets its own
// sentence.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork). The
//                 fetching, caching and https rules are lifted from
//                 RotorLogbookPanel::loadStationPhoto, which now calls
//                 in here instead of keeping its own copy.
// =================================================================

#include <QPixmap>
#include <QString>
#include <QWidget>

class QNetworkAccessManager;

namespace Longpath {

class StationPhoto : public QWidget {
    Q_OBJECT
public:
    explicit StationPhoto(QWidget* parent = nullptr);

    // Show the portrait at `url`, from the disk cache when it is there
    // and over the network when it is not. A url that is empty, not
    // https, or not an image leaves the placeholder up.
    void setUrl(const QString& url);

    // Clear the picture and say why there is none. `reason` is shown to
    // the operator, so it should name the thing they could do about it.
    void showPlaceholder(const QString& reason);

    // Whether a picture is currently displayed, as opposed to the
    // placeholder. Callers that lay out around it want to know.
    bool hasPhoto() const { return m_havePhoto; }

    // Where the portraits are kept. Beside the log rather than in the
    // system cache: they are small, they belong to contacts the operator
    // made, and a directory the OS may clear would silently start
    // costing a request per lookup again.
    static QString cacheDir();
    static QString cachePath(const QString& url);

    // Largest file that will be accepted from the network. A portrait
    // is tens of kilobytes; anything at this size is not one.
    static constexpr qint64 kMaxBytes = 4 * 1024 * 1024;

signals:
    // Emitted once a picture is actually up, so a pane that reserves
    // space for one can stop reserving it.
    void photoShown();

protected:
    void paintEvent(QPaintEvent* e) override;
    QSize sizeHint() const override;

private:
    // Decode and display. False when the bytes are not an image, which
    // is the case a redirect to an HTML error page produces.
    bool showBytes(const QByteArray& bytes);

    QNetworkAccessManager* m_net{nullptr};
    QPixmap m_pixmap;
    QString m_placeholder;
    QString m_pendingUrl;   // guards against a stale reply overwriting
    bool    m_havePhoto{false};
};

} // namespace Longpath
