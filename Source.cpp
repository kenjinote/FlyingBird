#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <shlobj.h>
#include <gdiplus.h>
#include "resource.h"

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

struct AppState {
    ULONG_PTR gdiplusToken = 0;
    HDC hScreen = nullptr;
    Bitmap* sprite = nullptr;
    int frameCount = 16;
    int frameIndex = 0;
    UINT intervalMs = 120;
    int frameWidth = 0;
    int frameHeight = 0;
    double posX, posY;
    double velX, velY;
    double accX, accY;
};

static AppState g;

static Bitmap* LoadPngFromResource(HINSTANCE hInst, int resId) {
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(resId), L"PNG");
    if (!hRes) return nullptr;
    DWORD size = SizeofResource(hInst, hRes);
    HGLOBAL hMem = LoadResource(hInst, hRes);
    if (!hMem) return nullptr;
    void* pData = LockResource(hMem);
    if (!pData) return nullptr;
    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hBuffer) return nullptr;
    void* pBuffer = GlobalLock(hBuffer);
    CopyMemory(pBuffer, pData, size);
    GlobalUnlock(hBuffer);
    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hBuffer, TRUE, &pStream) != S_OK) {
        GlobalFree(hBuffer);
        return nullptr;
    }
    Bitmap* bmp = Bitmap::FromStream(pStream);
    pStream->Release();
    return bmp;
}

static bool ShowFrame(HWND hwnd, int index) {
    if (!g.sprite) return false;
    int w = g.frameWidth;
    int h = g.frameHeight;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* pBits = nullptr;
    HDC hdcMem = CreateCompatibleDC(g.hScreen);
    HBITMAP hDib = CreateDIBSection(g.hScreen, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hDib);
    Graphics gfx(hdcMem);
    gfx.SetCompositingMode(CompositingModeSourceCopy);
    gfx.Clear(Color(0, 0, 0, 0));
    Rect srcRect(0, index * h, w, h);
    gfx.DrawImage(g.sprite, Rect(0, 0, w, h), srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height, UnitPixel);
    SIZE sz = { w, h };
    POINT ptSrc = { 0, 0 };
    RECT rc;
    GetWindowRect(hwnd, &rc);
    POINT ptDst = { rc.left, rc.top };
    BLENDFUNCTION bf = {};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    BOOL ok = UpdateLayeredWindow(hwnd, g.hScreen, &ptDst, &sz, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hDib);
    DeleteDC(hdcMem);
    return ok == TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, g.intervalMs, nullptr);
        ShowFrame(hwnd, 0);
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            g.frameIndex = (g.frameIndex + 1) % g.frameCount;
            ShowFrame(hwnd, g.frameIndex);
            g.velX += g.accX;
            g.velY += g.accY;
            g.posX += g.velX;
            g.posY += g.velY;
            RECT rcScreen;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0); // 作業領域を取得
            if ((g.posX + g.frameWidth < rcScreen.left) ||
                (g.posY + g.frameHeight < rcScreen.top)) {
                KillTimer(hwnd, 1);
                DestroyWindow(hwnd);
            }
            else {
                SetWindowPos(hwnd, HWND_TOPMOST,
                    (int)g.posX, (int)g.posY,
                    g.frameWidth, g.frameHeight,
                    SWP_NOZORDER | SWP_NOSIZE);
            }
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

BOOL SelfDelete() {
    WCHAR szFile[MAX_PATH];
    WCHAR szCommand[MAX_PATH];
    if (GetModuleFileNameW(0, szFile, MAX_PATH) &&
        GetShortPathNameW(szFile, szFile, MAX_PATH))
    {
        lstrcpyW(szCommand, L"/c del ");
        lstrcatW(szCommand, szFile);
        lstrcatW(szCommand, L" >> NUL");
        if (GetEnvironmentVariableW(
            L"ComSpec",
            szFile, MAX_PATH) &&
            (INT_PTR)ShellExecute(
                0,
                0,
                szFile,
                szCommand,
                0,
                SW_HIDE) > 32)
            return TRUE;
    }
    return FALSE;
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int)
{
    const wchar_t* kClass = L"FlyingBird";
    if (lpCmdLine == nullptr || wcslen(lpCmdLine) == 0) {
        wchar_t szSelf[MAX_PATH];
        GetModuleFileNameW(nullptr, szSelf, MAX_PATH);
        wchar_t szTempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, szTempPath);
        wchar_t szTarget[MAX_PATH];
        wsprintfW(szTarget, L"%s%s.exe", szTempPath, kClass);
        if (CopyFileW(szSelf, szTarget, FALSE)) {
            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi = {};
            if (CreateProcessW(szTarget, (LPWSTR)L" dummy",
                nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
        SelfDelete();
        return 0;
    }
    Sleep(1000);
    POINT point;
	GetCursorPos(&point);
    SetProcessDPIAware();
    GdiplusStartupInput si{};
    if (GdiplusStartup(&g.gdiplusToken, &si, nullptr) != Ok) return 0;
    g.hScreen = GetDC(nullptr);
    g.sprite = LoadPngFromResource(hInst, IDB_PNG1);
    if (!g.sprite) {
        return 0;
    }
    g.frameWidth = g.sprite->GetWidth();
    g.frameHeight = g.sprite->GetHeight() / g.frameCount;
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClass;
	wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    g.posX = point.x - g.frameWidth / 2;
    g.posY = point.y - g.frameHeight / 2;
    g.velX = -16;
    g.velY = -16;
    g.accX = -2.0;
    g.accY = -2.0;
    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kClass, 0,
        WS_POPUP,
        point.x - (g.frameWidth) / 2, point.y - (g.frameHeight) / 2, g.frameWidth, g.frameHeight,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    delete g.sprite;
    ReleaseDC(nullptr, g.hScreen);
    GdiplusShutdown(g.gdiplusToken);
    SelfDelete();
    return 0;
}