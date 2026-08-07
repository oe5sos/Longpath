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
class QPushButton;
class QTableWidget;

namespace NereusSDR {

class QrzClient;
class QrzLogbookUploader;
class RadioModel;
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
    void setStatus(const QString& text, bool warn = false);
    bool appendToLogFile(const LogEntry& entry, QString* error);
    LogEntry buildEntry() const;

    RadioModel*         m_radio{nullptr};
    QrzClient*          m_qrz{nullptr};
    QrzLogbookUploader* m_uploader{nullptr};

    RotorDialWidget* m_dial{nullptr};
    QLineEdit* m_callEdit{nullptr};
    QLineEdit* m_myGrid{nullptr};
    QLineEdit* m_dxGrid{nullptr};
    QLineEdit* m_rstSent{nullptr};
    QLineEdit* m_rstRcvd{nullptr};
    QLineEdit* m_comment{nullptr};
    QLabel*    m_status{nullptr};
    QLabel*    m_stationLine{nullptr};
    QTableWidget* m_recent{nullptr};
    QPushButton*  m_lookupBtn{nullptr};

    // Detail from the last successful QRZ lookup. Only folded into a
    // logged contact when it belongs to the callsign being logged —
    // never a leftover card from the previous station.
    CallsignInfo m_lastInfo;
};

} // namespace NereusSDR
