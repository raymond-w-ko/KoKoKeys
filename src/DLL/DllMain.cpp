// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "DllMain.h"

using namespace std;

static KeyRemapper* sInstance = NULL;
static const int TIMEOUT = 333;

///////////////////////////////////////////////////////////////////////////////
// Utility Functions
///////////////////////////////////////////////////////////////////////////////

void KeyRemapper::InjectKey(WORD virtual_key_code, bool up) {
  DWORD flags = 0;
  if (up) {
    flags |= KEYEVENTF_KEYUP;
  }

  INPUT input;
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = virtual_key_code;
  input.ki.wScan = (WORD)scancode_of_vkey_[virtual_key_code];
  input.ki.dwFlags = flags;
  input.ki.time = 0;
  input.ki.dwExtraInfo = 0;
  SendInput(1, &input, sizeof(INPUT));
}

std::wstring to_utf16(std::string utf8_string) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
  std::wstring utf16_string = convert.from_bytes(utf8_string);
  return utf16_string;
};

///////////////////////////////////////////////////////////////////////////////
// Hook Function
///////////////////////////////////////////////////////////////////////////////

LRESULT KeyRemapper::LowLevelKeyboardProc(
    int code, WPARAM wParam, LPARAM lParam)
{
  const KBDLLHOOKSTRUCT* key_info = (const KBDLLHOOKSTRUCT*)lParam;

  // specified by Win32 documentation that you must do this if code is < 0
  // also skip injected keys, otherwise we can get a infinite loop
  if (code < 0 || (key_info->flags & LLKHF_INJECTED)) {
abort:
    static const HHOOK ignored = 0;
    return CallNextHookEx(ignored, code, wParam, lParam);
  }

  static const int BUF_LEN = 1024;
  char buffer[BUF_LEN];

  bool alt = (GetAsyncKeyState(VK_MENU) & 0x80) != 0;
  bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x80) != 0;
  bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x80) != 0;

  HWND foreground_hwnd = GetForegroundWindow();
  std::string foreground_win_class;
  std::string foreground_win_title;
  if (foreground_hwnd) {
    GetClassNameA(foreground_hwnd, buffer, BUF_LEN);
    foreground_win_class = buffer;
    GetWindowTextA(foreground_hwnd, buffer, BUF_LEN);
    foreground_win_title = buffer;
  }

  for (auto title : title_blacklist_) {
    if (foreground_win_title.find(title) != std::string::npos) {
      goto abort;
    }
  }

  if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
    if (key_info->vkCode != VK_CAPITAL)
      caps_.abort = true;
    if (key_info->vkCode != VK_LSHIFT)
      lshift_.abort = true;
    if (key_info->vkCode != VK_RETURN)
      return_.abort = true;
  }

  if (mode_switch_ && mode_switch_map_.count(key_info->vkCode) > 0)
  {
    const Character& ch = mode_switch_map_[key_info->vkCode];
    if (ch.type == kUnicode && wParam == WM_KEYDOWN) {
      wstring unicode_string = to_utf16(ch.str);

      INPUT input;
      input.type = INPUT_KEYBOARD;
      input.ki.wVk = 0;
      input.ki.wScan = unicode_string[0];
      input.ki.dwFlags = KEYEVENTF_UNICODE;
      input.ki.time = 0;
      input.ki.dwExtraInfo = 0;
      SendInput(1, &input, sizeof(INPUT));
    } else if (ch.type == kScanCode) {
      if (wParam == WM_KEYDOWN) {
        InjectKey(ch.str[0], false);
      } else if (ch.type == kScanCode) {
        InjectKey(ch.str[0], true);
      }
    }
    return 1;
  }

  switch (key_info->vkCode) {
    case VK_CAPITAL:
      switch (wParam) {
        case WM_KEYDOWN:
          // this guard is to prevent key repeat from resetting the time
          if (!caps_.down) {
            caps_.down_time = GetTickCount64();
            caps_.abort = false;
            caps_.down = true;
          }

          InjectKey(VK_LCONTROL, false);
          break;
        case WM_KEYUP:
          InjectKey(VK_LCONTROL, true);

          auto current_tick = GetTickCount64();
          auto delta = current_tick - caps_.down_time;
          bool in_group = ctrl_tap_esc_.count(foreground_win_class) > 0;
          if (delta < TIMEOUT && !caps_.abort && in_group) {
            InjectKey(VK_ESCAPE, false);
            InjectKey(VK_ESCAPE, true);
          }
          caps_.down = false;
          break;
      }

      // always swallow CAPS LOCK to prevent it from turning on
      return 1;

      break;
    case VK_RETURN: {
      switch (wParam) {
        case WM_KEYDOWN:
          if (!return_.down) {
            return_.down_time = GetTickCount64();
            return_.abort = false;
            return_.down = true;
          }

          InjectKey(VK_RCONTROL, false);
          break;
        case WM_KEYUP:
          InjectKey(VK_RCONTROL, true);

          auto current_tick = GetTickCount64();
          auto delta = current_tick - return_.down_time;
          if (delta < 333 && !return_.abort) {
            InjectKey(VK_RETURN, false);
            InjectKey(VK_RETURN, true);
          }
          return_.down = false;
          break;
      }

      // always swallow Enter
      return 1;

      break;
    }
	/*
    case VK_LSHIFT: {
      if (shift_key_underscore_blacklist_.count(foreground_win_class) > 0)
        break;

      switch (wParam) {
        case WM_KEYDOWN:
          // this guard is to prevent key repeat from resetting the time
          if (!lshift_.down) {
            lshift_.down_time = GetTickCount64();
            lshift_.abort = false;
            lshift_.down = true;
          }
          break;
        case WM_KEYUP:
          ULONGLONG current_tick = GetTickCount64();
          ULONGLONG delta = current_tick - lshift_.down_time;
          if (delta < 333 && !lshift_.abort) {
            InjectKey(VK_OEM_MINUS, false);
            InjectKey(VK_OEM_MINUS, true);
          }
          lshift_.down = false;
          break;
      }
      break;
    }
	*/
    case VK_OEM_1: {
      // always eat ; since it is the mode switch key
      switch (wParam) {
        case WM_KEYDOWN:
          mode_switch_ = true;
          break;
        case WM_KEYUP:
          mode_switch_ = false;
          break;
      }
      return 1;
      break;
    }
    case VK_ESCAPE: {
      if (foreground_win_class == "Sy_ALIVE3_Resource" ||
          foreground_win_class == "Sy_ALIVE4_Resource") {
        PostMessage(foreground_hwnd, WM_QUIT, 0, 0);
        return 1;
      }
      break;
    }
    case VK_F9: {
      if (wParam == WM_KEYDOWN && ctrl && shift) {
        auto orig_style = orig_hwnd_styles_.find(foreground_hwnd);
        if (orig_style == orig_hwnd_styles_.end()) {
          LONG style = GetWindowLong(foreground_hwnd, GWL_STYLE);
          orig_hwnd_styles_[foreground_hwnd] = style;
          style &= ~WS_CAPTION;
          style &= ~WS_BORDER;
          style &= ~WS_DLGFRAME;
          style &= ~WS_SIZEBOX;
          SetWindowLong(foreground_hwnd, GWL_STYLE, style);
        } else {
          LONG style = orig_style->second;
          SetWindowLong(foreground_hwnd, GWL_STYLE, style);
          orig_hwnd_styles_.erase(foreground_hwnd);
        }

        return 1;
      }
      break;
    }
    case VK_F12: {
      if (wParam == WM_KEYDOWN && ctrl && shift) {
        SetWindowPos(foreground_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE);
      }
      break;
    }
  }

  static const int ignored = 0;
  return CallNextHookEx(ignored, code, wParam, lParam);
}

KeyRemapper::KeyRemapper(HMODULE dll_module)
    : dll_module_(dll_module),
      mode_switch_(false)
{
  ctrl_tap_esc_ = {
    "Vim",
    "mintty",
    "SynergyDesk",
    
    // VirtualBox
    "QWidget",
    "MozillaWindowClass",
  };

  normal_fn_keys_ = {
    "StarCraft II",
    "ArenaNet_Dx_Window_Class",
    "ZSystemClass000",
    "Sin of a Solar Empire: Rebellion",
    "WinClass_FXS",
    "Direct3DWindowClass",
    "Valve001",
    "SDL_app",
    "Sy_ALIVE3_Resource",
    "Sy_ALIVE4_Resource",
    "wxWindowClassNR",
  };

  shift_key_underscore_blacklist_ = {
    "TvnWindowClass",
    "vncviewer",
  };

  title_blacklist_ = {
    "Oracle VM VirtualBox",
  };

  vector<UINT> keys = {
    VK_LCONTROL,
    VK_RCONTROL,
    VK_RETURN,
    'B',
    'K',
    VK_LSHIFT,
    VK_ESCAPE,
    VK_OEM_MINUS,
    VK_SPACE,
  };
  for (UINT key : keys) {
    scancode_of_vkey_[key] = MapVirtualKey(key, MAPVK_VK_TO_VSC);
  };
  
  mode_switch_map_[VK_OEM_3] = {"0", kUnicode};
  mode_switch_map_['1']      = {"!", kUnicode};
  mode_switch_map_['2']      = {"@", kUnicode};
  mode_switch_map_['3']      = {"#", kUnicode};
  mode_switch_map_['4']      = {"$", kUnicode};
  mode_switch_map_['5']      = {"%", kUnicode};
  mode_switch_map_['6']      = {"^", kUnicode};
  mode_switch_map_['7']      = {"&", kUnicode};
  mode_switch_map_['8']      = {"*", kUnicode};
  mode_switch_map_['Q']      = {"θ", kUnicode};
  mode_switch_map_['W']      = {"\\", kUnicode};
  mode_switch_map_['E']      = {"=", kUnicode};
  mode_switch_map_['R']      = {"ρ", kUnicode};
  mode_switch_map_['T']      = {"~", kUnicode};
  mode_switch_map_['Y']      = {"υ", kUnicode};
  mode_switch_map_['U']      = {"ψ", kUnicode};
  mode_switch_map_['I']      = {string() + (char)VK_TAB, kScanCode};
  mode_switch_map_['O']      = {string() + (char)VK_DELETE, kScanCode};
  mode_switch_map_['P']      = {"π", kUnicode};
  mode_switch_map_['A']      = {"-", kUnicode};
  mode_switch_map_['S']      = {"_", kUnicode};
  mode_switch_map_['D']      = {":", kUnicode};
  mode_switch_map_['F']      = {"φ", kUnicode};
  mode_switch_map_['G']      = {">", kUnicode};
  mode_switch_map_['H']      = {"η", kUnicode};
  mode_switch_map_['J']      = {";", kUnicode};
  mode_switch_map_['L']      = {"<", kUnicode};
  mode_switch_map_['Z']      = {"+", kUnicode};
  mode_switch_map_['X']      = {"χ", kUnicode};
  mode_switch_map_['C']      = {"{", kUnicode};
  mode_switch_map_['V']      = {string() + (char)VK_RETURN, kScanCode};
  mode_switch_map_['B']      = {"β", kUnicode};
  mode_switch_map_['N']      = {"ν", kUnicode};
  mode_switch_map_['M']      = {"μ", kUnicode};
}

static LRESULT CALLBACK
LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (!sInstance) {
    static const HHOOK ignored = 0;
    return CallNextHookEx(ignored, code, wParam, lParam);
  } else {
    return sInstance->LowLevelKeyboardProc(code, wParam, lParam);
  }
}

HHOOK sHook = NULL;

KOKOKEYSDLL_API int install(void) {
  sHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                           sInstance->dll_module_, 0);
  return sHook != NULL;
}

KOKOKEYSDLL_API void uninstall(void) {
  if (sHook) {
    UnhookWindowsHookEx(sHook);
    sHook = NULL;
  }
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID) {
  if (!sInstance)
    sInstance = new KeyRemapper(hModule);

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
