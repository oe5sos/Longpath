// =================================================================
// src/gui/applets/GridCellWidget.cpp  (NereusSDR)
// =================================================================
// Siehe GridCellWidget.h — Kopfleiste plus N Inhalte.
//
// Die Kopfleiste ist Zeile fuer Zeile das, was
// AppletPanelWidget::wrapWithTitleBar bisher gebaut hat (Griff ⋮⋮ in
// kTextScale 10 px, Titel in kTitleText 10 px fett, Style::
// titleBarStyle(), Hoehe kTitleBarH bzw. 22 mit nachgestelltem
// Widget). Schritt 1 soll dasselbe Bild ergeben; wer hier etwas
// aendert, aendert das Aussehen jedes Applets auf einmal.
// =================================================================

#include "gui/applets/GridCellWidget.h"

#include "gui/StyleConstants.h"
#include "gui/applets/AppletWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace NereusSDR {

GridCellWidget::GridCellWidget(const QString& id, QWidget* parent)
    : QWidget(parent)
    , m_id(id)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new QWidget(this);
    m_titleBar->setStyleSheet(Style::titleBarStyle());

    m_titleLayout = new QHBoxLayout(m_titleBar);
    m_titleLayout->setContentsMargins(2, 0, 4, 0);
    m_titleLayout->setSpacing(4);

    auto* grip = new QLabel(QStringLiteral("⋮⋮"), m_titleBar);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; background: transparent; }"
    ).arg(Style::kTextScale));
    m_titleLayout->addWidget(grip);

    m_titleLabel = new QLabel(m_titleBar);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: bold;"
        " background: transparent; }"
    ).arg(Style::kTitleText));
    m_titleLayout->addWidget(m_titleLabel);
    m_titleLayout->addStretch();

    root->addWidget(m_titleBar);

    // KEINE Zwischenebene fuer den Inhalt: die Inhalte haengen direkt
    // im Wurzellayout, hinter der Kopfleiste. Ein zusaetzliches
    // Traegerwidget waere ein Elternteil mehr zwischen Feld und Inhalt
    // — und damit eine Ebene, an der jeder, der von einem Inhalt zu
    // seinem Feld hinaufsucht, vorbeilaeuft.
    //
    // Kein Abstand zwischen zwei Inhalten desselben Feldes: sie sollen
    // als EIN Feld lesbar sein. Zwischen zwei FELDERN steht der Abstand
    // des Rasters.
    m_contentLayout = root;

    m_titleBar->setCursor(Qt::OpenHandCursor);
    updateTitleBarHeight();
    refreshTitleText();
}

void GridCellWidget::addWidget(QWidget* w)
{
    if (!w || m_contents.contains(w)) { return; }
    m_contents.append(w);
    w->setParent(this);
    w->show();
    m_contentLayout->addWidget(w);
    refreshTitleText();
}

void GridCellWidget::removeWidget(QWidget* w)
{
    if (!w || !m_contents.contains(w)) { return; }
    m_contentLayout->removeWidget(w);
    // Elternschaft loesen, NICHT loeschen. Auf dieser Eigenschaft
    // beruht das Abloesen in ein eigenes Fenster (siehe
    // MainWindow::detachApplet) und das Umhaengen zwischen Feldern.
    w->setParent(nullptr);
    w->hide();
    m_contents.removeOne(w);
    refreshTitleText();
}

QList<AppletWidget*> GridCellWidget::applets() const
{
    QList<AppletWidget*> out;
    for (QWidget* w : m_contents) {
        if (auto* a = qobject_cast<AppletWidget*>(w)) { out.append(a); }
    }
    return out;
}

void GridCellWidget::setTitle(const QString& title)
{
    m_title = title;
    refreshTitleText();
}

void GridCellWidget::setTrailingWidget(QWidget* w)
{
    if (m_trailing == w) { return; }
    if (m_trailing) { m_titleLayout->removeWidget(m_trailing); }
    m_trailing = w;
    if (m_trailing) { m_titleLayout->addWidget(m_trailing); }
    updateTitleBarHeight();
}

void GridCellWidget::updateTitleBarHeight()
{
    m_titleBar->setFixedHeight(m_trailing ? 22 : Style::kTitleBarH);
}

void GridCellWidget::refreshTitleText()
{
    if (!m_titleLabel) { return; }
    if (!m_title.isEmpty()) {
        m_titleLabel->setText(m_title);
        return;
    }
    // Kein eigener Titel: bei EINEM Inhalt dessen Titel — so wie
    // bisher. Bei mehreren waere jede Wahl willkuerlich, also steht
    // dort nichts, bis das Feld benannt wird.
    const QList<AppletWidget*> a = applets();
    if (m_contents.size() == 1 && a.size() == 1) {
        m_titleLabel->setText(a.first()->appletTitle());
    } else {
        m_titleLabel->setText(QString());
    }
}

} // namespace NereusSDR
