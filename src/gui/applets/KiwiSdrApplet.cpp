// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/gui/KiwiSdrApplet.cpp [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Modification history (Longpath):
//   2026-08-23 — Portiert (Stufe 4). Die Abweichungen und ihre
//                Begruendung stehen in der Kopfdatei.

#include "gui/applets/KiwiSdrApplet.h"
#include "gui/Zahlentext.h"

#include "core/KiwiSdrManager.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/SliceColors.h"
#include "models/SliceModel.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QShowEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

namespace Longpath {

namespace {

// Aether loest seine Stilvorlagen ueber ThemeManager::resolve mit
// {{color.text.primary}} auf. Bei uns tut Style::role dasselbe, nur
// mit anderen Schluesseln — die Zuordnung steht hier an einer Stelle,
// damit sie nicht in jeder Vorlage einzeln nachzulesen ist.
QString ink()   { return Style::role("text",        Style::kTextPrimary); }
QString label() { return Style::role("text-scale",  Style::kTextScale);   }
QString panel() { return Style::role("panel",       Style::kPanelBg);     }
QString border(){ return Style::role("border",      Style::kBorder);      }

QString listStyle()
{
    return QStringLiteral(
        "QListWidget { background: transparent; border: none; "
        "color: %1; font-size: 11px; }"
        "QListWidget::item { padding: 0; margin: 0; }").arg(ink());
}

QString emptyStyle()
{
    return QStringLiteral(
        "QLabel { color: %1; font-size: 11px; padding: 4px 2px; }").arg(label());
}

QString labelStyle()
{
    return QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(label());
}

QString primaryLabelStyle()
{
    return QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold; }").arg(ink());
}

QString receiverRowStyle()
{
    return QStringLiteral(
        "QWidget#kiwiReceiverRow { background: %1; "
        "border: 1px solid %2; border-radius: 6px; }")
        .arg(panel(), border());
}

// Aethers stateToken() liefert einen Themen-Schluessel; bei uns ist es
// direkt die Farbe. Die Einteilung — gruen laeuft, gelb wartet, rot
// klemmt, grau steht — ist zeichengetreu uebernommen.
QString stateColor(KiwiSdrClient::State state)
{
    switch (state) {
    case KiwiSdrClient::State::Connected:
    case KiwiSdrClient::State::Camping:
        return Style::role("live", Style::kLiveGreen);
    case KiwiSdrClient::State::Connecting:
    case KiwiSdrClient::State::Waiting:
    case KiwiSdrClient::State::Busy:
        return Style::role("busy", Style::kBusyAmber);
    case KiwiSdrClient::State::Error:
    case KiwiSdrClient::State::CampDisconnected:
        return Style::role("danger", Style::kTxRed);
    case KiwiSdrClient::State::Disconnected:
        return label();
    }
    return label();
}

QString statusStyle(KiwiSdrClient::State state)
{
    return QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold; }")
        .arg(stateColor(state));
}

QString stateText(KiwiSdrClient::State state)
{
    switch (state) {
    case KiwiSdrClient::State::Disconnected:
        return QObject::tr("Getrennt");
    case KiwiSdrClient::State::Connecting:
        return QObject::tr("Verbindet");
    case KiwiSdrClient::State::Connected:
        return QObject::tr("Verbunden");
    case KiwiSdrClient::State::Busy:
        return QObject::tr("Belegt");
    case KiwiSdrClient::State::Waiting:
        return QObject::tr("Wartet");
    case KiwiSdrClient::State::Camping:
        return QObject::tr("Beobachtet");
    case KiwiSdrClient::State::CampDisconnected:
        return QObject::tr("Watching stopped");
    case KiwiSdrClient::State::Error:
        return QObject::tr("Error");
    }
    return QObject::tr("Getrennt");
}

QString detailStyle()
{
    return QStringLiteral("QLabel { color: %1; font-size: 9px; }").arg(label());
}

// Aether: SliceColorManager::instance().hexActive(idx). Bei uns steht
// dieselbe Tabelle in sliceColor() (gui/widgets/SliceColors.h) — eine
// Farbe je Scheibe, geteilt mit Panadapter-Marke und RxApplet-Reiter.
QString sliceBadgeStyle(int colorIdx, bool assigned)
{
    const QString color = sliceColor(colorIdx).name();
    if (assigned) {
        return QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %1; "
            "border-radius: 6px; font-weight: bold; font-size: 9px; padding: 0; }")
            .arg(color);
    }
    return QStringLiteral(
        "QLabel { background: %2; color: %1; border: 1px solid %1; "
        "border-radius: 6px; font-weight: bold; font-size: 9px; padding: 0; }")
        .arg(color, QLatin1String(Style::kButtonHover));
}

QString sliceLetterOf(SliceModel* slice)
{
    // Aether nimmt SliceLabel::unicodeForm(sliceId, letter). Bei uns ist
    // der Buchstabe direkt am Modell und aus sliceIndex abgeleitet; der
    // Rueckfall auf 'A'+Index ist derselbe, den SliceModel.h selbst
    // vorgibt.
    const QChar l = slice->sliceLetter();
    return l.isNull() ? QString(QChar('A' + slice->sliceIndex())) : QString(l);
}

QString receiverAccessibleText(const KiwiSdrReceiverStatus& receiver)
{
    QStringList parts;
    parts << receiver.name
          << stateText(receiver.state);
    if (!receiver.detail.trimmed().isEmpty()) {
        parts << receiver.detail.trimmed();
    }
    if (!receiver.metadataSummary.trimmed().isEmpty()) {
        parts << receiver.metadataSummary.trimmed();
    }
    if (!receiver.protocolSummary.trimmed().isEmpty()) {
        parts << receiver.protocolSummary.trimmed();
    }
    if (receiver.assignedSlice) {
        parts << QObject::tr("assigned to slice %1")
                     .arg(sliceLetterOf(receiver.assignedSlice.data()));
    } else {
        parts << QObject::tr("not assigned to a slice");
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace

KiwiSdrApplet::KiwiSdrApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 4);
    root->setSpacing(4);

    // ── Mein Signal am Kiwi ──────────────────────────────────────────
    auto* ownRow = new QHBoxLayout;
    ownRow->setContentsMargins(0, 0, 0, 0);
    ownRow->setSpacing(4);
    m_ownSignalButton = blueToggle(tr("My Signal"), -1, 22);
    m_ownSignalButton->setCheckable(true);
    m_ownSignalButton->setObjectName(QStringLiteral("kiwiOwnSignalButton"));
    m_ownSignalButton->setAccessibleName(
        tr("Hear my signal on the Kiwi"));
    m_ownSignalButton->setToolTip(
        tr("A Kiwi listens along: waterfall below the panadapter, "
           "mark on the transmit frequency, audio stays on while "
           "transmitting."));
    m_recordButton = blueToggle(tr("Record"), -1, 22);
    m_recordButton->setCheckable(true);
    m_recordButton->setObjectName(QStringLiteral("kiwiRecordButton"));
    m_recordButton->setAccessibleName(tr("Record the Kiwi audio"));
    m_otherButton = blueToggle(tr("Other Kiwi …"), 112, 22);
    m_otherButton->setObjectName(QStringLiteral("kiwiOtherButton"));
    // Eine Schaltflaeche, kein Schalter: styledButton() macht jeden Knopf
    // einrastbar, dann blieb die Pille nach dem Klick dunkel stehen
    // (Betreiber 2026-11-25: "kiwi pille wurde schwarz").
    m_otherButton->setCheckable(false);
    m_otherButton->setAccessibleName(tr("Choose another Kiwi"));
    ownRow->addWidget(m_ownSignalButton, 1);
    ownRow->addWidget(m_recordButton, 1);
    root->addLayout(ownRow);
    // Zweite Zeile: "Anderen Kiwi ..." mit dem gewaehlten Namen daneben --
    // in einer Zeile mit den zwei Schaltern wurde der Text abgeschnitten.
    auto* otherRow = new QHBoxLayout;
    otherRow->setContentsMargins(0, 0, 0, 0);
    otherRow->setSpacing(6);
    otherRow->addWidget(m_otherButton, 0);
    m_hearingButton = blueToggle(tr("Heard …"), 64, 22);
    m_hearingButton->setObjectName(QStringLiteral("kiwiHearingButton"));
    m_hearingButton->setCheckable(false);   // Schaltflaeche, rastet nicht ein
    m_hearingButton->setAccessibleName(tr("Where am I heard"));
    m_hearingButton->setToolTip(
        tr("Up to five Kiwis listen along while you transmit: map, "
           "waterfalls and who hears you."));
    otherRow->addWidget(m_hearingButton, 0);
    m_soloButton = blueToggle(tr("Solo"), 44, 22);
    m_soloButton->setCheckable(true);
    m_soloButton->setObjectName(QStringLiteral("kiwiSoloButton"));
    m_soloButton->setAccessibleName(tr("Hear only the Kiwi"));
    m_soloButton->setToolTip(
        tr("Mutes the radio's receive audio in the speakers so you hear "
           "only what the Kiwi hears. Switches itself off after 30 seconds."));
    otherRow->addWidget(m_soloButton, 0);
    otherRow->addStretch(1);
    root->addLayout(otherRow);
    // Der gewaehlte Name steht in einer EIGENEN Zeile: neben den Knoepfen
    // schob er sie aus dem Applet, die "Gehoert"-Pille verschwand
    // (Betreiber 2026-11-25).
    m_ownSignalLabel = new QLabel(this);
    m_ownSignalLabel->setTextFormat(Qt::PlainText);
    m_ownSignalLabel->setStyleSheet(detailStyle());
    m_ownSignalLabel->setVisible(false);
    m_ownSignalLabel->setWordWrap(false);
    m_ownSignalLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    root->addWidget(m_ownSignalLabel);
    connect(m_ownSignalButton, &QPushButton::toggled, this,
            &KiwiSdrApplet::ownSignalToggled);
    connect(m_recordButton, &QPushButton::toggled, this,
            &KiwiSdrApplet::recordToggled);
    connect(m_otherButton, &QPushButton::clicked, this,
            &KiwiSdrApplet::otherReceiverRequested);
    connect(m_soloButton, &QPushButton::toggled, this, &KiwiSdrApplet::soloToggled);
    connect(m_hearingButton, &QPushButton::clicked, this,
            &KiwiSdrApplet::hearingRequested);

    m_emptyLabel = new QLabel(tr("No KiwiSDR receive antenna configured"), this);
    m_emptyLabel->setAccessibleName(tr("KiwiSDR receiver status"));
    m_emptyLabel->setStyleSheet(emptyStyle());
    m_emptyLabel->setWordWrap(true);
    root->addWidget(m_emptyLabel);

    m_receiverList = new QListWidget(this);
    m_receiverList->setAccessibleName(tr("KiwiSDR receiver status"));
    m_receiverList->setAccessibleDescription(
        tr("Shows the configured KiwiSDR receive antennas, their "
            "connection state and the slice they are assigned to."));
    m_receiverList->setFocusPolicy(Qt::NoFocus);
    m_receiverList->setSelectionMode(QAbstractItemView::NoSelection);
    m_receiverList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_receiverList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Waechst mit dem Platz, den die Kachel / das Fenster hergibt (war: fest
    // 176 px, also drei Zeilen). Weniger als drei Zeilen nie.
    m_receiverList->setMinimumHeight(48);
    m_receiverList->setStyleSheet(listStyle());
    root->addWidget(m_receiverList);

    rebuildReceiverList();
}

void KiwiSdrApplet::setReceivers(const QVector<KiwiSdrReceiverStatus>& receivers)
{
    for (const QMetaObject::Connection& connection : m_sliceConnections) {
        disconnect(connection);
    }
    m_sliceConnections.clear();
    m_receivers = receivers;

    for (const KiwiSdrReceiverStatus& receiver : m_receivers) {
        SliceModel* slice = receiver.assignedSlice.data();
        if (!slice) {
            continue;
        }
        // Aether haengt hier zusaetzlich an letterChanged. Bei uns ist
        // der Buchstabe CONSTANT (aus sliceIndex abgeleitet), es gibt
        // also nichts zu horchen.
        m_sliceConnections.append(connect(slice, &SliceModel::frequencyChanged,
                                          this, &KiwiSdrApplet::rebuildReceiverList));
        m_sliceConnections.append(connect(slice, &SliceModel::dspModeChanged,
                                          this, &KiwiSdrApplet::rebuildReceiverList));
        m_sliceConnections.append(connect(slice, &SliceModel::filterChanged,
                                          this, &KiwiSdrApplet::rebuildReceiverList));
        m_sliceConnections.append(connect(slice, &SliceModel::panKeyChanged,
                                          this, &KiwiSdrApplet::rebuildReceiverList));
    }

    rebuildReceiverList();
}

void KiwiSdrApplet::setKiwiSdrManager(KiwiSdrManager* manager)
{
    m_kiwiSdrManager = manager;
}

void KiwiSdrApplet::setOwnSignalActive(bool active)
{
    const QSignalBlocker blocker(m_ownSignalButton);
    m_ownSignalButton->setChecked(active);
}

void KiwiSdrApplet::setRecordingActive(bool active)
{
    const QSignalBlocker blocker(m_recordButton);
    m_recordButton->setChecked(active);
}

void KiwiSdrApplet::setSoloActive(bool active)
{
    const QSignalBlocker blocker(m_soloButton);
    m_soloButton->setChecked(active);
}

void KiwiSdrApplet::setOwnSignalReceiverName(const QString& name)
{
    m_ownSignalLabel->setText(tr("My signal: %1").arg(name));
    m_ownSignalLabel->setVisible(!name.isEmpty());
}

void KiwiSdrApplet::rebuildReceiverList()
{
    if (!m_receiverList || !m_emptyLabel) {
        return;
    }

    m_receiverList->clear();
    const bool hasReceivers = !m_receivers.isEmpty();
    m_emptyLabel->setVisible(!hasReceivers);
    m_receiverList->setVisible(hasReceivers);

    for (const KiwiSdrReceiverStatus& receiver : m_receivers) {
        auto* item = new QListWidgetItem(m_receiverList);
        item->setData(Qt::UserRole, receiver.id);
        QWidget* row = buildReceiverRow(receiver);
        item->setSizeHint(row->sizeHint());
        m_receiverList->setItemWidget(item, row);
    }
    updateListHeight();
}

void KiwiSdrApplet::updateListHeight()
{
    if (!m_receiverList) {
        return;
    }
    constexpr int kMinHeight = 48;
    constexpr int kOldFixedCap = 176;   // die frueheren drei Zeilen
    constexpr int kChrome = 36;         // Kachel-Titel und Rand
    int content = 2 * m_receiverList->frameWidth();
    for (int i = 0; i < m_receiverList->count(); ++i) {
        content += m_receiverList->sizeHintForRow(i);
    }
    // Der Platz: die Applets stehen in einer Spalte, die scrollt (AppletGrid
    // in einer QScrollArea, auch im schwebenden Fenster). Gemessen wird darum
    // der sichtbare Ausschnitt dieser Spalte, nicht das Fenster -- das Fenster
    // ist hoeher als jede Spalte, und die Kachel selbst ist nur so hoch wie
    // ihr Inhalt (Bild 2026-11-25: drei Zeilen und leerer Platz darunter).
    int room = kOldFixedCap;
    QWidget* area = scrollViewport();
    // Beim Umhaengen des Applets (Andocken/Abloesen) ist die Liste kurz kein
    // Nachkomme -- mapTo() warnt dann; in dem Moment gibt es nichts zu messen.
    if (!isAncestorOf(m_receiverList)) {
        return;
    }
    if (area) {
        const int above = m_receiverList->mapTo(this, QPoint(0, 0)).y();
        room = area->height() - above - kChrome;
    } else if (const QWidget* top = window()) {
        room = top->height() - m_receiverList->mapTo(top, QPoint(0, 0)).y() - 12;
    }
    const int cap = qMax(kOldFixedCap, room);
    const int h = qBound(kMinHeight, content, cap);
    if (m_receiverList->minimumHeight() != h || m_receiverList->maximumHeight() != h) {
        m_receiverList->setFixedHeight(h);
        updateGeometry();
    }
}

QWidget* KiwiSdrApplet::scrollViewport() const
{
    for (QWidget* w = parentWidget(); w; w = w->parentWidget()) {
        if (auto* sa = qobject_cast<QAbstractScrollArea*>(w)) {
            return sa->viewport();
        }
    }
    return nullptr;
}

void KiwiSdrApplet::showEvent(QShowEvent* event)
{
    AppletWidget::showEvent(event);
    QWidget* top = scrollViewport();
    if (!top) { top = window(); }
    if (top && top != m_watchedWindow) {
        if (m_watchedWindow) {
            m_watchedWindow->removeEventFilter(this);
        }
        m_watchedWindow = top;
        top->installEventFilter(this);
    }
    updateListHeight();
}

bool KiwiSdrApplet::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_watchedWindow && event->type() == QEvent::Resize) {
        updateListHeight();
    }
    return AppletWidget::eventFilter(watched, event);
}

QWidget* KiwiSdrApplet::buildReceiverRow(const KiwiSdrReceiverStatus& receiver)
{
    auto* row = new QWidget(m_receiverList);
    row->setObjectName(QStringLiteral("kiwiReceiverRow"));
    row->setStyleSheet(receiverRowStyle());
    row->setAccessibleName(receiverAccessibleText(receiver));

    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(5, 4, 5, 4);
    layout->setSpacing(3);

    auto* topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(4);

    auto* name = new QLabel(receiver.name, row);
    name->setTextFormat(Qt::PlainText);
    name->setAccessibleName(tr("Name of the KiwiSDR receiver"));
    name->setAccessibleDescription(receiver.name);
    name->setStyleSheet(primaryLabelStyle());
    name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topRow->addWidget(name, 1);

    auto* status = new QLabel(stateText(receiver.state), row);
    status->setTextFormat(Qt::PlainText);
    status->setAccessibleName(tr("State of the KiwiSDR receiver"));
    status->setAccessibleDescription(receiverAccessibleText(receiver));
    status->setStyleSheet(statusStyle(receiver.state));
    status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    topRow->addWidget(status, 0, Qt::AlignRight);

    // Wasserfall-Vorschau im eigenstaendigen KIWI-WASSERFAELLE-Panel —
    // unabhaengig davon, ob dieses Profil einer Scheibe zugeordnet ist.
    auto* waterfallToggle = blueToggle(tr("WF"), 32, 20);
    waterfallToggle->setCheckable(true);
    waterfallToggle->setAccessibleName(
        tr("Waterfall preview for %1").arg(receiver.name));
    if (m_kiwiSdrManager) {
        waterfallToggle->setChecked(
            m_kiwiSdrManager->waterfallPreviewEnabled(receiver.id));
    }
    const QString receiverId = receiver.id;
    connect(waterfallToggle, &QPushButton::toggled, this,
            [this, receiverId](bool checked) {
        if (m_kiwiSdrManager) {
            m_kiwiSdrManager->setWaterfallPreviewEnabled(receiverId, checked);
        }
    });
    topRow->addWidget(waterfallToggle, 0, Qt::AlignRight);

    layout->addLayout(topRow);

    QStringList detailLines;
    if (!receiver.detail.trimmed().isEmpty()) {
        detailLines << receiver.detail.trimmed();
    }
    if (!receiver.metadataSummary.trimmed().isEmpty()) {
        detailLines << receiver.metadataSummary.trimmed();
    }
    if (!receiver.protocolSummary.trimmed().isEmpty()) {
        detailLines << receiver.protocolSummary.trimmed();
    }
    if (!detailLines.isEmpty()) {
        const QString detail = detailLines.join(QLatin1Char('\n'));
        auto* detailLabel = new QLabel(detail, row);
        detailLabel->setTextFormat(Qt::PlainText);
        detailLabel->setAccessibleName(tr("KiwiSDR receiver details"));
        detailLabel->setAccessibleDescription(detail);
        detailLabel->setStyleSheet(detailStyle());
        detailLabel->setWordWrap(true);
        layout->addWidget(detailLabel);
    }

    layout->addWidget(buildSliceAssignmentRow(receiver.assignedSlice.data(), row));
    return row;
}

QWidget* KiwiSdrApplet::buildSliceAssignmentRow(SliceModel* slice, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    if (!slice) {
        auto* text = new QLabel(tr("Not assigned"), row);
        text->setTextFormat(Qt::PlainText);
        text->setStyleSheet(labelStyle());
        layout->addWidget(text, 1);
        return row;
    }

    auto* badge = new QLabel(row);
    badge->setFixedSize(18, 19);
    badge->setAlignment(Qt::AlignCenter);
    badge->setTextFormat(Qt::PlainText);
    badge->setText(sliceLetterOf(slice));
    badge->setStyleSheet(sliceBadgeStyle(slice->sliceIndex(), true));
    layout->addWidget(badge, 0, Qt::AlignVCenter);

    auto* text = new QLabel(sliceText(slice), row);
    text->setTextFormat(Qt::PlainText);
    text->setStyleSheet(labelStyle());
    text->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    text->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(text, 1);
    return row;
}

QString KiwiSdrApplet::sliceText(SliceModel* slice) const
{
    if (!slice) {
        return QString();
    }

    const QString filter = QStringLiteral("%1..%2 Hz")
        .arg(slice->filterLow())
        .arg(slice->filterHigh());
    // Aether haelt den Modus als Zeichenkette; bei uns steht ein
    // DSPMode am Modell, und SliceModel::modeName macht daraus den
    // Namen, der auch sonst ueberall in der Oberflaeche steht.
    return QStringLiteral("%1 MHz  %2  %3")
        .arg(anzeigeZahl(slice->frequency() / 1.0e6, 6))
        .arg(SliceModel::modeName(slice->dspMode()))
        .arg(filter);
}

} // namespace Longpath
