# steam-wine-webhelper-fix

A minimal, 97-line 64-bit `winmm.dll` proxy that fixes the **blank / black Steam client window** under Wine on macOS without modifying Steam's binaries.

## Problem

Modern Steam renders its UI via Chromium Embedded Framework (`steamwebhelper.exe`). By default, Chromium renders in a separate GPU child process, and Wine's macOS display driver (`winemac.drv`) cannot composite content rendered by one process into another process's window — leaving the Steam client completely black.

Passing `-cef-*` flags to `steam.exe` does not fix this because Steam does not forward arbitrary CEF flags to `steamwebhelper.exe`.

## How It Works

1. The proxy DLL is loaded **only** into `steamwebhelper.exe` via Wine's per-application DLL override (`HKCU\Software\Wine\AppDefaults\steamwebhelper.exe\DllOverrides`, `winmm="native,builtin"`).
2. On `DLL_PROCESS_ATTACH`, before Chromium parses its command line, the proxy checks:
   - The current executable is `steamwebhelper.exe`
   - It is the browser (main) process (no `--type=` flag)
   - `--in-process-gpu` is not already present
3. It appends ` --in-process-gpu` to both:
   - `PEB->ProcessParameters->CommandLine`
   - `kernelbase.dll`'s cached `GetCommandLineW()` pointer (located safely by matching `[endbr64] 48 8B 05 <disp32> C3` — `mov rax, [rip+disp32]; ret`; if the instruction pattern does not match, nothing is patched).
4. All `winmm.dll` exports are forwarded transparently to Wine's original `winmm.dll` renamed to `winmm_orig.dll`.

Steam's own files are never touched, and renderer/GPU/utility subprocesses (`--type=...`) as well as games are completely unaffected.

## Build

Requires `x86_64-w64-mingw32-gcc` (mingw-w64):

```bash
./build.sh
# Outputs build/winmm.dll
```

## Usage in a Wine Prefix

1. Copy Wine's original 64-bit `winmm.dll` (`<wine>/lib/wine/x86_64-windows/winmm.dll`) next to `steamwebhelper.exe` as `winmm_orig.dll`.
2. Copy the built `winmm.dll` next to `steamwebhelper.exe`.
3. Enable the per-app DLL override in your Wine prefix:

```reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\Wine\AppDefaults\steamwebhelper.exe\DllOverrides]
"winmm"="native,builtin"
```

## Automated Test

`test/run.sh` builds a small test binary importing `winmm.dll`, runs it under real Wine, and verifies:

- `steamwebhelper.exe` (main process) receives `--in-process-gpu` and forwarded `timeGetTime()` works
- `steamwebhelper.exe --type=renderer` (child process) is untouched
- `other.exe` (unrelated process) is untouched
- `--in-process-gpu` is never appended twice

## Origin

Originally extracted from [Portico](https://portico.bixialab.com) (a macOS Windows Steam runner) and released under the MIT License for the broader Wine-on-macOS community.
