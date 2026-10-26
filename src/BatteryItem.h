#pragma once

#include "PluginInterface.h"
#include "RazerMonitor.h"

#include <string>

class BatteryItem : public IPluginItem {
public:
    explicit BatteryItem(DeviceSlot slot);

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    bool IsCustomDraw() const override;
    int GetItemWidth() const override;
    int GetItemWidthEx(void* hDC) const override;
    void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
    int IsDrawResourceUsageGraph() const override;
    float GetResourceUsageGraphValue() const override;

    void RefreshTexts(bool chinese);

private:
    DeviceSlot slot_;
    std::wstring name_;
    std::wstring label_;
    std::wstring value_;
    std::wstring sample_;
};
