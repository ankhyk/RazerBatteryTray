#pragma once

#include <windows.h>

enum class DeviceSlot;

namespace IconDraw {

void DrawDeviceIcon(HDC hdc, DeviceSlot slot, int x, int y, int size, COLORREF color, BYTE alpha);

}  // namespace IconDraw
