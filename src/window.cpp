#include "stdafx.h"
#include "window.h"
#include "KeyRemapper.h"

#define MAX_LOADSTRING 100

#ifdef UNICODE
#define stringcopy wcscpy
#else
#define stringcopy strcpy
#endif

HINSTANCE hInst;
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];
NOTIFYICONDATA sNotifyIconData;
HWND sMainHwnd;

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR lpCmdLine,
                       _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  MSG msg;
  HACCEL hAccelTable;

  LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadString(hInstance, IDC_KOKOKEYS, szWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  if (!InitInstance(hInstance, nCmdShow)) {
    return FALSE;
  }

  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_KOKOKEYS));
  
  KeyRemapper* kr = new KeyRemapper;

  while (GetMessage(&msg, NULL, 0, 0)) {
    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  Shell_NotifyIcon(NIM_DELETE, &sNotifyIconData);
  
  delete kr;

  return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style         = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc   = WndProc;
  wcex.cbClsExtra    = 0;
  wcex.cbWndExtra    = 0;
  wcex.hInstance     = hInstance;
  wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_KOKOKEYS));
  wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wcex.lpszMenuName  = MAKEINTRESOURCE(IDC_KOKOKEYS);
  wcex.lpszClassName = szWindowClass;
  wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  return RegisterClassEx(&wcex);
}

void InitTrayIconData() {
  // tray icon
  memset(&sNotifyIconData, 0, sizeof(sNotifyIconData));
  sNotifyIconData.hWnd = sMainHwnd;
  sNotifyIconData.uID = ID_TRAY_APP_ICON;
  sNotifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  sNotifyIconData.uCallbackMessage = WM_TRAY_ICON;
  sNotifyIconData.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_KOKOKEYS));
  // set the tooltip text.  must be LESS THAN 64 chars
  stringcopy(sNotifyIconData.szTip, TEXT("KoKoKeys"));
}

void MinimizeToTray() {
  Shell_NotifyIcon(NIM_ADD, &sNotifyIconData);
  ShowWindow(sMainHwnd, SW_HIDE);
}

void RestoreWindowFromTray() {
  Shell_NotifyIcon(NIM_DELETE, &sNotifyIconData);
  ShowWindow(sMainHwnd, SW_SHOW);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   sMainHwnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!sMainHwnd) {
      return FALSE;
   }

   InitTrayIconData();
   MinimizeToTray();

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, wmEvent;
  PAINTSTRUCT ps;
  HDC hdc;

  switch (message) {
    case WM_COMMAND: {
      wmId = LOWORD(wParam);
      wmEvent = HIWORD(wParam);
      // Parse the menu selections:
      switch (wmId) {
        case IDM_ABOUT:
          DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
          break;
        case IDM_EXIT:
          DestroyWindow(hWnd);
          break;
        default:
          return DefWindowProc(hWnd, message, wParam, lParam);
      }
      break;
    }
    case WM_PAINT: {
      hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      break;
    }
    case WM_DESTROY: {
      PostQuitMessage(0);
      break;
    }
    case WM_TRAY_ICON: {
      switch (wParam) {
        case ID_TRAY_APP_ICON: {
          switch (lParam) {
            case WM_LBUTTONUP:
              RestoreWindowFromTray();
              break;
            case WM_RBUTTONUP:
              PostMessage(sMainHwnd, WM_QUIT, 0, 0);
              break;
          }
          break;
        }
        default:
          break;
      }
      break;
    }
    case WM_SYSCOMMAND: {
      switch (wParam) {
        case SC_MINIMIZE:
          MinimizeToTray();
          break;
        default:
          return DefWindowProc(hWnd, message, wParam, lParam);
          break;
      }
      break;
    }
    default: {
      return DefWindowProc(hWnd, message, wParam, lParam);
      break;
    }
  }
  return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);
  switch (message)
  {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
    {
      EndDialog(hDlg, LOWORD(wParam));
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}
