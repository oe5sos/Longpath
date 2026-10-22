// =================================================================
// src/gui/RecordingScheduleDialog.cpp  (Longpath)
// =================================================================
//
// Modification history (Longpath):
//   2026-10-22 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "RecordingScheduleDialog.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QTimeZone>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "StyleConstants.h"
#include "core/audio/RecordingScheduler.h"
#include "models/RecordingModel.h"

namespace Longpath {

namespace {

QString zeileFuer(const ScheduledRecording& e)
{
    // UTC, wie im Dateinamen: eine Aufnahme ueber Mitternacht traegt
    // sonst zweimal denselben Namen -- und wer um 03:00 eine Bake
    // mitschneidet, denkt ohnehin in UTC.
    const QString art = (e.kind == ScheduledRecordingKind::Iq)
                            ? QStringLiteral("I/Q")
                            : QStringLiteral("Audio");
    const QString dauer = e.durationMinutes > 0
                              ? QStringLiteral("%1 min").arg(e.durationMinutes)
                              : QStringLiteral("open end");
    return QStringLiteral("%1Z   %2   %3")
        .arg(e.startAtUtc.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm")),
             art, dauer);
}

}  // namespace

RecordingScheduleDialog::RecordingScheduleDialog(RecordingModel* model,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(tr("Recording schedule"));
    setModal(false);
    buildUI();
    refresh();
}

void RecordingScheduleDialog::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    m_liste = new QListWidget(this);
    m_liste->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_liste->setMinimumHeight(140);
    m_liste->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; color: %2; border: 1px solid %3; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    root->addWidget(m_liste, 1);

    auto* form = new QFormLayout();
    form->setSpacing(6);

    m_start = new QDateTimeEdit(this);
    m_start->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_start->setTimeZone(QTimeZone::UTC);
    m_start->setDateTime(QDateTime::currentDateTimeUtc().addSecs(600));
    m_start->setStyleSheet(Style::lineEditStyle());
    form->addRow(tr("Start (UTC):"), m_start);

    m_dauer = new QSpinBox(this);
    m_dauer->setRange(0, 24 * 60);
    m_dauer->setValue(10);
    m_dauer->setSuffix(tr(" min"));
    m_dauer->setSpecialValueText(tr("open end"));
    m_dauer->setToolTip(tr("0 means: run until you stop it yourself."));
    m_dauer->setStyleSheet(Style::spinBoxStyle());
    form->addRow(tr("Duration:"), m_dauer);

    m_art = new QComboBox(this);
    m_art->addItem(tr("Audio"));
    m_art->addItem(tr("I/Q"));
    m_art->setToolTip(tr("I/Q needs a direct radio — a receiver fed by "
                         "another device delivers finished audio."));
    form->addRow(tr("Kind:"), m_art);

    root->addLayout(form);

    auto* reihe = new QHBoxLayout();
    m_addBtn = new QPushButton(tr("Add"), this);
    m_addBtn->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connect(m_addBtn, &QPushButton::clicked,
            this, &RecordingScheduleDialog::onAdd);
    reihe->addWidget(m_addBtn);

    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connect(m_removeBtn, &QPushButton::clicked,
            this, &RecordingScheduleDialog::onRemove);
    reihe->addWidget(m_removeBtn);
    reihe->addStretch(1);

    auto* zu = new QPushButton(tr("Close"), this);
    zu->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connect(zu, &QPushButton::clicked, this, &QDialog::accept);
    reihe->addWidget(zu);
    root->addLayout(reihe);

    m_hinweis = new QLabel(this);
    m_hinweis->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                 .arg(QLatin1String(Style::kTextInactive)));
    m_hinweis->setWordWrap(true);
    root->addWidget(m_hinweis);
}

void RecordingScheduleDialog::refresh()
{
    if (m_model == nullptr || m_liste == nullptr) {
        return;
    }
    m_liste->clear();
    const auto eintraege = m_model->schedule().entries();
    for (const ScheduledRecording& e : eintraege) {
        auto* item = new QListWidgetItem(zeileFuer(e), m_liste);
        // Die Kennung haengt am Eintrag, nicht an der Zeilennummer:
        // sortiert der Unterbau einmal um, loescht man sonst den
        // falschen.
        item->setData(Qt::UserRole, e.id);
    }

    // Ohne Radio feuert der Zeitplan nicht -- das gehoert gesagt,
    // solange man Eintraege anlegt, und nicht erst, wenn um 03:00
    // nichts passiert ist.
    m_hinweis->setText(m_model->ready()
                           ? tr("%1 entries. The schedule is armed.")
                                 .arg(eintraege.size())
                           : tr("%1 entries — but no radio is attached, so "
                                "nothing will fire.")
                                 .arg(eintraege.size()));
}

void RecordingScheduleDialog::onAdd()
{
    if (m_model == nullptr) {
        return;
    }
    ScheduledRecording e;
    e.startAtUtc      = m_start->dateTime().toUTC();
    e.durationMinutes = m_dauer->value();
    e.kind = (m_art->currentIndex() == 1) ? ScheduledRecordingKind::Iq
                                          : ScheduledRecordingKind::Audio;
    e.path    = QString();   // leer heisst: suggestedPath() beim Feuern
    e.enabled = true;

    m_model->schedule().add(e);
    m_model->schedule().saveToSettings();
    refresh();
}

void RecordingScheduleDialog::onRemove()
{
    if (m_model == nullptr || m_liste == nullptr) {
        return;
    }
    QListWidgetItem* item = m_liste->currentItem();
    if (item == nullptr) {
        return;
    }
    const int id = item->data(Qt::UserRole).toInt();
    m_model->schedule().remove(id);
    m_model->schedule().saveToSettings();
    refresh();
}

}  // namespace Longpath
