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

    m_stapel = new QStackedWidget(this);
    root->addWidget(m_stapel, 1);

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

void KombiApplet::nimmAuf(AppletWidget* panel)
{
    if (!panel || !m_stapel) { return; }
    for (const QPointer<AppletWidget>& w : m_inhalte) {
        if (w == panel) { return; }
    }
    m_inhalte.append(panel);
    m_stapel->addWidget(panel);   // Elternwechsel, kein Neubau
    panel->show();
    reiterNeu();
    zeigeSeite(m_inhalte.size() - 1);
}

void KombiApplet::gibFrei(AppletWidget* panel)
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

QList<AppletWidget*> KombiApplet::gibAlleFrei()
{
    QList<AppletWidget*> raus;
    while (!m_inhalte.isEmpty()) {
        AppletWidget* w = m_inhalte.first().data();
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
    const QString css =
        Style::buttonBaseStyle()
        + QStringLiteral(
              "QPushButton:checked {"
              "  background: %1; color: %2;"
              "  border: 1px solid %3;"
              "  border-bottom: 2px solid %4;"
              "}")
              .arg(QLatin1String(Style::kPanelBg),
                   QLatin1String(Style::kTextPrimary),
                   QLatin1String(Style::kBorder),
                   QLatin1String(Style::kAccent));
    for (int i = 0; i < m_inhalte.size(); ++i) {
        AppletWidget* w = m_inhalte.at(i).data();
        if (!w) { continue; }
        auto* b = new QPushButton(w->appletTitle(), m_reiterleiste);
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

    if (!leer) { zeigeSeite(qBound(0, seite(), m_inhalte.size() - 1)); }
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
    const QList<AppletWidget*> alle = seiten();

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
        AppletWidget* raus = m_zugPanel.data();
        m_zugPanel = nullptr;
        m_zugStart = QPoint();
        // Erst aus dem Feld, dann die Stelle melden: wer zuhoert,
        // bekommt ein Panel ohne Elternteil und eine Stelle dafuer.
        gibFrei(raus);
        emit herausgezogen(raus, jetzt);
        return true;
    }
    case QEvent::MouseButtonRelease:
        m_zugPanel = nullptr;
        m_zugStart = QPoint();
        break;
    default:
        break;
    }
    return AppletWidget::eventFilter(watched, ev);
}

QList<AppletWidget*> KombiApplet::seiten() const
{
    QList<AppletWidget*> raus;
    for (const QPointer<AppletWidget>& w : m_inhalte) {
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
    emit seiteGewechselt(w);
}

// Was drin liegt, steht als Liste von Applet-Kennungen in den
// Einstellungen -- so findet der naechste Start dieselben Panels
// wieder zusammen.
void KombiApplet::merkeStand() const
{
    QStringList ids;
    for (const QPointer<AppletWidget>& w : m_inhalte) {
        if (w) { ids << w->appletId(); }
    }
    AppSettings::instance().setValue(standSchluessel(),
                                     ids.join(QLatin1Char(',')));
}

void KombiApplet::syncFromModel()
{
    // ALLE, nicht nur die sichtbare: wer umschaltet, soll keinen
    // veralteten Stand vorfinden.
    for (const QPointer<AppletWidget>& w : m_inhalte) {
        if (w) { w->syncFromModel(); }
    }
}

bool KombiApplet::hasExtendedSettings() const
{
    const QList<AppletWidget*> s = seiten();
    const int i = seite();
    return i >= 0 && i < s.size() && s.at(i)->hasExtendedSettings();
}

void KombiApplet::openExtendedSettings()
{
    const QList<AppletWidget*> s = seiten();
    const int i = seite();
    if (i >= 0 && i < s.size()) { s.at(i)->openExtendedSettings(); }
}

} // namespace Longpath
