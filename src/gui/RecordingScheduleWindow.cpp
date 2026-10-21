// =================================================================
// src/gui/RecordingScheduleWindow.cpp  (Longpath)
// =================================================================
// Siehe RecordingScheduleWindow.h fuer das Warum.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include "RecordingScheduleWindow.h"

#include "StyleConstants.h"
#include "models/RecordingModel.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Longpath {

namespace {
// Spalten der Tabelle. Die Reihenfolge ist die des Entwurfsblattes.
enum Spalte { SpStart = 0, SpMin, SpArt, SpZustand, SpWeg, SpAnzahl };

void stupsPunktgroesse(QFont& f, double delta)
{
    // Dieselbe Wache wie in den Applets: kopflos laeuft Qt mit
    // Pixelgroessen, dort ist pointSizeF() == -1.
    if (f.pointSizeF() > 0.0) { f.setPointSizeF(f.pointSizeF() + delta); }
}
}  // namespace

RecordingScheduleWindow::RecordingScheduleWindow(RecordingModel* model,
                                                 QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_model(model)
{
    setWindowTitle(tr("Recording schedule"));
    setAttribute(Qt::WA_DeleteOnClose);
    buildUI();

    if (m_model) {
        connect(m_model, &RecordingModel::scheduleCountChanged,
                this, [this](int) { refresh(); });
        connect(m_model, &RecordingModel::audioRecordingChanged,
                this, [this](bool) { refresh(); });
        connect(m_model, &RecordingModel::iqRecordingChanged,
                this, [this](bool) { refresh(); });
    }
    refresh();
}

void RecordingScheduleWindow::buildUI()
{
    // Ohne das traegt das Fenster Qts Standardanstrich: allein gebaut
    // hell, in der App fast unsichtbar (Tabellenkopf und Eingabefelder
    // verschwanden im Schwarz). Die Marken sind dieselben, die
    // ConnectionPanel fuer seine Geraeteliste benutzt -- eine zweite
    // Tabellenschreibweise waere eine zuviel.
    setStyleSheet(QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QTableWidget {"
        "  background: %3; color: %2;"
        "  border: 1px solid %4;"
        "  gridline-color: %5;"
        "}"
        "QTableWidget::item { padding: 4px 6px; border: none; }"
        "QHeaderView::section {"
        "  background: %1; color: %6;"
        "  border: none;"
        "  border-right: 1px solid %4;"
        "  border-bottom: 1px solid %4;"
        "  padding: 4px 6px; font-size: 11px;"
        "}"
        "QDateTimeEdit, QSpinBox, QComboBox {"
        "  background: %3; color: %2;"
        "  border: 1px solid %4; border-radius: 6px;"
        "  padding: 3px 6px;"
        "}"
        "QPushButton {"
        "  background: %7; color: %6;"
        "  border: 1px solid %4; border-radius: 6px;"
        "  padding: 4px 10px;"
        "}"
        "QPushButton:hover { background: %8; }"
        "QPushButton:checked {"
        "  background: %9; color: %10; border: 1px solid %11;"
        "}"
        // Das Loeschkreuz sitzt in einer schmalen Spalte. Mit der
        // Polsterung oben (10 px je Seite) blieb vom Zeichen nichts
        // uebrig -- im Blatt standen leere Kaestchen.
        "QPushButton#zeitplanWeg {"
        "  background: transparent; border: none;"
        "  padding: 0px; color: %6; font-size: 16px;"
        "}"
        "QPushButton#zeitplanWeg:hover { color: %2; }")
        .arg(QLatin1String(Style::kPanelBg),
             QLatin1String(Style::kTextPrimary),
             QLatin1String(Style::kInsetBg),
             QLatin1String(Style::kBorder),
             QLatin1String(Style::kBorderSubtle),
             QLatin1String(Style::kTextScale),
             QLatin1String(Style::kButtonBg),
             QLatin1String(Style::kButtonHover),
             QLatin1String(Style::kBlueBg),
             QLatin1String(Style::kBlueText),
             QLatin1String(Style::kBlueBorder)));

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(8);

    m_table = new QTableWidget(0, SpAnzahl, this);
    m_table->setHorizontalHeaderLabels(
        {tr("START (UTC)"), tr("MIN"), tr("KIND"), QString(), QString()});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(SpStart, 150);
    m_table->setColumnWidth(SpMin, 54);
    m_table->setColumnWidth(SpArt, 70);
    m_table->setColumnWidth(SpZustand, 76);
    m_table->setColumnWidth(SpWeg, 34);
    m_table->setMinimumHeight(140);
    vbox->addWidget(m_table, 1);

    // ── Die Zeile zum Anlegen ────────────────────────────────────────
    auto* row = new QHBoxLayout;
    row->setSpacing(6);

    m_start = new QDateTimeEdit(this);
    m_start->setTimeSpec(Qt::UTC);
    // Eine Vorgabe, die man nicht erst wegklicken muss: die naechste
    // volle Stunde. Eine Startzeit in der Vergangenheit wuerde beim
    // naechsten Takt sofort ausloesen.
    QDateTime naechste = QDateTime::currentDateTimeUtc().addSecs(3600);
    naechste.setTime(QTime(naechste.time().hour(), 0));
    m_start->setDateTime(naechste);
    m_start->setDisplayFormat(QStringLiteral("dd.MM. HH:mm"));
    m_start->setMinimumWidth(130);
    row->addWidget(m_start);

    m_minutes = new QSpinBox(this);
    m_minutes->setRange(0, 24 * 60);
    m_minutes->setValue(30);
    m_minutes->setSpecialValueText(QStringLiteral("—"));  // 0 = kein Ende
    m_minutes->setToolTip(tr("Minutes. „—\" means no automatic stop."));
    m_minutes->setMinimumWidth(62);
    row->addWidget(m_minutes);

    m_kind = new QComboBox(this);
    m_kind->addItem(tr("Audio"));
    m_kind->addItem(tr("I/Q"));
    m_kind->setMinimumWidth(78);
    row->addWidget(m_kind);

    row->addStretch(1);

    m_add = new QPushButton(tr("Add"), this);
    m_add->setMinimumWidth(68);
    connect(m_add, &QPushButton::clicked,
            this, &RecordingScheduleWindow::onAdd);
    row->addWidget(m_add);

    vbox->addLayout(row);

    m_note = new QLabel(
        tr("Times are UTC — a recording across midnight keeps one name."),
        this);
    QFont nf = m_note->font();
    stupsPunktgroesse(nf, -2.0);
    m_note->setFont(nf);
    m_note->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                              .arg(QLatin1String(Style::kTextInactive)));
    vbox->addWidget(m_note);

    resize(440, 300);
}

void RecordingScheduleWindow::onAdd()
{
    if (!m_model) { return; }

    ScheduledRecording e;
    e.startAtUtc      = m_start->dateTime();
    e.startAtUtc.setTimeSpec(Qt::UTC);
    e.durationMinutes = m_minutes->value();
    e.kind            = m_kind->currentIndex() == 1
                            ? ScheduledRecordingKind::Iq
                            : ScheduledRecordingKind::Audio;
    // Leerer Pfad heisst „waehle selbst einen" -- RecordingModel baut
    // ihn dann aus Zeit, Band und Betriebsart, genau wie beim Druck auf
    // den Knopf. Ein zweiter Weg waere ein zweiter Ort, an dem der Name
    // anders aussieht.
    e.path    = QString();
    e.enabled = true;

    m_model->schedule().add(e);
    refresh();
}

void RecordingScheduleWindow::onRemove(int id)
{
    if (!m_model) { return; }
    m_model->schedule().remove(id);
    refresh();
}

void RecordingScheduleWindow::onToggle(int id, bool on)
{
    if (!m_model) { return; }
    m_model->schedule().setEnabled(id, on);
    refresh();
}

void RecordingScheduleWindow::refresh()
{
    if (!m_model || !m_table) { return; }

    const std::vector<ScheduledRecording> liste =
        m_model->schedule().entries();
    m_table->setRowCount(static_cast<int>(liste.size()));

    for (int r = 0; r < static_cast<int>(liste.size()); ++r) {
        const ScheduledRecording& e = liste[static_cast<size_t>(r)];

        auto* start = new QTableWidgetItem(
            e.startAtUtc.toUTC().toString(QStringLiteral("dd.MM. HH:mm")));
        m_table->setItem(r, SpStart, start);

        // Regel 7: Unbekannt ist ein Strich. 0 Minuten heisst „kein
        // automatisches Ende", nicht „null Minuten lang".
        m_table->setItem(r, SpMin, new QTableWidgetItem(
            e.durationMinutes > 0
                ? QString::number(e.durationMinutes)
                : QStringLiteral("—")));

        m_table->setItem(r, SpArt, new QTableWidgetItem(
            e.kind == ScheduledRecordingKind::Iq ? tr("I/Q") : tr("Audio")));

        // Der Zustand als Schalter, nicht als Text: wer ihn liest, will
        // ihn meistens auch umlegen.
        auto* zustand = new QPushButton(
            e.enabled ? tr("ARMED") : tr("OFF"), m_table);
        zustand->setCheckable(true);
        zustand->setChecked(e.enabled);
        const int id = e.id;
        connect(zustand, &QPushButton::toggled, this,
                [this, id](bool on) { onToggle(id, on); });
        m_table->setCellWidget(r, SpZustand, zustand);

        auto* weg = new QPushButton(QStringLiteral("×"), m_table);



        weg->setObjectName(QStringLiteral("zeitplanWeg"));
        weg->setToolTip(tr("Remove this entry"));
        weg->setFlat(true);
        connect(weg, &QPushButton::clicked, this,
                [this, id]() { onRemove(id); });
        m_table->setCellWidget(r, SpWeg, weg);
    }
}

}  // namespace Longpath
