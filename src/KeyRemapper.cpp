// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "KeyRemapper.h"

using namespace std;

static KeyRemapper* sInstance = NULL;
static const int TIMEOUT = 333;
static bool toggle_space_back = false;

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

  switch (key_info->vkCode) {
    case VK_LMENU:
      lalt = wParam == WM_KEYDOWN;
      break;
    case VK_LCONTROL:
      lctrl = wParam == WM_KEYDOWN;
      break;
    case VK_LSHIFT:
      lshift = wParam == WM_KEYDOWN;
      break;
  }

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
      wstring unicode_string = to_utf16(ch.ch.cstr);

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
        InjectKey(ch.ch.vk, false);
      } else if (ch.type == kScanCode) {
        InjectKey(ch.ch.vk, true);
      }
    }
    return 1;
  }

  switch (key_info->vkCode) {
  case VK_CAPITAL: {
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
    }
    /*
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
    */
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
      if (wParam == WM_KEYDOWN && lctrl && lshift) {
        auto orig_style = orig_hwnd_styles_.find(foreground_hwnd);
        if (orig_style == orig_hwnd_styles_.end()) {
          LONG style = GetWindowLong(foreground_hwnd, GWL_STYLE);
          orig_hwnd_styles_[foreground_hwnd] = style;
          style &= ~WS_OVERLAPPEDWINDOW;
          SetWindowLong(foreground_hwnd, GWL_STYLE, style);
        } else {
          LONG style = orig_style->second;
          SetWindowLong(foreground_hwnd, GWL_STYLE, style);
          orig_hwnd_styles_.erase(foreground_hwnd);
        }
        InvalidateRect(foreground_hwnd, NULL, true);
        UpdateWindow(foreground_hwnd);

        return 1;
      }
      break;
    }
    case VK_F12: {
      if (wParam == WM_KEYDOWN && lctrl && lshift) {
        SetWindowPos(foreground_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE);
      }
      break;
    }
    case VK_END: {
      if (wParam != WM_KEYDOWN) break;

      toggle_space_back = !toggle_space_back;
      return 1;
      break;
    }
    case VK_BACK: {
      bool up = wParam == WM_KEYDOWN ? false : true;
      WORD key;
      if (toggle_space_back) {
        key = VK_SPACE;
      } else {
        break;
      }
      InjectKey(key, up);
      return 1;
      break;
    }
    case VK_SPACE: {
      bool up = wParam == WM_KEYDOWN ? false : true;
      WORD key;
      if (toggle_space_back) {
        key = VK_BACK;
      } else {
        break;
      }
      InjectKey(key, up);
      return 1;
      break;
    }
  }

  static const int ignored = 0;
  return CallNextHookEx(ignored, code, wParam, lParam);
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

KeyRemapper::KeyRemapper()
    : mode_switch_(false),
      lalt(false),
      lctrl(false),
      lshift(false)
{
  sInstance = this;
  
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
  
  // tilde character '`'
  mode_switch_map_[VK_OEM_3] = {"0"};
  mode_switch_map_['1']      = {"!"};
  mode_switch_map_['2']      = {"@"};
  mode_switch_map_['3']      = {"#"};
  mode_switch_map_['4']      = {"$"};
  mode_switch_map_['5']      = {"%"};
  mode_switch_map_['6']      = {"^"};
  mode_switch_map_['7']      = {"&"};
  mode_switch_map_['8']      = {"*"};
  mode_switch_map_['Q']      = {"θ"};
  mode_switch_map_['W']      = {VK_OEM_5};
  mode_switch_map_['E']      = {"="};
  mode_switch_map_['R']      = {"ρ"};
  mode_switch_map_['T']      = {"~"};
  mode_switch_map_['Y']      = {"υ"};
  mode_switch_map_['U']      = {"ψ"};
  mode_switch_map_['I']      = {VK_TAB};
  mode_switch_map_['O']      = {VK_BACK};
  mode_switch_map_['P']      = {"π"};
  mode_switch_map_['A']      = {"-"};
  mode_switch_map_['S']      = {"_"};
  mode_switch_map_['D']      = {":"};
  mode_switch_map_['F']      = {"φ"};
  mode_switch_map_['G']      = {">"};
  mode_switch_map_['H']      = {"η"};
  mode_switch_map_['J']      = {";"};
  mode_switch_map_['L']      = {"<"};
  mode_switch_map_['Z']      = {"+"};
  mode_switch_map_['X']      = {"χ"};
  mode_switch_map_['C']      = {"σ"};
  mode_switch_map_['V']      = {VK_RETURN};
  mode_switch_map_['B']      = {"β"};
  mode_switch_map_['N']      = {"ν"};
  mode_switch_map_['M']      = {"μ"};
  
  this->install_hook();
}

void KeyRemapper::install_hook(void) {
  mHookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, ::LowLevelKeyboardProc,
                                 NULL, 0);
}

KeyRemapper::~KeyRemapper() {
  this->uninstall_hook();
}

void KeyRemapper::uninstall_hook(void) {
  UnhookWindowsHookEx(mHookHandle);
}
