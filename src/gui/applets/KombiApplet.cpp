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

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Longpath {

namespace {
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
    m_reiterReihe->addStretch(1);
    layout()->addWidget(m_reiterleiste);
}

QString KombiApplet::appletTitle() const
{
    // Der Titel sagt, was drin ist -- „RX / TX", nicht „Kombi". Bei
    // leerem Feld der Name der Sache selbst, damit es im Auswaehler
    // ueberhaupt zu finden ist.
    QStringList namen;
    for (const QPointer<AppletWidget>& w : m_inhalte) {
        if (w) { namen << w->appletTitle(); }
    }
    return namen.isEmpty() ? tr("Combined") : namen.join(QStringLiteral(" / "));
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

    const QString css = Style::buttonBaseStyle() + Style::blueCheckedStyle();
    for (int i = 0; i < m_inhalte.size(); ++i) {
        AppletWidget* w = m_inhalte.at(i).data();
        if (!w) { continue; }
        auto* b = new QPushButton(w->appletTitle(), m_reiterleiste);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(css);
        b->setFixedHeight(20);
        b->setToolTip(tr("Show this panel. Right-click to take it out again."));
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, &QPushButton::clicked, this,
                [this, i]() { zeigeSeite(i); });
        connect(b, &QWidget::customContextMenuRequested, this,
                [this, w](const QPoint&) { gibFrei(w); });
        m_reiterReihe->insertWidget(i, b);
        m_reiter.append(b);
    }

    const bool leer = m_inhalte.isEmpty();
    if (m_reiterleiste) { m_reiterleiste->setVisible(!leer); }
    if (m_hinweis)      { m_hinweis->setVisible(leer); }
    if (m_stapel)       { m_stapel->setVisible(!leer); }

    if (!leer) { zeigeSeite(qBound(0, seite(), m_inhalte.size() - 1)); }
    merkeStand();
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
