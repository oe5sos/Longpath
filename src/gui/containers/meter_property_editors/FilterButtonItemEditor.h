#pragma once
#include "ButtonBoxItemEditor.h"

namespace Longpath {
class FilterButtonItem;

class FilterButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit FilterButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildFilterSpecific();
};

} // namespace Longpath
