/* Test program: prints its command line as Chromium would read it.
 * Imports winmm (timeGetTime) so the proxy is loaded like it is for steamwebhelper.exe. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

int main(void)
{
    DWORD t = timeGetTime();   /* forwarded call must work */
    char buffer[4096];
    WideCharToMultiByte(CP_UTF8, 0, GetCommandLineW(), -1, buffer, sizeof(buffer), NULL, NULL);
    printf("CMDLINE=%s\n", buffer);
    printf("TIME_OK=%d\n", t != 0);
    return 0;
}
