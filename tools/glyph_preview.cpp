#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdio.h>
#pragma comment(lib, "gdiplus.lib")

static CLSID PngClsid() {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    auto* info = static_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
    Gdiplus::GetImageEncoders(num, size, info);
    CLSID id{};
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(info[i].MimeType, L"image/png") == 0) id = info[i].Clsid;
    }
    free(info);
    return id;
}

int wmain() {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&token, &in, nullptr);
    Gdiplus::Bitmap bmp(520, 220);
    Gdiplus::Graphics g(&bmp);
    g.Clear(Gdiplus::Color(32, 32, 32));
    g.FillRectangle(&Gdiplus::SolidBrush(Gdiplus::Color(245, 245, 245)), 0, 110, 520, 110);
    HDC hdc = g.GetHDC();
    SetBkMode(hdc, TRANSPARENT);

    const wchar_t* fonts[] = {L"Segoe MDL2 Assets", L"Segoe Fluent Icons"};
    const wchar_t glyphs[] = {0xE962, 0xE7F6, 0xE95B, 0xE8A5};
    const wchar_t* names[] = {L"E962 Mouse", L"E7F6 Headset", L"E95B Headph", L"E8A5 Touch"};
    int sizes[] = {16, 20, 24};

    for (int row = 0; row < 2; ++row) {
        COLORREF color = row == 0 ? RGB(240, 240, 240) : RGB(32, 32, 32);
        SetTextColor(hdc, color);
        int y = row * 110 + 12;
        int x = 8;
        for (int f = 0; f < 2; ++f) {
            for (int gi = 0; gi < 4; ++gi) {
                for (int s = 0; s < 3; ++s) {
                    HFONT font = CreateFontW(-sizes[s], 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                             OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                             DEFAULT_PITCH | FF_DONTCARE, fonts[f]);
                    HGDIOBJ old = SelectObject(hdc, font);
                    wchar_t ch[2] = {glyphs[gi], 0};
                    TextOutW(hdc, x, y, ch, 1);
                    SelectObject(hdc, old);
                    DeleteObject(font);
                    x += sizes[s] + 6;
                }
                x += 10;
            }
            x = 8;
            y += 36;
        }
        HFONT caption = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HGDIOBJ old = SelectObject(hdc, caption);
        TextOutW(hdc, 8, row * 110 + 88, L"MDL2 then Fluent: Mouse Headset Headphones Touch", 48);
        SelectObject(hdc, old);
        DeleteObject(caption);
    }
    g.ReleaseHDC(hdc);
    CLSID png = PngClsid();
    bmp.Save(L"C:\\Users\\ankh\\code\\github\\ankhyk\\RazerBatteryTray\\glyph_preview.png", &png, nullptr);
    Gdiplus::GdiplusShutdown(token);
    return 0;
}
