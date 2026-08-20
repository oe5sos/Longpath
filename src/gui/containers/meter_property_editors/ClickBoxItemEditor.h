#pragma once
#include "BaseItemEditor.h"

namespace Longpath {
class ClickBoxItem;

// ClickBoxItem is an invisible hit-target with no configurable visual
// properties — it has only the common MeterItem fields (position, binding,
// z-order). This editor therefore adds no type-specific rows.
class ClickBoxItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit ClickBoxItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;
};

} // namespace Longpath
