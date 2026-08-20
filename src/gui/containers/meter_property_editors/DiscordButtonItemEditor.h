#pragma once
#include "ButtonBoxItemEditor.h"

namespace Longpath {
class DiscordButtonItem;

class DiscordButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit DiscordButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildDiscordSpecific();
};

} // namespace Longpath
