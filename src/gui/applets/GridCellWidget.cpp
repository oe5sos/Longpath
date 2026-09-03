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
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {

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

    // Die gelbe Anfassmarke, wie an den eigenen Fenstern und wie bei
    // Zeus. Sie sagt „hier anfassen" — ein Kopf ohne diese Marke sieht
    // aus wie einer, den man nicht anfassen kann.
    auto* stripe = new QLabel(m_titleBar);
    stripe->setFixedWidth(3);
    // Aus der Palette: derselbe Ton wie an den Fensterleisten und an
    // den Containern (Style::kAmberText). Hier stand #d8a13a — eine
    // eigene Zahl fuer dieselbe Marke.
    stripe->setStyleSheet(QStringLiteral("background: %1;")
                              .arg(QLatin1String(Style::kAmberText)));
    m_titleLayout->addWidget(stripe);

    auto* grip = new QLabel(QStringLiteral("⋮⋮"), m_titleBar);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; background: transparent; }"
    ).arg(Style::kTextScale));
    m_titleLayout->addWidget(grip);

    m_titleLabel = new QLabel(m_titleBar);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold;"
        " background: transparent; }"
    ).arg(Style::kTitleText));
    m_titleLayout->addWidget(m_titleLabel);

    // ── Die Knoepfe DICHT beim Titel, nicht am Rand ──────────────────
    //
    // Hier stand addStretch() VOR den Knoepfen; sie sassen damit am
    // rechten Rand der Spalte. Am 2026-08-20 hat der Betreiber die
    // Spalte auf gut 700 px gezogen und gemeldet: „nur am pandaper
    // funktioniert das". Nachgemessen: bei 702 px Spaltenbreite endet
    // der Titel bei 40 px und das ↗ begann bei 662 — 622 px Abstand.
    //
    // Beim Panadapter steht sein ↗ dicht bei der Beschriftung, und
    // genau dort hat er es gefunden. Ein Knopf, den man erst am
    // anderen Ende der Zeile suchen muss, ist so gut wie keiner — das
    // ist dieselbe Lehre wie beim Knopf, den es gar nicht gab, nur
    // eine Stufe subtiler.
    //
    // Die Dehnung kommt jetzt DANACH: Marke, Griff, Titel, Knoepfe,
    // dann der leere Rest.
    buildCellButtons();
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

// Die beiden Knoepfe rechts in der Kopfleiste.
//
// Zeus Link setzt sie sichtbar in JEDEN Fensterkopf. Ein Weg, den man
// nicht sieht, ist kein Weg — das hat uns dieses Vorhaben achtmal
// gezeigt, zuletzt hier: der Ablöseknopf existierte seit dem
// 2026-08-19 im Quelltext, aber in einer Kopfleiste, die kein Applet
// mehr benutzt.
void GridCellWidget::buildCellButtons()
{
    const QString btnCss = QStringLiteral(
        "QPushButton { background: transparent; border: none;"
        "  color: %1; font-size: 11px; padding: 0; }"
        "QPushButton:hover { background: %2; color: %3;"
        "  border-radius: 3px; }")
        .arg(QString::fromLatin1(Style::kTextScale),
             QString::fromLatin1(Style::kButtonHover),
             QString::fromLatin1(Style::kTextPrimary));

    auto* detach = new QPushButton(QStringLiteral("\u2197"), m_titleBar);
    detach->setFixedSize(16, 14);
    detach->setCursor(Qt::PointingHandCursor);
    detach->setToolTip(QStringLiteral(
        "Als eigenes Fenster ablösen — dann frei verschiebbar "
        "und in der Ecke ziehbar."));
    detach->setStyleSheet(btnCss);
    connect(detach, &QPushButton::clicked, this, [this]() {
        // Das erste Applet im Feld. Bei mehreren Inhalten ist es das
        // oberste — dieselbe Auswahl, die auch das Rechtsklick-Menue
        // trifft.
        const QList<AppletWidget*> as = applets();
        if (!as.isEmpty()) { emit detachRequested(as.first()); }
    });
    m_titleLayout->addWidget(detach);

    auto* close = new QPushButton(QStringLiteral("\u2715"), m_titleBar);
    close->setFixedSize(16, 14);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(QStringLiteral(
        "Ausblenden. Mit dem + unten rechts kommt es zurück."));
    close->setStyleSheet(btnCss);
    connect(close, &QPushButton::clicked, this, [this]() {
        const QList<AppletWidget*> as = applets();
        if (!as.isEmpty()) { emit hideRequested(as.first()); }
    });
    m_titleLayout->addWidget(close);
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

} // namespace Longpath
