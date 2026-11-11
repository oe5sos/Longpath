#pragma once

// =================================================================
// src/gui/QsoMapWindow.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Where the contacts in a period actually were. Two views of the same
// data because they answer different questions: the globe shows the
// paths as the signals took them, the flat map shows all of them at
// once. Half the Earth is always facing away from a sphere, and "where
// have I worked this year" is a question about all of it.
//
// Only contacts with a usable locator can be placed. The window says
// how many were left out rather than quietly drawing fewer dots than
// the log has records — a map that silently omits a third of the log
// is worse than no map.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Band and mode filter pills, a station card for a
//                 clicked marker, the Maidenhead grid overlay switch,
//                 and export of the filtered view to Google Earth
//                 (KML). The pills are rebuilt from whatever bands and
//                 modes the log actually contains — a fixed list would
//                 offer 60 m to an operator who has never used it and
//                 omit whatever oddity they have. AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-09-21 — Luftbild (NASA GIBS) unter der flachen Karte, ein
//                 Rufzeichenfeld, das per QRZ-Lookup den Ort findet und
//                 die Karte dorthin fliegt (flyToStation), und der
//                 Anschluss aus dem Logbuch: wer dort eine Station
//                 nachschlaegt, sieht sie hier landen. Martin Fischer,
//                 AI-assisted via Anthropic Claude.
//   2026-11-10 — Vierte Ansicht „Azimuthal": die Azimutalkarte um das
//                 eigene QTH (AzimutalKarteWidget) mit jedem gezeigten
//                 QSO als Strahl, dem Rotor live und der nachgeschlagenen
//                 Station mit kurzem und langem Weg. Die gewaehlte
//                 Ansicht wird gemerkt (QsoMapView). Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
//   2026-11-10 — 3D-Flug: mit eigenem Google-Schluessel (Map Tiles API)
//                 fliegt eine vierte Ansicht vom QTH den Grosskreis
//                 entlang bis aufs Dach der Station (Station3dSurface,
//                 nur mit Qt WebEngine). Ohne Schluessel oder WebEngine
//                 bleibt der NASA-Flug, dazu „View in Google Earth".
//                 Martin Fischer (OE5SOS), AI-assisted via Anthropic
//                 Claude.
//   2026-11-11 — Locator-Jagd mit FT8: zum Grid-Haken das Mosaik je Band
//                 (bestaetigt / gearbeitet / noch nie, wahlweise nur FT8),
//                 auf flacher Karte und Azimutalkarte; die gehoerten
//                 FT8-Stationen der letzten Minuten als Punkte; ein Klick
//                 ruft ueber das FT8-Fenster an (Ft8KartenBruecke), der
//                 Zaehler „N new squares heard now on 20m". Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "core/LocatorJagd.h"
#include "models/LogEntry.h"

#include <QDialog>
#include <QSet>
#include <QTimer>
#include <QVector>

#include <QUrl>

#include <functional>

class QCheckBox;
class QDateEdit;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;

class QComboBox;
class QEvent;
class QLineEdit;
class QSpinBox;

namespace Longpath {

class FlatMapWidget;
class FlowLayout;
class GibsTileLayer;
class GlobeWidget;
class QrzClient;
class SatelliteService;

class QsoMapWindow : public QDialog {
    Q_OBJECT
public:
    explicit QsoMapWindow(QWidget* parent = nullptr);

    // Marked contacts each get their own line, up to this many.
    //
    // A cap rather than a promise to draw everything: the arcs are
    // slerped per repaint, and past a few hundred the globe stops
    // turning smoothly — which is precisely when a large selection is
    // being looked at. Contacts beyond the limit are counted in the
    // summary rather than silently dropped.
    static constexpr int kMaxMarkedPaths = 500;

    // The contacts to draw, and where the operator is. Both may be
    // replaced whenever the log changes.
    void setEntries(const QVector<LogEntry>& entries);
    void setHomeGrid(const QString& grid);

    /// Rufzeichen und Portrait auf den Kartenmarker legen. Aus den
    /// Einstellungen und den beiden Caches, ohne Netzwerk — was schon
    /// geholt wurde, liegt auf der Platte; was nicht, bleibt ein Punkt.
    void applyStationMarker();

    // Rows picked out in the log's table. Passing a non-empty selection
    // switches the window to showing only those; the operator can flip
    // back to the date range, where they stay picked out in colour
    // rather than being the only thing drawn.
    void setSelection(const QVector<LogEntry>& selected);

    // Last resort for a contact with no locator: the centre of its DXCC
    // entity. Injected because cty.dat lives with the radio model and
    // this window is opened from the logbook, which has no radio.
    //
    // Without it a log whose contacts carry no GRIDSQUARE — which is
    // most logs imported from elsewhere, and every QSO logged before a
    // QRZ lookup filled the field — draws an empty map.
    using PositionFallback =
        std::function<bool(const QString& call, double& lat, double& lon)>;
    void setPositionFallback(PositionFallback fn);

    // ── Hinflug zu einer Station ─────────────────────────────────────
    //
    // Der Ort einer Station, aus dem QRZ-Lookup oder dem Locator: die
    // flache Karte fliegt dorthin, zeichnet den Grosskreis vom eigenen
    // Standort und zeigt die Station als Ring mit Rufzeichen. Aufrufbar
    // von aussen (das Logbuch schlaegt eine Station nach) und aus dem
    // eigenen Rufzeichenfeld.
    void flyToStation(const QString& call, double lat, double lon,
                      const QString& caption = QString());
    /// Fuer den Lookup aus dem eigenen Rufzeichenfeld.
    void setQrzClient(QrzClient* qrz);
    /// Der geteilte Kachelspeicher, fuer Tests und Einstellungen.
    GibsTileLayer* imagery() const { return m_tiles; }
    FlatMapWidget* flatMapForTest() const { return m_flat; }
    class DxRadarWidget* radarForTest() const { return m_radar; }
    QPushButton* radarButtonForTest() const { return m_radarBtn; }
    class AzimutalKarteWidget* azimutalForTest() const { return m_azimut; }
    QPushButton* azimutalButtonForTest() const { return m_azimutBtn; }
    /// Wie viele Kontakte die Karte beim letzten Aufbau gezeichnet hat.
    int shownCountForTest() const { return m_lastShown.size(); }
    /// Ein Rufzeichen so behandeln, als waere es im Feld eingegeben.
    void lookupAndFly(const QString& call);

    // ── 3D-Flug mit Googles Kacheln (2026-11-10) ─────────────────────
    //
    // Seite 4 im Stapel (hinter der Azimutalkarte auf 3). Die Flaeche entsteht erst beim ersten 3D-Flug
    // (WebEngine startet einen eigenen Prozess -- nicht fuer jeden, der
    // die Karte nur oeffnet). Ohne WebEngine im Bau, ohne Schluessel
    // oder mit abgewaehltem „3D" fliegt die flache Karte wie bisher, und
    // „View in Google Earth" erscheint neben der Stationskarte.
    static constexpr int kView3d = 4;
    QPushButton* threeDButtonForTest() const { return m_3dBtn; }
    QPushButton* googleEarthButtonForTest() const { return m_googleEarthBtn; }
    class Station3dSurface* threeDSurfaceForTest() const { return m_3d; }
    int currentViewForTest() const;
    QWidget* summaryForTest() const;
    class FlowLayout* pillRowForTest() const { return m_pillRow; }
    const QVector<QWidget*>& flatOnlyControlsForTest() const { return m_flatOnlyControls; }
    QString infoTextForTest() const;
    /// Statt QDesktopServices: der Pruefstand faengt die URL ab.
    void setUrlOpenerForTest(std::function<void(const QUrl&)> fn) { m_openUrl = std::move(fn); }

    /// Satelliten ueber dem Horizont als Schicht auf der flachen Karte:
    /// Subsatellitenpunkt, Name, Elevation; alle 10 s neu gerechnet.
    /// Der Dienst gehoert dem Hauptfenster.
    void setSatellites(class SatelliteService* svc);
    void refreshSatellites();

    // ── Eingebettet im Logbuchfenster ────────────────────────────────
    //
    // Als Spalte neben der Logtabelle statt als eigenes Fenster: kein
    // Datumsbereich und keine Schnellwahl (die Tabelle filtert, die
    // Karte zeigt, was die Tabelle zeigt), Escape schliesst nichts, und
    // ein Knopf „↗" bittet um das grosse Fenster. Vor show() rufen.
    void setEmbedded(bool on);

    // ── Rotor neben der Karte (2026-09-26, Blatt "Radar 3") ─────────
    //
    // Ein kleines Radar links in der Kartenflaeche, immer da: Kegel =
    // wohin die Antenne gerade zeigt, blaue Marke = die Station. Zeigt die
    // Flaeche selbst das Radar, traegt das grosse den Rotor und das kleine
    // tritt zurueck. Negative Werte = unbekannt / kein Ziel.
    void setRotorRadarShown(bool on);
    void setRotorHeading(double deg);
    void setRotorTarget(double deg);
    void setRotorBeamWidth(double deg);
    class DxRadarWidget* rotorRadarForTest() const { return m_rotorRadar; }
    void showRadarViewForTest(bool on);
    bool isEmbedded() const { return m_embedded; }

    // ── Locator-Jagd mit FT8 (2026-11-11) ───────────────────────────
    //
    // Das Mosaik fragt das GANZE Logbuch („welches Feld fehlt mir"),
    // nicht das gefilterte: eingebettet bekommt die Karte nur, was die
    // Tabelle gerade zeigt, und ein Filter auf „diese Woche" machte die
    // halbe Welt zu „noch nie". Ohne Aufruf gilt, was setEntries() kam.
    void setMosaikLog(const QVector<LogEntry>& ganzesLog);
    /// Eine gehoerte Station (von Ft8KartenBruecke, im Pruefstand direkt).
    void nimmFt8(const Longpath::Ft8Gehoert& g);
    /// Ein Klick auf einen FT8-Punkt. Leer heisst: der Anruf laeuft;
    /// sonst der Grund, warum nicht. Die Antwort steht auch in der
    /// Infozeile unter der Karte.
    QString rufeLive(const QString& rufzeichen);
    /// Das Band, fuer das das Mosaik gerade steht („" = alle).
    QString mosaikBand() const;

    // Fuer Pruefstaende.
    void setUhrForTest(std::function<QDateTime()> uhr) { m_uhr = std::move(uhr); }
    const LocatorMosaik& mosaikForTest() const { return m_mosaik; }
    const LiveSchicht&   liveForTest() const { return m_live; }
    QLabel*    liveZaehlerForTest() const { return m_liveZaehler; }
    QComboBox* mosaikBandForTest() const { return m_mosaikBand; }
    QCheckBox* nurFt8ForTest() const { return m_nurFt8; }
    QCheckBox* gridForTest() const { return m_grid; }
    QSpinBox*  liveMinutenForTest() const { return m_liveMinuten; }
    void       refreshLiveForTest() { refreshLive(); }

signals:
    void popOutRequested();

protected:
    void closeEvent(QCloseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void buildUi();
    void rebuild();
    void setQuickRange(int days);   // 0 means everything
    // Zoom whichever of the two views is on screen.
    void zoomActiveView(double factor);
    void resetActiveView();

    // Pills for the bands and modes the log actually contains,
    // recreated whenever the entries change.
    void rebuildFilterPills();
    // The filtered view, as a KML file, opened in Google Earth.
    void exportKml();
    // Everything the log knows about a clicked marker's station.
    void showStationInfo(const QString& label);
    // How many contacts sit in a clicked grid square, and who.
    void showGridInfo(const QString& locator);

    // Filter keys: band lower-cased, mode upper-cased, blanks as "?".
    static QString bandKey(const LogEntry& e);
    static QString modeKey(const LogEntry& e);

    QVector<LogEntry> m_all;
    QVector<LogEntry> m_selected;
    QString m_homeGrid;
    PositionFallback m_fallback;

    QStackedWidget* m_stack{nullptr};
    GlobeWidget*    m_globe{nullptr};
    FlatMapWidget*  m_flat{nullptr};
    class DxRadarWidget* m_radar{nullptr};   // dritte Ansicht (2026-09-22)
    class AzimutalKarteWidget* m_azimut{nullptr};   // vierte (2026-11-10)
    class DxRadarWidget* m_rotorRadar{nullptr};   // klein, links (2026-09-26)
    bool m_rotorRadarShown{false};
    void updateRotorRadarVisibility();
    QPushButton*    m_radarBtn{nullptr};
    QPushButton*    m_azimutBtn{nullptr};
    int             m_viewBeforeRadar{0};    // Kugel, flach oder 3D, wohin „Radar"/„Azimuthal" aus zurueckfuehrt
    // Stapelplaetze. Radar und Azimutalkarte liegen UEBER Kugel/flach:
    // ihr Knopf ist ein Haken, der dorthin zurueckfuehrt, wo man herkam.
    static constexpr int kViewGlobe = 0;
    static constexpr int kViewFlat = 1;
    static constexpr int kViewRadar = 2;
    static constexpr int kViewAzimuthal = 3;
    // Radar und Azimutalkarte liegen ueber Kugel/flach und fuehren dorthin
    // zurueck; das 3D-Bild (kView3d) ist KEIN Overlay.
    static constexpr bool isOverlayView(int i) { return i == kViewRadar || i == kViewAzimuthal; }
    // Die vom Betreiber gewaehlte Ansicht merken / beim Oeffnen
    // wiederherstellen. Nur Knopfdruecke zaehlen -- ein Hinflug oder das
    // Durchzoomen schalten auch um, sind aber keine Wahl.
    void rememberView() const;
    void restoreView();

    // ── Wahl des Kartenhintergrunds ─────────────────────────────────
    //
    // Der Ordner wird beim OEFFNEN der Liste gelesen, nicht beim Start:
    // wer eine Datei ablegt, waehrend das Programm laeuft, soll sie ohne
    // Neustart sehen. Dafuer der Ereignisfilter auf der Liste selbst --
    // showPopup() ist kein Signal, an das man sich haengen koennte.
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    QComboBox* m_background{nullptr};
    void reloadBackgroundList();
    void applyBackgroundChoice(int index);

    QDateEdit*   m_from{nullptr};
    QDateEdit*   m_to{nullptr};
    QCheckBox*   m_paths{nullptr};
    QCheckBox*   m_grid{nullptr};
    QCheckBox*   m_imagery{nullptr};
    QLineEdit*   m_callEdit{nullptr};
    GibsTileLayer* m_tiles{nullptr};
    QCheckBox*     m_satellites{nullptr};
    SatelliteService* m_satService{nullptr};
    QTimer*        m_satTimer{nullptr};
    QrzClient*   m_qrz{nullptr};
    QString      m_pendingCall;   // Lookup unterwegs fuer dieses Rufzeichen
    QCheckBox*   m_onlySelected{nullptr};
    QLabel*      m_summary{nullptr};
    QLabel*      m_info{nullptr};        // station card / grid answer
    QPushButton* m_viewBtn{nullptr};

    // Filter pills and their state. An empty active set means "no
    // filtering" (everything shown), so a fresh window hides nothing.
    FlowLayout*   m_pillRow{nullptr};
    QSet<QString> m_offBands;    // pills the operator has turned OFF
    QSet<QString> m_offModes;

    // What the current filters left visible — the set the KML export
    // sends, so Google Earth shows exactly what the window shows.
    QVector<LogEntry> m_lastShown;
    // The date controls and the quick-range buttons, greyed together
    // while only the selection is shown — a live date field that
    // changes nothing is worse than a disabled one.
    QVector<QWidget*> m_rangeControls;
    QVector<QWidget*> m_flatOnlyControls;   // im 3D-Bild ausgeblendet
    QWidget*          m_rangeBox{nullptr};
    QWidget*          m_backgroundBox{nullptr};   // KARTE + Auswahl
    QPushButton*      m_earthBtn{nullptr};
    QPushButton*      m_popOutBtn{nullptr};
    bool              m_embedded{false};

    // ── 3D-Flug ──────────────────────────────────────────────────────
    struct LastFlight {
        bool    valid{false};
        QString call;
        QString caption;     // Rich Text, wie die Stationskarte ihn zeigt
        double  lat{0.0};
        double  lon{0.0};
    };
    bool use3d() const;
    void ensure3d();
    void update3dButton();
    void flyFlat(const QString& call, double lat, double lon);
    void fly3d();
    void on3dFailed(const QString& reason, bool keyProblem);
    void openGoogleEarth();
    void showFlightCard(const QString& extra);
    void apply3dRoom();   // im 3D-Bild treten Pillen, Zaehlung, Rotor-Grafik zurueck
    QPushButton*      m_3dBtn{nullptr};
    QPushButton*      m_googleEarthBtn{nullptr};
    QWidget*          m_3dHost{nullptr};
    class Station3dSurface* m_3d{nullptr};
    bool              m_3dWanted{true};
    QString           m_3dRefusedKey;    // Zugang (Quelle:Text), der abgelehnt wurde
    static QString    accessIdentity();
    LastFlight        m_lastFlight;

    // ── Locator-Jagd (2026-11-11) ─────────────────────────────────
    void refreshMosaik();
    void refreshLive();
    QDateTime jetzt() const;
    QVector<LogEntry> m_mosaikLog;
    bool              m_hatMosaikLog{false};
    LocatorMosaik     m_mosaik;
    LiveSchicht       m_live;
    QString           m_liveBand;          // Band der zuletzt gehoerten Station
    QComboBox*        m_mosaikBand{nullptr};
    QCheckBox*        m_nurFt8{nullptr};
    QWidget*          m_liveBox{nullptr};
    QLabel*           m_liveZaehler{nullptr};
    QSpinBox*         m_liveMinuten{nullptr};
    QTimer*           m_liveTakt{nullptr};
    std::function<QDateTime()> m_uhr;
    std::function<void(const QUrl&)> m_openUrl;
};

} // namespace Longpath
