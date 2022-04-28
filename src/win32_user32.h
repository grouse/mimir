#ifndef WIN32_USER32_H
#define WIN32_USER32_H

#include "win32_lite.h"

#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEA

#define IDC_SIZENWSE MAKEINTRESOURCE(32642)
#define IDC_ARROW MAKEINTRESOURCE(32512)

#define COLOR_WINDOW 5

#define WM_TIMER                        0x0113
#define WM_QUIT                         0x0012
#define WM_CLOSE                        0x0010
#define WM_KEYDOWN                      0x0100
#define WM_KEYUP                        0x0101
#define WM_SYSKEYDOWN                   0x0104
#define WM_SYSKEYUP                     0x0105
#define WM_MOUSEMOVE                    0x0200
#define WM_MOUSEWHEEL                   0x020A
#define WM_LBUTTONDOWN                  0x0201
#define WM_LBUTTONUP                    0x0202
#define WM_MBUTTONDOWN                  0x0207
#define WM_MBUTTONUP                    0x0208
#define WM_RBUTTONDOWN                  0x0204
#define WM_RBUTTONUP                    0x0205
#define WM_PAINT                        0x000F
#define WM_CHAR                         0x0102
#define WM_COMMAND                      0x0111
#define WM_SIZE                         0x0005
#define WM_SIZING                       0x0214
#define WM_ENTERSIZEMOVE                0x0231
#define WM_EXITSIZEMOVE                 0x0232
#define WM_SETCURSOR                    0x0020

typedef struct tagPAINTSTRUCT {
    HDC  hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *NPPAINTSTRUCT, *LPPAINTSTRUCT;

extern "C" {
    BOOL OpenClipboard(HWND hWndNewOwner);
    BOOL CloseClipboard();
    
    BOOL EmptyClipboard();
    
    HANDLE GetClipboardData(UINT uFormat);
    HANDLE SetClipboardData(UINT uFormat, HANDLE hMem);
    
    BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
    BOOL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
    BOOL TranslateMessage(const MSG *lpMsg);
    LRESULT DispatchMessageA(const MSG *lpMsg);
    
    SHORT GetAsyncKeyState(int vKey);

    BOOL GetClientRect(HWND hWnd, LPRECT lpRect);
    
    int FillRect(HDC hDC, const RECT *lprc, HBRUSH hbr);
    
    HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
    BOOL EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint);
    
    HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
    
    ATOM RegisterClassA(const WNDCLASSA *lpWndClass);
    
    HDC GetDC(HWND hWnd);
    int ReleaseDC(HWND hWnd, HDC hDC);

    HWND CreateWindowExA(
        DWORD dwExStyle,
        LPCSTR lpClassName,
        LPCSTR lpWindowName,
        DWORD dwStyle,
        int X,
        int Y,
        int nWidth,
        int nHeight,
        HWND hWndParent,
        HMENU hMenu,
        HINSTANCE hInstance,
        LPVOID lpParam);

    BOOL DestroyWindow(HWND hWnd);
    
    LRESULT DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    BOOL SetWindowTextA(HWND hWnd, LPCSTR lpString);
    BOOL ShowWindow(HWND hWnd, int nCmdShow);
    HWND SetCapture(HWND hWnd);
    BOOL ReleaseCapture();
    
    HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
    HCURSOR SetCursor(HCURSOR hCursor);
}

#endif // WIN32_USER32_H