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

    QString appletId()    const override { return QStringLiteral("kiwiWaterfalls"); }
    QString appletTitle() const override { return tr("KIWI WATERFALLS"); }
    // Der Inhalt kommt vom KiwiSdrManager, nicht aus dem RadioModel.
    void syncFromModel() override {}

signals:
    // Wuensche der Streifen, weitergereicht (das Panel entscheidet nichts).
    void removeRequested(const QString& profileId);
    void renameRequested(const QString& profileId);

public:
    KiwiWaterfallStripWidget* stripForTest(const QString& id) const { return m_strips.value(id, nullptr); }

public slots:
    void setStripEnabled(const QString& profileId, bool enabled,
                         const QString& displayName);
    void pushRow(const QString& profileId, const QVector<float>& binsDbm,
                 double lowHz = 0.0, double highHz = 0.0);
    void resetStrip(const QString& profileId);

private:
    void updateEmptyState();

    QLabel* m_emptyLabel{nullptr};
    QVBoxLayout* m_stripsLayout{nullptr};
    QHash<QString, KiwiWaterfallStripWidget*> m_strips;
};

} // namespace Longpath
