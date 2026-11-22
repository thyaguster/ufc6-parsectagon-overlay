#ifndef OVERLAY_WIDGET_H
#define OVERLAY_WIDGET_H
#include <windows.h>

static void overlay_rounded_box(HDC dc, RECT box, int radius, COLORREF fill, COLORREF edge) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ old_brush = SelectObject(dc, brush), old_pen = SelectObject(dc, pen);
    RoundRect(dc, box.left, box.top, box.right, box.bottom, radius, radius);
    SelectObject(dc, old_pen); SelectObject(dc, old_brush);
    DeleteObject(pen); DeleteObject(brush);
}

static void overlay_row_surface(HDC dc, RECT box, int style, int radius, COLORREF fill, COLORREF edge) {
    if (style != 4) { overlay_rounded_box(dc, box, radius, fill, edge); return; }
    int cut = (box.bottom - box.top) / 3;
    if (cut > 9) cut = 9;
    POINT points[] = {{box.left+cut,box.top},{box.right-cut-1,box.top},{box.right-1,box.top+cut},
                      {box.right-1,box.bottom-cut-1},{box.right-cut-1,box.bottom-1},
                      {box.left+cut,box.bottom-1},{box.left,box.bottom-cut-1},{box.left,box.top+cut}};
    HPEN pen = CreatePen(PS_SOLID, 1, edge);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ old_pen = SelectObject(dc, pen), old_brush = SelectObject(dc, brush);
    Polygon(dc, points, 8);
    SelectObject(dc, old_pen); SelectObject(dc, old_brush);
    DeleteObject(pen); DeleteObject(brush);
}
#endif
