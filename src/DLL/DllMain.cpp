// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "DllMain.h"

#pragma data_seg(".shared")
// all variables below are shared by different instances of the DLL
HHOOK sHook = NULL;

#pragma data_seg()

static HINSTANCE sHinst;
static WORD sLControlScancode;
static WORD sLShiftScancode;
static WORD sBScancode;
static WORD sKScancode;
static WORD sEscapeScancode;
static WORD sHyphenScancode;
static WORD sSpaceScancode;

static ULONGLONG sLastCapsLockDownTime = 0;
static bool sAbortCapsLockConversion = false;
static bool sCapsLockDown = false;

static ULONGLONG sLastLShiftDownTime = 0;
static bool sAbortLShiftConversion = false;
static bool sLShiftDown = false;

static bool sAbortSpace2CtrlConversion = false;
static bool sSpaceDown = false;

static std::set<std::string> sCtrlTapEqualsEsc;
static std::set<std::string> sNormalFunctionKeys;
static std::set<std::string> sNormalSpacebarClasses;
static boost::unordered_map<HWND, LONG> sOrigWindowStyles;

static void _install();
static void _uninstall();

///////////////////////////////////////////////////////////////////////////////
// Utility Functions
///////////////////////////////////////////////////////////////////////////////
//
template <typename Container, typename Key>
inline bool Contains(const Container& container, const Key& key)
{
   return container.find(key) != container.end();
}


static BOOL DirectoryExists(const char* szPath) {
  DWORD dwAttrib = GetFileAttributesA(szPath);

  return ((dwAttrib != INVALID_FILE_ATTRIBUTES) &&
          (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static BOOL FileExists(const char* szPath)
{
  DWORD dwAttrib = GetFileAttributesA(szPath);
  return dwAttrib != INVALID_FILE_ATTRIBUTES;
}

static void InjectKeybdEvent(WORD wVk, WORD wScan, DWORD dwFlags) {
  _uninstall();
  INPUT input;
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = wVk;
  input.ki.wScan = wScan;
  input.ki.dwFlags = dwFlags;
  input.ki.time = 0;
  input.ki.dwExtraInfo = 0;
  SendInput(1, &input, sizeof(INPUT));
  _install();
}

static POINT GetAbsoluteScreenCoordinates(int x, int y) {
  POINT p;
  p.x = static_cast<int>(x * (65536.0 / GetSystemMetrics(SM_CXSCREEN)));
  p.y = static_cast<int>(y * (65536.0 / GetSystemMetrics(SM_CYSCREEN)));
  return p;
}

///////////////////////////////////////////////////////////////////////////////
// Hook Function
///////////////////////////////////////////////////////////////////////////////

static void F3() {
  HWND syrefresh = FindWindowA(NULL, "SyRefresh 4");
  if (!syrefresh)
    return;
  POINT cursor_pos;
  GetCursorPos(&cursor_pos);
  SetForegroundWindow(syrefresh);
  SetWindowPos(syrefresh, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE);
  INPUT click;
  POINT p;
  click.type = INPUT_MOUSE;
  p = GetAbsoluteScreenCoordinates(230, 418);
  click.mi.dx = p.x;
  click.mi.dy = p.y;
  click.mi.mouseData = 0;
  click.mi.time = 0;
  click.mi.dwExtraInfo = 0;

  click.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
  SendInput(1, &click, sizeof(click));

  click.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
  SendInput(1, &click, sizeof(click));

  click.mi.dwFlags = MOUSEEVENTF_LEFTUP;
  SendInput(1, &click, sizeof(click));

  p = GetAbsoluteScreenCoordinates(cursor_pos.x, cursor_pos.y);
  click.mi.dx = p.x;
  click.mi.dy = p.y;
  click.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
  SendInput(1, &click, sizeof(click));
}

static LRESULT CALLBACK LowLevelKeyboardProc(
    int code, WPARAM wParam, LPARAM lParam) {
  // specified by Win32 documentation that you must do this if code is < 0;
  HHOOK dummy = 0;
  if (code < 0)
    return CallNextHookEx(dummy, code, wParam, lParam);

  const KBDLLHOOKSTRUCT* key_info = (const KBDLLHOOKSTRUCT*)lParam;
  char buffer[8192];

  BYTE keyboard_state[256];
  // unexplainable function call to make it work???
  GetKeyState(0);
  GetKeyboardState(keyboard_state);
  bool alt = (keyboard_state[VK_MENU] & 0x80) != 0;
  bool ctrl = (keyboard_state[VK_CONTROL] & 0x80) != 0;
  bool shift = (keyboard_state[VK_SHIFT] & 0x80) != 0;

  HWND foreground_hwnd = GetForegroundWindow();
  std::string foreground_win_class;
  if (foreground_hwnd) {
    GetClassNameA(foreground_hwnd, buffer, 8192);
    foreground_win_class = buffer;
  }
  std::string foreground_win_title;
  if (foreground_hwnd) {
    GetWindowTextA(foreground_hwnd, buffer, 8192);
    foreground_win_title = buffer;
  }

  if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
    if (key_info->vkCode != VK_CAPITAL)
      sAbortCapsLockConversion = true;
    if (key_info->vkCode != VK_LSHIFT)
      sAbortLShiftConversion = true;
    if (key_info->vkCode != VK_SPACE)
      sAbortSpace2CtrlConversion = true;
  }

  switch (key_info->vkCode) {
    case VK_CAPITAL:
      switch (wParam) {
        case WM_KEYDOWN:
          // this guard is to prevent key repeat from resetting the time
          if (!sCapsLockDown) {
            sLastCapsLockDownTime = GetTickCount64();
            sAbortCapsLockConversion = false;
            sCapsLockDown = true;
          }

          InjectKeybdEvent(VK_LCONTROL, sLControlScancode, 0);
          break;
        case WM_KEYUP:
          InjectKeybdEvent(VK_LCONTROL, sLControlScancode, KEYEVENTF_KEYUP);

          ULONGLONG current_tick = GetTickCount64();
          ULONGLONG delta = current_tick - sLastCapsLockDownTime;
          bool in_group =
              sCtrlTapEqualsEsc.find(foreground_win_class) != sCtrlTapEqualsEsc.end();
          if (delta < 333 && !sAbortCapsLockConversion && in_group) {
            InjectKeybdEvent(VK_ESCAPE, sEscapeScancode, 0);
            InjectKeybdEvent(VK_ESCAPE, sEscapeScancode, KEYEVENTF_KEYUP);
          }
          sCapsLockDown = false;
          break;
      }

      // always swallow CAPS LOCK
      return 1;

      break;
    case VK_LSHIFT:
      switch (wParam) {
        case WM_KEYDOWN:
          // this guard is to prevent key repeat from resetting the time
          if (!sLShiftDown) {
            sLastLShiftDownTime = GetTickCount64();
            sAbortLShiftConversion = false;
            sLShiftDown = true;
          }
          //InjectKeybdEvent(VK_LSHIFT, sLShiftScancode, 0);
          break;
        case WM_KEYUP:
          ULONGLONG current_tick = GetTickCount64();
          ULONGLONG delta = current_tick - sLastLShiftDownTime;
          if (delta < 333 && !sAbortLShiftConversion) {
            InjectKeybdEvent(VK_OEM_MINUS, sHyphenScancode, 0);
            InjectKeybdEvent(VK_OEM_MINUS, sHyphenScancode, KEYEVENTF_KEYUP);
          }
          //InjectKeybdEvent(VK_LSHIFT, sLShiftScancode, KEYEVENTF_KEYUP);

          sLShiftDown = false;
          break;
      }

      break;
    case VK_SPACE:
      /*
      switch (wParam) {
        case WM_KEYDOWN:
          if (!sSpaceDown) {
            InjectKeybdEvent(VK_LCONTROL, sLControlScancode, 0);

            sAbortSpace2CtrlConversion = false;
            sSpaceDown = true;
          }

          return 1;
          break;
        case WM_KEYUP:
          InjectKeybdEvent(VK_LCONTROL, sLControlScancode, KEYEVENTF_KEYUP);
          if (!sAbortSpace2CtrlConversion) {
            InjectKeybdEvent(VK_SPACE, sSpaceScancode, 0);
            InjectKeybdEvent(VK_SPACE, sSpaceScancode, KEYEVENTF_KEYUP);
          }

          sSpaceDown = false;

          return 1;
          break;
      }
      */

      break;
    case VK_F1:
      if (wParam == WM_KEYDOWN &&
          !Contains(sNormalFunctionKeys, foreground_win_class)) {
        if (foreground_win_title.find("Microsoft Visual Studio") != std::string::npos) {
          InjectKeybdEvent(VK_LCONTROL, sLControlScancode, 0);
          InjectKeybdEvent(VK_LSHIFT, sLShiftScancode, 0);
          InjectKeybdEvent('B', sBScancode, 0);

          InjectKeybdEvent('B', sBScancode, KEYEVENTF_KEYUP);
          InjectKeybdEvent(VK_LCONTROL, sLControlScancode, KEYEVENTF_KEYUP);
          InjectKeybdEvent(VK_LSHIFT, sLShiftScancode, KEYEVENTF_KEYUP);

          return 1;
        }
        /*
        else {
          STARTUPINFOA startup_info;
          PROCESS_INFORMATION process_info;
          memset(&startup_info, 0, sizeof(startup_info));
          startup_info.cb = sizeof(startup_info);

          std::string exe;
          if (FileExists("C:/Program Files (x86)/Vim/vim73/gvim.exe"))
            exe = "C:/Program Files (x86)/Vim/vim73/gvim.exe";
          if (FileExists("C:/Program Files/Vim/vim73/gvim.exe"))
            exe = "C:/Program Files/Vim/vim73/gvim.exe";
          if (FileExists("C:/Program Files (x86)/Vim/vim74/gvim.exe"))
            exe = "C:/Program Files (x86)/Vim/vim74/gvim.exe";
          if (FileExists("C:/Program Files/Vim/vim74/gvim.exe"))
            exe = "C:/Program Files/Vim/vim74/gvim.exe";

          std::string working_dir;
          if (DirectoryExists("C:/cygwin/home/root"))
            working_dir = "C:/cygwin/home/root";
          if (DirectoryExists("C:/cygwin/home/rko"))
            working_dir = "C:/cygwin/home/rko";

          CreateProcessA(
              exe.c_str(),
              NULL,
              NULL,
              NULL,
              FALSE,
              CREATE_DEFAULT_ERROR_MODE,
              0,
              working_dir.c_str(),
              &startup_info,
              &process_info);
        }
        */
      }
      break;
    /*
    case VK_F2:
      if (wParam == WM_KEYDOWN &&
          !Contains(sNormalFunctionKeys, foreground_win_class)) {
        STARTUPINFOA startup_info;
        PROCESS_INFORMATION process_info;
        memset(&startup_info, 0, sizeof(startup_info));
        startup_info.cb = sizeof(startup_info);
        std::string working_dir;
        if (DirectoryExists("C:/cygwin/home/root"))
          working_dir = "C:/cygwin/home/root";
        if (DirectoryExists("C:/cygwin/home/rko"))
          working_dir = "C:/cygwin/home/rko";
        CreateProcessA(
            "C:/cygwin/bin/mintty.exe",
            NULL,
            NULL,
            NULL,
            FALSE,
            CREATE_DEFAULT_ERROR_MODE,
            0,
            working_dir.c_str(),
            &startup_info,
            &process_info);

        return 1;
      }

      break;
    */
    case VK_F3:
      if (wParam == WM_KEYDOWN &&
          !Contains(sNormalFunctionKeys, foreground_win_class)) {
        F3();
        return 1;
      }

      break;
    case VK_F4:
      if (wParam == WM_KEYDOWN &&
          !Contains(sNormalFunctionKeys, foreground_win_class)) {
        ShellExecuteA(NULL, "open",
                      "C:/SVN/_my_launch.bat",
                      NULL, NULL,
                      SW_MINIMIZE);
        return 1;
      }

      break;
    case VK_ESCAPE:
      if (foreground_win_class == "Sy_ALIVE3_Resource" ||
          foreground_win_class == "Sy_ALIVE4_Resource") {
        PostMessage(foreground_hwnd, WM_QUIT, 0, 0);
        return 1;
      }

      break;
    case VK_F9:
      if (wParam == WM_KEYDOWN && ctrl && shift) {
        auto orig_style = sOrigWindowStyles.find(foreground_hwnd);
        if (orig_style == sOrigWindowStyles.end()) {
          LONG style = GetWindowLong(foreground_hwnd, GWL_STYLE);
          sOrigWindowStyles[foreground_hwnd] = style;
          style &= ~WS_CAPTION;
          style &= ~WS_BORDER;
          style &= ~WS_DLGFRAME;
          style &= ~WS_SIZEBOX;
          SetWindowLong(foreground_hwnd, GWL_STYLE, style);

          SetWindowPos(foreground_hwnd, HWND_TOP, 0, 1080, 0, 0, SWP_NOSIZE);
        } else {
          LONG style = orig_style->second;
          SetWindowLong(foreground_hwnd, GWL_STYLE, style);
          sOrigWindowStyles.erase(foreground_hwnd);

          SetWindowPos(foreground_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE);
        }

        return 1;
      }

      break;
  }

  return CallNextHookEx(dummy, code, wParam, lParam);
}

static void _install() {
  sHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, sHinst, 0);
}

static void _uninstall() {
  if (sHook) {
    UnhookWindowsHookEx(sHook);
    sHook = NULL;
  }
}

KOKOKEYSDLL_API int install(void) {
  sLControlScancode = (WORD) MapVirtualKey(VK_LCONTROL, MAPVK_VK_TO_VSC);
  sBScancode = (WORD) MapVirtualKey('B', MAPVK_VK_TO_VSC);
  sKScancode = (WORD) MapVirtualKey('K', MAPVK_VK_TO_VSC);
  sLShiftScancode = (WORD) MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
  sEscapeScancode = (WORD) MapVirtualKey(VK_ESCAPE, MAPVK_VK_TO_VSC);
  sHyphenScancode = (WORD) MapVirtualKey(VK_OEM_MINUS, MAPVK_VK_TO_VSC);
  sSpaceScancode = (WORD) MapVirtualKey(VK_SPACE, MAPVK_VK_TO_VSC);
  _install();
  return sHook != NULL;
}

KOKOKEYSDLL_API void uninstall(void) {
  _uninstall();
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID) {
  sHinst = hModule;

  if (sCtrlTapEqualsEsc.size() == 0) {
    sCtrlTapEqualsEsc.insert("Vim");
    sCtrlTapEqualsEsc.insert("mintty");
    sCtrlTapEqualsEsc.insert("SynergyDesk");
    // VirtualBox
    sCtrlTapEqualsEsc.insert("QWidget");
    sCtrlTapEqualsEsc.insert("MozillaWindowClass");
  }

  if (sNormalFunctionKeys.size() == 0) {
    sNormalFunctionKeys.insert("StarCraft II");
    sNormalFunctionKeys.insert("ArenaNet_Dx_Window_Class");
    sNormalFunctionKeys.insert("ZSystemClass000");
    sNormalFunctionKeys.insert("Sin of a Solar Empire: Rebellion");
    sNormalFunctionKeys.insert("WinClass_FXS");
    sNormalFunctionKeys.insert("Direct3DWindowClass");
    sNormalFunctionKeys.insert("Valve001");
    sNormalFunctionKeys.insert("SDL_app");
    sNormalFunctionKeys.insert("Sy_ALIVE3_Resource");
    sNormalFunctionKeys.insert("Sy_ALIVE4_Resource");
  }

  if (sNormalSpacebarClasses.size() == 0) {
    sNormalSpacebarClasses.insert("Valve001");
  }

  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
  }
  return TRUE;
}
