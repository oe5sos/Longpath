#pragma once
#include "ButtonBoxItemEditor.h"

namespace Longpath {
class TuneStepButtonItem;

class TuneStepButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit TuneStepButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTuneStepSpecific();
};

} // namespace Longpath
