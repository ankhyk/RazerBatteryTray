#pragma once

#include <cstdint>
#include <mutex>
#include <string>

enum class DeviceSlot {
    Mouse,
    Headset,
    Keyboard,
    Combined
};

struct BatteryReading {
    bool online = false;
    int percent = -1;
    bool charging = false;
    std::wstring name;
    std::wstring source;
    uint16_t pid = 0;
};

class RazerMonitor {
public:
    static RazerMonitor& Instance();

    void Poll(bool force);
    BatteryReading Mouse() const;
    BatteryReading Headset() const;
    BatteryReading Keyboard() const;

    std::wstring ValueText(DeviceSlot slot) const;
    std::wstring SampleText(DeviceSlot slot) const;
    std::wstring LabelText(DeviceSlot slot, bool chinese) const;
    std::wstring ItemName(DeviceSlot slot, bool chinese) const;
    std::wstring Tooltip(bool chinese) const;
    float GraphValue(DeviceSlot slot) const;

    bool ConsumeLowBatteryNotify(std::wstring& message, bool chinese);

    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;

    int poll_interval_sec() const { return poll_interval_sec_; }
    int low_battery_percent() const { return low_battery_percent_; }

private:
    RazerMonitor() = default;

    bool PollMouseHid(BatteryReading& out);
    bool PollHeadsetHid(BatteryReading& out);
    bool PollKeyboardHid(BatteryReading& out);
    static bool PollSynapseLog(uint16_t pid, BatteryReading& out);

    mutable std::mutex mutex_;
    BatteryReading mouse_;
    BatteryReading headset_;
    BatteryReading keyboard_;
    std::wstring mouse_path_;
    std::wstring headset_path_;
    std::wstring keyboard_path_;
    uint8_t mouse_txn_ = 0x1F;
    uint64_t last_poll_ms_ = 0;
    bool mouse_was_low_ = false;
    bool headset_was_low_ = false;
    bool keyboard_was_low_ = false;
    bool mouse_notify_pending_ = false;
    bool headset_notify_pending_ = false;
    bool keyboard_notify_pending_ = false;

    int poll_interval_sec_ = 10;
    int low_battery_percent_ = 20;
    int notify_low_battery_ = 1;
    std::wstring config_path_;
};
