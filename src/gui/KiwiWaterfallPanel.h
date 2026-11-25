// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe docs/architecture/
// 2026-08-27-kiwisdr-self-report-concept.md, Variante C.
//
//   2026-08-27 — Neu angelegt.

#pragma once

#include "gui/applets/AppletWidget.h"

#include <QHash>
#include <QString>
#include <QVector>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace Longpath {

class KiwiWaterfallStripWidget;

// "KIWI-WASSERFAELLE" — eigenes, andockbares Panel: fuer jedes Profil,
// dessen Wasserfall-Vorschau der Betreiber im KIWISDR-Applet
// eingeschaltet hat, ein KiwiWaterfallStripWidget. Die Schalter selbst
// sitzen im KiwiSdrApplet (Empfaengerliste); dieses Panel ist nur die
// Anzeige, komplett unabhaengig davon, ob ein Profil einer Scheibe
// zugeordnet ist.
class KiwiWaterfallPanel : public AppletWidget {
    Q_OBJECT

public:
    explicit KiwiWaterfallPanel(RadioModel* model, QWidget* parent = nullptr);
    bool isEmptyForTest() const { return m_strips.isEmpty(); }
    QSize sizeHint() const override { return QSize(560, 460); }

    QString appletId()    const override { return QStringLiteral("kiwiWaterfalls"); }
    QString appletTitle() const override { return tr("KIWI WATERFALLS"); }
    // Der Inhalt kommt vom KiwiSdrManager, nicht aus dem RadioModel.
    void syncFromModel() override {}

signals:
    void emptyChanged(bool empty);   ///< ohne Streifen: das Fenster wird nicht gebraucht
    // Wuensche der Streifen, weitergereicht (das Panel entscheidet nichts).
    void removeRequested(const QString& profileId);
    void renameRequested(const QString& profileId);
    void soundToggled(const QString& profileId, bool on);
    void zoomRequested(const QString& profileId, int direction);

public:
    KiwiWaterfallStripWidget* stripForTest(const QString& id) const { return m_strips.value(id, nullptr); }

public slots:
    void setStripEnabled(const QString& profileId, bool enabled,
                         const QString& displayName);
    void pushRow(const QString& profileId, const QVector<float>& binsDbm,
                 double lowHz = 0.0, double highHz = 0.0);
    void resetStrip(const QString& profileId);
    void setNotice(const QString& profileId, const QString& text);
    void setSoundActive(const QString& profileId, bool on);

private:
    void updateEmptyState();

    QLabel* m_emptyLabel{nullptr};
    QVBoxLayout* m_stripsLayout{nullptr};
    QHash<QString, KiwiWaterfallStripWidget*> m_strips;
private:
    bool m_wasEmpty{true};
    QString m_expandedId;
};

} // namespace Longpath
