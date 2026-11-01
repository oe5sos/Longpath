// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/applets/KombiApplet.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe KombiApplet.h fuer das Warum.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-01 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/KombiApplet.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Longpath {

namespace {
QList<KombiApplet*> g_alle;
QString standSchluessel() { return QStringLiteral("Kombi/Inhalt"); }
QString seitenSchluessel() { return QStringLiteral("Kombi/Seite"); }
}  // namespace

KombiApplet::KombiApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(Style::kPanelGapPx);

    baueReiter();

    // ── Schmal gestaltbar (2026-11-01) ──────────────────────────────
    //
    // Betreiber: „sollte ggf. eher schmal gestaltbar sein, weil
    // Spalte schmal und hoch ist." Ohne das bestimmt die BREITESTE
    // Seite die Breite des ganzen Feldes -- und damit die der Spalte.
    // Ein Rollbereich nimmt der breitesten Seite dieses Vorrecht: sie
    // darf breiter sein als das Feld, dann wird eben geschoben.
    //
    // Nur waagerecht. Senkrecht rollt die Spalte selbst; zwei
    // ineinander liegende senkrechte Rollbereiche sind eine Falle,
    // weil das Rad dann mal den einen und mal den anderen bewegt.
    m_stapel = new QStackedWidget;
    m_rollbereich = new QScrollArea(this);
    m_rollbereich->setWidget(m_stapel);
    m_rollbereich->setWidgetResizable(true);
    m_rollbereich->setFrameShape(QFrame::NoFrame);
    m_rollbereich->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_rollbereich->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(m_rollbereich, 1);
    // Das Feld selbst darf so schmal werden, wie der Betreiber die
    // Spalte zieht.
    setMinimumWidth(0);

    m_aufklapp = new QWidget(this);
    auto* senkrecht = new QVBoxLayout(m_aufklapp);
    senkrecht->setContentsMargins(0, 0, 0, 0);
    senkrecht->setSpacing(Style::kPanelGapPx);
    m_aufklapp->setVisible(false);
    root->addWidget(m_aufklapp, 1);

    // Der leere Zustand sagt, was zu tun ist. Ein leeres Feld ohne ein
    // Wort sieht aus wie ein Fehler.
    m_hinweis = new QLabel(tr("Drag panels in here by their title bar"), this);
    m_hinweis->setAlignment(Qt::AlignCenter);
    m_hinweis->setWordWrap(true);
    m_hinweis->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; padding: 12px;")
            .arg(QLatin1String(Style::kTextTertiary)));
    root->addWidget(m_hinweis, 1);

    reiterNeu();
    g_alle.append(this);

    // Rechtsklick auf das Feld fragt nach der Liste. Auf dem Feld,
    // nicht nur auf dem leeren Hinweis: auch mit zwei Seiten darin
    // will man eine dritte dazulegen koennen.
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& wo) {
        emit listeGewuenscht(mapToGlobal(wo));
    });
}

KombiApplet::~KombiApplet()
{
    g_alle.removeAll(this);
}

QList<KombiApplet*> KombiApplet::alle()
{
    return g_alle;
}

// Das sichtbare Rechteck auf dem SCHIRM entscheidet -- nicht, in
// welchem Fenster das Feld haengt.
KombiApplet* KombiApplet::unter(const QPoint& globalPos)
{
    for (KombiApplet* k : g_alle) {
        if (!k || !k->isVisible()) { continue; }
        const QRect r(k->mapToGlobal(QPoint(0, 0)), k->size());
        if (r.contains(globalPos)) { return k; }
    }
    return nullptr;
}

void KombiApplet::baueReiter()
{
    // Oben, wie der Betreiber es gesagt hat („oben im Panel zum
    // umschalten"), und LINKS: rechts oben sitzt das Zahnrad, und das
    // ist seit dem 2026-09-20 seine feste Regel fuer Optionen.
    m_reiterleiste = new QWidget(this);
    m_reiterReihe = new QHBoxLayout(m_reiterleiste);
    m_reiterReihe->setContentsMargins(0, 0, 0, 0);
    m_reiterReihe->setSpacing(Style::kPanelGapPx);
    // KEIN Dehnfuge am Ende: die Reiter teilen sich die volle Breite.
    // Betreiber, 2026-11-01, zu Entwurf 2: „ziel des fenster ist es
    // ja, eines offen zu lassen und schnell umschalten zu können."
    // Schnell heiszt: nicht zielen muessen. Zwei halbe Panelbreiten
    // sind das groesztmoegliche Ziel, das hier zu haben ist.
    layout()->addWidget(m_reiterleiste);
}

QString KombiApplet::appletTitle() const
{
    // IMMER „Combined", nie der Inhalt.
    //
    // Betreiber, 2026-11-01: „das RX/TX (combined) panel sollte
    // generell combined heißen, sofern man hier dann alle widget
    // reinziehen kann!" -- und man kann. Ein Name, der sich mit dem
    // Inhalt aendert, haette das Feld jedes Mal zu etwas anderem
    // gemacht; dabei ist es immer dasselbe Ding. Was gerade drin ist,
    // steht ohnehin auf den Reitern.
    return tr("Combined");
}

void KombiApplet::nimmAuf(QWidget* panel)
{
    if (!panel || !m_stapel) { return; }
    for (const QPointer<QWidget>& w : m_inhalte) {
        if (w == panel) { return; }
    }
    m_inhalte.append(panel);
    m_stapel->addWidget(panel);   // Elternwechsel, kein Neubau
    panel->show();
    reiterNeu();
    zeigeSeite(m_inhalte.size() - 1);
}

void KombiApplet::gibFrei(QWidget* panel)
{
    if (!panel || !m_stapel) { return; }
    int weg = -1;
    for (int i = 0; i < m_inhalte.size(); ++i) {
        if (m_inhalte.at(i) == panel) { weg = i; break; }
    }
    if (weg < 0) { return; }
    m_inhalte.removeAt(weg);
    if (m_stapel->indexOf(panel) >= 0) { m_stapel->removeWidget(panel); }
    // Ohne Elternteil zurueck: unversehrt, aber ohne Platz. Wer es
    // bekommt, legt es in ein eigenes Feld.
    panel->setParent(nullptr);
    reiterNeu();
    emit herausgeloest(panel);
}

QList<QWidget*> KombiApplet::gibAlleFrei()
{
    QList<QWidget*> raus;
    while (!m_inhalte.isEmpty()) {
        QWidget* w = m_inhalte.first().data();
        if (!w) { m_inhalte.removeFirst(); continue; }
        raus.append(w);
        gibFrei(w);
    }
    return raus;
}

// Die Reiterleiste wird neu gebaut, nicht nachgepflegt: bei zwei bis
// fuenf Knoepfen ist das billiger als jede Buchhaltung, und es gibt
// keinen Zustand, der auseinanderlaufen kann.
void KombiApplet::reiterNeu()
{
    for (QPushButton* b : m_reiter) { b->deleteLater(); }
    m_reiter.clear();

    // ── Schwarz, nicht blau (2026-11-01) ────────────────────────────
    //
    // Betreiber zum ersten Bau: „die farbe ist auch blau und nicht
    // schwarz". Zu Recht -- ein Reiter ueber die volle Panelbreite in
    // der Akzentfarbe ist eine grosze FLAECHE Farbe, und der Hausstil
    // sagt: Farbe bedeckt hoechstens zwei Prozent. Der aktive Reiter
    // bleibt jetzt dunkel wie das Panel; erkennbar ist er an der
    // helleren Schrift und an einem schmalen Strich darunter. Das ist
    // dieselbe Menge Farbe wie in Entwurf 4, nur ueber die ganze
    // Breite.
    // ── Schwarz. Ohne Blau. (2026-11-01) ────────────────────────────
    //
    // Betreiber zweimal zu diesem Reiter: „die farbe ist auch blau
    // und nicht schwarz" und dann „1. muss schwarz sein". Also kein
    // Akzent, auch nicht als Strich.
    //
    // Erkennbar bleibt der aktive Reiter an zwei Dingen, die keine
    // Farbe kosten: er liegt TIEFER (dunkel wie das Panel, waehrend
    // die anderen aufgesetzt sind) und seine Schrift ist hell. Das
    // ist dieselbe Sprache, die der Hausstil ueberall spricht --
    // Licht von oben, Vertiefung heiszt gewaehlt.
    const QString css =
        Style::buttonBaseStyle()
        + QStringLiteral(
              "QPushButton:checked {"
              "  background: %1; color: %2;"
              "  border: 1px solid %3;"
              "}"
              "QPushButton:!checked { color: %4; }")
              .arg(QLatin1String(Style::kPanelBg),
                   QLatin1String(Style::kTextPrimary),
                   QLatin1String(Style::kBorder),
                   QLatin1String(Style::kTextTertiary));
    for (int i = 0; i < m_inhalte.size(); ++i) {
        QWidget* w = m_inhalte.at(i).data();
        if (!w) { continue; }
        auto* b = new QPushButton(nameVon(w), m_reiterleiste);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(css);
        // Hoeher als ein gewoehnlicher Knopf (20) und ueber die ganze
        // Breite: dieser hier wird oft und im Vorbeigehen getroffen.
        b->setFixedHeight(24);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setToolTip(tr("Show this panel. Right-click to take it out again."));
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QPushButton::clicked, this,
                [this, i]() { zeigeSeite(i); });
        connect(b, &QWidget::customContextMenuRequested, this,
                [this, w](const QPoint&) { gibFrei(w); });
        // Herausziehen: der Reiter selbst ist der Griff.
        b->installEventFilter(this);
        b->setProperty("kombiSeite", i);
        m_reiterReihe->insertWidget(i, b, 1);   // gleiche Breite fuer jeden
        m_reiter.append(b);
    }

    const bool leer = m_inhalte.isEmpty();
    if (m_reiterleiste) { m_reiterleiste->setVisible(!leer); }
    if (m_hinweis)      { m_hinweis->setVisible(leer); }
    if (m_stapel)       { m_stapel->setVisible(!leer); }
    if (m_rollbereich)  { m_rollbereich->setVisible(!leer && !m_aufgeklappt); }

    if (!leer) { zeigeSeite(qBound(0, seite(), m_inhalte.size() - 1)); }
    // Die feste Hoehe haengt an den Seiten -- kommt eine dazu oder
    // geht eine weg, wird sie neu bestimmt.
    if (leer) {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        ordneSeitenEin();
    }
    chipNachziehen();
    merkeStand();
}

// ── Ein Reiter als Griff ───────────────────────────────────────────
//
// Betreiber, 2026-11-01: „frequenz lässt sich reinziehen, soll sich
// aber wieder rausziehen lassen!" Hinein geht es durch Ziehen; hinaus
// ging es nur ueber einen Rechtsklick -- einen Weg, den man kennen
// musz. Was auf einem Weg hineingeht, musz denselben Weg hinaus.
//
// Erst ab einer Strecke wird aus dem Druck ein Zug (Qt's eigene
// Startstrecke), sonst waere jeder Klick ein halbes Herausziehen.
bool KombiApplet::eventFilter(QObject* watched, QEvent* ev)
{
    auto* knopf = qobject_cast<QPushButton*>(watched);
    if (!knopf) { return AppletWidget::eventFilter(watched, ev); }

    const int i = knopf->property("kombiSeite").toInt();
    const QList<QWidget*> alle = seiten();

    switch (ev->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() == Qt::LeftButton && i >= 0 && i < alle.size()) {
            m_zugStart = me->globalPosition().toPoint();
            m_zugPanel = alle.at(i);
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (!m_zugPanel || m_zugStart.isNull()) { break; }
        const QPoint jetzt = me->globalPosition().toPoint();
        if ((jetzt - m_zugStart).manhattanLength()
                < QApplication::startDragDistance()) {
            break;
        }
        QWidget* raus = m_zugPanel.data();
        m_zugPanel = nullptr;
        m_zugStart = QPoint();
        // Erst aus dem Feld, dann die Stelle melden: wer zuhoert,
        // bekommt ein Panel ohne Elternteil und eine Stelle dafuer.
        gibFrei(raus);
        emit herausgezogen(raus, jetzt);
        return true;
    }
    case QEvent::MouseButtonDblClick:
        // Die Reiter fuellen die ganze Leiste -- eine freie Stelle
        // zum Doppelklicken gibt es nicht, also zaehlt der Reiter
        // selbst. Einfach klicken schaltet um, doppelt klappt auf.
        m_zugPanel = nullptr;
        m_zugStart = QPoint();
        setAufgeklappt(!m_aufgeklappt);
        return true;
    case QEvent::MouseButtonRelease:
        m_zugPanel = nullptr;
        m_zugStart = QPoint();
        break;
    default:
        break;
    }
    return AppletWidget::eventFilter(watched, ev);
}

// ── Auf- und zuklappen ─────────────────────────────────────────────
//
// Zugeklappt ist das Feld ein Stapel mit Reitern und einer FESTEN
// Hoehe -- es springt beim Umschalten nicht, und das war der halbe
// Sinn der Sache („eine größe, die fix bleibt").
//
// Aufgeklappt stehen alle Seiten untereinander: man sieht die
// anderen Panels mit, ohne sie herauszuziehen. Beim Zuklappen
// bekommt das Feld seine feste Hoehe zurueck.
void KombiApplet::setAufgeklappt(bool an)
{
    if (m_aufgeklappt == an) { return; }
    m_aufgeklappt = an;
    ordneSeitenEin();
}

void KombiApplet::ordneSeitenEin()
{
    if (!m_stapel || !m_aufklapp) { return; }
    auto* senkrecht = qobject_cast<QVBoxLayout*>(m_aufklapp->layout());
    if (!senkrecht) { return; }

    if (m_aufgeklappt) {
        for (const QPointer<QWidget>& w : m_inhalte) {
            if (!w) { continue; }
            if (m_stapel->indexOf(w) >= 0) { m_stapel->removeWidget(w); }
            senkrecht->addWidget(w.data());
            w->show();
        }
        m_stapel->setVisible(false);
        if (m_rollbereich) { m_rollbereich->setVisible(false); }
        m_aufklapp->setVisible(true);
        setMaximumHeight(QWIDGETSIZE_MAX);
        setMinimumHeight(0);
    } else {
        for (const QPointer<QWidget>& w : m_inhalte) {
            if (!w) { continue; }
            senkrecht->removeWidget(w.data());
            if (m_stapel->indexOf(w) < 0) { m_stapel->addWidget(w.data()); }
        }
        m_aufklapp->setVisible(false);
        m_stapel->setVisible(!m_inhalte.isEmpty());
        if (m_rollbereich) { m_rollbereich->setVisible(!m_inhalte.isEmpty()); }
        zeigeSeite(seite());
        // ── KEINE feste Hoehe mehr (2026-11-01) ──────────────────────
        //
        // Betreiber: „beim drüber scrollen verschwindet einiges".
        // Gesehen in der laufenden App: das Feld war viel hoeher als
        // sein Inhalt, darunter eine groesze schwarze Flaeche, und in
        // der Spalte schob es die Nachbarn aus dem Bild.
        //
        // `setFixedHeight` war der falsche Griff. Die feste Groesze,
        // um die es ihm ging, macht der Stapel von selbst: ein
        // QStackedWidget nimmt die Hoehe der GROESZTEN Seite, nicht
        // die der sichtbaren -- das Feld springt beim Umschalten also
        // ohnehin nicht. Eine Untergrenze genuegt, damit es nicht
        // zusammenfaellt; nach oben entscheidet das Gehaeuse.
        setMaximumHeight(QWIDGETSIZE_MAX);
        setMinimumHeight(0);
    }
    updateGeometry();
}

// Der Umschalter in der Kopfleiste (Entwurf 3).
//
// EIN Knopf, kein Streifen: er zeigt, was gerade laeuft, und schaltet
// beim Klick auf die naechste Seite weiter. Der Gewinn ist Hoehe --
// Betreiber, 2026-11-01: "das TX panel ist oben so gross, das bei
// meinem mac das nicht otimal ist".
//
// Rechtsklick loest die gezeigte Seite wieder heraus, Doppelklick
// klappt auf und zu: dieselben Griffe wie vorher am Reiter.
QWidget* KombiApplet::kopfleistenWidget()
{
    if (!m_chip) {
        m_chip = new QPushButton(this);
        m_chip->setCursor(Qt::PointingHandCursor);
        m_chip->setFixedHeight(16);
        m_chip->setStyleSheet(
            QStringLiteral(
                "QPushButton {"
                "  background: %1; color: %2;"
                "  border: 1px solid %3; border-radius: 4px;"
                "  font-size: 9px; font-weight: bold;"
                "  padding: 0px 8px;"
                "}"
                "QPushButton:hover { background: %4; }")
                .arg(QLatin1String(Style::kPanelBg),
                     QLatin1String(Style::kTextPrimary),
                     QLatin1String(Style::kBorder),
                     QLatin1String(Style::kButtonHover)));
        m_chip->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_chip, &QPushButton::clicked, this, [this]() {
            if (m_inhalte.size() < 2) { return; }
            zeigeSeite((seite() + 1) % m_inhalte.size());
        });
        connect(m_chip, &QWidget::customContextMenuRequested, this,
                [this](const QPoint&) {
            const QList<QWidget*> s = seiten();
            const int i = seite();
            if (i >= 0 && i < s.size()) { gibFrei(s.at(i)); }
        });
        m_chip->installEventFilter(this);
        m_chip->setProperty("kombiChip", true);
    }
    chipNachziehen();
    return m_chip;
}

// Was auf dem Chip steht: die laufende Seite und, wenn es mehr als
// eine gibt, ein Pfeil als Hinweis, dasz es weitergeht.
// Der Name einer Seite. Ein Applet kennt seinen Titel selbst; alles
// andere (Rotor, Logbuch) traegt ihn als Fenstertitel.
QString KombiApplet::nameVon(const QWidget* w)
{
    if (!w) { return QString(); }
    if (auto* a = qobject_cast<const AppletWidget*>(w)) {
        return a->appletTitle();
    }
    if (!w->windowTitle().isEmpty()) { return w->windowTitle(); }
    return w->objectName().isEmpty() ? QStringLiteral("Panel")
                                     : w->objectName();
}

void KombiApplet::chipNachziehen()
{
    if (!m_chip) { return; }
    const QList<QWidget*> s = seiten();
    const int i = seite();
    const QString name = (i >= 0 && i < s.size()) ? nameVon(s.at(i))
                                                  : tr("empty");
    m_chip->setText(s.size() > 1 ? QStringLiteral("%1 \u203a").arg(name)
                                 : name);
    m_chip->setToolTip(s.size() > 1
        ? tr("Next panel. Right-click takes this one out, "
             "double-click unfolds them all.")
        : tr("Drag panels onto this field by their title bar."));
    m_chip->setVisible(!s.isEmpty());
    // Der eigene Streifen ist dann ueberfluessig -- genau das war der
    // Punkt: er kostete Hoehe.
    if (m_reiterleiste) { m_reiterleiste->setVisible(false); }
}

QList<QWidget*> KombiApplet::seiten() const
{
    QList<QWidget*> raus;
    for (const QPointer<QWidget>& w : m_inhalte) {
        if (w) { raus.append(w.data()); }
    }
    return raus;
}

int KombiApplet::seite() const
{
    return m_stapel ? qMax(0, m_stapel->currentIndex()) : 0;
}

bool KombiApplet::istLeer() const
{
    return m_inhalte.isEmpty();
}

void KombiApplet::zeigeSeite(int welche)
{
    if (m_inhalte.isEmpty()) { return; }
    const int w = qBound(0, welche, m_inhalte.size() - 1);
    if (m_stapel) { m_stapel->setCurrentIndex(w); }
    for (int i = 0; i < m_reiter.size(); ++i) {
        m_reiter.at(i)->setChecked(i == w);
    }
    AppSettings::instance().setValue(seitenSchluessel(), QString::number(w));
    chipNachziehen();
    emit seiteGewechselt(w);
}

// Was drin liegt, steht als Liste von Applet-Kennungen in den
// Einstellungen -- so findet der naechste Start dieselben Panels
// wieder zusammen.
void KombiApplet::merkeStand() const
{
    QStringList ids;
    for (const QPointer<QWidget>& w : m_inhalte) {
        if (auto* a = qobject_cast<AppletWidget*>(w.data())) {
            ids << a->appletId();
        } else if (w) {
            ids << w->objectName();
        }
    }
    AppSettings::instance().setValue(standSchluessel(),
                                     ids.join(QLatin1Char(',')));
}

void KombiApplet::syncFromModel()
{
    // ALLE, nicht nur die sichtbare: wer umschaltet, soll keinen
    // veralteten Stand vorfinden.
    // Nur die Applets koennen das -- ein Rotorfenster hat keinen
    // Modellabgleich.
    for (const QPointer<QWidget>& w : m_inhalte) {
        if (auto* a = qobject_cast<AppletWidget*>(w.data())) {
            a->syncFromModel();
        }
    }
}

bool KombiApplet::hasExtendedSettings() const
{
    const QList<QWidget*> s = seiten();
    const int i = seite();
    if (i < 0 || i >= s.size()) { return false; }
    auto* a = qobject_cast<AppletWidget*>(s.at(i));
    return a && a->hasExtendedSettings();
}

void KombiApplet::openExtendedSettings()
{
    const QList<QWidget*> s = seiten();
    const int i = seite();
    if (i < 0 || i >= s.size()) { return; }
    if (auto* a = qobject_cast<AppletWidget*>(s.at(i))) {
        a->openExtendedSettings();
    }
}

} // namespace Longpath
