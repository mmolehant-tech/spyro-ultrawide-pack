#include <windows.h>
#include <stdio.h>
static unsigned char *b; static volatile int *key; static const char *ini;
static float scalar(void) { return *(float *)(b + 0xA55A4); }
static void show_ini(void) {
    char buf[1024]; DWORD n = 0; HANDLE h = CreateFileA(ini, GENERIC_READ, 3, NULL, OPEN_EXISTING, 0, NULL);
    ReadFile(h, buf, sizeof buf - 1, &n, NULL); CloseHandle(h); buf[n] = 0;
    char *p = strstr(buf, "FOVScalar"); char *e = p ? strpbrk(p, "\r\n") : NULL; if (e) *e = 0;
    printf("      ini: %s | file size %lu\n", p ? p : "(missing)", n);
}
static void hold(int vk, int ms, const char *what) {
    *key = vk; Sleep(ms); *key = 0; Sleep(120);
    printf("%-34s -> scalar %.2f\n", what, scalar()); show_ini();
}
int main(int argc, char **argv) {
    char fix[MAX_PATH], plug[MAX_PATH], inip[MAX_PATH];
    snprintf(fix, sizeof fix, "%s\\SpyroReignitedTrilogy.WidescreenFix.asi", argv[1]);
    snprintf(plug, sizeof plug, "%s\\SpyroFOVHotkeys.asi", argv[1]);
    snprintf(inip, sizeof inip, "%s\\SpyroReignitedTrilogy.WidescreenFix.ini", argv[1]); ini = inip;
    b = (unsigned char *)LoadLibraryA(fix);
    HMODULE me = LoadLibraryA(plug);
    if (!b || !me) { printf("load failed\n"); return 1; }
    key = (volatile int *)GetProcAddress(me, "FovKeys_SimKey");
    if (!key) { printf("no sim export\n"); return 1; }
    *(float *)(b + 0xA55A4) = 1.07f; *(int *)(b + 0xA55A8) = (int)0x80000001;  /* fix "has read its ini" */
    Sleep(800);
    printf("start: scalar %.2f\n", scalar()); show_ini();
    hold(0x6B, 60, "tap  Numpad +");
    hold(0x6B, 60, "tap  Numpad +");
    hold(0x6D, 60, "tap  Numpad -");
    hold(0x6B, 760, "hold Numpad + for 0.76 s");
    hold(0x6A, 60, "tap  Numpad * (reset)");
    hold(0x6D, 6000, "hold Numpad - for 6 s (to min)");
    hold(0x6B, 9000, "hold Numpad + for 9 s (to max)");
    hold(0x6A, 60, "tap  Numpad * (reset)");
    hold(0x41, 300, "hold unrelated key A");
    fflush(stdout); ExitProcess(0);
}
