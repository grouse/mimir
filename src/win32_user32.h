#ifndef WIN32_USER32_H
#define WIN32_USER32_H

#include "win32_lite.h"

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
    

}

#endif // WIN32_USER32_H