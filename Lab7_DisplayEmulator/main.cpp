#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "DisplayProtocol.h"
#include "UDPServer.h"

#include <windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <array>
#include <cctype>
#include <algorithm>

const int DISPLAY_WIDTH = 320;
const int DISPLAY_HEIGHT = 240;
const int UDP_PORT = 12345;

HWND windowHandle = nullptr;
HBITMAP displayBitmap = nullptr;
HBITMAP previousBitmap = nullptr;
HDC memoryDC = nullptr;

std::mutex displayMutex;

uint8_t currentOrientation = 0;

uint16_t getDisplayWidth()
{
    std::lock_guard<std::mutex> lock(displayMutex);

    return (currentOrientation % 2 == 1)
        ? DISPLAY_HEIGHT : DISPLAY_WIDTH;
}

uint16_t getDisplayHeight()
{
    std::lock_guard<std::mutex> lock(displayMutex);

    return (currentOrientation % 2 == 1)
        ? DISPLAY_WIDTH : DISPLAY_HEIGHT;
}

void updateDisplay()
{
    if (windowHandle != nullptr)
        InvalidateRect(windowHandle, NULL, FALSE);
}

COLORREF convertColor(uint16_t color)
{
    int r = ((color >> 11) & 31) * 255 / 31;
    int g = ((color >> 5) & 63) * 255 / 63;
    int b = (color & 31) * 255 / 31;

    return RGB(r, g, b);
}

void transformPoint(
    int x, int y,
    int& newX, int& newY)
{
    switch (currentOrientation)
    {
    case 0:
        newX = x;
        newY = y;
        break;

    case 1:
        newX = DISPLAY_WIDTH - 1 - y;
        newY = x;
        break;

    case 2:
        newX = DISPLAY_WIDTH - 1 - x;
        newY = DISPLAY_HEIGHT - 1 - y;
        break;

    case 3:
        newX = y;
        newY = DISPLAY_HEIGHT - 1 - x;
        break;

    default:
        newX = x;
        newY = y;
        break;
    }
}

void transformRectangle(
    int x, int y, int w, int h,
    int& newX, int& newY,
    int& newW, int& newH)
{
    switch (currentOrientation)
    {
    case 0:
        newX = x;
        newY = y;
        newW = w;
        newH = h;
        break;

    case 1:
        newX = DISPLAY_WIDTH - (y + h);
        newY = x;
        newW = h;
        newH = w;
        break;

    case 2:
        newX = DISPLAY_WIDTH - (x + w);
        newY = DISPLAY_HEIGHT - (y + h);
        newW = w;
        newH = h;
        break;

    case 3:
        newX = y;
        newY = DISPLAY_HEIGHT - (x + w);
        newW = h;
        newH = w;
        break;

    default:
        newX = x;
        newY = y;
        newW = w;
        newH = h;
        break;
    }
}

void clearDisplay(uint16_t color)
{
    HBRUSH brush = CreateSolidBrush(convertColor(color));

    RECT rect = {
        0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT
    };

    FillRect(memoryDC, &rect, brush);

    DeleteObject(brush);
}

void drawPixel(int x, int y, uint16_t color)
{
    int newX, newY;

    transformPoint(x, y, newX, newY);

    SetPixel(memoryDC, newX, newY, convertColor(color));
}

void drawLine(
    int x0, int y0,
    int x1, int y1,
    uint16_t color)
{
    int newX0, newY0, newX1, newY1;

    transformPoint(x0, y0, newX0, newY0);
    transformPoint(x1, y1, newX1, newY1);

    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    MoveToEx(memoryDC, newX0, newY0, NULL);
    LineTo(memoryDC, newX1, newY1);

    SelectObject(memoryDC, oldPen);
    DeleteObject(pen);
}

void drawRectangle(
    int x, int y, int w, int h,
    uint16_t color, bool filled)
{
    int newX, newY, newW, newH;

    transformRectangle(
        x, y, w, h,
        newX, newY, newW, newH
    );

    HBRUSH brush;

    if (filled)
        brush = CreateSolidBrush(convertColor(color));
    else
        brush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));

    HBRUSH oldBrush = (HBRUSH)SelectObject(memoryDC, brush);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    Rectangle(
        memoryDC,
        newX, newY,
        newX + newW, newY + newH
    );

    SelectObject(memoryDC, oldBrush);
    SelectObject(memoryDC, oldPen);

    if (filled)
        DeleteObject(brush);

    DeleteObject(pen);
}

void drawEllipse(
    int x, int y, int rx, int ry,
    uint16_t color, bool filled)
{
    int newX, newY;

    transformPoint(x, y, newX, newY);

    if (currentOrientation == 1 ||
        currentOrientation == 3)
    {
        std::swap(rx, ry);
    }

    HBRUSH brush;

    if (filled)
        brush = CreateSolidBrush(convertColor(color));
    else
        brush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));

    HBRUSH oldBrush = (HBRUSH)SelectObject(memoryDC, brush);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    Ellipse(
        memoryDC,
        newX - rx, newY - ry,
        newX + rx, newY + ry
    );

    SelectObject(memoryDC, oldBrush);
    SelectObject(memoryDC, oldPen);

    if (filled)
        DeleteObject(brush);

    DeleteObject(pen);
}

void drawCircle(
    int x, int y, int radius,
    uint16_t color, bool filled)
{
    drawEllipse(x, y, radius, radius, color, filled);
}

void drawRoundedRectangle(
    int x, int y, int w, int h,
    int radius, uint16_t color, bool filled)
{
    int newX, newY, newW, newH;

    transformRectangle(
        x, y, w, h,
        newX, newY, newW, newH
    );

    HBRUSH brush;

    if (filled)
        brush = CreateSolidBrush(convertColor(color));
    else
        brush = (HBRUSH)GetStockObject(NULL_BRUSH);

    HPEN pen = CreatePen(PS_SOLID, 1, convertColor(color));

    HBRUSH oldBrush = (HBRUSH)SelectObject(memoryDC, brush);
    HPEN oldPen = (HPEN)SelectObject(memoryDC, pen);

    RoundRect(
        memoryDC,
        newX, newY,
        newX + newW, newY + newH,
        radius * 2, radius * 2
    );

    SelectObject(memoryDC, oldBrush);
    SelectObject(memoryDC, oldPen);

    if (filled)
        DeleteObject(brush);

    DeleteObject(pen);
}

std::array<uint8_t, 7> getCharPattern(char c)
{
    switch (std::toupper(static_cast<unsigned char>(c)))
    {
    case '0': return { 14,17,19,21,25,17,14 };
    case '1': return { 4,12,4,4,4,4,14 };
    case '2': return { 14,17,1,2,4,8,31 };
    case '3': return { 30,1,1,14,1,1,30 };
    case '4': return { 2,6,10,18,31,2,2 };
    case '5': return { 31,16,16,30,1,1,30 };
    case '6': return { 14,16,16,30,17,17,14 };
    case '7': return { 31,1,2,4,8,8,8 };
    case '8': return { 14,17,17,14,17,17,14 };
    case '9': return { 14,17,17,15,1,1,14 };

    case 'A': return { 14,17,17,31,17,17,17 };
    case 'B': return { 30,17,17,30,17,17,30 };
    case 'C': return { 14,17,16,16,16,17,14 };
    case 'D': return { 28,18,17,17,17,18,28 };
    case 'E': return { 31,16,16,30,16,16,31 };
    case 'F': return { 31,16,16,30,16,16,16 };
    case 'G': return { 14,17,16,23,17,17,15 };
    case 'H': return { 17,17,17,31,17,17,17 };
    case 'I': return { 14,4,4,4,4,4,14 };
    case 'J': return { 7,2,2,2,2,18,12 };
    case 'K': return { 17,18,20,24,20,18,17 };
    case 'L': return { 16,16,16,16,16,16,31 };
    case 'M': return { 17,27,21,21,17,17,17 };
    case 'N': return { 17,25,21,19,17,17,17 };
    case 'O': return { 14,17,17,17,17,17,14 };
    case 'P': return { 30,17,17,30,16,16,16 };
    case 'Q': return { 14,17,17,17,21,18,13 };
    case 'R': return { 30,17,17,30,20,18,17 };
    case 'S': return { 15,16,16,14,1,1,30 };
    case 'T': return { 31,4,4,4,4,4,4 };
    case 'U': return { 17,17,17,17,17,17,14 };
    case 'V': return { 17,17,17,17,17,10,4 };
    case 'W': return { 17,17,17,21,21,21,10 };
    case 'X': return { 17,17,10,4,10,17,17 };
    case 'Y': return { 17,17,10,4,4,4,4 };
    case 'Z': return { 31,1,2,4,8,16,31 };

    case ':': return { 0,4,4,0,4,4,0 };
    case '.': return { 0,0,0,0,0,12,12 };
    case ',': return { 0,0,0,0,4,4,8 };
    case '-': return { 0,0,0,31,0,0,0 };
    case '/': return { 1,2,2,4,8,8,16 };
    case '!': return { 4,4,4,4,4,0,4 };
    case '?': return { 14,17,1,2,4,0,4 };
    case ' ': return { 0,0,0,0,0,0,0 };

    default: return { 31,17,2,4,4,0,4 };
    }
}

void drawTextCommand(const Command& cmd)
{
    int x = cmd.parameters[0];
    int y = cmd.parameters[1];

    int fontSize = 16 + cmd.font * 2;
    int scale = std::max(1, fontSize / 8);

    for (char c : cmd.text)
    {
        auto pattern = getCharPattern(c);

        for (int row = 0; row < 7; row++)
        {
            int col = 0;

            while (col < 5)
            {
                if (!(pattern[row] & (1 << (4 - col))))
                {
                    col++;
                    continue;
                }

                int start = col;

                while (col < 5 &&
                    (pattern[row] & (1 << (4 - col))))
                {
                    col++;
                }

                int x0 = x + start * scale;
                int x1 = x + col * scale - 1;
                int y0 = y + row * scale;

                for (int dy = 0; dy < scale; dy++)
                {
                    drawLine(
                        x0, y0 + dy,
                        x1, y0 + dy,
                        cmd.color
                    );
                }
            }
        }

        x += 6 * scale;
    }
}

void drawImageCommand(const Command& cmd)
{
    int x = cmd.parameters[0];
    int y = cmd.parameters[1];
    int w = cmd.parameters[2];
    int h = cmd.parameters[3];

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            size_t index =
                (static_cast<size_t>(row) * w + col) * 2;

            if (index + 1 >= cmd.imageData.size())
                return;

            uint16_t color = static_cast<uint16_t>(
                cmd.imageData[index] |
                (static_cast<uint16_t>(cmd.imageData[index + 1]) << 8)
                );

            drawPixel(x + col, y + row, color);
        }
    }
}

void executeCommand(const Command& cmd)
{
    {
        std::lock_guard<std::mutex> lock(displayMutex);

        switch (cmd.type)
        {
        case CLEAR_DISPLAY:
            clearDisplay(cmd.color);
            break;

        case DRAW_PIXEL:
            drawPixel(
                cmd.parameters[0],
                cmd.parameters[1],
                cmd.color
            );
            break;

        case DRAW_LINE:
            drawLine(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.parameters[3],
                cmd.color
            );
            break;

        case DRAW_RECTANGLE:
            drawRectangle(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.parameters[3],
                cmd.color, false
            );
            break;

        case FILL_RECTANGLE:
            drawRectangle(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.parameters[3],
                cmd.color, true
            );
            break;

        case DRAW_ELLIPSE:
            drawEllipse(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.parameters[3],
                cmd.color, false
            );
            break;

        case FILL_ELLIPSE:
            drawEllipse(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.parameters[3],
                cmd.color, true
            );
            break;

        case DRAW_CIRCLE:
            drawCircle(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.color, false
            );
            break;

        case FILL_CIRCLE:
            drawCircle(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.color, true
            );
            break;

        case DRAW_ROUNDED_RECTANGLE:
            drawRoundedRectangle(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.parameters[3],
                cmd.parameters[4], cmd.color, false
            );
            break;

        case FILL_ROUNDED_RECTANGLE:
            drawRoundedRectangle(
                cmd.parameters[0], cmd.parameters[1],
                cmd.parameters[2], cmd.parameters[3],
                cmd.parameters[4], cmd.color, true
            );
            break;

        case DRAW_TEXT:
            drawTextCommand(cmd);
            break;

        case DRAW_IMAGE:
            drawImageCommand(cmd);
            break;

        case SET_ORIENTATION:
            currentOrientation = cmd.orientation;
            break;

        case GET_WIDTH:
        case GET_HEIGHT:
            break;
        }
    }

    updateDisplay();
}

LRESULT CALLBACK WindowProcedure(
    HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        {
            std::lock_guard<std::mutex> lock(displayMutex);

            BitBlt(
                hdc, 0, 0,
                DISPLAY_WIDTH, DISPLAY_HEIGHT,
                memoryDC, 0, 0,
                SRCCOPY
            );
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

int main()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSA wc = {};

    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DisplayEmulator";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    windowHandle = CreateWindowA(
        "DisplayEmulator",
        "Display Module Emulator - Lab 7",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        DISPLAY_WIDTH + 16, DISPLAY_HEIGHT + 39,
        NULL, NULL, hInstance,
        NULL
    );

    if (!windowHandle)
        return 1;

    HDC windowDC = GetDC(windowHandle);

    memoryDC = CreateCompatibleDC(windowDC);

    displayBitmap = CreateCompatibleBitmap(
        windowDC,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT
    );

    ReleaseDC(windowHandle, windowDC);

    if (!memoryDC || !displayBitmap)
        return 1;

    previousBitmap = (HBITMAP)SelectObject(
        memoryDC, displayBitmap
    );

    clearDisplay(0x0000);

    ShowWindow(windowHandle, SW_SHOW);
    UpdateWindow(windowHandle);

    UDPServer server;

    if (!server.start(UDP_PORT))
    {
        MessageBoxA(
            windowHandle,
            "Failed to start UDP server",
            "Error",
            MB_ICONERROR
        );
    }

    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    server.stop();

    SelectObject(memoryDC, previousBitmap);

    DeleteObject(displayBitmap);
    DeleteDC(memoryDC);

    return 0;
}