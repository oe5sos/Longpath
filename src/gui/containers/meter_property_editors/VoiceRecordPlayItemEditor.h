#pragma once
#include "ButtonBoxItemEditor.h"

namespace Longpath {
class VoiceRecordPlayItem;

class VoiceRecordPlayItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit VoiceRecordPlayItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildVoiceSpecific();
};

} // namespace Longpath
