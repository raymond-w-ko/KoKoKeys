#pragma once

#define KOKOKEYSDLL_API extern "C" __declspec(dllexport)

class KeyRemapper {
 public:
  KeyRemapper(HMODULE dll_module);
  ~KeyRemapper();

  LRESULT LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam);

  const HMODULE dll_module_;

 private:
  enum CharacterType {
    kScanCode,
    kUnicode,
  };
  struct Character {
    CharacterType type;

    union CHAR {
      const char* cstr;
      WORD vk;
    } ch;

    Character() {
      this->type = kScanCode;
      this->ch.vk = VK_SPACE;
    }

    Character(const char* unicode_char) {
      this->type = kUnicode;
      this->ch.cstr = unicode_char;
    }

    Character(WORD vk) {
      this->type = kScanCode;
      this->ch.vk = vk;
    }
  };

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

  bool mode_switch_;
  std::unordered_map<UINT, Character> mode_switch_map_;
};
