#pragma once
#include "ButtonBoxItemEditor.h"

namespace Longpath {
class BandButtonItem;

class BandButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit BandButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildBandSpecific();
};

} // namespace Longpath
