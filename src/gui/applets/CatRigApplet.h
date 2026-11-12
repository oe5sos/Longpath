// =================================================================
// src/gui/applets/CatRigApplet.h  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original.
//
// Bedienfeld fuer ein Funkgeraet hinter CAT (TS-590S): was das Geraet
// meldet, und was man dort aus Longpath verstellen darf. Gesendet wird
// am Geraet, darum gibt es hier keine Sendetaste; siehe core/Ts590Cat.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#pragma once
#include "AppletWidget.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

namespace Longpath {

class CatRigApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CatRigApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("CatRig"); }
    QString appletTitle() const override { return QStringLiteral("CAT Rig"); }
    void    syncFromModel() override;

private:
    void buildUI();

    QLabel*      m_status{nullptr};
    QLabel*      m_wege{nullptr};
    QPushButton* m_vorverstaerker{nullptr};
    QPushButton* m_daempfung{nullptr};
    QPushButton* m_nb{nullptr};
    QPushButton* m_nr{nullptr};
    QComboBox*   m_agc{nullptr};
    QPushButton* m_filter{nullptr};
    QPushButton* m_rit{nullptr};
    QPushButton* m_ritRunter{nullptr};
    QPushButton* m_ritHoch{nullptr};
    QPushButton* m_ritNull{nullptr};
    QLabel*      m_ritWert{nullptr};
    QPushButton* m_notch{nullptr};
    QSlider*     m_nbPegel{nullptr};
    QLabel*      m_nbPegelWert{nullptr};
    QSlider*     m_nrPegel{nullptr};
    QLabel*      m_nrPegelWert{nullptr};
    QSlider*     m_rfGain{nullptr};
    QLabel*      m_rfWert{nullptr};
    QLabel*      m_messwerte{nullptr};
};

} // namespace Longpath
