#pragma once

#define KOKOKEYSDLL_API extern "C" __declspec(dllexport)

class KeyRemapper {
 public:
  KeyRemapper(HMODULE dll_module);
  ~KeyRemapper();

  LRESULT LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam);

  const HMODULE dll_module_;

 private:
  void InjectKey(WORD virtual_key_code, bool up);

  std::unordered_set<std::string> ctrl_tap_esc_;
  std::unordered_set<std::string> normal_fn_keys_;
  std::unordered_set<std::string> shift_key_underscore_blacklist_;
  std::unordered_set<std::string> title_blacklist_;
  std::unordered_map<UINT, UINT> scancode_of_vkey_;
  std::unordered_map<HWND, LONG> orig_hwnd_styles_;

  struct ConversionState {
    ConversionState()
        : down_time(0),
          abort(false),
          down(false)
    {
    }
    ULONGLONG down_time;
    bool abort;
    bool down;
  };

  ConversionState caps_;
  ConversionState return_;
  ConversionState lshift_;
};
