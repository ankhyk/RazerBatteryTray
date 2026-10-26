#include "Plugin.h"
#include "RazerMonitor.h"

#include <windows.h>

RazerBatteryPlugin& RazerBatteryPlugin::Instance() {
    static RazerBatteryPlugin instance;
    return instance;
}

RazerBatteryPlugin::RazerBatteryPlugin()
    : mouse_item_(DeviceSlot::Mouse),
      headset_item_(DeviceSlot::Headset),
      combined_item_(DeviceSlot::Combined) {}

IPluginItem* RazerBatteryPlugin::GetItem(int index) {
    switch (index) {
    case 0:
        return &mouse_item_;
    case 1:
        return &headset_item_;
    case 2:
        return &combined_item_;
    default:
        return nullptr;
    }
}

void RazerBatteryPlugin::DataRequired() {
    RazerMonitor::Instance().Poll(false);
    RefreshItems();
    MaybeNotify();
}

const wchar_t* RazerBatteryPlugin::GetInfo(PluginInfoIndex index) {
    switch (index) {
    case TMI_NAME:
        return IsChinese() ? L"Razer 电量" : L"Razer Battery";
    case TMI_DESCRIPTION:
        return IsChinese()
                   ? L"在任务栏显示 Razer 鼠标和耳机的电量与充电状态"
                   : L"Show Razer mouse and headset battery on the taskbar";
    case TMI_AUTHOR:
        return L"ankhyk";
    case TMI_COPYRIGHT:
        return L"Copyright (C) 2026 ankhyk";
    case TMI_VERSION:
        return L"1.0.0";
    case TMI_URL:
        return L"https://github.com/ankhyk/RazerBatteryTray";
    default:
        return L"";
    }
}

const wchar_t* RazerBatteryPlugin::GetTooltipInfo() {
    tooltip_ = RazerMonitor::Instance().Tooltip(IsChinese());
    return tooltip_.c_str();
}

void RazerBatteryPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) {
    switch (index) {
    case EI_CONFIG_DIR:
        if (data != nullptr) {
            RazerMonitor::Instance().LoadConfig(data);
        }
        break;
    case EI_VALUE_TEXT_COLOR:
        if (data != nullptr) {
            value_color_ = static_cast<COLORREF>(_wtol(data));
        }
        break;
    case EI_LABEL_TEXT_COLOR:
        if (data != nullptr) {
            label_color_ = static_cast<COLORREF>(_wtol(data));
        }
        break;
    default:
        break;
    }
}

void RazerBatteryPlugin::OnInitialize(ITrafficMonitor* pApp) {
    app_ = pApp;
    if (app_ != nullptr && app_->GetPluginConfigDir() != nullptr) {
        RazerMonitor::Instance().LoadConfig(app_->GetPluginConfigDir());
    }
    RazerMonitor::Instance().Poll(true);
    RefreshItems();
}

int RazerBatteryPlugin::GetCommandCount() {
    return 1;
}

const wchar_t* RazerBatteryPlugin::GetCommandName(int command_index) {
    if (command_index != 0) {
        return nullptr;
    }
    return IsChinese() ? L"立即刷新电量" : L"Refresh battery now";
}

void RazerBatteryPlugin::OnPluginCommand(int command_index, void* /*hWnd*/, void* /*para*/) {
    if (command_index != 0) {
        return;
    }
    RazerMonitor::Instance().Poll(true);
    RefreshItems();
    MaybeNotify();
}

bool RazerBatteryPlugin::IsChinese() const {
    LANGID lang = 0;
    if (app_ != nullptr) {
        lang = app_->GetLanguageId();
    }
    if (lang == 0) {
        lang = GetUserDefaultUILanguage();
    }
    return PRIMARYLANGID(lang) == LANG_CHINESE;
}

void RazerBatteryPlugin::RefreshItems() {
    const bool chinese = IsChinese();
    mouse_item_.RefreshTexts(chinese);
    headset_item_.RefreshTexts(chinese);
    combined_item_.RefreshTexts(chinese);
}

void RazerBatteryPlugin::MaybeNotify() {
    if (app_ == nullptr) {
        return;
    }
    std::wstring message;
    if (RazerMonitor::Instance().ConsumeLowBatteryNotify(message, IsChinese())) {
        app_->ShowNotifyMessage(message.c_str());
    }
}

ITMPlugin* TMPluginGetInstance() {
    return &RazerBatteryPlugin::Instance();
}
