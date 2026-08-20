#pragma once
#include "ButtonBoxItemEditor.h"

namespace Longpath {
class ModeButtonItem;

class ModeButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit ModeButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildModeSpecific();
};

} // namespace Longpath
