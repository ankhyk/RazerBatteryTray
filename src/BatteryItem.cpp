#include "BatteryItem.h"
#include "IconDraw.h"
#include "Plugin.h"

#include <windows.h>

namespace {

constexpr int kGap96 = 3;
constexpr int kSpacer96 = 8;
constexpr int kIcon96 = 16;

int IconSizeFromDc(HDC hdc, int height) {
    TEXTMETRICW tm{};
    GetTextMetricsW(hdc, &tm);
    int icon = tm.tmHeight;
    if (icon < 12) {
        icon = 12;
    }
    if (height > 0 && icon > height) {
        icon = height;
    }
    return icon;
}

struct ValueMetrics {
    int percent_box = 0;
    int plus = 0;
    int total() const { return percent_box + plus; }
};

ValueMetrics MeasureValue(HDC hdc) {
    SIZE percent{};
    SIZE plus{};
    GetTextExtentPoint32W(hdc, L"100%", 4, &percent);
    GetTextExtentPoint32W(hdc, L"+", 1, &plus);
    return {percent.cx, plus.cx};
}

int MeasurePair(HDC hdc, int icon, int gap) {
    return icon + gap + MeasureValue(hdc).total();
}

std::wstring PercentCore(const BatteryReading& reading) {
    if (!reading.online || reading.percent < 0) {
        return L"--";
    }
    wchar_t buf[16];
    swprintf_s(buf, L"%d%%", reading.percent);
    return buf;
}

int DrawPair(HDC hdc, int x, int y, int right, int height, int icon, int gap, DeviceSlot slot,
             COLORREF color) {
    const BatteryReading reading =
        slot == DeviceSlot::Mouse ? RazerMonitor::Instance().Mouse() : RazerMonitor::Instance().Headset();
    const BYTE alpha = reading.online ? 255 : 96;
    const int icon_y = y + (height - icon) / 2;
    IconDraw::DrawDeviceIcon(hdc, slot, x, icon_y, icon, color, alpha);

    const ValueMetrics metrics = MeasureValue(hdc);
    const int value_left = x + icon + gap;
    int value_right = right;
    if (value_right < value_left + metrics.total()) {
        value_right = value_left + metrics.total();
    }
    const int percent_right = value_right - metrics.plus;

    SetTextColor(hdc, color);
    RECT percent_rect{value_left, y, percent_right, y + height};
    const std::wstring core = PercentCore(reading);
    DrawTextW(hdc, core.c_str(), -1, &percent_rect,
              DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX | DT_NOCLIP);

    if (reading.online && reading.charging) {
        RECT plus_rect{percent_right, y, value_right, y + height};
        DrawTextW(hdc, L"+", 1, &plus_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_NOCLIP);
    }

    return value_right - x;
}

}  // namespace

BatteryItem::BatteryItem(DeviceSlot slot) : slot_(slot) {
    RefreshTexts(true);
}

const wchar_t* BatteryItem::GetItemName() const {
    return name_.c_str();
}

const wchar_t* BatteryItem::GetItemId() const {
    switch (slot_) {
    case DeviceSlot::Mouse:
        return L"RazerMouseBatt";
    case DeviceSlot::Headset:
        return L"RazerHeadsetBatt";
    case DeviceSlot::Combined:
    default:
        return L"RazerComboBatt";
    }
}

const wchar_t* BatteryItem::GetItemLableText() const {
    return L"";
}

const wchar_t* BatteryItem::GetItemValueText() const {
    return value_.c_str();
}

const wchar_t* BatteryItem::GetItemValueSampleText() const {
    return sample_.c_str();
}

bool BatteryItem::IsCustomDraw() const {
    return true;
}

int BatteryItem::GetItemWidth() const {
    if (slot_ == DeviceSlot::Combined) {
        return kIcon96 + kGap96 + 40 + kSpacer96 + kIcon96 + kGap96 + 40;
    }
    return kIcon96 + kGap96 + 40;
}

int BatteryItem::GetItemWidthEx(void* hDC) const {
    HDC hdc = static_cast<HDC>(hDC);
    if (hdc == nullptr) {
        return 0;
    }
    const int icon = IconSizeFromDc(hdc, 0);
    const int pair = MeasurePair(hdc, icon, kGap96);
    if (slot_ == DeviceSlot::Combined) {
        return pair + kSpacer96 + pair;
    }
    return pair;
}

void BatteryItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) {
    HDC hdc = static_cast<HDC>(hDC);
    if (hdc == nullptr) {
        return;
    }

    const int saved = SaveDC(hdc);
    SetBkMode(hdc, TRANSPARENT);

    COLORREF color = RazerBatteryPlugin::Instance().ValueColor();
    if (color == 0) {
        color = dark_mode ? RGB(240, 240, 240) : RGB(32, 32, 32);
    }
    SetTextColor(hdc, color);

    const int icon = IconSizeFromDc(hdc, h);
    if (slot_ == DeviceSlot::Combined) {
        const int column = (w - kSpacer96) / 2;
        DrawPair(hdc, x, y, x + column, h, icon, kGap96, DeviceSlot::Mouse, color);
        DrawPair(hdc, x + column + kSpacer96, y, x + w, h, icon, kGap96, DeviceSlot::Headset, color);
    } else {
        DrawPair(hdc, x, y, x + w, h, icon, kGap96, slot_, color);
    }

    RestoreDC(hdc, saved);
}

int BatteryItem::IsDrawResourceUsageGraph() const {
    return 1;
}

float BatteryItem::GetResourceUsageGraphValue() const {
    return RazerMonitor::Instance().GraphValue(slot_);
}

void BatteryItem::RefreshTexts(bool chinese) {
    auto& monitor = RazerMonitor::Instance();
    name_ = monitor.ItemName(slot_, chinese);
    label_ = monitor.LabelText(slot_, chinese);
    value_ = monitor.ValueText(slot_);
    sample_ = monitor.SampleText(slot_);
}
