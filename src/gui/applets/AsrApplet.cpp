// =================================================================
// src/gui/applets/AsrApplet.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung steht in der Kopfdatei.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/AsrApplet.h"

#include "asr/AsrService.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTime>
#include <QVBoxLayout>

namespace Longpath {

AsrApplet::AsrApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 4, 6, 6);
    root->setSpacing(4);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(8);

    m_enable = new QCheckBox(tr("Mitschreiben"), this);
    connect(m_enable, &QCheckBox::toggled, this, &AsrApplet::enableRequested);
    head->addWidget(m_enable);

    m_status = new QLabel(this);
    m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget(m_status, 1);
    root->addLayout(head);

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    // Kein Zeilenumbruch am Wort, sondern am Rand: eine Mitschrift ist
    // Fliesstext, kein Protokoll mit fester Spaltenbreite.
    m_text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    // Ein Deckel, sonst waechst das Feld ueber Stunden ins Unendliche.
    // Zweihundert Zeilen sind mehr, als jemand zurueckliest, und
    // kosten nichts.
    m_text->setMaximumBlockCount(200);
    m_text->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; border: 1px solid %2; "
        "border-radius: 3px; color: %3; font-size: 11px; padding: 4px; }")
        .arg(Style::role("inset-bg", Style::kInsetBg),
             Style::role("border", Style::kBorder),
             Style::role("text", Style::kTextPrimary)));
    m_text->setMinimumHeight(90);
    root->addWidget(m_text, 1);

    setStatus(tr("aus"), Style::role("text-scale", Style::kTextScale));
}

void AsrApplet::setService(AsrService* svc)
{
    if (m_service) { disconnect(m_service, nullptr, this, nullptr); }
    m_service = svc;
    if (!svc) { return; }

    connect(svc, &AsrService::transcript, this, &AsrApplet::appendLine);
    connect(svc, &AsrService::listeningChanged, this, [this](bool on) {
        if (!m_service || !m_service->isRunning()) { return; }
        // Gruen, solange gesprochen wird. Das ist die einzige
        // Rueckmeldung, die zeigt, dass der Abgriff wirklich Ton sieht
        // — ohne sie wuesste der Betreiber bei ausbleibendem Text
        // nicht, ob niemand spricht oder nichts ankommt.
        setStatus(on ? tr("hört") : tr("wartet"),
                  on ? QStringLiteral("#4caf6a")
                     : Style::role("text-scale", Style::kTextScale));
    });
    connect(svc, &AsrService::failed, this, [this](const QString& reason) {
        setStatus(tr("Fehler"), QStringLiteral("#c85a5a"));
        // Der Grund gehoert in den Text, nicht nur in eine Farbe. Wer
        // "Fehler" liest und nicht weiss, welchen, ist nicht besser
        // dran als vorher.
        m_text->appendPlainText(tr("— %1").arg(reason));
    });
}

QString AsrApplet::text() const
{
    return m_text ? m_text->toPlainText() : QString();
}

void AsrApplet::appendLine(const QString& text, float confidence)
{
    if (!m_text || text.trimmed().isEmpty()) { return; }
    // Uhrzeit davor: eine Mitschrift ohne Zeitbezug laesst sich nicht
    // mit dem Logbuch zusammenbringen.
    const QString stamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    m_text->appendPlainText(QStringLiteral("%1  %2   (%3)")
                                .arg(stamp, text.trimmed())
                                .arg(confidence, 0, 'f', 2));
    m_text->verticalScrollBar()->setValue(
        m_text->verticalScrollBar()->maximum());
}

void AsrApplet::setStatus(const QString& s, const QString& colour)
{
    if (!m_status) { return; }
    m_status->setText(s);
    m_status->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(colour));
}

} // namespace Longpath
