#include "RazerMonitor.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

namespace {

constexpr USHORT kRazerVid = 0x1532;
constexpr USHORT kPidViperWired = 0x00A5;
constexpr USHORT kPidViperWireless = 0x00A6;
constexpr USHORT kPidBlackShark = 0x0528;

constexpr int kRazerReportSize = 90;
constexpr int kMouseFeatureLen = 91;
constexpr int kHeadsetFeatureLen = 64;
constexpr int kHeadsetBatteryIndex = 14;
constexpr int kHeadsetChargeIndex = 2;

#pragma pack(push, 1)
struct RazerReport {
    uint8_t status;
    uint8_t transaction_id;
    uint16_t remaining_packets;
    uint8_t protocol_type;
    uint8_t data_size;
    uint8_t command_class;
    uint8_t command_id;
    uint8_t arguments[80];
    uint8_t crc;
    uint8_t reserved;
};
#pragma pack(pop)
static_assert(sizeof(RazerReport) == kRazerReportSize, "RazerReport must be 90 bytes");

uint8_t RazerCrc(const RazerReport& report) {
    const auto* p = reinterpret_cast<const uint8_t*>(&report);
    uint8_t crc = 0;
    for (int i = 2; i < 88; ++i) {
        crc ^= p[i];
    }
    return crc;
}

RazerReport MakeGet(uint8_t cls, uint8_t cmd, uint8_t txn) {
    RazerReport report{};
    report.transaction_id = txn;
    report.data_size = 0x02;
    report.command_class = cls;
    report.command_id = cmd;
    report.crc = RazerCrc(report);
    return report;
}

HANDLE OpenHid(const std::wstring& path) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        return handle;
    }
    return CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       nullptr, OPEN_EXISTING, 0, nullptr);
}

bool SendRazerCommand(HANDLE handle, uint8_t txn, uint8_t cls, uint8_t cmd, RazerReport* response) {
    uint8_t buf[kMouseFeatureLen]{};
    RazerReport request = MakeGet(cls, cmd, txn);
    buf[0] = 0x00;
    memcpy(buf + 1, &request, kRazerReportSize);
    if (!HidD_SetFeature(handle, buf, kMouseFeatureLen)) {
        return false;
    }
    Sleep(40);
    memset(buf, 0, sizeof(buf));
    if (!HidD_GetFeature(handle, buf, kMouseFeatureLen)) {
        return false;
    }
    memcpy(response, buf + 1, kRazerReportSize);
    return response->status == 0x02;
}

struct HidInfo {
    std::wstring path;
    USHORT pid = 0;
    USHORT usage_page = 0;
    USHORT usage = 0;
    USHORT feature_len = 0;
    std::wstring product;
};

std::vector<HidInfo> EnumRazerHid() {
    std::vector<HidInfo> devices;
    GUID hid_guid{};
    HidD_GetHidGuid(&hid_guid);
    HDEVINFO info = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) {
        return devices;
    }

    SP_DEVICE_INTERFACE_DATA ifdata{};
    ifdata.cbSize = sizeof(ifdata);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(info, nullptr, &hid_guid, i, &ifdata); ++i) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &ifdata, nullptr, 0, &need, nullptr);
        if (need == 0) {
            continue;
        }
        std::vector<uint8_t> mem(need);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(mem.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &ifdata, detail, need, nullptr, nullptr)) {
            continue;
        }

        HANDLE handle = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            continue;
        }

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        if (!HidD_GetAttributes(handle, &attr) || attr.VendorID != kRazerVid) {
            CloseHandle(handle);
            continue;
        }

        HidInfo item;
        item.path = detail->DevicePath;
        item.pid = attr.ProductID;
        wchar_t product[256]{};
        if (HidD_GetProductString(handle, product, sizeof(product))) {
            item.product = product;
        }

        PHIDP_PREPARSED_DATA parsed = nullptr;
        if (HidD_GetPreparsedData(handle, &parsed)) {
            HIDP_CAPS caps{};
            if (HidP_GetCaps(parsed, &caps) == HIDP_STATUS_SUCCESS) {
                item.usage_page = caps.UsagePage;
                item.usage = caps.Usage;
                item.feature_len = caps.FeatureReportByteLength;
            }
            HidD_FreePreparsedData(parsed);
        }
        CloseHandle(handle);
        devices.push_back(std::move(item));
    }

    SetupDiDestroyDeviceInfoList(info);
    return devices;
}

int HidRawToPercent(uint8_t raw) {
    return static_cast<int>(raw) * 100 / 255;
}

std::wstring FormatPercent(const BatteryReading& reading) {
    if (!reading.online || reading.percent < 0) {
        return L"--";
    }
    wchar_t buf[32];
    if (reading.charging) {
        swprintf_s(buf, L"%d%%+", reading.percent);
    } else {
        swprintf_s(buf, L"%d%%", reading.percent);
    }
    return buf;
}

bool ParseHexByte(const char* s, int* value) {
    unsigned int parsed = 0;
    if (sscanf_s(s, "%x", &parsed) != 1) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

std::wstring SynapseLogPath(uint16_t pid) {
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"C:\\ProgramData\\Razer\\Synapse3\\Service\\Bin\\Devices\\Mw\\Log\\RzCtl_%04X.log", pid);
    return path;
}

void WritePrivateProfileInt(const wchar_t* section, const wchar_t* key, int value, const wchar_t* file) {
    wchar_t buf[16];
    swprintf_s(buf, L"%d", value);
    WritePrivateProfileStringW(section, key, buf, file);
}

}  // namespace

RazerMonitor& RazerMonitor::Instance() {
    static RazerMonitor instance;
    return instance;
}

void RazerMonitor::LoadConfig(const std::wstring& config_dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_dir.empty()) {
        config_path_ = config_dir;
        if (config_path_.back() != L'\\' && config_path_.back() != L'/') {
            config_path_ += L'\\';
        }
        config_path_ += L"RazerBattery.ini";
    } else {
        config_path_ = L"RazerBattery.ini";
    }
    poll_interval_sec_ = GetPrivateProfileIntW(L"config", L"poll_interval_sec", 10, config_path_.c_str());
    low_battery_percent_ = GetPrivateProfileIntW(L"config", L"low_battery_percent", 20, config_path_.c_str());
    notify_low_battery_ = GetPrivateProfileIntW(L"config", L"notify", 1, config_path_.c_str());
    if (poll_interval_sec_ < 3) {
        poll_interval_sec_ = 3;
    }
    if (poll_interval_sec_ > 300) {
        poll_interval_sec_ = 300;
    }
    if (low_battery_percent_ < 1) {
        low_battery_percent_ = 1;
    }
    if (low_battery_percent_ > 90) {
        low_battery_percent_ = 90;
    }
}

void RazerMonitor::SaveConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_path_.empty()) {
        return;
    }
    WritePrivateProfileInt(L"config", L"poll_interval_sec", poll_interval_sec_, config_path_.c_str());
    WritePrivateProfileInt(L"config", L"low_battery_percent", low_battery_percent_, config_path_.c_str());
    WritePrivateProfileInt(L"config", L"notify", notify_low_battery_, config_path_.c_str());
}

void RazerMonitor::Poll(bool force) {
    const uint64_t now = GetTickCount64();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!force && last_poll_ms_ != 0 && now - last_poll_ms_ < static_cast<uint64_t>(poll_interval_sec_) * 1000ULL) {
            return;
        }
        last_poll_ms_ = now;
    }

    BatteryReading mouse;
    BatteryReading headset;
    const bool mouse_hid = PollMouseHid(mouse);
    if (!mouse_hid) {
        if (!PollSynapseLog(kPidViperWireless, mouse)) {
            PollSynapseLog(kPidViperWired, mouse);
        }
    }

    const bool headset_hid = PollHeadsetHid(headset);
    if (!headset_hid) {
        PollSynapseLog(kPidBlackShark, headset);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (notify_low_battery_) {
        if (mouse.online && mouse.percent >= 0 && mouse.percent < low_battery_percent_) {
            if (!mouse_was_low_) {
                mouse_notify_pending_ = true;
            }
            mouse_was_low_ = true;
        } else if (!mouse.online || mouse.percent >= low_battery_percent_ + 5) {
            mouse_was_low_ = false;
        }

        if (headset.online && headset.percent >= 0 && headset.percent < low_battery_percent_) {
            if (!headset_was_low_) {
                headset_notify_pending_ = true;
            }
            headset_was_low_ = true;
        } else if (!headset.online || headset.percent >= low_battery_percent_ + 5) {
            headset_was_low_ = false;
        }
    }

    mouse_ = std::move(mouse);
    headset_ = std::move(headset);
}

BatteryReading RazerMonitor::Mouse() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mouse_;
}

BatteryReading RazerMonitor::Headset() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return headset_;
}

std::wstring RazerMonitor::ValueText(DeviceSlot slot) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (slot == DeviceSlot::Combined) {
        return FormatPercent(mouse_) + L" / " + FormatPercent(headset_);
    }
    return FormatPercent(slot == DeviceSlot::Mouse ? mouse_ : headset_);
}

std::wstring RazerMonitor::SampleText(DeviceSlot slot) const {
    if (slot == DeviceSlot::Combined) {
        return L"100%+ / 100%+";
    }
    return L"100%+";
}

std::wstring RazerMonitor::LabelText(DeviceSlot slot, bool chinese) const {
    switch (slot) {
    case DeviceSlot::Mouse:
        return chinese ? L"鼠标" : L"Mouse";
    case DeviceSlot::Headset:
        return chinese ? L"耳机" : L"HS";
    case DeviceSlot::Combined:
    default:
        return L"Razer";
    }
}

std::wstring RazerMonitor::ItemName(DeviceSlot slot, bool chinese) const {
    switch (slot) {
    case DeviceSlot::Mouse:
        return chinese ? L"Razer 鼠标电量" : L"Razer mouse battery";
    case DeviceSlot::Headset:
        return chinese ? L"Razer 耳机电量" : L"Razer headset battery";
    case DeviceSlot::Combined:
    default:
        return chinese ? L"Razer 电量" : L"Razer battery";
    }
}

std::wstring RazerMonitor::Tooltip(bool chinese) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto line = [&](const BatteryReading& reading, const wchar_t* zh, const wchar_t* en) {
        std::wstring text = chinese ? zh : en;
        text += L": ";
        if (!reading.online || reading.percent < 0) {
            text += chinese ? L"未连接" : L"offline";
            return text;
        }
        text += FormatPercent(reading);
        if (reading.charging) {
            text += chinese ? L" 充电中" : L" charging";
        }
        if (!reading.name.empty()) {
            text += L"  (";
            text += reading.name;
            text += L")";
        }
        return text;
    };
    return line(mouse_, L"鼠标", L"Mouse") + L"\n" + line(headset_, L"耳机", L"Headset");
}

float RazerMonitor::GraphValue(DeviceSlot slot) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto clamp = [](const BatteryReading& reading) -> float {
        if (!reading.online || reading.percent < 0) {
            return 0.0f;
        }
        return (std::max)(0.0f, (std::min)(1.0f, reading.percent / 100.0f));
    };
    if (slot == DeviceSlot::Combined) {
        if (mouse_.online && headset_.online) {
            return (std::min)(clamp(mouse_), clamp(headset_));
        }
        if (mouse_.online) {
            return clamp(mouse_);
        }
        return clamp(headset_);
    }
    return clamp(slot == DeviceSlot::Mouse ? mouse_ : headset_);
}

bool RazerMonitor::ConsumeLowBatteryNotify(std::wstring& message, bool chinese) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mouse_notify_pending_) {
        mouse_notify_pending_ = false;
        wchar_t buf[128];
        swprintf_s(buf, chinese ? L"Razer 鼠标电量低：%d%%" : L"Razer mouse battery low: %d%%", mouse_.percent);
        message = buf;
        return true;
    }
    if (headset_notify_pending_) {
        headset_notify_pending_ = false;
        wchar_t buf[128];
        swprintf_s(buf, chinese ? L"Razer 耳机电量低：%d%%" : L"Razer headset battery low: %d%%", headset_.percent);
        message = buf;
        return true;
    }
    return false;
}

bool RazerMonitor::PollMouseHid(BatteryReading& out) {
    auto try_path = [&](const std::wstring& path, uint8_t txn) -> bool {
        HANDLE handle = OpenHid(path);
        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        RazerReport battery{};
        RazerReport charge{};
        const bool ok_batt = SendRazerCommand(handle, txn, 0x07, 0x80, &battery);
        const bool ok_chg = SendRazerCommand(handle, txn, 0x07, 0x84, &charge);
        CloseHandle(handle);
        if (!ok_batt) {
            return false;
        }
        out.online = true;
        out.percent = HidRawToPercent(battery.arguments[1]);
        out.charging = ok_chg && charge.arguments[1] != 0;
        out.source = L"HID";
        if (out.name.empty()) {
            out.name = L"Razer Viper V2 Pro";
        }
        mouse_txn_ = txn;
        mouse_path_ = path;
        return true;
    };

    uint8_t txns[2] = {mouse_txn_, static_cast<uint8_t>(mouse_txn_ == 0x1F ? 0x3F : 0x1F)};
    if (!mouse_path_.empty()) {
        for (uint8_t txn : txns) {
            if (try_path(mouse_path_, txn)) {
                return true;
            }
        }
        mouse_path_.clear();
    }

    const auto devices = EnumRazerHid();
    for (const auto& device : devices) {
        if (device.feature_len != kMouseFeatureLen) {
            continue;
        }
        if (device.pid != kPidViperWireless && device.pid != kPidViperWired) {
            continue;
        }
        for (uint8_t txn : txns) {
            if (try_path(device.path, txn)) {
                out.name = device.product.empty() ? L"Razer Viper V2 Pro" : device.product;
                out.pid = device.pid;
                return true;
            }
        }
    }
    return false;
}

bool RazerMonitor::PollHeadsetHid(BatteryReading& out) {
    auto try_path = [&](const std::wstring& path, const std::wstring& product, USHORT pid) -> bool {
        HANDLE handle = OpenHid(path);
        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        uint8_t buf[kHeadsetFeatureLen]{};
        buf[0] = 0xFF;
        const BOOL ok = HidD_GetFeature(handle, buf, kHeadsetFeatureLen);
        CloseHandle(handle);
        if (!ok) {
            return false;
        }
        const int percent = buf[kHeadsetBatteryIndex];
        if (percent > 100) {
            return false;
        }
        out.online = true;
        out.percent = percent;
        out.charging = buf[kHeadsetChargeIndex] != 0;
        out.source = L"HID";
        out.name = (product.empty() || product == L"Hid Interface") ? L"Razer BlackShark V2 Pro" : product;
        out.pid = pid;
        headset_path_ = path;
        return true;
    };

    if (!headset_path_.empty()) {
        if (try_path(headset_path_, L"Razer BlackShark V2 Pro", kPidBlackShark)) {
            return true;
        }
        headset_path_.clear();
    }

    const auto devices = EnumRazerHid();
    for (const auto& device : devices) {
        if (device.pid != kPidBlackShark || device.feature_len != kHeadsetFeatureLen) {
            continue;
        }
        if (try_path(device.path, device.product, device.pid)) {
            return true;
        }
    }
    return false;
}

bool RazerMonitor::PollSynapseLog(uint16_t pid, BatteryReading& out) {
    const std::wstring path = SynapseLogPath(pid);
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart <= 0) {
        CloseHandle(handle);
        return false;
    }

    constexpr DWORD kTail = 65536;
    DWORD to_read = kTail;
    if (size.QuadPart < kTail) {
        to_read = static_cast<DWORD>(size.QuadPart);
    } else {
        LARGE_INTEGER pos{};
        pos.QuadPart = size.QuadPart - kTail;
        SetFilePointerEx(handle, pos, nullptr, FILE_BEGIN);
    }

    std::vector<char> buf(to_read + 1);
    DWORD read = 0;
    if (!ReadFile(handle, buf.data(), to_read, &read, nullptr)) {
        CloseHandle(handle);
        return false;
    }
    CloseHandle(handle);
    buf[read] = 0;

    int percent = -1;
    int charging = -1;
    const char* p = buf.data();
    while (p && *p) {
        const char* batt = strstr(p, "Power_BatteryLevel");
        const char* chg = strstr(p, "Power_ChargingStatus");
        const char* next = nullptr;
        if (batt && chg) {
            next = batt < chg ? batt : chg;
        } else {
            next = batt ? batt : chg;
        }
        if (!next) {
            break;
        }
        const char* hex = strstr(next, "0x");
        const char* after = next + 1;
        if (hex) {
            int value = 0;
            if (ParseHexByte(hex + 2, &value)) {
                if (next == batt) {
                    percent = value;
                } else {
                    charging = value;
                }
            }
            after = hex + 2;
        }
        p = after;
    }

    if (percent < 0) {
        return false;
    }

    out.online = true;
    out.percent = percent;
    out.charging = charging > 0;
    out.source = L"Synapse";
    out.pid = pid;
    if (pid == kPidBlackShark) {
        out.name = L"Razer BlackShark V2 Pro";
    } else {
        out.name = L"Razer Viper V2 Pro";
    }
    return true;
}
