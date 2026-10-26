#include "IconDraw.h"
#include "RazerMonitor.h"

#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

namespace {

HFONT CreateSymbolFont(int height, const wchar_t* face) {
    return CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

bool DrawSymbolGlyph(HDC hdc, int x, int y, int size, COLORREF color, wchar_t glyph) {
    const wchar_t* faces[] = {L"Segoe Fluent Icons", L"Segoe MDL2 Assets"};
    for (const wchar_t* face : faces) {
        HFONT font = CreateSymbolFont(size, face);
        if (font == nullptr) {
            continue;
        }
        HGDIOBJ old_font = SelectObject(hdc, font);
        wchar_t actual[LF_FACESIZE]{};
        GetTextFaceW(hdc, LF_FACESIZE, actual);
        WORD index = 0xFFFF;
        GetGlyphIndicesW(hdc, &glyph, 1, &index, GGI_MARK_NONEXISTING_GLYPHS);
        const bool usable = (_wcsicmp(actual, face) == 0 && index != 0xFFFF);
        if (usable) {
            SetTextColor(hdc, color);
            RECT rc{x, y, x + size, y + size};
            wchar_t text[2] = {glyph, 0};
            DrawTextW(hdc, text, 1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOCLIP);
        }
        SelectObject(hdc, old_font);
        DeleteObject(font);
        if (usable) {
            return true;
        }
    }
    return false;
}

ULONG_PTR EnsureGdiplus() {
    static ULONG_PTR token = 0;
    static bool started = false;
    if (!started) {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok) {
            started = true;
        }
    }
    return token;
}

void AddRoundRect(Gdiplus::GraphicsPath& path, Gdiplus::RectF r, float radius) {
    const float d = (std::min)(radius * 2.0f, (std::min)(r.Width, r.Height));
    path.AddArc(r.X, r.Y, d, d, 180.0f, 90.0f);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270.0f, 90.0f);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

Gdiplus::Color MakeColor(COLORREF color, BYTE alpha) {
    return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

void DrawMouseFallback(Gdiplus::Graphics& g, Gdiplus::RectF box, Gdiplus::Color color) {
    const float stroke = (std::max)(1.2f, box.Width / 12.0f);
    Gdiplus::Pen pen(color, stroke);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::RectF body(box.X + box.Width * 0.18f, box.Y + box.Height * 0.08f, box.Width * 0.64f,
                        box.Height * 0.84f);
    Gdiplus::GraphicsPath path;
    AddRoundRect(path, body, body.Width * 0.42f);
    g.DrawPath(&pen, &path);
    const float cx = box.X + box.Width * 0.5f;
    g.DrawLine(&pen, cx, body.Y + stroke, cx, body.Y + body.Height * 0.40f);
}

void DrawHeadsetFallback(Gdiplus::Graphics& g, Gdiplus::RectF box, Gdiplus::Color color) {
    const float stroke = (std::max)(1.2f, box.Width / 12.0f);
    Gdiplus::Pen pen(color, stroke);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    const float cup_w = box.Width * 0.26f;
    const float cup_h = box.Height * 0.40f;
    const float cup_y = box.Y + box.Height * 0.40f;
    Gdiplus::RectF left(box.X + box.Width * 0.10f, cup_y, cup_w, cup_h);
    Gdiplus::RectF right(box.X + box.Width * 0.64f, cup_y, cup_w, cup_h);
    g.DrawEllipse(&pen, left);
    g.DrawEllipse(&pen, right);
    const float band_l = left.X + cup_w * 0.5f;
    const float band_r = right.X + cup_w * 0.5f;
    Gdiplus::RectF band(band_l, box.Y + box.Height * 0.06f, band_r - band_l, box.Height * 0.62f);
    g.DrawArc(&pen, band, 200.0f, 140.0f);
}

COLORREF ScaleColor(COLORREF color, BYTE alpha) {
    if (alpha >= 255) {
        return color;
    }
    return RGB(static_cast<int>(GetRValue(color)) * alpha / 255,
               static_cast<int>(GetGValue(color)) * alpha / 255,
               static_cast<int>(GetBValue(color)) * alpha / 255);
}

}  // namespace

namespace IconDraw {

void DrawDeviceIcon(HDC hdc, DeviceSlot slot, int x, int y, int size, COLORREF color, BYTE alpha) {
    if (hdc == nullptr || size <= 0) {
        return;
    }
    const COLORREF draw_color = ScaleColor(color, alpha);
    const wchar_t glyph = (slot == DeviceSlot::Headset) ? L'\uE7F6' : L'\uE962';
    if (DrawSymbolGlyph(hdc, x, y, size, draw_color, glyph)) {
        return;
    }
    if (EnsureGdiplus() == 0) {
        return;
    }
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    const Gdiplus::RectF box(static_cast<Gdiplus::REAL>(x), static_cast<Gdiplus::REAL>(y),
                             static_cast<Gdiplus::REAL>(size), static_cast<Gdiplus::REAL>(size));
    const Gdiplus::Color gcolor = MakeColor(color, alpha);
    if (slot == DeviceSlot::Headset) {
        DrawHeadsetFallback(graphics, box, gcolor);
    } else {
        DrawMouseFallback(graphics, box, gcolor);
    }
}

}  // namespace IconDraw
