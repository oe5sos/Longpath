#pragma once

// =================================================================
// src/gui/widgets/QsoDetailPane.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Everything about one logged contact that the table has no room for.
//
// ── Why a pane rather than more columns ──────────────────────────────
//
// The logbook keeps every ADIF field it does not model — DXCC, CQZ,
// ITUZ, IOTA, CONTEST_ID, SRX, STX, QSL_SENT, LOTW_QSL_SENT, and every
// APP_ tag any other logger ever wrote. That was the point of the
// import fix: rewriting the file no longer destroys them.
//
// But keeping a field and showing it are different things, and until
// now they were only kept. A column each is not the answer — there is
// no bound on how many there are, and a table thirty columns wide is a
// table nobody scrolls. One pane beside the table shows all of them for
// the row you are looking at and costs the table no width at all.
//
// ── The photo, and why it is not fetched automatically ───────────────
//
// A logbook row is a callsign. The portrait comes from QRZ, so showing
// one means a lookup, and arrow-keying down a log would be one lookup
// per row — fifty requests in ten seconds against an API that rate
// limits and a subscription that is metered.
//
// So: the cache is consulted always and the network only on request.
// A callsign already looked up appears complete and instantly. One that
// has not shows a placeholder and a button. An operator who wants the
// automatic behaviour can switch it on, and then it waits 400 ms, which
// is long enough that scrolling past a row is not a lookup.
//
// ── What it says when there is nothing to say ────────────────────────
//
// Every empty state names its own reason. "No photo" is four different
// situations — no lookup yet, no QRZ account, no XML subscription, no
// portrait on file — and each has a different thing to do about it. A
// blank frame tells the operator to go and find out which; a sentence
// does not.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/CallsignInfo.h"
#include "models/LogEntry.h"

#include <QWidget>

class QFormLayout;
class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace NereusSDR {

class CallsignCache;
class QrzClient;
class StationPhoto;

class QsoDetailPane : public QWidget {
    Q_OBJECT
public:
    explicit QsoDetailPane(QWidget* parent = nullptr);

    // Neither is owned. The window that opens this pane borrows both
    // from whoever holds the credentials, and a pane that deleted the
    // shared QRZ client on close would take the rest of the program's
    // lookups with it.
    void setQrzClient(QrzClient* qrz);
    void setCache(CallsignCache* cache);

    // Show this contact. An invalid entry (empty callsign) clears the
    // pane to its "nothing selected" state, which is what a logbook
    // with no selection should look like.
    void setEntry(const LogEntry& entry);
    void clearEntry();

    // Where the rotor is now, so the pane can say how far it will turn
    // rather than only where it will end up. NaN when unknown, which is
    // the honest state when no rotor is connected.
    void setRotorBearing(double deg);

signals:
    void turnRotorRequested(double bearingDeg, const QString& call);
    void editRequested();

private:
    void buildUi();
    void refresh();               // redraw from m_entry + m_info
    void refreshExtras();         // the ADIF fields with no column
    void refreshBeam();
    void applyInfo(const CallsignInfo& info, bool stale);
    void requestLookup();
    void wireQrz();

    // Is automatic lookup on? Off by default — see the header note.
    static bool autoLookupEnabled();

    LogEntry     m_entry;
    CallsignInfo m_info;
    bool         m_haveEntry{false};

    QrzClient*     m_qrz{nullptr};
    CallsignCache* m_cache{nullptr};
    QTimer*        m_lookupDelay{nullptr};

    StationPhoto* m_photo{nullptr};
    QLabel*  m_call{nullptr};
    QLabel*  m_name{nullptr};
    QLabel*  m_where{nullptr};
    QLabel*  m_stale{nullptr};      // "looked up in March" when old
    QWidget* m_badges{nullptr};
    QLabel*  m_qslCard{nullptr};
    QLabel*  m_qslLotw{nullptr};
    QLabel*  m_qslEqsl{nullptr};

    QLabel*      m_shortPath{nullptr};
    QLabel*      m_longPath{nullptr};
    QLabel*      m_distance{nullptr};
    QPushButton* m_turnShort{nullptr};
    QPushButton* m_turnLong{nullptr};
    QLabel*      m_travel{nullptr};

    QWidget*     m_extrasBox{nullptr};
    QFormLayout* m_extras{nullptr};
    QLabel*      m_extrasEmpty{nullptr};

    QPushButton* m_lookupBtn{nullptr};
    QPushButton* m_editBtn{nullptr};
    QPushButton* m_qrzPageBtn{nullptr};

    double m_rotorDeg{0.0};
    bool   m_haveRotor{false};
};

} // namespace NereusSDR
