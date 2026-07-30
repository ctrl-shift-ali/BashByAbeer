/*
AFTER PRESSING F5, WHEN TERMINAL OPENS, TYPE THESE COMMANDS ONE-BY-ONE:
 FIRST: g++ shell.cpp -o shell.exe
 THEN: g++ -municode -mwindows terminal.cpp -o terminal.exe 
 LASTLY: terminal.exe
  THEN HIT ENTER, AND BashByAbeer WILL APPEAR!
*/
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000006
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <iostream>

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:wWinMainCRTStartup")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static const COLORREF kDefaultFg = RGB(255, 255, 255);
static const COLORREF kDefaultBg = RGB(0x00, 0x21, 0x3A);

struct Cell {
    wchar_t ch = L' ';
    COLORREF fg = kDefaultFg;
    COLORREF bg = kDefaultBg;
};

struct Screen {
    int cols = 100;
    int rows = 32;
    std::vector<Cell> cells;
    int cursorX = 0, cursorY = 0;
    COLORREF curFg = kDefaultFg;
    COLORREF curBg = kDefaultBg;
    std::mutex m;

    void resize(int c, int r) {
        std::lock_guard<std::mutex> lock(m);
        std::vector<Cell> newCells(c * r);
        for (int y = 0; y < std::min(r, rows); y++)
            for (int x = 0; x < std::min(c, cols); x++)
                newCells[y * c + x] = cells.empty() ? Cell{} : cells[y * cols + x];
        cells = std::move(newCells);
        cols = c; rows = r;
        if (cursorX >= cols) cursorX = cols - 1;
        if (cursorY >= rows) cursorY = rows - 1;
    }

    Cell& at(int x, int y) { return cells[y * cols + x]; }

    void scrollUp() {
        for (int y = 1; y < rows; y++)
            for (int x = 0; x < cols; x++)
                at(x, y - 1) = at(x, y);
        for (int x = 0; x < cols; x++) at(x, rows - 1) = Cell{ L' ', curFg, curBg };
    }

    void newline() {
        cursorX = 0;
        cursorY++;
        if (cursorY >= rows) { scrollUp(); cursorY = rows - 1; }
    }

    void putChar(wchar_t c) {
        if (c == L'\r') { cursorX = 0; return; }
        if (c == L'\n') { newline(); return; }
        if (c == L'\b') { if (cursorX > 0) cursorX--; return; }
        if (cursorX >= cols) newline();
        at(cursorX, cursorY) = Cell{ c, curFg, curBg };
        cursorX++;
    }

    void eraseLine(int mode) {
        int y = cursorY;
        if (mode == 0) for (int x = cursorX; x < cols; x++) at(x, y) = Cell{ L' ', curFg, curBg };
        else if (mode == 1) for (int x = 0; x <= cursorX; x++) at(x, y) = Cell{ L' ', curFg, curBg };
        else for (int x = 0; x < cols; x++) at(x, y) = Cell{ L' ', curFg, curBg };
    }

    void writeText(const std::string& text) {
        std::lock_guard<std::mutex> lock(m);
        for (char ch : text) {
            if (ch == '\n') { newline(); }
            else if (ch == '\r') { cursorX = 0; }
            else if (ch == '\t') { cursorX = ((cursorX / 8) + 1) * 8; }
            else {
                if (cursorX >= cols) newline();
                at(cursorX, cursorY) = Cell{ (wchar_t)ch, curFg, curBg };
                cursorX++;
            }
        }
    }

    void eraseScreen(int mode) {
        if (mode == 2 || mode == 3) {
            for (auto& c : cells) c = Cell{ L' ', curFg, curBg };
            cursorX = cursorY = 0;
        } else if (mode == 0) {
            for (int y = cursorY; y < rows; y++)
                for (int x = (y == cursorY ? cursorX : 0); x < cols; x++)
                    at(x, y) = Cell{ L' ', curFg, curBg };
        } else if (mode == 1) {
            for (int y = 0; y <= cursorY; y++)
                for (int x = 0; x <= (y == cursorY ? cursorX : cols - 1); x++)
                    at(x, y) = Cell{ L' ', curFg, curBg };
        }
    }
};

static Screen g_screen;

static COLORREF palette16(int idx) {
    static const COLORREF table[16] = {
        RGB(0,0,0),     RGB(128,0,0),   RGB(0,128,0),   RGB(128,128,0),
        RGB(0,0,128),   RGB(128,0,128), RGB(0,128,128), RGB(192,192,192),
        RGB(128,128,128), RGB(255,0,0), RGB(0,255,0),   RGB(255,255,0),
        RGB(0,0,255),   RGB(255,0,255), RGB(0,255,255), RGB(255,255,255)
    };
    return table[idx & 0x0F];
}

class AnsiParser {
public:
    void feed(const std::wstring& data) {
        for (wchar_t c : data) feedChar(c);
    }
private:
    enum State { GROUND, ESC, CSI, OSC } state = GROUND;
    std::wstring paramBuf;

    void feedChar(wchar_t c) {
        std::lock_guard<std::mutex> lock(g_screen.m);
        switch (state) {
        case GROUND:
            if (c == L'\x1b') { state = ESC; paramBuf.clear(); }
            else g_screen.putChar(c);
            break;
        case ESC:
            if (c == L'[') state = CSI;
            else if (c == L']') state = OSC;
            else state = GROUND;
            break;
        case CSI:
            if (c == L'?' || c == L'=' || c == L'>' || c == L'<') break;
            if ((c >= L'0' && c <= L'9') || c == L';') {
                paramBuf += c;
            } else {
                dispatchCSI(c, paramBuf);
                state = GROUND;
                paramBuf.clear();
            }
            break;
        case OSC:
            if (c == L'\x07') { state = GROUND; paramBuf.clear(); }
            else if (c == L'\x1b') { state = GROUND; paramBuf.clear(); }
            else paramBuf += c;
            break;
        }
    }

    std::vector<int> splitParams(const std::wstring& s) {
        std::vector<int> out;
        std::wstring cur;
        for (wchar_t c : s) {
            if (c == L';') { out.push_back(cur.empty() ? 0 : _wtoi(cur.c_str())); cur.clear(); }
            else cur += c;
        }
        out.push_back(cur.empty() ? 0 : _wtoi(cur.c_str()));
        return out;
    }

    void dispatchCSI(wchar_t final, const std::wstring& params) {
        auto p = splitParams(params);
        int p0 = p.size() > 0 ? p[0] : 0;
        switch (final) {
        case L'm':
            if (params.empty()) { g_screen.curFg = kDefaultFg; g_screen.curBg = kDefaultBg; }
            else {
                for (size_t i = 0; i < p.size(); i++) {
                    int code = p[i];
                    if (code == 0) { g_screen.curFg = kDefaultFg; g_screen.curBg = kDefaultBg; }
                    else if (code >= 30 && code <= 37) g_screen.curFg = palette16(code - 30);
                    else if (code >= 90 && code <= 97) g_screen.curFg = palette16(8 + (code - 90));
                    else if (code >= 40 && code <= 47) g_screen.curBg = palette16(code - 40);
                    else if (code >= 100 && code <= 107) g_screen.curBg = palette16(8 + (code - 100));
                    else if (code == 39) g_screen.curFg = kDefaultFg;
                    else if (code == 49) g_screen.curBg = kDefaultBg;
                    else if (code == 38 || code == 48) {
                        bool isFg = (code == 38);
                        if (i + 1 < p.size() && p[i + 1] == 2 && i + 4 < p.size()) {
                            COLORREF col = RGB(p[i + 2], p[i + 3], p[i + 4]);
                            if (isFg) g_screen.curFg = col; else g_screen.curBg = col;
                            i += 4;
                        } else if (i + 1 < p.size() && p[i + 1] == 5 && i + 2 < p.size()) {
                            COLORREF col = palette16(p[i + 2] % 16);
                            if (isFg) g_screen.curFg = col; else g_screen.curBg = col;
                            i += 2;
                        }
                    }
                }
            }
            break;
        case L'H': case L'f': {
            int row = p.size() > 0 && p[0] ? p[0] : 1;
            int col = p.size() > 1 && p[1] ? p[1] : 1;
            g_screen.cursorY = std::min(g_screen.rows - 1, std::max(0, row - 1));
            g_screen.cursorX = std::min(g_screen.cols - 1, std::max(0, col - 1));
            break;
        }
        case L'A': g_screen.cursorY = std::max(0, g_screen.cursorY - std::max(1, p0)); break;
        case L'B': g_screen.cursorY = std::min(g_screen.rows - 1, g_screen.cursorY + std::max(1, p0)); break;
        case L'C': g_screen.cursorX = std::min(g_screen.cols - 1, g_screen.cursorX + std::max(1, p0)); break;
        case L'D': g_screen.cursorX = std::max(0, g_screen.cursorX - std::max(1, p0)); break;
        case L'J': g_screen.eraseScreen(p0); break;
        case L'K': g_screen.eraseLine(p0); break;
        default: break;
        }
    }
};

static AnsiParser g_parser;
static HPCON g_hPC = NULL;
static HANDLE g_hProcess = NULL;
static HANDLE g_hPipeIn = NULL;
static HANDLE g_hPipeOut = NULL;
static std::atomic<bool> g_running{ true };
static HWND g_hwnd = NULL;

static void showStartupBanner() {
    const std::string banner =
        "\t\t\t======================\n"
        "\t\t\t\tHello!\n"
        "\t\t\t     \"BashByAbeer\"\n"
        "\t\t\t======================\n"
        "Hello There!\n"
        "Welcome to the terminal made by Me. Type 'exit' to quit.\n\n";

    g_screen.eraseScreen(2);
    g_screen.cursorX = 0;
    g_screen.cursorY = 0;
    g_screen.writeText(banner);
    if (g_hwnd) {
        RedrawWindow(g_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
}

static void readerThread() {
    char buf[4096];
    DWORD n;
    while (g_running) {
        if (!ReadFile(g_hPipeOut, buf, sizeof(buf), &n, NULL) || n == 0) break;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, n, NULL, 0);
        std::wstring wbuf(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, buf, n, &wbuf[0], wlen);
        g_parser.feed(wbuf);
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static bool startPty(int cols, int rows, const std::wstring& shellPath) {
    HANDLE hPipeInRead, hPipeOutWrite;
    if (!CreatePipe(&hPipeInRead, &g_hPipeIn, NULL, 0)) return false;
    if (!CreatePipe(&g_hPipeOut, &hPipeOutWrite, NULL, 0)) return false;

    COORD size{ (SHORT)cols, (SHORT)rows };
    HRESULT hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &g_hPC);
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeOutWrite);
    if (FAILED(hr)) return false;

    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(si);

    SIZE_T bytesRequired = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired);
    UpdateProcThreadAttribute(si.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, g_hPC, sizeof(HPCON), NULL, NULL);

    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = L"\"" + shellPath + L"\"";
    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        NULL, buf.data(), NULL, NULL, FALSE,
        EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
        &si.StartupInfo, &pi);

    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

    if (!ok) return false;
    g_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    return true;
}

static void sendToPty(const std::string& utf8) {
    DWORD written;
    WriteFile(g_hPipeIn, utf8.data(), (DWORD)utf8.size(), &written, NULL);
}

static std::wstring getShellPath() {
    wchar_t exePath[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, exePath, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::wstring dir(exePath, n);
        size_t pos = dir.find_last_of(L"\\/");
        dir = (pos != std::wstring::npos) ? dir.substr(0, pos + 1) : L"";
        std::wstring candidate = dir + L"shell.exe";
        DWORD attr = GetFileAttributesW(candidate.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return candidate;
        }
    }
    wchar_t buf[MAX_PATH];
    DWORD cn = GetEnvironmentVariableW(L"COMSPEC", buf, MAX_PATH);
    if (cn > 0 && cn < MAX_PATH) return std::wstring(buf, cn);
    return L"cmd.exe";
}

static int g_charW = 8, g_charH = 16;
static HFONT g_font = NULL;

static void computeFontMetrics(HDC hdc) {
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    g_charW = tm.tmAveCharWidth;
    g_charH = tm.tmHeight;
}

static void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HDC mem = CreateCompatibleDC(hdc);
    RECT rc; GetClientRect(hwnd, &rc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);
    HFONT oldFont = (HFONT)SelectObject(mem, g_font);
    SetBkMode(mem, OPAQUE);

    HBRUSH bgBrush = CreateSolidBrush(kDefaultBg);
    FillRect(mem, &rc, bgBrush);
    DeleteObject(bgBrush);

    {
        std::lock_guard<std::mutex> lock(g_screen.m);
        for (int y = 0; y < g_screen.rows; y++) {
            for (int x = 0; x < g_screen.cols; x++) {
                Cell& cell = g_screen.at(x, y);
                SetTextColor(mem, cell.fg);
                SetBkColor(mem, cell.bg);
                RECT cellRc{ x * g_charW, y * g_charH, (x + 1) * g_charW, (y + 1) * g_charH };
                wchar_t ch = cell.ch;
                ExtTextOutW(mem, cellRc.left, cellRc.top, ETO_OPAQUE, &cellRc, &ch, 1, NULL);
            }
        }
        RECT curRc{ g_screen.cursorX * g_charW, g_screen.cursorY * g_charH,
                    (g_screen.cursorX + 1) * g_charW, (g_screen.cursorY + 1) * g_charH };
        InvertRect(mem, &curRc);
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldFont);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HDC hdc = GetDC(hwnd);
        g_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        SelectObject(hdc, g_font);
        computeFontMetrics(hdc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }
    case WM_SIZE: {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        int cols = std::max(10, w / g_charW);
        int rows = std::max(3, h / g_charH);
        g_screen.resize(cols, rows);
        if (g_hPC) {
            COORD size{ (SHORT)cols, (SHORT)rows };
            ResizePseudoConsole(g_hPC, size);
        }
        return 0;
    }
    case WM_CHAR: {
        wchar_t wc = (wchar_t)wParam;
        char utf8[4]; int len = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, 4, NULL, NULL);
        sendToPty(std::string(utf8, len));
        return 0;
    }
    case WM_KEYDOWN: {
        switch (wParam) {
        case VK_UP:    sendToPty("\x1b[A"); return 0;
        case VK_DOWN:  sendToPty("\x1b[B"); return 0;
        case VK_RIGHT: sendToPty("\x1b[C"); return 0;
        case VK_LEFT:  sendToPty("\x1b[D"); return 0;
        }
        return 0;
    }
    case WM_PAINT:
        paint(hwnd);
        return 0;
    case WM_DESTROY:
        g_running = false;
        if (g_hPC) ClosePseudoConsole(g_hPC);
        if (g_hProcess) TerminateProcess(g_hProcess, 0);
        if (g_hPipeIn) CloseHandle(g_hPipeIn);
        if (g_font) DeleteObject(g_font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MyTerminalClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    g_hwnd = CreateWindowW(L"MyTerminalClass", L"BashByAbeer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        NULL, NULL, hInst, NULL);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG pending;
    while (PeekMessage(&pending, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&pending);
        DispatchMessage(&pending);
    }

    std::wstring shellPath = getShellPath();
    if (!startPty(g_screen.cols, g_screen.rows, shellPath)) {
        std::wstring msg = L"Failed to start the pseudoconsole or launch the shell:\n" + shellPath +
            L"\n\nThis needs Windows 10 1809+ (ConPTY support).";
        MessageBoxW(g_hwnd, msg.c_str(), L"Error", MB_OK);
        return 1;
    }

    showStartupBanner();

    std::thread reader(readerThread);
    reader.detach();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}