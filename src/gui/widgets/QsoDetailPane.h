#pragma once

// =================================================================
// src/gui/widgets/QsoDetailPane.h  (Longpath)
// =================================================================
//
// Longpath-original.
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
// Modification history (Longpath):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/CallsignInfo.h"
#include "models/LogEntry.h"

#include <QPointer>
#include <QWidget>

#include <functional>

class QFormLayout;
class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace Longpath {

class CallsignCache;
class HoehenprofilFenster;
class CtyDatParser;
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

    // Ask QRZ now, whatever the automatic setting says. For the places
    // where the operator has plainly asked — pressing Return on a
    // callsign is not the same as scrolling past one, and the whole
    // reason lookups are throttled is scrolling.
    void lookUpNow();

    // Show a station that is not in the log. "Who is this?" is a fair
    // question about somebody you have never worked, and there is no
    // LogEntry to hang the answer on — so one is made with nothing in
    // it but the callsign, and the pane says the rest is unknown rather
    // than inventing a bearing.
    void showCallsign(const QString& call);

    // Where the rotor is now, so the pane can say how far it will turn
    // rather than only where it will end up. NaN when unknown, which is
    // the honest state when no rotor is connected.
    void setRotorBearing(double deg);

    // ── Peilung auch fuer Stationen, die nicht im Log stehen (2026-11-10)
    //
    // Betreiber: Rufzeichen eingeben, ROTOR, sehen wohin der Rotor zeigt
    // und wo die Station steht. Bis heute stand dort fuer jede neue
    // Station „Not in your log — no bearing until you work them": der
    // eigene Standort kam nur aus dem QSO (MY_GRIDSQUARE) oder aus
    // StationGridSquare, und das ist bei Martin leer. Jetzt:
    //   eigener Standort  QSO -> StationGridSquare -> setHomeGrid()
    //   Station           SOTA-Gipfel -> QRZ-Koordinaten -> QRZ-Locator -> Log-Locator
    //                     -> Landesmitte (setPositionFallback, cty.dat)
    // und die Karte sagt, woher die Lage stammt -- eine Landesmitte ist
    // keine Hausnummer.
    using PositionFallback =
        std::function<bool(const QString& call, double& lat, double& lon)>;
    void setPositionFallback(PositionFallback fn);
    void setHomeGrid(std::function<QString()> fn);

    // Gipfel (Probe-Merge Rotor + Hoehenprofil): die Gegenstation steht auf
    // einem SOTA-Gipfel (SOTA_REF) -- dessen Lage geht allem anderen vor.
    enum class Lage { Keine, Gipfel, QrzKoordinaten, QrzLocator, LogLocator, Landesmitte };
    // Woher die Lage der Station gerade stammt, und wie genau sie ist --
    // als Satz fuer die Oberflaeche ("street address (QRZ)").
    Lage    lage() const { return m_lage; }
    QString lageText() const;
    bool    lageGrob() const;
    // Peilung (kurzer Weg) und Entfernung, wie die Karte sie zeigt.
    bool peilung(double& bearingDeg, double& km) const;
    QString gezeigtesRufzeichen() const { return m_haveEntry ? m_entry.call : QString(); }
    QPushButton* profilButtonForTest() const { return m_profilBtn; }
    HoehenprofilFenster* profilFensterForTest() const;
    // ── Grauzone, SFI, Kp (2026-11-10) ───────────────────────────────
    //
    // Die Zeile unter BEAM: wie das Licht an beiden Enden stand und wie
    // Sonne und Magnetfeld an dem Tag waren (core/Grauzone.h,
    // core/GfzIndizes.h). cty.dat fuer die Landesmitte, wenn der Locator
    // fehlt; das Ersatzfeld fuer QSOs ohne MY_GRIDSQUARE (im Logbuch: das
    // juengste im Log). Beides nicht besessen.
    void setCty(const CtyDatParser* cty);
    void setMeinFeldErsatz(const QString& feld);

signals:
    void turnRotorRequested(double bearingDeg, const QString& call);
    /// Ein Lookup hat die Station verortet (QRZ-Koordinaten oder
    /// Locator). Die Karte fliegt dorthin, wenn sie offen ist.
    void stationLocated(const QString& call, double lat, double lon,
                        const QString& caption);
    void editRequested();
    // Die Richtung zur gezeigten Station, sobald sie sich aendert (-1 =
    // keine). Fuer das Rotor-Radar neben der Karte (2026-09-26).
    void beamChanged(double bearingDeg);
    // Nach JEDER Neuberechnung der Peilung, auch wenn die Richtung gleich
    // bleibt (JA1XY -> JA1XYZ: beide Landesmitte). Fuer die Rotorzeile
    // im Logbuch, die Rufzeichen und Lagequelle mitzeigt (2026-11-10).
    void peilungNeu();

private:
    void buildUi();
    void refresh();               // redraw from m_entry + m_info
    void refreshExtras();         // the ADIF fields with no column
    void refreshBeam();
    void refreshLicht();
    void applyInfo(const CallsignInfo& info, bool stale);
    void requestLookup();
    void wireQrz();

    // Is automatic lookup on? Off by default — see the header note.
    static bool autoLookupEnabled();

    LogEntry     m_entry;
    CallsignInfo m_info;
    bool         m_haveEntry{false};
    // Set by showCallsign(): this is a station looked up by name, not a
    // contact out of the log. It changes what the empty states should
    // say — "no bearing, needs both locators" is wrong advice for
    // somebody you have never worked.
    bool         m_notLogged{false};

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
    QLabel*      m_licht{nullptr};      // "Grey line at their end · SFI 142 · Kp 2"

    const CtyDatParser* m_cty{nullptr};
    QString             m_meinFeldErsatz;

    QWidget*     m_extrasBox{nullptr};
    QFormLayout* m_extras{nullptr};
    QLabel*      m_extrasEmpty{nullptr};

    QPushButton* m_lookupBtn{nullptr};
    QPushButton* m_editBtn{nullptr};
    QPushButton* m_qrzPageBtn{nullptr};

    // ── Hoehenprofil (2026-11-11) ─────────────────────────────────────
    // "Profile" fuer 2-m-, 70-cm- und 23-cm-QSOs; ein Fenster, das beim
    // naechsten Druck den Inhalt wechselt (HoehenprofilFenster.h sagt,
    // warum hier und nicht in der Karte).
    QPushButton* m_profilBtn{nullptr};
    QPointer<HoehenprofilFenster> m_profilFenster;
    void refreshProfil();
    void oeffneProfil();
    // Der eigene Locator, wenn das QSO keinen hat.
    static QString ersatzLocator();

    double m_rotorDeg{0.0};
    bool   m_haveRotor{false};

    // Die Peilung, die refreshBeam() gerade zeigt und die Drehknoepfe
    // schicken. Aus der genauesten Lage, die da ist (siehe beamNow()).
    double m_beamDeg{0.0};
    bool   m_haveBeam{false};
    // Peilung und Entfernung von hier zur Station, aus QRZ-Koordinaten
    // vor dem Locator, und nicht aus dem Wert im Log.
    bool beamNow(double& bearingDeg, double& km, Lage* lage = nullptr) const;
    // Wo die Station steht, in EINER Reihenfolge fuer Peilung und Karte
    // (2026-11-11). mitLandesmitte: cty.dat als letzter Ausweg.
    bool stationsPunkt(double& lat, double& lon, Lage& woher, bool mitLandesmitte) const;
    // Ein 6-stelliger Log-Locator, in dessen Feld der QRZ-Punkt NICHT
    // liegt -- Contest, portabel: dann gilt das Log.
    bool logLocatorSchlaegtQrz() const;
    double m_beamKm{0.0};
    Lage   m_lage{Lage::Keine};
    QLabel* m_lageLabel{nullptr};
    PositionFallback       m_fallback;
    std::function<QString()> m_homeGrid;
};

} // namespace Longpath
