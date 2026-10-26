#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

static const USHORT kRazerVid = 0x1532;

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
static_assert(sizeof(RazerReport) == 90, "RazerReport must be 90 bytes");

static uint8_t RazerCrc(const RazerReport& report) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&report);
    uint8_t crc = 0;
    for (int i = 2; i < 88; ++i) crc ^= p[i];
    return crc;
}

static RazerReport MakeGet(uint8_t cls, uint8_t cmd, uint8_t size, uint8_t txn) {
    RazerReport r{};
    r.transaction_id = txn;
    r.data_size = size;
    r.command_class = cls;
    r.command_id = cmd;
    r.crc = RazerCrc(r);
    return r;
}

static void PrintHex(const uint8_t* p, int n, int maxn = 32) {
    int m = n < maxn ? n : maxn;
    for (int i = 0; i < m; ++i) printf("%02X ", p[i]);
    if (n > maxn) printf("...");
}

struct HidDev {
    std::wstring path;
    USHORT vid = 0, pid = 0, usage_page = 0, usage = 0;
    USHORT feature_len = 0, input_len = 0, output_len = 0;
    WCHAR product[256]{};
};

static std::vector<HidDev> EnumRazer() {
    std::vector<HidDev> out;
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);
    HDEVINFO info = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return out;

    SP_DEVICE_INTERFACE_DATA ifdata{};
    ifdata.cbSize = sizeof(ifdata);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(info, nullptr, &hid_guid, i, &ifdata); ++i) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &ifdata, nullptr, 0, &need, nullptr);
        if (need == 0) continue;
        std::vector<uint8_t> buf(need);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &ifdata, detail, need, nullptr, nullptr)) continue;

        HANDLE h = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        HIDP_CAPS caps{};
        PHIDP_PREPARSED_DATA parsed = nullptr;
        HidDev d;
        d.path = detail->DevicePath;
        if (HidD_GetAttributes(h, &attr)) {
            d.vid = attr.VendorID;
            d.pid = attr.ProductID;
        }
        if (d.vid != kRazerVid) {
            CloseHandle(h);
            continue;
        }
        HidD_GetProductString(h, d.product, sizeof(d.product));
        if (HidD_GetPreparsedData(h, &parsed)) {
            if (HidP_GetCaps(parsed, &caps) == HIDP_STATUS_SUCCESS) {
                d.usage_page = caps.UsagePage;
                d.usage = caps.Usage;
                d.feature_len = caps.FeatureReportByteLength;
                d.input_len = caps.InputReportByteLength;
                d.output_len = caps.OutputReportByteLength;
            }
            HidD_FreePreparsedData(parsed);
        }
        CloseHandle(h);
        out.push_back(std::move(d));
    }
    SetupDiDestroyDeviceInfoList(info);
    return out;
}

static HANDLE OpenHid(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) return h;
    h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    return h;
}

static bool FeatureSetGet(HANDLE h, uint8_t* buf, ULONG len) {
    if (!HidD_SetFeature(h, buf, len)) return false;
    Sleep(40);
    return HidD_GetFeature(h, buf, len) != FALSE;
}

static bool TryRazerCmd(HANDLE h, USHORT feature_len, uint8_t txn, uint8_t cls, uint8_t cmd, RazerReport* resp) {
    if (feature_len < 91) return false;
    uint8_t buf[256]{};
    ULONG len = feature_len;
    if (len > sizeof(buf)) len = sizeof(buf);
    RazerReport req = MakeGet(cls, cmd, 0x02, txn);
    buf[0] = 0x00;
    memcpy(buf + 1, &req, 90);
    if (!FeatureSetGet(h, buf, len)) return false;
    memcpy(resp, buf + 1, 90);
    return true;
}

int wmain() {
    SetConsoleOutputCP(65001);
    auto devs = EnumRazer();
    printf("Found %zu Razer HID interfaces\n\n", devs.size());
    uint8_t txns[] = {0x1F, 0x3F, 0xFF};
    for (size_t i = 0; i < devs.size(); ++i) {
        const auto& d = devs[i];
        wprintf(L"[%zu] PID=%04X usage=%04X/%04X feat=%u in=%u out=%u\n    %s\n    %s\n",
                i, d.pid, d.usage_page, d.usage, d.feature_len, d.input_len, d.output_len,
                d.product, d.path.c_str());
        HANDLE h = OpenHid(d.path);
        if (h == INVALID_HANDLE_VALUE) {
            printf("    OPEN FAIL err=%lu\n\n", GetLastError());
            continue;
        }

        bool any = false;
        for (uint8_t txn : txns) {
            RazerReport batt{}, chg{};
            bool okb = TryRazerCmd(h, d.feature_len, txn, 0x07, 0x80, &batt);
            bool okc = TryRazerCmd(h, d.feature_len, txn, 0x07, 0x84, &chg);
            if (okb || okc) {
                any = true;
                printf("    txn=0x%02X set/get ok  batt_status=0x%02X arg0=%u arg1=%u  chg_status=0x%02X arg0=%u arg1=%u\n",
                       txn,
                       okb ? batt.status : 0xFF, okb ? batt.arguments[0] : 0, okb ? batt.arguments[1] : 0,
                       okc ? chg.status : 0xFF, okc ? chg.arguments[0] : 0, okc ? chg.arguments[1] : 0);
                if (okb && batt.status == 0x02) {
                    int pct = batt.arguments[1] * 100 / 255;
                    printf("    >>> BATTERY raw=%u percent~%d%%\n", batt.arguments[1], pct);
                }
                if (okc && chg.status == 0x02) {
                    printf("    >>> CHARGING arg1=%u\n", chg.arguments[1]);
                }
            }
        }

        if (d.feature_len >= 2) {
            uint8_t ff[256]{};
            ULONG len = d.feature_len;
            if (len > sizeof(ff)) len = sizeof(ff);
            ff[0] = 0xFF;
            if (HidD_GetFeature(h, ff, len)) {
                printf("    Feature 0xFF len=%lu: ", len);
                PrintHex(ff, (int)len, 40);
                printf("\n");
            } else {
                printf("    Feature 0xFF FAIL err=%lu\n", GetLastError());
            }
            uint8_t z[256]{};
            z[0] = 0x00;
            if (HidD_GetFeature(h, z, len)) {
                printf("    Feature 0x00 len=%lu: ", len);
                PrintHex(z, (int)len, 40);
                printf("\n");
            }
        }
        if (!any) printf("    OpenRazer battery command: no successful set/get\n");
        CloseHandle(h);
        printf("\n");
    }
    return 0;
}
