static BOOL DirectoryExists(const std::string path) {
  DWORD dwAttrib = GetFileAttributesA(path.c_str());

  return ((dwAttrib != INVALID_FILE_ATTRIBUTES) &&
          (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static BOOL FileExists(const char* szPath)
{
  DWORD dwAttrib = GetFileAttributesA(szPath);
  return dwAttrib != INVALID_FILE_ATTRIBUTES;
}

static std::string GetCygwinDir() {
  std::string dir;

  dir = "C:/cygwin";
  if (DirectoryExists(dir))
    return dir;

  dir = "C:/cygwin64";
  if (DirectoryExists(dir))
    return dir;

  dir = "";
  return dir;
}

static POINT GetAbsoluteScreenCoordinates(int x, int y) {
  POINT p;
  p.x = static_cast<int>(x * (65536.0 / GetSystemMetrics(SM_CXSCREEN)));
  p.y = static_cast<int>(y * (65536.0 / GetSystemMetrics(SM_CYSCREEN)));
  return p;
}

static void SyRefresh() {
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

static void StartVim() {
  std::string exe;
  if (FileExists("C:/Program Files (x86)/Vim/vim73/gvim.exe"))
    exe = "C:/Program Files (x86)/Vim/vim73/gvim.exe";
  if (FileExists("C:/Program Files/Vim/vim73/gvim.exe"))
    exe = "C:/Program Files/Vim/vim73/gvim.exe";
  if (FileExists("C:/Program Files (x86)/Vim/vim74/gvim.exe"))
    exe = "C:/Program Files (x86)/Vim/vim74/gvim.exe";
  if (FileExists("C:/Program Files/Vim/vim74/gvim.exe"))
    exe = "C:/Program Files/Vim/vim74/gvim.exe";

  std::string candidate;
  std::string working_dir;

  std::string cygwin_dir = GetCygwinDir();
  candidate = cygwin_dir + "/home/root";
  if (DirectoryExists(candidate))
    working_dir = candidate;

  candidate = cygwin_dir + "/home/rko";
  if (DirectoryExists(candidate))
    working_dir = candidate;

  ShellExecuteA(
      NULL,
      "open",
      exe.c_str(),
      "",
      working_dir.c_str(),
      SW_SHOWDEFAULT);
}

static void StartMintty() {
  std::string working_dir;

  std::string cygwin_dir = GetCygwinDir();
  std::string candidate;

  candidate = cygwin_dir + "/home/root";
  if (DirectoryExists(candidate))
    working_dir = candidate;

  candidate = cygwin_dir + "/home/rko";
  if (DirectoryExists(candidate))
    working_dir = candidate;

  ShellExecuteA(
      NULL,
      "open",
      (cygwin_dir + "/bin/mintty.exe").c_str(),
      "-i /Cygwin-Terminal.ico -",
      working_dir.c_str(),
      SW_SHOWDEFAULT);
}

