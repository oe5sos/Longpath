// no-port-check: Longpath-original UI file. Kein Thetis-Code.
// =================================================================
// src/gui/setup/AsrPage.h  (Longpath)
// =================================================================
//
// Einstellungen fuer die Spracherkennung (Mitschrift).
//
// Bis hierher standen Adresse, Sprache und Modell des Whisper-Dienstes
// nur in der XML-Datei unter ~/Library/Preferences. Diese Seite macht
// sie sichtbar -- samt einer Erreichbarkeitsprobe, denn der haeufigste
// Fehlerfall ist schlicht ein nicht laufender whisper-server, und der
// meldet sich als "Connection refused" nur im Protokoll.
// =================================================================
#pragma once

#include "gui/SetupPage.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class QNetworkAccessManager;
class QNetworkReply;

namespace Longpath {

class AsrPage : public SetupPage {
    Q_OBJECT
public:
    explicit AsrPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();
    void buildServerGroup();
    void buildRecognitionGroup();
    void buildHintGroup();

    void probeEndpoint();
    void showProbeResult(const QString& text, const QString& colour);

    QLineEdit* m_urlEdit      = nullptr;
    QLineEdit* m_keyEdit      = nullptr;
    QLineEdit* m_languageEdit = nullptr;
    QLineEdit* m_modelEdit    = nullptr;
    QPushButton* m_probeButton = nullptr;
    QLabel*    m_probeLabel   = nullptr;

    QNetworkAccessManager* m_net = nullptr;
    QNetworkReply*         m_probe = nullptr;
};

} // namespace Longpath
