// =================================================================
// src/gui/QsoMapWindow.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see QsoMapWindow.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-10 — Band/mode filter pills, station card on marker click,
//                 grid overlay switch, Google Earth (KML) export; see
//                 QsoMapWindow.h. AI-assisted via Anthropic Claude
//                 (Cowork), operator Martin Fischer.
//   2026-11-10 — Azimutalkarte als vierte Ansicht, gemerkte Ansicht;
//                 see QsoMapWindow.h. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code).
//   2026-11-10 — 3D-Flug ueber Googles Photorealistic 3D Tiles mit dem
//                 eigenen Schluessel, Rueckfall NASA + Google Earth; siehe
//                 QsoMapWindow.h. Martin Fischer (OE5SOS), AI-assisted via
//                 Anthropic Claude.
//   2026-11-11 — Locator-Jagd mit FT8 (Mosaik, gehoerte Stationen, Klick
//                 ruft an, Zaehler); siehe QsoMapWindow.h. Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "QsoMapWindow.h"

#include "core/AdifLog.h"
#include "core/AppSettings.h"
#include "core/BeamHeading.h"
#include "core/KmlExport.h"
#include "core/Maidenhead.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/DxRadarWidget.h"
#include "gui/widgets/AzimutalKarteWidget.h"
#include "gui/widgets/FlowLayout.h"
#include "gui/widgets/GibsTileLayer.h"
#include "core/sat/SatelliteService.h"
#include "gui/widgets/GlobeWidget.h"
#include "core/QrzClient.h"
#include "gui/widgets/StationPhoto.h"
#include "core/CallsignCache.h"
#include "core/GoogleMapTiles.h"
#include "gui/Google3dSetupDialog.h"
#include "gui/Ft8KartenBruecke.h"
#include "gui/widgets/Station3dSurface.h"

#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QComboBox>
#include <QStandardItemModel>
#include "gui/widgets/WorldMapCatalog.h"
#include "gui/widgets/WorldTexture.h"
#include <QCloseEvent>
#include <QDateEdit>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextDocumentFragment>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <cmath>

namespace Longpath {

namespace {
// Der 3D-Knopf meldet jeden Druck ins Protokoll: Schluessel da, WebEngine
// da, wo (Live-Befund 2026-11-10: „tut nichts" -- und keine Zeile dazu).
Q_LOGGING_CATEGORY(lcQsoMap3d, "longpath.flug3d.karte")
} // namespace

QsoMapWindow::QsoMapWindow(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("QSO map"));
    setModal(false);

    // A plain QDialog on macOS gets a close button and nothing else —
    // no zoom, no full screen — so a map window could not be made
    // bigger, which is the first thing anyone wants from a map. Asking
    // for the window flags explicitly gets the whole title bar.
    setWindowFlags(Qt::Window
                   | Qt::WindowTitleHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint);
    setSizeGripEnabled(true);

    // Remember how big it was. A window that opens small every time is
    // a window you resize every time.
    const QByteArray geom = AppSettings::instance()
        .value(QStringLiteral("QsoMapGeometry")).toByteArray();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    } else {
        resize(1100, 660);
    }

    m_openUrl = [](const QUrl& u) { QDesktopServices::openUrl(u); };
    m_3dWanted = AppSettings::instance()
        .value(QStringLiteral("QsoMap3dFlight"), true).toBool();

    buildUi();
}

void QsoMapWindow::closeEvent(QCloseEvent* e)
{
    AppSettings::instance().setValue(QStringLiteral("QsoMapGeometry"),
                                     saveGeometry());
    QDialog::closeEvent(e);
}

void QsoMapWindow::keyPressEvent(QKeyEvent* e)
{
    // The usual keys. Someone who has zoomed anything else on this
    // machine will try them before finding the buttons.
    switch (e->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:  zoomActiveView(1.25);  return;
    case Qt::Key_Minus:  zoomActiveView(1 / 1.25); return;
    case Qt::Key_0:      resetActiveView();     return;
    // Enter gehoert dem Feld, in dem er gedrueckt wurde (Rufzeichen:
    // hinfliegen). QDialog drueckte sonst seinen Standardknopf -- beim
    // Zeigen der erste Knopf der Fokuskette, „24 h" -- und stellte den
    // Zeitraum still auf heute: „0 contacts in range" (Befund 2026-11-10).
    case Qt::Key_Return:
    case Qt::Key_Enter:  e->accept();           return;
    default: break;
    }
    // Eingebettet ist Escape kein „Fenster zu" — QDialog wuerde das
    // Widget sonst mitten aus dem Logbuch ausblenden.
    if (m_embedded && e->key() == Qt::Key_Escape) { e->ignore(); return; }
    QDialog::keyPressEvent(e);
}

void QsoMapWindow::setEmbedded(bool on)
{
    if (m_embedded == on) { return; }
    m_embedded = on;
    setWindowFlags(on ? Qt::Widget : Qt::Dialog);
    if (m_rangeBox) { m_rangeBox->setVisible(!on); }
    for (QWidget* w : m_rangeControls) { w->setVisible(!on); }
    // Weltbild-Auswahl und KML-Export gehoeren ins grosse Fenster; in
    // der Spalte kosten sie nur eine Zeile.
    if (m_backgroundBox) { m_backgroundBox->setVisible(!on); }
    if (m_earthBtn) { m_earthBtn->setVisible(!on); }
    if (m_popOutBtn) { m_popOutBtn->setVisible(on); }
    rebuild();
}

void QsoMapWindow::zoomActiveView(double factor)
{
    // Radar und Azimutalkarte zoomen nicht: beide zeigen immer die
    // ganze Welt um das eigene QTH. 3D auch nicht ueber die Knoepfe;
    // dort dreht das Rad.
    if (isOverlayView(m_stack->currentIndex()) || m_stack->currentIndex() == kView3d) { return; }
    if (m_stack->currentIndex() == 0) { m_globe->zoomBy(factor); }
    else                              { m_flat->zoomBy(factor); }
}

void QsoMapWindow::resetActiveView()
{
    if (isOverlayView(m_stack->currentIndex()) || m_stack->currentIndex() == kView3d) { return; }
    if (m_stack->currentIndex() == 0) { m_globe->resetView(); }
    else                              { m_flat->resetView(); }
}

void QsoMapWindow::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QString::fromLatin1(Style::kAppBg)));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // Umbruch-Leiste wie im Logbuchfenster: zwanzig Elemente in einer
    // Zeile hielten das Fenster ueber 1300 px breit (2026-09-21).
    auto* bar = new FlowLayout(nullptr, 6, 6);

    auto caption = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                             .arg(QString::fromLatin1(Style::kTextScale)));
        return l;
    };

    // Beschriftung und Feld als ein Element, damit der Umbruch sie nicht
    // trennt; beide Daten zusammen, ein Bereich ist ein Bereich.
    QLabel* fromCap = caption(QStringLiteral("FROM"));
    m_from = new QDateEdit(QDate::currentDate().addDays(-30), this);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setFixedWidth(118);

    QLabel* toCap = caption(QStringLiteral("TO"));
    m_to = new QDateEdit(QDate::currentDate(), this);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setFixedWidth(118);
    {
        auto* box = new QWidget(this);
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        h->addWidget(fromCap);
        h->addWidget(m_from);
        h->addWidget(toCap);
        h->addWidget(m_to);
        bar->addWidget(box);
        m_rangeBox = box;
    }
    m_rangeControls << fromCap << m_from << toCap << m_to;

    // Quick ranges, because typing two dates to answer "what did I work
    // this week" is three interactions too many.
    struct Quick { const char* label; int days; };
    for (const Quick& q : {Quick{"24 h", 1}, Quick{"7 d", 7},
                           Quick{"30 d", 30}, Quick{"1 y", 365},
                           Quick{"All", 0}}) {
        auto* b = new QPushButton(QString::fromLatin1(q.label), this);
        b->setStyleSheet(Style::buttonBaseStyle());
        bar->addWidget(b);
        m_rangeControls << b;
        const int days = q.days;
        connect(b, &QPushButton::clicked, this,
                [this, days]() { setQuickRange(days); });
    }

    // ── Kartenhintergrund ───────────────────────────────────────────
    auto* bgCap = new QLabel(QStringLiteral("KARTE"), this);
    bgCap->setStyleSheet(QStringLiteral("color: %1; font-size: 9px;")
                             .arg(QString::fromLatin1(Style::kTextScale)));

    m_background = new QComboBox(this);
    m_background->setMinimumWidth(150);
    m_background->setToolTip(tr("World image for map and globe. "
                                            "The files live "
                                            "in\n%1\n\nExpected is an "
                                            "equirectangular projection at a "
                                            "2:1 ratio centred on 0 degrees "
                                            "longitude. An image with a "
                                            "different ratio is offered "
                                            "greyed out and not drawn \u2014 "
                                            "stretched, the map would look "
                                            "right and only the station "
                                            "markers would sit wrong.")
            .arg(WorldMapCatalog::directory()));
    // Die Liste erst beim Aufklappen fuellen. Der Filter, weil
    // showPopup() kein Signal ist.
    m_background->installEventFilter(this);
    reloadBackgroundList();
    connect(m_background, QOverload<int>::of(&QComboBox::activated),
            this, &QsoMapWindow::applyBackgroundChoice);
    {
        auto* box = new QWidget(this);
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        h->addWidget(bgCap);
        h->addWidget(m_background);
        bar->addWidget(box);
        m_backgroundBox = box;
    }

    // ── Zu einer Station fliegen ─────────────────────────────────────
    //
    // Ein Rufzeichen, Enter — und die flache Karte fliegt dorthin, wo
    // QRZ die Station verortet, zeichnet den Grosskreis von zu Hause
    // und setzt den Ring. Ohne QRZ-Konto bleibt der Locator aus dem
    // Log oder die Landesmitte.
    m_callEdit = new QLineEdit(this);
    m_callEdit->setPlaceholderText(QStringLiteral("Fly to call…"));
    m_callEdit->setToolTip(QStringLiteral(
        "Type a callsign and press Enter: the flat map flies to the "
        "station (QRZ position, else the locator, else the country) and "
        "draws the path from home."));
    m_callEdit->setFixedWidth(150);
    m_callEdit->setClearButtonEnabled(true);
    bar->addWidget(m_callEdit);
    connect(m_callEdit, &QLineEdit::returnPressed, this,
            [this]() { lookupAndFly(m_callEdit->text()); });

    bar->addStretch();

    m_onlySelected = new QCheckBox(QStringLiteral("Only marked"), this);
    m_onlySelected->setEnabled(false);
    m_onlySelected->setToolTip(QStringLiteral(
        "Show only the rows picked out in the logbook. Unticked, they "
        "stay marked in colour among the rest."));
    m_onlySelected->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                      .arg(QString::fromLatin1(Style::kAmberText)));
    bar->addWidget(m_onlySelected);

    m_paths = new QCheckBox(QStringLiteral("Paths"), this);
    m_paths->setChecked(true);
    m_paths->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                               .arg(QString::fromLatin1(Style::kTextPrimary)));
    bar->addWidget(m_paths);

    m_grid = new QCheckBox(QStringLiteral("Grid"), this);
    m_grid->setToolTip(QStringLiteral(
        "Maidenhead locator overlay on the flat and the azimuthal map, with "
        "every square of the log filled in: green confirmed, sand worked, "
        "empty never — for the band chosen next to it. Click a square to "
        "see its name and what the log has there."));
    m_grid->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                              .arg(QString::fromLatin1(Style::kTextPrimary)));
    bar->addWidget(m_grid);

    // ── Locator-Jagd mit FT8 (2026-11-11) ───────────────────────────
    //
    // Betreiber: „besonders bei FT8 sehen, welche Locator ich noch nicht
    // gearbeitet habe und welche gerade erreichbar sind". Das Band des
    // Mosaiks folgt von selbst dem, was das FT8-Fenster hoert -- wer auf
    // 20 m jagt, fragt nach 20 m.
    m_mosaikBand = new QComboBox(this);
    m_mosaikBand->setObjectName(QStringLiteral("qsoMapMosaicBand"));
    m_mosaikBand->setToolTip(QStringLiteral(
        "Which band the square mosaic shows. \"FT8 band\" follows the band "
        "the FT8 decoder is listening on."));
    m_mosaikBand->setFocusPolicy(Qt::NoFocus);   // keep the +/- keys working
    // Waechst mit „FT8 band (20m)" -- sonst blieb es bei der Breite
    // von „FT8 band" und die Klammer war abgeschnitten.
    m_mosaikBand->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    bar->addWidget(m_mosaikBand);

    m_nurFt8 = new QCheckBox(QStringLiteral("FT8 only"), this);
    m_nurFt8->setObjectName(QStringLiteral("qsoMapMosaicFt8Only"));
    m_nurFt8->setToolTip(QStringLiteral(
        "Count only FT8 contacts for the mosaic and for what is new: a "
        "square worked on SSB is then still new for FT8."));
    m_nurFt8->setChecked(AppSettings::instance()
        .value(QStringLiteral("QsoMapMosaicFt8Only"), QStringLiteral("False"))
        .toString() == QStringLiteral("True"));
    m_nurFt8->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                .arg(QString::fromLatin1(Style::kTextPrimary)));
    bar->addWidget(m_nurFt8);

    // Der Zaehler und das Fenster der gehoerten Stationen. Unsichtbar,
    // bis das FT8-Fenster etwas meldet -- ohne FT8 waere „0 new squares"
    // nur Rauschen in der Leiste.
    {
        m_liveBox = new QWidget(this);
        auto* h = new QHBoxLayout(m_liveBox);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        m_liveZaehler = new QLabel(QString{}, m_liveBox);
        m_liveZaehler->setObjectName(QStringLiteral("qsoMapFt8Counter"));
        m_liveZaehler->setToolTip(QStringLiteral(
            "Squares heard on FT8 within the window that are not yet in your "
            "log on that band. Click an amber point on the map to call."));
        m_liveMinuten = new QSpinBox(m_liveBox);
        m_liveMinuten->setRange(1, 60);
        m_liveMinuten->setPrefix(QStringLiteral("last "));
        m_liveMinuten->setSuffix(QStringLiteral(" min"));
        m_liveMinuten->setToolTip(QStringLiteral(
            "How long a station heard on FT8 stays on the map"));
        const int sek = AppSettings::instance()
            .value(QStringLiteral("QsoMapFt8LiveSeconds"),
                   LiveSchicht::kVorgabeSekunden).toInt();
        m_live.setFensterSekunden(sek);
        m_liveMinuten->setValue(std::max(1, m_live.fensterSekunden() / 60));
        h->addWidget(m_liveZaehler);
        h->addWidget(m_liveMinuten);
        m_liveBox->setVisible(false);
        bar->addWidget(m_liveBox);
    }

    // Luftbild: Blue Marble und Landsat von NASA GIBS, gezeichnet sobald
    // die Karte hineingezoomt ist. Voreingestellt an — wer die Karte
    // aufzieht, will den Ort sehen, nicht ein verwaschenes Weltbild.
    m_imagery = new QCheckBox(QStringLiteral("Imagery"), this);
    m_imagery->setChecked(true);
    m_imagery->setToolTip(QStringLiteral(
        "Satellite imagery under the flat map once zoomed in (NASA GIBS: "
        "Blue Marble, Landsat). Fetched from the internet and cached on "
        "disk; untick to stay offline."));
    m_imagery->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                 .arg(QString::fromLatin1(Style::kTextPrimary)));
    bar->addWidget(m_imagery);

    // Satelliten ueber dem Horizont, als Dreiecke auf der flachen Karte.
    // Voreingestellt an; ohne Bahndaten (kein Netz, noch nie geholt)
    // zeichnet die Schicht schlicht nichts.
    m_satellites = new QCheckBox(QStringLiteral("Satellites"), this);
    m_satellites->setChecked(true);
    m_satellites->setToolTip(QStringLiteral(
        "Amateur satellites above your horizon right now: sub-satellite "
        "point, name and elevation, refreshed every 10 s (orbital elements "
        "from CelesTrak, fetched once a day)."));
    m_satellites->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                    .arg(QString::fromLatin1(Style::kTextPrimary)));
    bar->addWidget(m_satellites);
    connect(m_satellites, &QCheckBox::toggled, this, [this](bool on) {
        m_flat->setShowSatellites(on);
        m_globe->setShowSatellites(on);
        if (on) { refreshSatellites(); }
    });

    auto* earthBtn = new QPushButton(QStringLiteral("Google Earth…"), this);
    m_earthBtn = earthBtn;
    earthBtn->setStyleSheet(Style::buttonBaseStyle());
    earthBtn->setToolTip(QStringLiteral(
        "Export what the filters currently show as a KML file and open "
        "it in Google Earth — pins with the OM's data, lines from home, "
        "one folder per band, time slider ready"));
    bar->addWidget(earthBtn);
    connect(earthBtn, &QPushButton::clicked,
            this, &QsoMapWindow::exportKml);

    // Zoom controls. The wheel already did this; nothing said so.
    auto* zoomOut = new QPushButton(QStringLiteral("−"), this);
    auto* zoomIn  = new QPushButton(QStringLiteral("+"), this);
    auto* fit     = new QPushButton(QStringLiteral("Fit"), this);
    zoomOut->setToolTip(QStringLiteral("Zoom out  (− key, or the wheel)"));
    zoomIn->setToolTip(QStringLiteral("Zoom in  (+ key, or the wheel)"));
    fit->setToolTip(QStringLiteral("Back to the whole view  (0 key, or "
                                   "double-click)"));
    for (QPushButton* b : {zoomOut, zoomIn, fit}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        b->setFocusPolicy(Qt::NoFocus);   // keep the +/- keys working
        bar->addWidget(b);
    }
    connect(zoomOut, &QPushButton::clicked,
            this, [this]() { zoomActiveView(1 / 1.25); });
    connect(zoomIn, &QPushButton::clicked,
            this, [this]() { zoomActiveView(1.25); });
    connect(fit, &QPushButton::clicked,
            this, [this]() { resetActiveView(); });

    // Was nur Kugel und flache Karte betrifft -- im 3D-Bild treten diese
    // Schalter zurueck und geben dem Bild ihre Zeile (apply3dRoom).
    m_flatOnlyControls << m_onlySelected << m_paths << m_grid << m_imagery
                       << m_satellites << zoomOut << zoomIn << fit;

    m_viewBtn = new QPushButton(QStringLiteral("Flat map"), this);
    m_viewBtn->setStyleSheet(Style::buttonBaseStyle());
    m_viewBtn->setToolTip(QStringLiteral(
        "Switch between the globe and the whole world at once"));
    bar->addWidget(m_viewBtn);

    // DX-Radar: Peilung und Entfernung von zu Hause, Norden oben.
    m_radarBtn = new QPushButton(QStringLiteral("Radar"), this);
    m_radarBtn->setCheckable(true);
    m_radarBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
    m_radarBtn->setToolTip(QStringLiteral(
        "DX radar: the same contacts by bearing and distance from home — "
        "north up, rings at 500 … 20 000 km"));
    bar->addWidget(m_radarBtn);

    // Azimutalkarte: die ganze Welt um das eigene QTH, mittabstandstreu.
    // Wie das Radar ein Haken, der dorthin zurueckfuehrt, wo man herkam.
    m_azimutBtn = new QPushButton(QStringLiteral("Azimuthal"), this);
    m_azimutBtn->setCheckable(true);
    m_azimutBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
    m_azimutBtn->setToolTip(QStringLiteral(
        "Azimuthal map around your QTH: every straight line from the centre "
        "is a rotor heading, every ring a distance — each contact as a ray "
        "in its band colour, the rotor live, a looked-up station with short "
        "and long path"));
    bar->addWidget(m_azimutBtn);

    // 3D: Googles fotorealistische Kacheln, wenn der Bediener einen
    // eigenen Schluessel hat. Ohne Schluessel oeffnet der Knopf die
    // Einrichtung; ohne WebEngine im Bau ist er aus und sagt warum.
    m_3dBtn = new QPushButton(QStringLiteral("3D"), this);
    m_3dBtn->setObjectName(QStringLiteral("qsoMap3dButton"));
    m_3dBtn->setCheckable(true);
    m_3dBtn->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
    bar->addWidget(m_3dBtn);
    // toggled, nicht clicked (Live-Befund 2026-11-10): die Bedienhilfe
    // (AXPress, VoiceOver, Automatisierung) drueckt einen ankreuzbaren
    // Knopf ueber toggle(), und das sendet KEIN clicked -- der Knopf tat
    // dann sichtbar nichts. Eigene Umstellungen laufen ueber
    // update3dButton() hinter einem QSignalBlocker und kommen hier nicht an.
    connect(m_3dBtn, &QPushButton::toggled, this, [this](bool checked) {
        const bool built = Station3dSurface::isAvailable();
        const bool key   = Map3dAccess::isSet();
        qCInfo(lcQsoMap3d).noquote()
            << "3D-Knopf:" << (checked ? "an" : "aus")
            << "| Zugang" << Map3dAccess::sourceName(Map3dAccess::chosenSource())
            << (key ? "da" : "fehlt")
            << "| WebEngine" << (built ? "da" : "fehlt")
            << "|" << (m_embedded ? "Logbuch-Spalte" : "Kartenfenster");
        if (!built) {
            // Ein abgeschalteter Knopf, der nichts sagt, ist genau das
            // „tut nichts". Also an bleiben und sagen, warum nicht.
            update3dButton();
            m_info->setText(QStringLiteral(
                "<b>3D</b> — this build of Longpath has no Qt WebEngine, so "
                "Google's 3D view is not available here. "
                "\"View in Google Earth\" shows the station in 3D in your browser."));
            m_info->setVisible(true);
            if (m_lastFlight.valid) { m_googleEarthBtn->setVisible(true); }
            return;
        }
        if (!key) {
            update3dButton();
            Google3dSetupDialog::showFor(this);
            return;
        }
        m_3dWanted = checked;
        AppSettings::instance().setValue(QStringLiteral("QsoMap3dFlight"), checked);
        if (!m_lastFlight.valid) { return; }
        if (checked) {
            m_3dRefusedKey.clear();   // ausdruecklich gewollt: noch einmal fragen
            // Ausdruecklich gewollt heisst auch: aus der Azimutalkarte
            // heraus, in der flyToStation() sonst stehen bliebe.
            if (use3d()) {
                fly3d();
            } else {
                flyToStation(m_lastFlight.call, m_lastFlight.lat, m_lastFlight.lon,
                             m_lastFlight.caption);
            }
        } else if (m_stack->currentIndex() == kView3d) {
            flyFlat(m_lastFlight.call, m_lastFlight.lat, m_lastFlight.lon);
            m_googleEarthBtn->setVisible(true);
        }
    });
    connect(&Map3dAccess::notifier(), &Map3dAccess::changed, this, [this]() {
        m_3dRefusedKey.clear();
        update3dButton();
    });

    // Nur eingebettet sichtbar: das grosse Fenster holen.
    m_popOutBtn = new QPushButton(QStringLiteral("\u2197"), this);
    m_popOutBtn->setStyleSheet(Style::buttonBaseStyle());
    m_popOutBtn->setToolTip(QStringLiteral("Open the map in its own window"));
    m_popOutBtn->setVisible(false);
    bar->addWidget(m_popOutBtn);
    connect(m_popOutBtn, &QPushButton::clicked, this, &QsoMapWindow::popOutRequested);

    col->addLayout(bar);

    // Second row: one pill per band and mode the log actually has,
    // filled in by rebuildFilterPills() once entries arrive.
    //
    // Umbrechend wie die Zeile darueber (2026-09-26): als starre Zeile
    // verlangte sie mit 12 Baendern und 8 Betriebsarten 1105 px -- im
    // Logbuch eingebettet blieben der Tabelle bei 1600 px Fensterbreite
    // 187 px.
    m_pillRow = new FlowLayout(nullptr, 4, 4);
    col->addLayout(m_pillRow);

    m_globe = new GlobeWidget(this);
    // No beam spread here: the flanking arcs answer a question about
    // aiming right now, and drawing them for every logged contact would
    // treble the clutter for nothing.
    m_globe->setBeamSpread(0.0);
    m_flat = new FlatMapWidget(this);

    m_radar = new DxRadarWidget(this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_globe);
    m_stack->addWidget(m_flat);
    m_stack->addWidget(m_radar);   // Index 2
    m_azimut = new AzimutalKarteWidget(this);
    m_stack->addWidget(m_azimut);  // Index 3 (kViewAzimuthal)
    // Index 4 (kView3d): Platz fuer die 3D-Flaeche, die erst beim ersten
    // Flug entsteht (ensure3d).
    m_3dHost = new QWidget(this);
    {
        auto* l = new QVBoxLayout(m_3dHost);
        l->setContentsMargins(0, 0, 0, 0);
    }
    m_stack->addWidget(m_3dHost);
    Q_ASSERT(m_stack->indexOf(m_azimut) == kViewAzimuthal);
    Q_ASSERT(m_stack->indexOf(m_3dHost) == kView3d);

    // Das kleine Rotor-Radar links der Karte. Quadratisch, so hoch wie
    // die Flaeche (resizeEvent); die Gradzahlen stehen in der Karteikarte.
    m_rotorRadar = new DxRadarWidget(this);
    // Der Stand des Rotors immer sichtbar: Grad unten links, und
    // "not connected", solange keine Ablesung kommt.
    m_rotorRadar->setRotorReadout(true);
    m_rotorRadar->setRotorCompact(true);
    m_rotorRadar->setRotorStatusShown(true);
    m_rotorRadar->setVisible(false);
    // Ein Viertel der Breite, hoechstens 300 -- und keine Mindestbreite,
    // damit das Logbuch weiter auf die halbe Bildschirmbreite schrumpft
    // (tst_logbook_toolbar_wraps).
    m_rotorRadar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_rotorRadar->setMinimumWidth(0);
    m_rotorRadar->setMaximumWidth(300);
    auto* mapRow = new QHBoxLayout;
    mapRow->setContentsMargins(0, 0, 0, 0);
    mapRow->setSpacing(8);
    mapRow->addWidget(m_rotorRadar, 1);
    mapRow->addWidget(m_stack, 3);
    col->addLayout(mapRow, 1);

    m_summary = new QLabel(QString{}, this);
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(m_summary);

    // The answer to a click: a station's data, or a grid square's
    // contents. A label rather than a popup — a popup over a map hides
    // the very thing that was clicked.
    m_info = new QLabel(QString{}, this);
    m_info->setWordWrap(true);
    m_info->setTextFormat(Qt::RichText);
    m_info->setVisible(false);
    m_info->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; background: %2; "
        "border: 1px solid %3; border-radius: 6px; padding: 6px; }")
        .arg(QString::fromLatin1(Style::kTextPrimary),
             QString::fromLatin1(Style::kInsetBg),
             QString::fromLatin1(Style::kBorderSubtle)));
    connect(m_info, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link == QLatin1String("longpath:google3d")) { Google3dSetupDialog::showFor(this); }
    });

    // Der Rueckfall fuer den Blick aufs Dach: dieselbe Station in Google
    // Earth im Browser -- ohne Schluessel, ohne WebEngine, auf jedem
    // System. Nur sichtbar, wenn der Flug flach war.
    m_googleEarthBtn = new QPushButton(QStringLiteral("View in Google Earth \u2197"), this);
    m_googleEarthBtn->setObjectName(QStringLiteral("qsoMapGoogleEarthStation"));
    m_googleEarthBtn->setStyleSheet(Style::buttonBaseStyle());
    m_googleEarthBtn->setToolTip(QStringLiteral(
        "Open this station in Google Earth in your browser — 3D houses and "
        "trees wherever Google has them, no key needed"));
    m_googleEarthBtn->setVisible(false);
    connect(m_googleEarthBtn, &QPushButton::clicked, this, &QsoMapWindow::openGoogleEarth);
    {
        auto* infoRow = new QHBoxLayout;
        infoRow->setContentsMargins(0, 0, 0, 0);
        infoRow->setSpacing(8);
        infoRow->addWidget(m_info, 1);
        infoRow->addWidget(m_googleEarthBtn, 0, Qt::AlignTop);
        col->addLayout(infoRow);
    }
    update3dButton();

    m_tiles = new GibsTileLayer(this);
    m_flat->setImagery(m_tiles);
    m_flat->setShowImagery(m_imagery->isChecked());
    connect(m_imagery, &QCheckBox::toggled, this, [this](bool on) {
        m_tiles->setNetworkEnabled(on);
        m_flat->setShowImagery(on);
    });

    connect(m_grid, &QCheckBox::toggled, this, [this](bool on) {
        m_flat->setShowGrid(on);
        // Das Mosaik auch auf der Azimutalkarte (2026-11-11).
        m_azimut->setShowMosaik(on);
        // The overlay lives on the flat map; looking at the globe with
        // Grid ticked would show nothing and look broken.
        if (on && m_stack->currentIndex() == 0) { m_viewBtn->click(); }
    });
    connect(m_flat, &FlatMapWidget::pointClicked, this,
            [this](const QString& label, double, double) {
        showStationInfo(label);
    });
    connect(m_flat, &FlatMapWidget::gridClicked,
            this, &QsoMapWindow::showGridInfo);

    // ── Locator-Jagd (2026-11-11) ───────────────────────────────────
    connect(m_flat, &FlatMapWidget::livePunktGeklickt,
            this, [this](const QString& call) { rufeLive(call); });
    connect(m_azimut, &AzimutalKarteWidget::livePunktGeklickt,
            this, [this](const QString& call) { rufeLive(call); });
    connect(m_mosaikBand, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        AppSettings::instance().setValue(QStringLiteral("QsoMapMosaicBand"),
                                         m_mosaikBand->currentData().toString());
        refreshMosaik();
    });
    connect(m_nurFt8, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::instance().setValue(QStringLiteral("QsoMapMosaicFt8Only"),
                                         on ? QStringLiteral("True")
                                            : QStringLiteral("False"));
        m_mosaik.baue(m_hatMosaikLog ? m_mosaikLog : m_all, on);
        refreshMosaik();
        refreshLive();
    });
    connect(m_liveMinuten, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int min) {
        m_live.setFensterSekunden(min * 60);
        AppSettings::instance().setValue(QStringLiteral("QsoMapFt8LiveSeconds"),
                                         m_live.fensterSekunden());
        refreshLive();
    });
    connect(&Ft8KartenBruecke::instanz(), &Ft8KartenBruecke::gehoert,
            this, &QsoMapWindow::nimmFt8);
    // Altern lassen: wer still ist, faellt nach dem Fenster von der
    // Karte, auch wenn keine neuen Funde mehr kommen (Band gewechselt,
    // FT8 aus).
    m_liveTakt = new QTimer(this);
    m_liveTakt->setInterval(5000);
    connect(m_liveTakt, &QTimer::timeout, this, [this]() {
        if (m_live.anzahl() > 0) { refreshLive(); }
    });
    m_liveTakt->start();

    connect(m_viewBtn, &QPushButton::clicked, this, [this]() {
        // Aus Radar oder Azimutalkarte heraus fuehrt der Knopf zur jeweils
        // anderen Kartenansicht; deren Haken geht dabei aus.
        // 3D zaehlt hier nicht als Overlay: dort steht auf dem Knopf
        // „Globe", und dorthin fuehrt er.
        const int cur = isOverlayView(m_stack->currentIndex()) ? m_viewBeforeRadar
                                                               : m_stack->currentIndex();
        const bool toFlat = cur == kViewGlobe;
        for (QPushButton* b : {m_radarBtn, m_azimutBtn}) {
            if (b && b->isChecked()) {
                QSignalBlocker block(b);
                b->setChecked(false);
            }
        }
        m_stack->setCurrentIndex(toFlat ? kViewFlat : kViewGlobe);
        m_viewBtn->setText(toFlat ? QStringLiteral("Globe")
                                  : QStringLiteral("Flat map"));
        rememberView();
    });
    // Radar und Azimutalkarte schliessen einander aus; beide merken sich
    // Kugel oder flach als den Weg zurueck.
    const auto overlayToggled = [this](int index, QPushButton* other, bool on) {
        if (on) {
            if (other && other->isChecked()) {
                QSignalBlocker block(other);
                other->setChecked(false);
            }
            // Kugel, flach -- oder 3D: wer aus dem 3D-Bild ins Radar
            // schaut, kommt dorthin zurueck (so im 3D-Zweig).
            if (!isOverlayView(m_stack->currentIndex())) {
                m_viewBeforeRadar = m_stack->currentIndex();
            }
            m_stack->setCurrentIndex(index);
        } else if (m_stack->currentIndex() == index) {
            m_stack->setCurrentIndex(m_viewBeforeRadar);
        }
        rememberView();
    };
    connect(m_radarBtn, &QPushButton::toggled, this, [this, overlayToggled](bool on) {
        overlayToggled(kViewRadar, m_azimutBtn, on);
    });
    connect(m_azimutBtn, &QPushButton::toggled, this, [this, overlayToggled](bool on) {
        overlayToggled(kViewAzimuthal, m_radarBtn, on);
    });
    connect(m_azimut, &AzimutalKarteWidget::pointClicked, this,
            [this](const QString& label, double, double) {
        showStationInfo(label);
    });
    connect(m_stack, &QStackedWidget::currentChanged,
            this, [this](int) {
        updateRotorRadarVisibility();
        apply3dRoom();
    });
    connect(m_radar, &DxRadarWidget::pointClicked, this,
            [this](const QString& label, double, double) {
        showStationInfo(label);
    });

    // ── Durchzoomen zwischen den beiden Ansichten (2026-08-19) ───────
    //
    // Auf Ansage des Betreibers („die Weltkugel wuerde ich mit Zoom
    // machen, wie bei Google Earth", danach: Weg B). Der Uebergang
    // passiert genau dort, wo eine Ansicht an ihr Ende kommt:
    //
    //   * Kugel am oberen Anschlag, weiter hinein  -> flache Karte,
    //     zentriert auf denselben Ort
    //   * flache Karte bei 1x, weiter heraus       -> Kugel
    //
    // Kein neuer Schalter: der Knopf bleibt, was er war. Wer nichts von
    // Ansichten wissen will, dreht einfach weiter und bekommt jedes Mal
    // die, die mehr zeigen kann.
    connect(m_globe, &GlobeWidget::zoomedInPastCeiling, this,
            [this](double lat, double lon) {
        m_stack->setCurrentIndex(1);
        m_viewBtn->setText(QStringLiteral("Globe"));
        // Erst der Zoom, dann das Zentrieren: centreOn rechnet mit dem
        // AKTUELLEN Zoom, und andersherum saesse der Ort daneben.
        m_flat->resetView();
        m_flat->zoomBy(3.0);
        m_flat->centreOn(lat, lon);
    });

    connect(m_flat, &FlatMapWidget::zoomedOutPastFloor, this, [this]() {
        m_stack->setCurrentIndex(0);
        m_viewBtn->setText(QStringLiteral("Flat map"));
        m_globe->resetView();
    });
    connect(m_paths, &QCheckBox::toggled, this, [this](bool on) {
        m_globe->setShowPointPaths(on);
        m_flat->setShowPaths(on);
    });
    connect(m_from, &QDateEdit::dateChanged, this, &QsoMapWindow::rebuild);
    connect(m_to,   &QDateEdit::dateChanged, this, &QsoMapWindow::rebuild);
    connect(m_onlySelected, &QCheckBox::toggled, this, [this](bool on) {
        // A live date field that changes nothing on screen is worse
        // than a disabled one.
        for (QWidget* w : m_rangeControls) { w->setEnabled(!on); }
        rebuild();
    });

    restoreView();
}

// ── Die gewaehlte Ansicht (2026-11-10) ──────────────────────────────

void QsoMapWindow::rememberView() const
{
    QString name = QStringLiteral("globe");
    switch (m_stack->currentIndex()) {
    case kViewFlat:      name = QStringLiteral("flat"); break;
    case kViewRadar:     name = QStringLiteral("radar"); break;
    case kViewAzimuthal: name = QStringLiteral("azimuthal"); break;
    default: break;
    }
    AppSettings::instance().setValue(QStringLiteral("QsoMapView"), name);
}

void QsoMapWindow::restoreView()
{
    const QString name = AppSettings::instance()
                             .value(QStringLiteral("QsoMapView"), QString{})
                             .toString();
    if (name == QLatin1String("flat")) {
        m_stack->setCurrentIndex(kViewFlat);
        m_viewBtn->setText(QStringLiteral("Globe"));
    } else if (name == QLatin1String("radar")) {
        m_radarBtn->setChecked(true);
    } else if (name == QLatin1String("azimuthal")) {
        m_azimutBtn->setChecked(true);
    }
}

void QsoMapWindow::setSelection(const QVector<LogEntry>& selected)
{
    m_selected = selected;
    const bool any = !m_selected.isEmpty();

    QSignalBlocker block(m_onlySelected);
    m_onlySelected->setEnabled(any);
    m_onlySelected->setText(any
        ? QStringLiteral("Only marked (%1)").arg(m_selected.size())
        : QStringLiteral("Only marked"));
    // Opened from a selection, start by showing just that — it is what
    // the operator asked for. The tick is there to widen it back out.
    // Eingebettet nicht: dort ist die aktuelle Zeile immer „markiert",
    // und die Karte soll weiter alles zeigen, was die Tabelle zeigt —
    // die Markierung hebt hervor, sie filtert nicht (ausser der Haken
    // wird von Hand gesetzt).
    if (!m_embedded) { m_onlySelected->setChecked(any); }
    for (QWidget* w : m_rangeControls) { w->setEnabled(!any); }

    rebuild();
}

void QsoMapWindow::setQuickRange(int days)
{
    QSignalBlocker b1(m_from);
    QSignalBlocker b2(m_to);
    m_to->setDate(QDate::currentDate());
    if (days <= 0) {
        // "All" means all, including a log imported from thirty years
        // ago — so the earliest contact, not an arbitrary floor.
        QDate earliest = QDate::currentDate();
        for (const LogEntry& e : m_all) {
            const QDate d = e.timeOn.toUTC().date();
            if (d.isValid() && d < earliest) { earliest = d; }
        }
        m_from->setDate(earliest);
    } else {
        m_from->setDate(QDate::currentDate().addDays(-days + 1));
    }
    rebuild();
}

void QsoMapWindow::setPositionFallback(PositionFallback fn)
{
    m_fallback = std::move(fn);
    rebuild();
}

void QsoMapWindow::setHomeGrid(const QString& grid)
{
    m_homeGrid = grid.trimmed().toUpper();

    // Fall back to the operator's own locator. A log written before
    // MY_GRIDSQUARE was recorded, or imported from a logger that omits
    // it, has no home in it at all — and with no home there is nothing
    // to draw a line from, so the map came up with dots and no paths
    // and no obvious reason why.
    if (!isValidGridSquare(m_homeGrid)) {
        m_homeGrid = AppSettings::instance()
                         .value(QStringLiteral("StationGridSquare"), QString{})
                         .toString().trimmed().toUpper();
    }

    if (isValidGridSquare(m_homeGrid)) {
        double lat = 0, lon = 0;
        calculateLatLonFromGridSquare(m_homeGrid, lat, lon);
        m_globe->setHome(lat, lon);
        m_globe->resetView();
        m_flat->setHome(lat, lon);
        m_radar->setHome(lat, lon);
        m_azimut->setHome(lat, lon, m_homeGrid.left(6));
        m_rotorRadar->setHome(lat, lon);
        applyStationMarker();
    } else {
        m_flat->clearHome();
        m_radar->clearHome();
        m_azimut->clearHome();
        m_rotorRadar->clearHome();
    }
    // Entfernung und Richtung der gehoerten Stationen haengen daran.
    if (m_live.anzahl() > 0) { refreshLive(); }
}

// ── Der eigene Standort als Bild ─────────────────────────────────────
//
// Rufzeichen aus den Einstellungen, Portraitadresse aus dem
// Rufzeichen-Cache, Bilddatei aus dem Portrait-Cache.
//
// Bewusst OHNE Netzwerk: ein Kartenfenster, das beim Öffnen eine
// Anfrage stellt, ist ein Kartenfenster, das ohne Internet später
// aufgeht. Was schon einmal geholt wurde, liegt auf der Platte; was
// nicht, bleibt ein Punkt mit Rufzeichen daneben. Beides ist brauchbar,
// nur eines ist hübscher.
//
// Jede Stufe darf fehlschlagen, ohne dass etwas kaputtgeht — kein
// Rufzeichen eingetragen, nie bei QRZ nachgeschlagen, kein Portrait
// hinterlegt: alle drei enden beim Punkt.
void QsoMapWindow::applyStationMarker()
{
    const QString call = AppSettings::stationCallsign().toUpper();
    if (call.isEmpty()) {
        m_flat->setStationMarker(QString{}, QImage{});
        return;
    }

    QImage photo;
    const CallsignInfo info = CallsignCache().get(call);
    if (!info.imageUrl.isEmpty()) {
        const QString file = StationPhoto::cachePath(info.imageUrl);
        if (QFileInfo::exists(file)) {
            photo.load(file);   // schlägt still fehl -> leeres Bild
        }
    }
    m_flat->setStationMarker(call, photo);
}

void QsoMapWindow::setEntries(const QVector<LogEntry>& entries)
{
    m_all = entries;
    rebuildFilterPills();
    rebuild();
    if (!m_hatMosaikLog) {
        m_mosaik.baue(m_all, m_nurFt8 && m_nurFt8->isChecked());
        refreshMosaik();
        refreshLive();
    }
}

// ── Filters ─────────────────────────────────────────────────────────

QString QsoMapWindow::bandKey(const LogEntry& e)
{
    const QString b = e.band.trimmed().toLower();
    return b.isEmpty() ? QStringLiteral("?") : b;
}

QString QsoMapWindow::modeKey(const LogEntry& e)
{
    const QString m = e.mode.trimmed().toUpper();
    return m.isEmpty() ? QStringLiteral("?") : m;
}

void QsoMapWindow::rebuildFilterPills()
{
    while (QLayoutItem* it = m_pillRow->takeAt(0)) {
        delete it->widget();
        delete it;
    }

    QSet<QString> bands, modes;
    for (const LogEntry& e : m_all) {
        bands.insert(bandKey(e));
        modes.insert(modeKey(e));
    }
    // A pill switched off for a band that later vanishes from the log
    // must not go on silently filtering from beyond the grave.
    m_offBands.intersect(bands);
    m_offModes.intersect(modes);

    QStringList bandList = bands.values();
    std::sort(bandList.begin(), bandList.end(),
              [](const QString& a, const QString& b) {
        return AdifLog::bandSortKeyMHz(a) < AdifLog::bandSortKeyMHz(b);
    });
    QStringList modeList = modes.values();
    std::sort(modeList.begin(), modeList.end());

    auto addCaption = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextScale)));
        m_pillRow->addWidget(l);
    };
    auto addPill = [this](const QString& key, const QString& label,
                          bool isBand) {
        auto* b = new QPushButton(label, this);
        b->setCheckable(true);
        b->setChecked(isBand ? !m_offBands.contains(key)
                             : !m_offModes.contains(key));
        b->setStyleSheet(Style::buttonBaseStyle()
                         + Style::greenCheckedStyle());
        b->setFocusPolicy(Qt::NoFocus);   // keep the +/- keys working
        connect(b, &QPushButton::toggled, this,
                [this, key, isBand](bool on) {
            QSet<QString>& off = isBand ? m_offBands : m_offModes;
            if (on) { off.remove(key); } else { off.insert(key); }
            rebuild();
        });
        m_pillRow->addWidget(b);
    };

    // Pills only earn their row when there is something to choose
    // between; a log entirely on one band in one mode gets no row.
    if (bandList.size() > 1) {
        addCaption(QStringLiteral("BAND"));
        for (const QString& b : bandList) { addPill(b, b, true); }
    }
    if (modeList.size() > 1) {
        addCaption(QStringLiteral("MODE"));
        for (const QString& m : modeList) { addPill(m, m, false); }
    }
    m_pillRow->addStretch();
    apply3dRoom();   // frisch gebaute Pillen folgen der Ansicht
}

// ── Platz fuer die 3D-Ansicht (2026-11-10) ───────────────────────────
//
// Betreiber nach dem ersten echten Flug: „die groesse der ansicht von 3d
// muss groesser sein". In der Logbuch-Spalte blieben dem Bild gut 160 von
// 360 Punkten Hoehe: darueber die Band- und Betriebsarten-Pillen (sie
// filtern Kontakte auf Kugel und flacher Karte, im 3D-Flug tun sie
// nichts), darunter die Zaehlung, daneben die kleine Rotor-Grafik. Im
// 3D-Bild treten alle drei zurueck; die Leiste mit „3D"/„Globe" bleibt,
// damit man wieder hinauskommt.
void QsoMapWindow::apply3dRoom()
{
    const bool in3d = m_stack && m_stack->currentIndex() == kView3d;
    if (m_pillRow) {
        for (int i = 0; i < m_pillRow->count(); ++i) {
            if (QWidget* w = m_pillRow->itemAt(i)->widget()) { w->setVisible(!in3d); }
        }
        m_pillRow->invalidate();
    }
    if (m_summary) { m_summary->setVisible(!in3d); }
    // Live 2026-11-10: in der Logbuch-Spalte war das 3D-Bild ein flacher
    // Streifen. Die Schalter, die im 3D-Bild nichts tun, geben ihre Zeile
    // ab; „Fly to call", Globe, Radar, 3D und ↗ bleiben.
    for (QWidget* w : std::as_const(m_flatOnlyControls)) { w->setVisible(!in3d); }
}

namespace {

// Identity of one logged contact, for telling a marked row from an
// identical-looking neighbour. Callsign plus timestamp: the same
// station worked twice is two rows, and marking one must not light the
// other.
QString entryKey(const LogEntry& e)
{
    return e.call.trimmed().toUpper() + QLatin1Char('|')
           + e.timeOn.toUTC().toString(Qt::ISODate);
}

} // namespace

void QsoMapWindow::rebuild()
{
    const bool onlyMarked = m_onlySelected->isChecked()
                            && !m_selected.isEmpty();

    // Marked rows, identified per contact rather than per grid square.
    // Keying on the square meant marking one QSO lit every other QSO
    // from the same square — which for a station worked forty times is
    // forty lines the operator did not ask for.
    QSet<QString> markedRows;
    for (const LogEntry& e : m_selected) { markedRows.insert(entryKey(e)); }

    const QVector<LogEntry>& source = onlyMarked ? m_selected : m_all;
    const QDate from = m_from->date();
    const QDate to   = m_to->date();

    QVector<MapPoint> points;
    // Die Azimutalkarte bekommt JEDEN gezeigten Kontakt, nicht einen je
    // Ort: dort ist die Dichte die Aussage -- ein Strahl wird hell, wo
    // viele zusammenlaufen. Ihre Strahlen liegen als fertige Ebene vor,
    // die Obergrenze fuer markierte Linien (Kugel) gilt hier nicht.
    QVector<AzimutalKarteWidget::Punkt> azPunkte;
    QSet<QString> seenPlaces;
    int considered   = 0;
    int noGrid       = 0;
    int markedShown  = 0;
    int markedCut    = 0;
    int markedNoGrid = 0;
    int approxCount  = 0;

    m_lastShown.clear();

    for (const LogEntry& e : source) {
        const bool marked = markedRows.contains(entryKey(e));

        // Eingebettet filtert die Logtabelle; hier kommt an, was sie zeigt.
        if (!onlyMarked && !m_embedded) {
            // A contact with no timestamp is kept rather than dropped:
            // an imported log full of dateless records would otherwise
            // produce an empty map for every range the operator tries.
            const QDate d = e.timeOn.toUTC().date();
            if (d.isValid() && (d < from || d > to)) { continue; }
        }

        // Band and mode pills. Applied to the marked view too — a pill
        // switched off means "I do not want to see 40 m right now", and
        // that intent does not flip with the Only-marked tick.
        if (m_offBands.contains(bandKey(e))) { continue; }
        if (m_offModes.contains(modeKey(e))) { continue; }

        ++considered;
        // What the KML export sends: exactly what the filters let
        // through, placed or not — the exporter has its own fallback.
        m_lastShown.append(e);

        double lat = 0, lon = 0;
        QString place;
        bool approximate = false;

        if (isValidGridSquare(e.gridSquare)) {
            calculateLatLonFromGridSquare(e.gridSquare, lat, lon);
            place = e.gridSquare.left(6).toUpper();
        } else if (m_fallback && m_fallback(e.call, lat, lon)) {
            // The middle of the country, which is a guess — but a
            // contact drawn approximately in Japan tells the operator
            // more than a contact not drawn at all.
            approximate = true;
            ++approxCount;
            place = QStringLiteral("~") + e.call.trimmed().toUpper();
        } else {
            ++noGrid;
            if (marked) { ++markedNoGrid; }
            continue;
        }

        azPunkte.append(AzimutalKarteWidget::Punkt{lat, lon, e.band.trimmed(),
                                                   e.call.trimmed().toUpper(),
                                                   !approximate, marked});

        if (marked) {
            // Every marked contact gets its own line, not one per
            // place. Two marked QSOs in the same square draw the same
            // path twice, which is what "show them all" means.
            if (markedShown >= kMaxMarkedPaths) { ++markedCut; continue; }
            ++markedShown;
            points.append(MapPoint{lat, lon, e.call, true, approximate});
            continue;
        }

        // Unmarked contacts stay one dot per place. Working the same
        // station forty times should not stack forty arcs on one pixel
        // and make that direction look busier than it is.
        if (seenPlaces.contains(place)) { continue; }
        seenPlaces.insert(place);
        points.append(MapPoint{lat, lon, e.call, false, approximate});
    }

    m_globe->setPoints(points);
    m_flat->setPoints(points);
    m_radar->setPoints(points);
    m_azimut->setPunkte(azPunkte);
    // Das kleine Rotor-Radar neben der Karte nur mit dem, was markiert
    // ist -- der gewaehlten Station (Betreiber 2026-09-27, Blatt R3). Die
    // Karte daneben zeigt schon jeden Kontakt; zwischen 9271 Punkten
    // ging der Kegel des Rotors unter. Das grosse Radar zeigt weiter alle.
    QVector<MapPoint> marked;
    for (const MapPoint& p : points) {
        if (p.highlight) { marked.append(p); }
    }
    m_rotorRadar->setPoints(marked);

    // Say what could not be placed. A map quietly showing a third of the
    // log looks exactly like a map of the whole log, and the operator
    // has no way to tell the difference.
    QStringList bits;
    bits << (onlyMarked
        ? QStringLiteral("%1 marked contacts").arg(considered)
        : QStringLiteral("%1 contacts in range").arg(considered));

    if (markedShown > 0) {
        bits << QStringLiteral("%1 marked lines").arg(markedShown);
    }
    if (markedCut > 0) {
        bits << QStringLiteral("%1 more marked not drawn (limit %2)")
                    .arg(markedCut).arg(kMaxMarkedPaths);
    }
    if (markedNoGrid > 0) {
        bits << QStringLiteral("%1 marked without a locator")
                    .arg(markedNoGrid);
    }
    if (!onlyMarked) {
        bits << QStringLiteral("%1 other places").arg(seenPlaces.size());
        // A row marked outside the date range simply is not there. Say
        // so, or the operator counts the amber lines, gets fewer than
        // they marked, and concludes the marking did not work.
        const int outside = m_selected.size() - markedShown - markedCut
                            - markedNoGrid;
        if (outside > 0) {
            bits << QStringLiteral("%1 marked outside the dates")
                        .arg(outside);
        }
    }
    if (approxCount > 0) {
        bits << QStringLiteral("%1 placed from the country only (rings)")
                    .arg(approxCount);
    }
    if (noGrid - markedNoGrid > 0) {
        bits << QStringLiteral("%1 could not be placed at all")
                    .arg(noGrid - markedNoGrid);
    }
    m_summary->setText(bits.join(QStringLiteral("  ·  ")));

    // The one condition under which nothing can be drawn, said on its
    // own line and in amber rather than buried at the end of a summary.
    // Without a home there is no line to draw, and the map otherwise
    // shows dots with no explanation of the missing paths.
    if (!isValidGridSquare(m_homeGrid)) {
        m_summary->setText(
            QStringLiteral("No home locator, so no paths can be drawn. "
                           "Set MY in the Rotor / Log panel.  ·  ")
            + m_summary->text());
        m_summary->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kAmberText)));
    } else {
        m_summary->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }")
            .arg(QString::fromLatin1(Style::kTextSecondary)));
    }
}

// ── Locator-Jagd mit FT8 (2026-11-11) ───────────────────────────────

QDateTime QsoMapWindow::jetzt() const
{
    return m_uhr ? m_uhr() : QDateTime::currentDateTimeUtc();
}

void QsoMapWindow::setMosaikLog(const QVector<LogEntry>& ganzesLog)
{
    // Das Logbuchfenster ruft das bei jedem Filterwechsel. Dasselbe
    // Logbuch (geteilte Daten, Qt-Kopie beim Schreiben) muss nicht neu
    // gezaehlt werden.
    if (m_hatMosaikLog && ganzesLog.constData() == m_mosaikLog.constData()
        && ganzesLog.size() == m_mosaikLog.size()) {
        return;
    }
    m_mosaikLog = ganzesLog;
    m_hatMosaikLog = true;
    m_mosaik.baue(m_mosaikLog, m_nurFt8 && m_nurFt8->isChecked());
    refreshMosaik();
    refreshLive();
}

QString QsoMapWindow::mosaikBand() const
{
    if (!m_mosaikBand || m_mosaikBand->currentIndex() < 0) { return m_liveBand; }
    const QString d = m_mosaikBand->currentData().toString();
    // „FT8 band": das Band, auf dem zuletzt gehoert wurde -- solange
    // nichts gehoert wurde, alle Baender.
    return d == QStringLiteral("*") ? m_liveBand : d;
}

void QsoMapWindow::refreshMosaik()
{
    // Die Auswahl neu fuellen: Baender kommen und gehen mit dem Logbuch.
    if (m_mosaikBand) {
        QString gewaehlt = m_mosaikBand->currentIndex() >= 0
            ? m_mosaikBand->currentData().toString()
            : AppSettings::instance().value(QStringLiteral("QsoMapMosaicBand"),
                                            QStringLiteral("*")).toString();
        QSignalBlocker block(m_mosaikBand);
        m_mosaikBand->clear();
        m_mosaikBand->addItem(m_liveBand.isEmpty()
                                  ? QStringLiteral("FT8 band")
                                  : QStringLiteral("FT8 band (%1)").arg(m_liveBand),
                              QStringLiteral("*"));
        m_mosaikBand->addItem(QStringLiteral("All bands"), QString());
        for (const QString& b : m_mosaik.baender()) {
            m_mosaikBand->addItem(b, b);
        }
        int i = m_mosaikBand->findData(gewaehlt);
        if (i < 0) { i = 0; }
        m_mosaikBand->setCurrentIndex(i);
    }
    const QHash<QString, FeldStand> felder = m_mosaik.felder(mosaikBand());
    m_flat->setMosaik(felder);
    m_azimut->setMosaik(felder);
}

void QsoMapWindow::refreshLive()
{
    const QDateTime t = jetzt();
    m_live.altere(t);
    const QVector<LivePunkt> punkte = m_live.punkte(t, m_mosaik, m_homeGrid);
    m_flat->setLivePunkte(punkte);
    m_azimut->setLivePunkte(punkte);

    if (!m_liveZaehler) { return; }
    if (m_liveBand.isEmpty() && m_live.anzahl() == 0) { return; }
    m_liveBox->setVisible(true);
    const int neu = LiveSchicht::neueFelder(punkte, m_liveBand);
    m_liveZaehler->setText(LiveSchicht::zaehlerText(neu, m_liveBand));
    m_liveZaehler->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(neu > 0 ? Style::kJagdNeu : Style::kTextSecondary)));
}

void QsoMapWindow::nimmFt8(const Ft8Gehoert& g)
{
    m_live.nimm(g);
    if (!g.band.isEmpty() && g.band != m_liveBand) {
        m_liveBand = g.band;
        // Das Mosaik folgt dem FT8-Band (wenn so gewaehlt), und die
        // Beschriftung „FT8 band (20m)" ebenso.
        refreshMosaik();
    }
    refreshLive();
}

QString QsoMapWindow::rufeLive(const QString& rufzeichen)
{
    const QString call = rufzeichen.trimmed().toUpper();
    const Ft8Gehoert* g = m_live.finde(call);
    const QString grund = g ? Ft8KartenBruecke::instanz().rufe(*g)
                            : QStringLiteral("%1 is no longer heard").arg(call);
    if (grund.isEmpty()) {
        m_info->setText(QStringLiteral(
            "<b>Calling %1</b> — answering “%2” in the FT8 window, exactly as "
            "a double-click there would. The QSO runs to 73 and is logged; "
            "Stop is in the FT8 window.")
            .arg(call.toHtmlEscaped(), g->nachricht.toHtmlEscaped()));
    } else {
        m_info->setText(QStringLiteral("<b>Cannot call %1</b> — %2")
            .arg(call.toHtmlEscaped(), grund.toHtmlEscaped()));
    }
    m_info->setVisible(true);
    return grund;
}

// ── Click answers ───────────────────────────────────────────────────

void QsoMapWindow::showStationInfo(const QString& label)
{
    // A '~' marks a country-guess position; the callsign follows it.
    const QString call = (label.startsWith(QLatin1Char('~'))
                              ? label.mid(1) : label).trimmed().toUpper();
    if (call.isEmpty()) { return; }

    QVector<const LogEntry*> mine;
    for (const LogEntry& e : m_all) {
        if (e.call.trimmed().toUpper() == call) { mine.append(&e); }
    }
    if (mine.isEmpty()) {
        m_info->setText(QStringLiteral("<b>%1</b> — not in the log")
                            .arg(call.toHtmlEscaped()));
        m_info->setVisible(true);
        return;
    }

    // Newest first; the newest record has the freshest name and QTH.
    std::sort(mine.begin(), mine.end(),
              [](const LogEntry* a, const LogEntry* b) {
        return a->timeOn > b->timeOn;
    });
    const LogEntry& last = *mine.first();

    QStringList who;
    if (!last.name.trimmed().isEmpty())    { who << last.name.toHtmlEscaped(); }
    if (!last.qth.trimmed().isEmpty())     { who << last.qth.toHtmlEscaped(); }
    if (!last.country.trimmed().isEmpty()) { who << last.country.toHtmlEscaped(); }

    QString text = QStringLiteral("<b>%1</b> — %2 QSO%3")
        .arg(call.toHtmlEscaped())
        .arg(mine.size())
        .arg(mine.size() == 1 ? QString{} : QStringLiteral("s"));
    if (!who.isEmpty()) {
        text += QStringLiteral(" · ") + who.join(QStringLiteral(" · "));
    }
    if (isValidGridSquare(last.gridSquare)) {
        text += QStringLiteral(" · %1").arg(last.gridSquare.toHtmlEscaped());
        if (!last.mySotaRef.trimmed().isEmpty() || !last.sotaRef.trimmed().isEmpty()) {
            // Vom Gipfel (2026-11-11): das Logbuch hat Entfernung und
            // Peilung dieses QSOs schon aus der Lage des Gipfels gerechnet
            // (SotaGipfelVerzeichnis::korrigiereStrecken) -- die Mitte
            // des Heimatlocators laege sieben Kilometer daneben.
            text += QStringLiteral(" · %1 km · %2°")
                .arg(last.distanceKm, 0, 'f', 0)
                .arg(last.bearingDeg, 0, 'f', 0);
        } else if (isValidGridSquare(m_homeGrid)) {
            text += QStringLiteral(" · %1 km · %2°")
                .arg(calculateDistanceKm(m_homeGrid, last.gridSquare),
                     0, 'f', 0)
                .arg(calculateBearingInDegrees(m_homeGrid, last.gridSquare),
                     0, 'f', 0);
        }
    }

    // The most recent few, so "when did I last work him" is answered
    // without opening anything else.
    text += QStringLiteral("<br>");
    QStringList recent;
    for (int i = 0; i < mine.size() && i < 5; ++i) {
        const LogEntry& e = *mine.at(i);
        QStringList bits;
        const QDateTime u = e.timeOn.toUTC();
        if (u.isValid()) {
            bits << u.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
        }
        if (!e.band.trimmed().isEmpty()) { bits << e.band.toHtmlEscaped(); }
        const QString m = e.submode.isEmpty() ? e.mode : e.submode;
        if (!m.trimmed().isEmpty()) { bits << m.toHtmlEscaped(); }
        recent << bits.join(QStringLiteral(" "));
    }
    text += recent.join(QStringLiteral(" &nbsp;·&nbsp; "));
    if (mine.size() > 5) {
        text += QStringLiteral(" &nbsp;·&nbsp; +%1 more").arg(mine.size() - 5);
    }

    m_info->setText(text);
    m_info->setVisible(true);
}

void QsoMapWindow::showGridInfo(const QString& locator)
{
    const QString loc = locator.trimmed().toUpper();
    if (loc.isEmpty()) { return; }

    QStringList calls;
    int count = 0;
    for (const LogEntry& e : m_all) {
        if (!e.gridSquare.trimmed().toUpper().startsWith(loc)) { continue; }
        ++count;
        const QString c = e.call.trimmed().toUpper();
        if (!calls.contains(c) && calls.size() < 8) { calls << c; }
    }

    if (count == 0) {
        m_info->setText(QStringLiteral(
            "<b>%1</b> — nothing in the log from this square")
            .arg(loc.toHtmlEscaped()));
    } else {
        QString text = QStringLiteral("<b>%1</b> — %2 QSO%3: %4")
            .arg(loc.toHtmlEscaped())
            .arg(count)
            .arg(count == 1 ? QString{} : QStringLiteral("s"))
            .arg(calls.join(QStringLiteral(", ")).toHtmlEscaped());
        if (count > calls.size()) {
            text += QStringLiteral(" …");
        }
        m_info->setText(text);
    }
    m_info->setVisible(true);
}

// ── Google Earth ────────────────────────────────────────────────────

void QsoMapWindow::exportKml()
{
    if (m_lastShown.isEmpty()) {
        m_info->setText(QStringLiteral(
            "Nothing to export — the current filters show no contacts."));
        m_info->setVisible(true);
        return;
    }

    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
        + QStringLiteral("/Longpath-log.kml");
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export for Google Earth"), suggested,
        QStringLiteral("Google Earth KML (*.kml)"));
    if (path.isEmpty()) { return; }

    KmlExport::Options opt;
    opt.myGrid = m_homeGrid;
    opt.fallback = m_fallback;

    QString err;
    KmlExport::Result res;
    if (!KmlExport::writeKml(path, m_lastShown, opt, &err, &res)) {
        m_info->setText(QStringLiteral("KML export failed: %1")
                            .arg(err.toHtmlEscaped()));
        m_info->setVisible(true);
        return;
    }

    QString text = QStringLiteral(
        "<b>%1</b> contacts exported to %2 — opening it now. Bands are "
        "folders in Google Earth's sidebar; the time slider filters by "
        "date.").arg(res.placed).arg(path.toHtmlEscaped());
    if (res.skipped > 0) {
        text += QStringLiteral(" %1 without a locator were left out.")
                    .arg(res.skipped);
    }
    m_info->setText(text);
    m_info->setVisible(true);

    // Whatever handles .kml — Google Earth when installed, otherwise
    // the OS says so, which is a better error than any dialog here.
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}


bool QsoMapWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Vor dem Aufklappen neu einlesen -- siehe die Notiz im Header.
    if (obj == m_background && event->type() == QEvent::MouseButtonPress) {
        reloadBackgroundList();
    }
    return QDialog::eventFilter(obj, event);
}

void QsoMapWindow::reloadBackgroundList()
{
    if (!m_background) { return; }
    const QString current = WorldTexture::currentPath();

    QSignalBlocker block(m_background);
    m_background->clear();
    m_background->addItem(tr("\u2014 no image \u2014"), QString{});

    const auto list = WorldMapCatalog::entries();
    for (const auto& e : list) {
        m_background->addItem(e.usable
                                  ? e.name
                                  : QStringLiteral("%1  \u2014  %2")
                                        .arg(e.name, e.reason),
                              e.path);
        const int row = m_background->count() - 1;
        if (!e.usable) {
            // Ausgegraut MIT Grund statt unsichtbar: ein Bild, das der
            // Betreiber hingelegt hat und das dann nirgends auftaucht,
            // liest sich als Fehler des Programms.
            auto* model = qobject_cast<QStandardItemModel*>(
                m_background->model());
            if (model) {
                if (QStandardItem* it = model->item(row)) {
                    it->setEnabled(false);
                }
            }
        }
        if (!current.isEmpty() && e.path == current) {
            m_background->setCurrentIndex(row);
        }
    }
    if (current.isEmpty()) { m_background->setCurrentIndex(0); }
}

void QsoMapWindow::applyBackgroundChoice(int index)
{
    if (!m_background) { return; }
    const QString path = m_background->itemData(index).toString();
    if (path.isEmpty()) {
        AppSettings::instance().setValue(WorldTexture::settingsKey(),
                                         QString{});
        WorldTexture::reload();   // loest den Geber aus
        return;
    }
    // setPath() prueft die Lesbarkeit und loest bei Erfolg den Geber
    // aus; schlaegt es fehl, bleibt das vorherige Bild stehen.
    WorldTexture::setPath(path);
}

} // namespace Longpath

namespace Longpath {

// ── Hinflug zu einer Station ────────────────────────────────────────

void QsoMapWindow::setSatellites(SatelliteService* svc)
{
    if (m_satService == svc) { return; }
    if (m_satService) { disconnect(m_satService, nullptr, this, nullptr); }
    m_satService = svc;
    if (!m_satTimer) {
        m_satTimer = new QTimer(this);
        m_satTimer->setInterval(10 * 1000);
        connect(m_satTimer, &QTimer::timeout, this, &QsoMapWindow::refreshSatellites);
    }
    if (!m_satService) {
        m_satTimer->stop();
        m_flat->setSatellites({});
        m_globe->setSatellites({});
        return;
    }
    connect(m_satService, &SatelliteService::catalogChanged,
            this, &QsoMapWindow::refreshSatellites);
    m_satTimer->start();
    refreshSatellites();
}

void QsoMapWindow::refreshSatellites()
{
    if (!m_satService || !m_flat) { return; }
    QVector<FlatMapWidget::SatelliteMarker> flat;
    QVector<GlobeWidget::SatelliteMarker>   globe;
    if (m_satellites && m_satellites->isChecked()) {
        const auto sights = m_satService->inView(QDateTime::currentDateTimeUtc(), m_homeGrid, 0.0);
        flat.reserve(sights.size());
        globe.reserve(sights.size());
        for (const SatelliteSight& s : sights) {
            flat.push_back({s.name, s.subLatDeg, s.subLonDeg, s.elevationDeg});
            globe.push_back({s.name, s.subLatDeg, s.subLonDeg, s.elevationDeg, s.altitudeKm});
        }
    }
    m_flat->setSatellites(flat);
    m_globe->setSatellites(globe);
}

void QsoMapWindow::setQrzClient(QrzClient* qrz)
{
    if (m_qrz == qrz) { return; }
    if (m_qrz) { disconnect(m_qrz, nullptr, this, nullptr); }
    m_qrz = qrz;
    if (!m_qrz) { return; }

    connect(m_qrz, &QrzClient::lookupSucceeded, this,
            [this](const QString& call, const CallsignInfo& info) {
        // Nur die Antwort auf DIESES Feld; das Logbuch fragt denselben
        // Client und meldet seine Treffer ueber flyToStation() selbst.
        if (m_pendingCall.isEmpty() || call != m_pendingCall) { return; }
        m_pendingCall.clear();
        double lat = 0.0, lon = 0.0;
        if (info.hasLatLon) {
            lat = info.latitude; lon = info.longitude;
        } else if (isValidGridSquare(info.grid)) {
            calculateLatLonFromGridSquare(info.grid, lat, lon);
        } else if (!(m_fallback && m_fallback(call, lat, lon))) {
            m_info->setText(QStringLiteral("<b>%1</b> — QRZ has no position for this call")
                                .arg(call.toHtmlEscaped()));
            m_info->setVisible(true);
            return;
        }
        QStringList bits;
        if (!info.displayName().isEmpty()) { bits << info.displayName().toHtmlEscaped(); }
        QStringList place;
        if (!info.city.isEmpty())    { place << info.city.toHtmlEscaped(); }
        if (!info.state.isEmpty())   { place << info.state.toHtmlEscaped(); }
        if (!info.country.isEmpty()) { place << info.country.toHtmlEscaped(); }
        if (!place.isEmpty()) { bits << place.join(QStringLiteral(", ")); }
        if (!info.grid.isEmpty()) { bits << info.grid.toHtmlEscaped(); }
        flyToStation(call, lat, lon, bits.join(QStringLiteral(" · ")));
    });
    connect(m_qrz, &QrzClient::lookupFailed, this,
            [this](const QString& call, QrzClient::Error, const QString& message) {
        if (m_pendingCall.isEmpty() || call != m_pendingCall) { return; }
        m_pendingCall.clear();
        double lat = 0.0, lon = 0.0;
        if (m_fallback && m_fallback(call, lat, lon)) {
            flyToStation(call, lat, lon, QStringLiteral("country centre — QRZ: %1")
                                             .arg(message.toHtmlEscaped()));
            return;
        }
        m_info->setText(QStringLiteral("<b>%1</b> — %2")
                            .arg(call.toHtmlEscaped(), message.toHtmlEscaped()));
        m_info->setVisible(true);
    });
}

void QsoMapWindow::lookupAndFly(const QString& text)
{
    const QString call = text.trimmed().toUpper();
    if (call.isEmpty()) { return; }
    if (m_callEdit && m_callEdit->text() != call) { m_callEdit->setText(call); }

    // Was das Log schon weiss, braucht kein Netz: der Locator der
    // letzten Verbindung mit dieser Station.
    for (const LogEntry& e : m_all) {
        if (e.call.trimmed().toUpper() == call && isValidGridSquare(e.gridSquare)) {
            double lat = 0.0, lon = 0.0;
            calculateLatLonFromGridSquare(e.gridSquare, lat, lon);
            flyToStation(call, lat, lon, QStringLiteral("from the log · %1")
                                             .arg(e.gridSquare.toHtmlEscaped()));
            // QRZ darf es genauer machen, wenn ein Konto da ist.
            if (m_qrz && m_qrz->hasCredentials()) {
                m_pendingCall = call;
                m_qrz->lookup(call);
            }
            return;
        }
    }

    if (m_qrz && m_qrz->hasCredentials()) {
        m_pendingCall = call;
        m_info->setText(QStringLiteral("Looking up <b>%1</b>…").arg(call.toHtmlEscaped()));
        m_info->setVisible(true);
        m_qrz->lookup(call);
        return;
    }

    double lat = 0.0, lon = 0.0;
    if (m_fallback && m_fallback(call, lat, lon)) {
        flyToStation(call, lat, lon, QStringLiteral("country centre — add a QRZ account in Tools for the exact position"));
        return;
    }
    m_info->setText(QStringLiteral("<b>%1</b> — not in the log, and no QRZ account to ask")
                        .arg(call.toHtmlEscaped()));
    m_info->setVisible(true);
}

void QsoMapWindow::flyToStation(const QString& call, double lat, double lon,
                                const QString& caption)
{
    m_lastFlight.valid   = true;
    m_lastFlight.call    = call.trimmed().toUpper();
    m_lastFlight.caption = caption;
    m_lastFlight.lat     = lat;
    m_lastFlight.lon     = lon;

    // Die Azimutalkarte bekommt die Station immer -- kurzer Weg hell,
    // langer gestrichelt. Ist sie die gezeigte Ansicht, bleibt sie es:
    // Peilung und beide Wege sind genau die Antwort, die man dort sucht.
    // Darum fliegt 3D nur, wenn NICHT die Azimutalkarte gezeigt wird
    // (Probe-Merge Azimutalkarte + 3D); der Knopf „3D" holt ausdruecklich
    // ins 3D-Bild.
    m_azimut->setZiel(m_lastFlight.call, lat, lon);
    if (m_stack->currentIndex() != kViewAzimuthal && use3d()) {
        fly3d();
        return;
    }
    flyFlat(m_lastFlight.call, lat, lon);
    showFlightCard(QString());
    // Kein 3D hier -- der Blick aufs Dach geht trotzdem, im Browser.
    m_googleEarthBtn->setVisible(true);
}

void QsoMapWindow::flyFlat(const QString& c, double lat, double lon)
{
    // Wer das 3D-Bild verlaesst, soll aus Radar/Azimutalkarte nicht
    // dorthin zurueckkehren (es kann abgelehnt oder abgewaehlt sein).
    if (m_viewBeforeRadar == kView3d) { m_viewBeforeRadar = kViewFlat; }
    const bool stayAzimuthal = m_stack->currentIndex() == kViewAzimuthal;

    // Sonst ist der Flug eine Sache der flachen Karte; die Kugel kennt
    // ihn nicht. Also erst umschalten, dann fliegen.
    if (!stayAzimuthal && m_stack->currentIndex() != kViewFlat) {
        if (m_radarBtn && m_radarBtn->isChecked()) {
            QSignalBlocker b(m_radarBtn);
            m_radarBtn->setChecked(false);
        }
        m_stack->setCurrentIndex(kViewFlat);
    }
    if (m_viewBtn && !stayAzimuthal) { m_viewBtn->setText(QStringLiteral("Globe")); }

    m_flat->setFocusStation(c, lat, lon);
    // ~55 m je Bildpunkt: bei 900 px Breite ein Ausschnitt von 50 km,
    // in dem man Stadt und Landschaft erkennt und Landsat noch scharf ist.
    m_flat->flyTo(lat, lon, m_flat->zoomForDegPerPixel(0.0005));
}

void QsoMapWindow::showFlightCard(const QString& extra)
{
    const LastFlight& f = m_lastFlight;
    QString text = QStringLiteral("<b>%1</b>").arg(f.call.toHtmlEscaped());
    if (!f.caption.isEmpty()) { text += QStringLiteral(" — ") + f.caption; }
    if (isValidGridSquare(m_homeGrid)) {
        double hlat = 0.0, hlon = 0.0;
        calculateLatLonFromGridSquare(m_homeGrid, hlat, hlon);
        // Entfernung und Peilung ueber den Grosskreis, wie die Karte sie
        // zeichnet -- dieselbe Rechnung wie die Karteikarte daneben.
        const BeamHeading::GreatCircle g = BeamHeading::greatCircle(hlat, hlon, f.lat, f.lon);
        text += QStringLiteral(" · %1 km · %2°")
                    .arg(g.km, 0, 'f', 0).arg(g.bearingDeg, 0, 'f', 0);
    }
    text += extra;
    m_info->setText(text);
    m_info->setVisible(true);
}

// ── 3D-Flug (2026-11-10) ─────────────────────────────────────────────

int QsoMapWindow::currentViewForTest() const { return m_stack->currentIndex(); }
QWidget* QsoMapWindow::summaryForTest() const { return m_summary; }
QString QsoMapWindow::infoTextForTest() const { return m_info->text(); }

bool QsoMapWindow::use3d() const
{
    if (!m_3dWanted || !Station3dSurface::isAvailable()) { return false; }
    const QString key = Map3dAccess::secret();
    if (key.isEmpty()) { return false; }
    // Einmal abgelehnt heisst: bis zu einem neuen Zugang (oder dem
    // ausdruecklichen Klick auf „3D") nicht bei jedem Rufzeichen wieder
    // die 3D-Seite aufziehen, nur um gleich zurueckzufallen. Quelle und
    // Zugang zusammen: derselbe Text als ion-Token und als Google-
    // Schluessel waeren zwei verschiedene Zugaenge.
    return accessIdentity() != m_3dRefusedKey;
}

void QsoMapWindow::ensure3d()
{
    if (m_3d) { return; }
    m_3d = Station3dSurface::create(m_3dHost);
    if (!m_3d) { return; }
    m_3dHost->layout()->addWidget(m_3d);
    connect(m_3d, &Station3dSurface::failed, this, &QsoMapWindow::on3dFailed);
}

void QsoMapWindow::fly3d()
{
    ensure3d();
    if (!m_3d) {
        flyFlat(m_lastFlight.call, m_lastFlight.lat, m_lastFlight.lon);
        showFlightCard(QString());
        m_googleEarthBtn->setVisible(true);
        return;
    }
    for (QPushButton* b : {m_radarBtn, m_azimutBtn}) {
        if (b && b->isChecked()) {
            QSignalBlocker block(b);
            b->setChecked(false);
        }
    }
    m_stack->setCurrentIndex(kView3d);
    if (m_viewBtn) { m_viewBtn->setText(QStringLiteral("Globe")); }
    // Die flache Karte kennt die Station trotzdem: wer zurueckschaltet,
    // sieht sie dort, wo der Flug geendet hat.
    m_flat->setFocusStation(m_lastFlight.call, m_lastFlight.lat, m_lastFlight.lon);
    m_googleEarthBtn->setVisible(false);
    showFlightCard(QStringLiteral(" · Google 3D"));

    Station3dFlight f;
    f.call    = m_lastFlight.call;
    f.caption = QTextDocumentFragment::fromHtml(m_lastFlight.caption).toPlainText();
    f.lat     = m_lastFlight.lat;
    f.lon     = m_lastFlight.lon;
    f.viaCesiumIon = Map3dAccess::chosenSource() == Map3dAccess::Source::CesiumIon;
    f.secret       = Map3dAccess::secret();
    if (isValidGridSquare(m_homeGrid)) {
        f.hasHome = true;
        calculateLatLonFromGridSquare(m_homeGrid, f.homeLat, f.homeLon);
        const BeamHeading::GreatCircle g =
            BeamHeading::greatCircle(f.homeLat, f.homeLon, f.lat, f.lon);
        f.km = g.km;
        f.bearingDeg = g.bearingDeg;
    }
    m_3d->fly(f);
}

void QsoMapWindow::on3dFailed(const QString& reason, bool keyProblem)
{
    if (keyProblem) { m_3dRefusedKey = accessIdentity(); }
    if (!m_lastFlight.valid) { return; }
    // Zurueck auf den alten Weg, mit dem Grund in der Stationskarte.
    flyFlat(m_lastFlight.call, m_lastFlight.lat, m_lastFlight.lon);
    QString extra = QStringLiteral("<br>Google 3D: %1").arg(reason.toHtmlEscaped());
    if (keyProblem) {
        extra += QStringLiteral(" — <a href=\"longpath:google3d\" style=\"color:%1;\">"
                                "Google 3D settings…</a>")
                     .arg(QLatin1String(Style::kAccent));
    }
    showFlightCard(extra);
    m_googleEarthBtn->setVisible(true);
}

void QsoMapWindow::openGoogleEarth()
{
    if (!m_lastFlight.valid) { return; }
    double heading = 0.0;
    if (isValidGridSquare(m_homeGrid)) {
        double hlat = 0.0, hlon = 0.0;
        calculateLatLonFromGridSquare(m_homeGrid, hlat, hlon);
        heading = BeamHeading::greatCircle(hlat, hlon, m_lastFlight.lat, m_lastFlight.lon)
                      .bearingDeg;
    }
    if (m_openUrl) {
        m_openUrl(Station3dPage::googleEarthWebUrl(m_lastFlight.lat, m_lastFlight.lon, heading));
    }
}

void QsoMapWindow::update3dButton()
{
    if (!m_3dBtn) { return; }
    const bool built = Station3dSurface::isAvailable();
    const bool key   = Map3dAccess::isSet();
    QSignalBlocker b(m_3dBtn);
    // Immer bedienbar: ohne WebEngine sagt der Klick, warum es nicht geht.
    m_3dBtn->setEnabled(true);
    m_3dBtn->setChecked(built && key && m_3dWanted);
    if (!built) {
        m_3dBtn->setToolTip(QStringLiteral(
            "This build of Longpath has no Qt WebEngine, so the 3D view is not "
            "available. Use \"View in Google Earth\" next to the station card."));
    } else if (!key) {
        m_3dBtn->setToolTip(QStringLiteral(
            "Show looked-up stations in Google's photorealistic 3D, down to the "
            "house and the antenna. Needs a free Cesium ion account (or your own "
            "Google key) — click to set it up."));
    } else {
        m_3dBtn->setToolTip(QStringLiteral(
            "Fly to looked-up stations in Google's photorealistic 3D (via %1). "
            "Untick for the flat map with NASA imagery.")
            .arg(Map3dAccess::chosenSource() == Map3dAccess::Source::CesiumIon
                     ? QStringLiteral("your Cesium ion account")
                     : QStringLiteral("your Google key")));
    }
}

QString QsoMapWindow::accessIdentity()
{
    const QString secret = Map3dAccess::secret();
    if (secret.isEmpty()) { return QString(); }
    return Map3dAccess::sourceName(Map3dAccess::chosenSource()) + QLatin1Char(':') + secret;
}


// ── Rotor neben der Karte (2026-09-26) ──────────────────────────────────

void QsoMapWindow::setRotorRadarShown(bool on)
{
    m_rotorRadarShown = on;
    m_radar->setRotorStatusShown(on);
    updateRotorRadarVisibility();
}

void QsoMapWindow::updateRotorRadarVisibility()
{
    if (!m_rotorRadar) { return; }
    // Zeigt die Flaeche schon das Radar, waere ein zweites daneben doppelt;
    // die Azimutalkarte traegt den Rotorkeil ebenfalls selbst. Im 3D-Bild
    // gehoert die ganze Breite dem Bild (2026-11-10); wohin die Station
    // liegt, sagt dort die Anzeige im Bild.
    const bool bigRadar = m_stack && (isOverlayView(m_stack->currentIndex())
                                      || m_stack->currentIndex() == kView3d);
    // Zu schmal zum Ablesen: dann lieber nur die Karte.
    const bool roomy = width() >= 420;
    m_rotorRadar->setVisible(m_rotorRadarShown && !bigRadar && roomy);
}

void QsoMapWindow::resizeEvent(QResizeEvent* e)
{
    QDialog::resizeEvent(e);
    updateRotorRadarVisibility();
}

void QsoMapWindow::setRotorHeading(double deg)
{
    m_rotorRadar->setRotorHeading(deg);
    m_radar->setRotorHeading(deg);
    m_azimut->setRotorHeading(deg);
}

void QsoMapWindow::setRotorTarget(double deg)
{
    m_rotorRadar->setRotorTarget(deg);
    m_radar->setRotorTarget(deg);
}

void QsoMapWindow::setRotorBeamWidth(double deg)
{
    m_rotorRadar->setRotorBeamWidth(deg);
    m_radar->setRotorBeamWidth(deg);
    m_azimut->setRotorBeamWidth(deg);
}

void QsoMapWindow::showRadarViewForTest(bool on)
{
    if (m_radarBtn) { m_radarBtn->setChecked(on); }
}

} // namespace Longpath
