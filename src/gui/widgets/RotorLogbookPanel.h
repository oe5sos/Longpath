#pragma once

// =================================================================
// src/gui/widgets/RotorLogbookPanel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Thetis has neither a rotator display nor a
// logbook; AetherSDR's callsign card is lookup-only.
//
// Extracted from MainWindow::openRotorDial(), where the same UI had
// grown into a 200-line function of nested lambdas capturing each
// other's widgets. Everything below is the same behaviour, moved
// somewhere it can grow — and the move is what lets the panel be
// docked rather than living in a window of its own.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "core/CallsignInfo.h"
#include "models/LogEntry.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace NereusSDR {

class QrzClient;
class QrzLogbookUploader;
class RadioModel;
class GlobeWidget;
class LogbookWindow;
class RotorDialWidget;

// Aim the antenna and log the contact, in one surface.
//
// The two belong together: the callsign you are about to log is the
// same one you just turned the antenna towards, and both need the same
// locator, distance and bearing. Splitting them would mean typing the
// callsign twice.
//
// None of the three collaborators is owned, and all may be null — the
// panel degrades to manual locators and local-only logging rather than
// refusing to appear.
class RotorLogbookPanel : public QWidget {
    Q_OBJECT
public:
    RotorLogbookPanel(RadioModel* radio, QrzClient* qrz,
                      QrzLogbookUploader* uploader,
                      QWidget* parent = nullptr);

    // Where the ADIF log is written.
    static QString logbookPath();

signals:
    void qsoLogged(const LogEntry& entry);

private:
    void buildUi();
    void wireQrz();

    void onCallsignEdited(const QString& raw);   // country estimate
    void onLookupRequested();                    // QRZ, on demand
    void onLogQso();
    void applyLocators();
    void refreshRecentList();
    void openLogbookWindow();
    void setStatus(const QString& text, bool warn = false);
    // Fetch and show the QRZ portrait. Cached on disk so working the
    // same station twice costs one request, not two.
    void loadStationPhoto(const QString& url);
    void showStationVisuals(const CallsignInfo& info);
    // Flag emoji for the callsign's DXCC prefix, or empty. Kept as a
    // separate step from displaying it so the lookup can be tested and
    // so the flag survives a station line being rewritten.
    void updateFlagFor(const QString& call);
    // Station line, with the current flag in front of it.
    void setStationLine(const QString& text);
    // Point the globe at home and, if known, at the worked station.
    void updateGlobeFromLocators();
    // Sunrise, sunset and grey line at the far end. Refreshed on a
    // timer as well as on edit, because the sun moves while you talk.
    void updateSolarLine();
    // Ask for an equirectangular world image and remember the choice.
    void chooseWorldImage();
    // Fetch one from NASA. Public-domain imagery, so nothing has to be
    // bundled and nothing has to be agreed to. Tries the candidates in
    // order — deep image paths move, and one dead link should cost a
    // second, not the whole feature.
    void downloadWorldImage(const QStringList& candidates, int index,
                            const QString& label);
    bool appendToLogFile(const LogEntry& entry, QString* error);
    LogEntry buildEntry() const;

    RadioModel*         m_radio{nullptr};
    QrzClient*          m_qrz{nullptr};
    QrzLogbookUploader* m_uploader{nullptr};

    RotorDialWidget* m_dial{nullptr};
    GlobeWidget*     m_globe{nullptr};
    QStackedWidget*  m_viewStack{nullptr};
    QPushButton*     m_globeBtn{nullptr};
    QLabel*          m_photo{nullptr};
    QString          m_flagEmoji;
    // Created on first portrait fetch — a panel that never looks anyone
    // up should not open a network stack.
    QNetworkAccessManager* m_net{nullptr};
    QLineEdit* m_callEdit{nullptr};
    QLineEdit* m_myGrid{nullptr};
    QLineEdit* m_dxGrid{nullptr};
    QLineEdit* m_rstSent{nullptr};
    QLineEdit* m_rstRcvd{nullptr};
    QLineEdit* m_comment{nullptr};
    QLabel*    m_status{nullptr};
    QLabel*    m_stationLine{nullptr};
    QLabel*    m_solarLine{nullptr};
    QTableWidget* m_recent{nullptr};
    QPushButton*  m_lookupBtn{nullptr};

    // Created on first use and kept. Two windows over one file is how
    // one of them ends up writing over the other's correction.
    LogbookWindow* m_logWindow{nullptr};

    // Best known position of the far end, from whichever source spoke
    // last: a typed locator, a QRZ grid, or the DXCC entity centre
    // resolved from the prefix. Kept separately from the locator field
    // so the grey line still works while only a prefix is known.
    double m_dxLat{0.0};
    double m_dxLon{0.0};
    bool   m_hasDxPos{false};

    // Detail from the last successful QRZ lookup. Only folded into a
    // logged contact when it belongs to the callsign being logged —
    // never a leftover card from the previous station.
    CallsignInfo m_lastInfo;
};

} // namespace NereusSDR
