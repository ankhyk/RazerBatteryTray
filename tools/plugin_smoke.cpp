#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "PluginInterface.h"

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(65001);
    const wchar_t* path = argc > 1 ? argv[1] : L"RazerBattery.dll";
    HMODULE module = LoadLibraryW(path);
    if (!module) {
        wprintf(L"LoadLibrary failed %lu: %s\n", GetLastError(), path);
        return 1;
    }
    auto get_instance = reinterpret_cast<ITMPlugin* (*)()>(GetProcAddress(module, "TMPluginGetInstance"));
    if (!get_instance) {
        wprintf(L"TMPluginGetInstance missing %lu\n", GetLastError());
        return 1;
    }
    ITMPlugin* plugin = get_instance();
    if (!plugin) {
        wprintf(L"plugin instance is null\n");
        return 1;
    }
    wprintf(L"API=%d name=%s ver=%s\n", plugin->GetAPIVersion(), plugin->GetInfo(ITMPlugin::TMI_NAME),
            plugin->GetInfo(ITMPlugin::TMI_VERSION));
    plugin->DataRequired();
    for (int i = 0; i < 8; ++i) {
        IPluginItem* item = plugin->GetItem(i);
        if (!item) {
            break;
        }
        wprintf(L"[%d] id=%s name=%s label=%s value=%s sample=%s graph=%.2f\n", i, item->GetItemId(),
                item->GetItemName(), item->GetItemLableText(), item->GetItemValueText(),
                item->GetItemValueSampleText(), item->GetResourceUsageGraphValue());
    }
    wprintf(L"tooltip:\n%s\n", plugin->GetTooltipInfo());
    FreeLibrary(module);
    return 0;
}
