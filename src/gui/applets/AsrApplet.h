#pragma once

// =================================================================
// src/gui/applets/AsrApplet.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Was hier steht und was nicht ────────────────────────────────────
//
// Der mitgeschriebene Text der laufenden Verbindung. Ein Feld, ein
// Schalter, ein Zustandspunkt — mehr nicht.
//
// Bewusst KEINE Vertrauensfaerbung je Wort, wie sie AetherSDR hat: die
// setzt voraus, dass man den Zahlen des Erkenners glaubt. Bei einem
// oertlichen Whisper-Dienst haengen sie an Modell und Einstellung, und
// eine Farbe, die Zuverlaessigkeit behauptet, ohne sie zu haben, ist
// schlechter als keine. Die Zuversicht steht als Zahl an der Zeile;
// wer sie deuten will, kann es.
//
// Bewusst KEINE Sprechertrennung: die braucht Einbettungsmodelle ueber
// onnxruntime, und die haben wir nicht. Siehe AsrService.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/AppletWidget.h"

#include <QPointer>

class QCheckBox;
class QLabel;
class QPlainTextEdit;

namespace Longpath {

class AsrService;

class AsrApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit AsrApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("asr"); }
    QString appletTitle() const override { return QStringLiteral("MITSCHRIFT"); }
    void    syncFromModel() override {}

    /// Der Dienst, dessen Text hier landet. Nicht im Besitz.
    void setService(AsrService* svc);

    /// Fuer Pruefungen: was gerade im Feld steht.
    QString text() const;

signals:
    /// Der Betreiber will die Erkennung an- oder abschalten. Das
    /// Applet schaltet NICHT selbst — MainWindow kennt den Abgriff und
    /// die Scheibe, dieses Feld kennt nur Buchstaben.
    void enableRequested(bool on);

private:
    void appendLine(const QString& text, float confidence);
    void setStatus(const QString& s, const QString& colour);

    QPointer<AsrService> m_service;
    QPlainTextEdit* m_text{nullptr};
    QCheckBox*      m_enable{nullptr};
    QLabel*         m_status{nullptr};
};

} // namespace Longpath
