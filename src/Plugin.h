#pragma once

#include "PluginInterface.h"
#include "BatteryItem.h"

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class RazerBatteryPlugin : public ITMPlugin {
public:
    static RazerBatteryPlugin& Instance();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    void OnInitialize(ITrafficMonitor* pApp) override;
    int GetCommandCount() override;
    const wchar_t* GetCommandName(int command_index) override;
    void OnPluginCommand(int command_index, void* hWnd, void* para) override;

    bool IsChinese() const;
    COLORREF ValueColor() const { return value_color_; }
    COLORREF LabelColor() const { return label_color_; }

private:
    RazerBatteryPlugin();
    void RefreshItems();
    void MaybeNotify();

    BatteryItem mouse_item_;
    BatteryItem headset_item_;
    BatteryItem keyboard_item_;
    BatteryItem combined_item_;
    ITrafficMonitor* app_ = nullptr;
    COLORREF value_color_ = 0;
    COLORREF label_color_ = 0;
    std::wstring tooltip_;
    std::wstring info_;
};

#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif
