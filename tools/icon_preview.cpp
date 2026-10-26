#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdio.h>

#include "PluginInterface.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

int wmain(int argc, wchar_t** argv) {
    const wchar_t* dll_path = argc > 1 ? argv[1] : L"RazerBattery.dll";
    const wchar_t* out_path = argc > 2 ? argv[2] : L"icon_preview.png";

    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);

    HMODULE module = LoadLibraryW(dll_path);
    if (!module) {
        wprintf(L"LoadLibrary failed %lu\n", GetLastError());
        return 1;
    }
    auto get_instance = reinterpret_cast<ITMPlugin* (*)()>(GetProcAddress(module, "TMPluginGetInstance"));
    ITMPlugin* plugin = get_instance();
    plugin->DataRequired();

    const int width = 460;
    const int height = 120;
    Gdiplus::Bitmap bitmap(width, height);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.Clear(Gdiplus::Color(30, 30, 30));
    graphics.FillRectangle(&Gdiplus::SolidBrush(Gdiplus::Color(245, 245, 245)), 0, 60, width, 60);

    HDC hdc = graphics.GetHDC();
    HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old_font = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    IPluginItem* mouse = plugin->GetItem(0);
    IPluginItem* headset = plugin->GetItem(1);
    IPluginItem* combined = plugin->GetItem(2);
    const int col_w = mouse->GetItemWidthEx(hdc);
    const int combined_w = combined->GetItemWidthEx(hdc);

    for (int row = 0; row < 2; ++row) {
        const bool dark = row == 0;
        plugin->OnExtenedInfo(ITMPlugin::EI_VALUE_TEXT_COLOR, dark ? L"15790320" : L"2105376");
        const int y0 = row * 60 + 8;
        mouse->DrawItem(hdc, 8, y0, col_w, 20, dark);
        headset->DrawItem(hdc, 8, y0 + 22, col_w, 20, dark);
        combined->DrawItem(hdc, 8 + col_w + 24, y0 + 10, combined_w, 20, dark);
    }

    SelectObject(hdc, old_font);
    DeleteObject(font);
    graphics.ReleaseHDC(hdc);

    CLSID png_clsid{};
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    auto* info = static_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
    Gdiplus::GetImageEncoders(num, size, info);
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(info[i].MimeType, L"image/png") == 0) {
            png_clsid = info[i].Clsid;
            break;
        }
    }
    free(info);
    bitmap.Save(out_path, &png_clsid, nullptr);
    wprintf(L"saved %s\n", out_path);

    FreeLibrary(module);
    Gdiplus::GdiplusShutdown(token);
    return 0;
}
