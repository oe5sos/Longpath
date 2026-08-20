#pragma once
#include <QLabel>

namespace Longpath {

class NyiOverlay : public QLabel {
    Q_OBJECT
public:
    explicit NyiOverlay(const QString& phaseHint, QWidget* parent = nullptr);
    void attachTo(QWidget* target);
    static NyiOverlay* markNyi(QWidget* target, const QString& phaseHint);
};

} // namespace Longpath
