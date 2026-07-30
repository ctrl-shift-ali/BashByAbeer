# CustomTerminal

A from-scratch Windows terminal, built the same way real ones are:

- **`shell/shell.cpp`** — the shell. Reads a command line, runs built-ins
  (`cd`, `dir`, `cls`, `echo`, `set`, `type`, `copy`, `del`, `mkdir`, etc.),
  and launches external `.exe`s found on `PATH`. Supports `>`, `>>`, `<`
  redirection and `|` piping.
- **`terminal/terminal.cpp`** — the terminal. Opens a window, creates a
  **ConPTY** (Windows' pseudoconsole API — the same one Windows Terminal
  and VS Code's integrated terminal use), launches `shell.exe` attached to
  it, parses the ANSI/VT escape codes it emits, and draws the result with
  GDI. Keyboard input is forwarded straight into the pseudoconsole.

This mirrors the real architecture: CMD.exe is a shell, `conhost.exe`/
Windows Terminal is the console host. Git Bash is `bash` (shell) inside
`mintty` (terminal). Here, `shell.exe` and `terminal.exe` play those roles.

## Requirements

- Windows 10, build **17763** (October 2018 Update) or later — this is the
  minimum version with the ConPTY API (`CreatePseudoConsole`).
- A C++ compiler: either **MinGW-w64** (`g++`) or **MSVC** (Visual Studio
  Build Tools / Developer Command Prompt).

## Build

Open a terminal on Windows in this folder.

**Shell (build first):**

```
cd shell
g++ -O2 -std=c++17 -static -o shell.exe shell.cpp
```

MSVC equivalent:
```
cl /O2 /EHsc /std:c++17 /MT shell.cpp /Fe:shell.exe
```

**Terminal:**

```
cd ..\terminal
g++ -O2 -std=c++17 -static -municode -o terminal.exe terminal.cpp -lgdi32 -luser32
```

MSVC equivalent:
```
cl /O2 /EHsc /std:c++17 /MT terminal.cpp /Fe:terminal.exe user32.lib gdi32.lib
```

**Then copy `shell.exe` next to `terminal.exe`** (same folder) — the
terminal launches `shell.exe` by relative path. Double-click
`terminal.exe`, and you have your own terminal window running your own
shell.

The `-static` (or `/MT`) flags statically link the C/C++ runtime, so
each `.exe` is fully portable — copy it to any Windows machine and run it,
no installer, no DLLs to ship alongside it.

## Try just the shell first

You don't need the GUI terminal at all to test `shell.cpp` — build it and
run `shell.exe` directly inside any existing console (CMD, PowerShell,
Windows Terminal). It behaves like a drop-in CMD replacement on its own.

## What's implemented vs. simplified

Implemented:
- Prompt showing current directory, built-in commands, PATH-based external
  program execution, `>` `>>` `<` redirection, `|` piping
- ConPTY-backed terminal window, ANSI color (SGR) rendering, cursor
  movement, screen/line erase, scrolling, live resize
- Arrow keys forwarded to the shell

Simplified (real terminals like Windows Terminal handle much more):
- No command history / tab completion in `shell.exe` yet — arrow-key
  recall isn't implemented in the shell's `std::getline` loop
- ANSI parser handles the common CSI sequences (colors, cursor moves,
  erase) but not the full VT100/VT220 spec (no alternate screen buffer,
  no scrollback buffer in the window, no mouse reporting, no OSC
  sequences for window title, no wide/combining Unicode handling)
- Only single-byte code page input is forwarded (no full UTF-16 surrogate
  pair handling for input)
- 16-color palette only, no 256-color / true-color SGR codes

## Suggested next steps, in order

1. Add command history (store previous lines in a `std::vector`, wire up
   arrow-key handling with `ReadConsoleInput` instead of `std::getline`)
2. Add a scrollback buffer to `terminal.cpp` (keep N screens of history,
   let PageUp/PageDown or the mouse wheel scroll through it)
3. Add 256-color and true-color (`38;2;r;g;b`) SGR support in the parser
4. Add OSC sequence handling so `shell.exe` can set the window title
5. Add copy/paste (select text with mouse, `Ctrl+Shift+C/V`)
6. Add tabs (multiple ConPTY sessions in one window)

Each of these is additive — the architecture (ConPTY + ANSI parser + GDI
grid) doesn't change, you're just extending the parser's dispatch table
and the window's message handling.
