/*
 * Portico: Steam webhelper fix (winmm.dll proxy).
 *
 * Problem: Steam's UI is Chromium (steamwebhelper.exe). Chromium draws in a separate GPU
 * process, and Wine's macOS display driver cannot show what one process draws into another
 * process's window, so Steam's window stays black. Steam does not pass its -cef-* options on
 * to Chromium.
 *
 * Fix: this DLL is loaded only into steamwebhelper.exe (Wine AppDefaults DllOverrides
 * winmm=native,builtin). Before Chromium reads its command line, it appends
 * --in-process-gpu to the browser process's command line, so drawing happens in the process
 * that owns the window. Renderer/GPU/utility child processes (--type=...) are left alone.
 * Every winmm function is forwarded to Wine's own winmm, loaded as winmm_orig.dll.
 *
 * GetCommandLineW() doesn't read the PEB live: kernelbase returns a pointer it cached at
 * startup ("mov rax, [rip+command_lineW]; ret" in Wine, the same shape in Windows). So we
 * update both the PEB and that cached pointer, found by decoding GetCommandLineW's first
 * instruction. If the code doesn't have the expected shape, nothing is patched.
 * Steam's files are not modified.
 */
#include <windows.h>
#include <winternl.h>

static const wchar_t ADDED[] = L" --in-process-gpu";

static int contains(const wchar_t *haystack, size_t len, const wchar_t *needle)
{
    size_t n = lstrlenW(needle), i;
    if (n > len) return 0;
    for (i = 0; i + n <= len; i++)
        if (!memcmp(haystack + i, needle, n * sizeof(wchar_t))) return 1;
    return 0;
}

/* Point kernelbase's cached command-line pointer at our buffer.
 * Expected code: [endbr64] 48 8B 05 <disp32> C3   (mov rax, [rip+disp32]; ret) */
static void patch_cached_pointer(wchar_t *buffer)
{
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    const unsigned char *code;
    wchar_t **cached;
    DWORD old;
    int i;

    if (!kb) kb = GetModuleHandleW(L"kernel32.dll");
    if (!kb) return;
    code = (const unsigned char *)GetProcAddress(kb, "GetCommandLineW");
    if (!code) return;
    for (i = 0; i <= 8; i++) {
        if (code[i] == 0x48 && code[i + 1] == 0x8B && code[i + 2] == 0x05 && code[i + 7] == 0xC3) {
            INT32 disp;
            memcpy(&disp, code + i + 3, sizeof(disp));
            cached = (wchar_t **)(code + i + 7 + disp);
            if (VirtualProtect(cached, sizeof(*cached), PAGE_READWRITE, &old)) {
                *cached = buffer;
                VirtualProtect(cached, sizeof(*cached), old, &old);
            }
            return;
        }
    }
}

static void patch_command_line(void)
{
    PEB *peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    UNICODE_STRING *cmd = &peb->ProcessParameters->CommandLine;
    size_t len = cmd->Length / sizeof(wchar_t), add = lstrlenW(ADDED);
    wchar_t module[MAX_PATH];
    wchar_t *buffer;

    if (!cmd->Buffer) return;
    /* only steamwebhelper.exe, only its browser (main) process, only once */
    if (!GetModuleFileNameW(NULL, module, MAX_PATH)) return;
    CharLowerW(module);
    if (!contains(module, lstrlenW(module), L"steamwebhelper.exe")) return;
    if (contains(cmd->Buffer, len, L"--type=")) return;
    if (contains(cmd->Buffer, len, L"--in-process-gpu")) return;

    buffer = HeapAlloc(GetProcessHeap(), 0, (len + add + 1) * sizeof(wchar_t));
    if (!buffer) return;
    memcpy(buffer, cmd->Buffer, len * sizeof(wchar_t));
    memcpy(buffer + len, ADDED, (add + 1) * sizeof(wchar_t));
    cmd->Buffer = buffer;              /* intentionally never freed: lives for the process */
    cmd->Length = (USHORT)((len + add) * sizeof(wchar_t));
    cmd->MaximumLength = (USHORT)(cmd->Length + sizeof(wchar_t));
    patch_cached_pointer(buffer);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        patch_command_line();
    }
    return TRUE;
}
