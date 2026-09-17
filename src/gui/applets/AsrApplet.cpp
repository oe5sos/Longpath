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
#include "asr/WhisperServerLauncher.h"

#include "asr/AsrService.h"
#include "gui/StyleConstants.h"

#include <QPushButton>
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

    // Ein Schalter, kein Haekchen (Glas & Tiefe, 2026-09-17): dieselbe
    // leise einrastende Bauform wie die Zustandsschalter im TX-Feld.
    m_enable = styledButton(tr("Mitschreiben"), 110, 26);
    m_enable->setStyleSheet(m_enable->styleSheet() + Style::quietCheckedStyle());
    connect(m_enable, &QPushButton::toggled, this, &AsrApplet::enableRequested);
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
    // Versenkt wie ein Zahlenfeld: Glasfeld-Stil (dunkle Kante oben,
    // Licht unten, Radius 7, nie 3), Schrift per setFont.
    m_text->setStyleSheet(Style::glassFieldStyle()
                          + QStringLiteral("QPlainTextEdit { background: %1; border: 1px solid %2;"
                                           " border-top-color: %3; border-bottom-color: %4;"
                                           " border-radius: %5px; color: %6; padding: 4px; }")
                                .arg(Style::role("inset-bg", Style::kInsetBg),
                                     Style::role("border", Style::kBorder),
                                     QLatin1String(Style::kGlassShade),
                                     QLatin1String(Style::kGlassLight))
                                .arg(Style::kGlassChipRadius)
                                .arg(Style::role("text", Style::kTextPrimary)));
    m_text->setFont([this] { QFont f = font(); f.setPixelSize(Style::kFontSmall); return f; }());
    m_text->setMinimumHeight(90);
    root->addWidget(m_text, 1);

    setStatus(tr("aus"), Style::role("text-scale", Style::kTextScale));

    // Der Dienst, den Longpath selbst startet: sein Zustand steht hier
    // mit, weil der Bediener sonst nur "Fehler" saehe und nicht, dass
    // das Modell fehlt oder das Programm nicht da ist.
    auto& launcher = WhisperServerLauncher::instance();
    connect(&launcher, &WhisperServerLauncher::stateChanged, this,
            [this](WhisperServerLauncher::State st, const QString& reason) {
        using S = WhisperServerLauncher::State;
        switch (st) {
        case S::Starting:
            setStatus(tr("Dienst startet …"), Style::role("text-scale", Style::kTextScale));
            break;
        case S::Running:
            setStatus(tr("Dienst läuft"), QString::fromLatin1(Style::kGreenText));
            break;
        case S::External:
            setStatus(tr("Dienst läuft (extern)"), QString::fromLatin1(Style::kGreenText));
            break;
        case S::Failed:
            setStatus(tr("Dienst: Fehler"), QString::fromLatin1(Style::kTxRed));
            if (m_text) { m_text->appendPlainText(tr("— Dienst: %1").arg(reason)); }
            break;
        case S::Stopped:
            break;
        }
    });
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
                  on ? QString::fromLatin1(Style::kGreenText)
                     : Style::role("text-scale", Style::kTextScale));
    });
    connect(svc, &AsrService::failed, this, [this](const QString& reason) {
        setStatus(tr("Fehler"), QString::fromLatin1(Style::kTxRed));
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
        QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(colour));
}

} // namespace Longpath
