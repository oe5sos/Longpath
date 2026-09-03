// =================================================================
// src/gui/widgets/VfoTileRow.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung steht in der Kopfdatei.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/VfoTileRow.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/SliceColors.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace Longpath {

// Eine Kachel ist ein QFrame-artiges Feld, das auf einen Klick
// antwortet. Ein QPushButton waere naheliegend, gibt aber zwei
// Zeilen Text nur ueber Umwege her und bringt eine Knopfoptik mit,
// die neben der Ziffernanzeige zu laut ist.
class VfoTile : public QWidget {
public:
    explicit VfoTile(QWidget* parent = nullptr) : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(7, 3, 7, 3);
        lay->setSpacing(0);
        m_top = new QLabel(this);
        m_bottom = new QLabel(this);
        lay->addWidget(m_top);
        lay->addWidget(m_bottom);
    }

    void setLines(const QString& top, const QString& bottom)
    {
        m_top->setText(top);
        m_bottom->setText(bottom);
    }

    void setLook(const QColor& accent, bool active, bool alert)
    {
        // Aktiv: Rahmen in der Scheibenfarbe, Grund leicht angehoben.
        // Sendend: roter Rahmen — auf dem Bild des Betreibers ist das
        // die einzige Kachel mit Farbe, und das ist richtig, denn es
        // ist die einzige, bei der ein Irrtum Folgen hat.
        const QString border = alert ? QString::fromLatin1(Style::kTxRed)
                                     : (active ? accent.name()
                                               : Style::role("border", Style::kBorder));
        setStyleSheet(QStringLiteral(
            "QWidget { background: %1; border: 1px solid %2; border-radius: 3px; }")
            .arg(active ? QStringLiteral("#16161c")
                        : Style::role("panel", Style::kPanelBg), border));
        m_top->setStyleSheet(QStringLiteral(
            "QLabel { border: none; color: %1; font-size: 9px; font-weight: bold; }")
            .arg(alert ? QString::fromLatin1(Style::kTxRed) : accent.name()));
        m_bottom->setStyleSheet(QStringLiteral(
            "QLabel { border: none; color: %1; font-size: 11px; }")
            .arg(Style::role("text", Style::kTextPrimary)));
    }

    void setOnClick(std::function<void()> fn) { m_onClick = std::move(fn); }

protected:
    void mousePressEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton && m_onClick) { m_onClick(); }
        QWidget::mousePressEvent(ev);
    }

private:
    QLabel* m_top{nullptr};
    QLabel* m_bottom{nullptr};
    std::function<void()> m_onClick;
};

namespace {
QString mhz(double hz)
{
    return QStringLiteral("%1").arg(hz / 1.0e6, 0, 'f', 3);
}
} // namespace

VfoTileRow::VfoTileRow(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(4);
    rebuild();

    if (m_model) {
        // Zu- und Abgang bauen um, alles andere frischt nur auf. Ein
        // Umbau bei jeder Frequenzaenderung naehme dem Betreiber die
        // Kachel unter dem Mauszeiger weg, waehrend er darauf zielt.
        connect(m_model, &RadioModel::sliceAdded,
                this, [this](int) { rebuild(); });
        connect(m_model, &RadioModel::sliceRemoved,
                this, [this](int) { rebuild(); });
        connect(m_model, &RadioModel::activeSliceChanged,
                this, [this](int) { refresh(); });
    }
}

void VfoTileRow::setKiwiOn(bool on)
{
    if (m_kiwiOn == on) { return; }
    m_kiwiOn = on;
    refresh();
}

void VfoTileRow::rebuild()
{
    for (const QMetaObject::Connection& c : m_conns) { disconnect(c); }
    m_conns.clear();

    m_sliceTiles.clear();
    m_kiwiTile = nullptr;

    auto* lay = qobject_cast<QHBoxLayout*>(layout());
    if (!lay) { return; }
    while (QLayoutItem* it = lay->takeAt(0)) {
        if (QWidget* w = it->widget()) { w->deleteLater(); }
        delete it;
    }

    if (!m_model) { return; }
    const QList<SliceModel*> slices = m_model->slices();
    for (int i = 0; i < slices.size(); ++i) {
        SliceModel* s = slices.at(i);
        if (!s) { continue; }
        VfoTile* tile = buildSliceTile(s, i);
        m_sliceTiles.append(tile);
        lay->addWidget(tile);
        // Jede Scheibe frischt ihre eigene Kachel auf. Gesammelt in
        // einer Liste, damit rebuild() sie wieder loesen kann —
        // sonst haengen nach dem dritten Umbau drei Verbindungen an
        // derselben Scheibe.
        m_conns.append(connect(s, &SliceModel::frequencyChanged,
                               this, [this](double) { refresh(); }));
    }
    m_kiwiTile = buildKiwiTile();
    lay->addWidget(m_kiwiTile);
    lay->addStretch(1);
    refresh();
}

VfoTile* VfoTileRow::buildSliceTile(SliceModel* slice, int index)
{
    auto* t = new VfoTile(this);
    t->setProperty("sliceIndex", slice->sliceIndex());
    t->setOnClick([this, index]() {
        if (m_model) { m_model->setActiveSlice(index); }
    });
    return t;
}

VfoTile* VfoTileRow::buildKiwiTile()
{
    auto* t = new VfoTile(this);
    t->setProperty("kiwiTile", true);
    t->setOnClick([this]() { emit kiwiToggleRequested(); });
    return t;
}

void VfoTileRow::refresh()
{
    if (!m_model) { return; }
    const SliceModel* active = m_model->activeSlice();

    if (m_kiwiTile) {
        m_kiwiTile->setLines(QStringLiteral("KIWI"),
                             m_kiwiOn ? QStringLiteral("AN")
                                      : QStringLiteral("AUS"));
        m_kiwiTile->setLook(
            QColor(m_kiwiOn ? QStringLiteral("#4caf6a")
                            : Style::role("text-scale", Style::kTextScale)),
            m_kiwiOn, false);
    }

    for (VfoTile* t : m_sliceTiles) {
        if (!t) { continue; }
        const int idx = t->property("sliceIndex").toInt();
        SliceModel* s = m_model->sliceById(idx);
        if (!s) { continue; }

        // Die sendende Scheibe traegt "TX". Bis der Sendeschiedsrichter
        // etwas anderes sagt, ist das die aktive — geraten wird hier
        // nichts, es steht am Modell.
        const bool isActive = (s == active);
        const bool isTx = isActive && m_model->transmitModel().isMox();
        const QString head = QStringLiteral("%1%2")
                                 .arg(idx + 1)
                                 .arg(isTx ? QStringLiteral("  TX") : QString());
        t->setLines(head,
                    QStringLiteral("%1  %2")
                        .arg(mhz(s->frequency()),
                             bandLabel(bandFromFrequency(s->frequency()))));
        t->setLook(sliceColor(s->sliceIndex()), isActive, isTx);
    }
}

} // namespace Longpath
