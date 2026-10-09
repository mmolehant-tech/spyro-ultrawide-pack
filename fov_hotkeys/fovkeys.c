/*
 * SpyroFOVHotkeys - live FOV hotkeys for Spyro Reignited Trilogy.
 *
 * Companion to the "Spyro Reignited Trilogy Widescreen Fix" by xan1242
 * (ThirteenAG's WidescreenFixesPack). That fix multiplies the camera FOV by a
 * number (FOVScalar) it reads once from its ini. This plugin changes that number
 * while the game runs and writes it back to the ini, so the choice is kept.
 *
 * It is loaded by the Ultimate ASI Loader from the "scripts" folder, next to
 * SpyroReignitedTrilogy.WidescreenFix.asi. It does nothing unless it finds
 * exactly the released build of that fix.
 *
 * Plain C, no C runtime: only kernel32 and user32.
 */

/* ---------------------------------------------------------------- pure helpers */

typedef unsigned long long usize;

static usize str_len(const char *s) { usize n = 0; while (s[n]) n++; return n; }

static int is_space(char c) { return c == ' ' || c == '\t'; }

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

/* Start of the line holding `key` (case-insensitive, at line start, followed by
 * blanks or '='), or -1. Lines starting with ';' or '#' or '[' never match. */
static long find_key_line(const char *buf, long len, const char *key)
{
    long klen = (long)str_len(key);
    long i = 0;
    while (i < len) {
        long line = i;
        while (i < len && is_space(buf[i])) i++;
        if (i + klen <= len) {
            long k = 0;
            while (k < klen && lower(buf[i + k]) == lower(key[k])) k++;
            if (k == klen) {
                long j = i + klen;
                while (j < len && is_space(buf[j])) j++;
                if (j < len && buf[j] == '=') return line;
            }
        }
        while (i < len && buf[i] != '\n') i++;
        if (i < len) i++;
    }
    return -1;
}

/* Value of `key` as a number in hundredths (1.07 -> 107, 0x6B -> 10700). Accepts
 * decimal with optional fraction and a trailing 'f', or hex integers. */
static int read_centi(const char *buf, long len, const char *key, int fallback)
{
    long i = find_key_line(buf, len, key);
    if (i < 0) return fallback;
    while (i < len && buf[i] != '=') i++;
    i++;
    while (i < len && is_space(buf[i])) i++;
    int neg = 0;
    if (i < len && (buf[i] == '-' || buf[i] == '+')) { neg = buf[i] == '-'; i++; }
    long whole = 0, digits = 0;
    if (i + 1 < len && buf[i] == '0' && lower(buf[i + 1]) == 'x') {
        i += 2;
        for (; i < len; i++, digits++) {
            char c = lower(buf[i]);
            if (c >= '0' && c <= '9') whole = whole * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') whole = whole * 16 + (c - 'a' + 10);
            else break;
            if (whole > 0xFFFFF) return fallback;
        }
        if (!digits) return fallback;
        return (int)(neg ? -whole : whole) * 100;
    }
    for (; i < len && buf[i] >= '0' && buf[i] <= '9'; i++, digits++) {
        whole = whole * 10 + (buf[i] - '0');
        if (whole > 100000) return fallback;
    }
    long frac = 0, scale = 1;
    if (i < len && buf[i] == '.') {
        i++;
        for (; i < len && buf[i] >= '0' && buf[i] <= '9'; i++, digits++) {
            if (scale < 1000) { frac = frac * 10 + (buf[i] - '0'); scale *= 10; }
        }
    }
    if (!digits) return fallback;
    long centi = whole * 100;
    if (scale == 10) centi += frac * 10;
    else if (scale == 100) centi += frac;
    else if (scale == 1000) centi += (frac + 5) / 10;
    return (int)(neg ? -centi : centi);
}

/* "1.07f" for 107. Returns the number of characters written. */
static int format_centi(char *out, int centi)
{
    int n = 0;
    if (centi < 0) { out[n++] = '-'; centi = -centi; }
    int whole = centi / 100, frac = centi % 100;
    char tmp[12]; int t = 0;
    do { tmp[t++] = (char)('0' + whole % 10); whole /= 10; } while (whole);
    while (t) out[n++] = tmp[--t];
    out[n++] = '.';
    out[n++] = (char)('0' + frac / 10);
    out[n++] = (char)('0' + frac % 10);
    out[n++] = 'f';
    return n;
}

#define VALUE_COLUMN 41 /* the fix's own ini puts its comments in this column */

/* Rewrites the FOVScalar line of the fix's ini in `in` to `out`, keeping every
 * other byte, including the comment on that line. Returns the new length, or
 * -1 when it would not fit. */
static long rewrite_scalar(const char *in, long len, char *out, long cap, int centi)
{
    static const char key[] = "FOVScalar";
    char line[64];
    int n = 0;
    for (const char *k = key; *k; k++) line[n++] = *k;
    line[n++] = ' '; line[n++] = '='; line[n++] = ' ';
    n += format_centi(line + n, centi);

    long at = find_key_line(in, len, key);
    long o = 0;
    if (at < 0) {
        /* no such line: keep the file and add a [MAIN] entry at the end */
        static const char sect[] = "[MAIN]\r\n";
        int has_main = find_key_line(in, len, "OverrideFOV") >= 0;
        if (len + n + 16 > cap) return -1;
        for (long i = 0; i < len; i++) out[o++] = in[i];
        if (len && in[len - 1] != '\n') { out[o++] = '\r'; out[o++] = '\n'; }
        if (!has_main) for (const char *s = sect; *s; s++) out[o++] = *s;
        for (int i = 0; i < n; i++) out[o++] = line[i];
        out[o++] = '\r'; out[o++] = '\n';
        return o;
    }
    long end = at;
    while (end < len && in[end] != '\r' && in[end] != '\n') end++;
    long comment = at;
    while (comment < end && in[comment] != ';') comment++;
    if (at + n + VALUE_COLUMN + (len - comment) + 2 > cap) return -1;
    for (long i = 0; i < at; i++) out[o++] = in[i];
    for (int i = 0; i < n; i++) out[o++] = line[i];
    if (comment < end) {
        do { out[o++] = ' '; } while (o - at < VALUE_COLUMN);
        for (long i = comment; i < end; i++) out[o++] = in[i];
    }
    for (long i = end; i < len; i++) out[o++] = in[i];
    return o;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

#ifndef FOVKEYS_TEST /* ------------------------------------------ the plugin */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int _fltused = 0;

/* the compiler may still ask for these when there is no C runtime */
void *memcpy(void *dst, const void *src, size_t n)
{
    volatile unsigned char *d = (volatile unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}
void *memset(void *dst, int c, size_t n)
{
    volatile unsigned char *d = (volatile unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

#define FIX_ASI "SpyroReignitedTrilogy.WidescreenFix.asi"
#define FIX_INI "SpyroReignitedTrilogy.WidescreenFix.ini"
#define OWN_INI "SpyroFOVHotkeys.ini"

/* The released SpyroReignitedTrilogy.WidescreenFix.asi (built 2023-10-23). */
#define FIX_TIMESTAMP    0x65360102u
#define FIX_IMAGE_SIZE   0x000AF000u
#define RVA_SCALAR       0x000A55A4u /* static float fFOVScalar            */
#define RVA_SCALAR_GUARD 0x000A55A8u /* set once fFOVScalar is initialised */
#define RVA_SCALAR_WRITE 0x000459ADu /* movss [fFOVScalar], xmm0           */
#define RVA_SCALAR_READ  0x000466DCu /* mulss xmm1, [fFOVScalar]           */
static const unsigned char SIG_WRITE[8] = {0xF3, 0x0F, 0x11, 0x05, 0xEF, 0xFB, 0x05, 0x00};
static const unsigned char SIG_READ[8]  = {0xF3, 0x0F, 0x59, 0x0D, 0xC0, 0xEE, 0x05, 0x00};

#define BUF_SIZE 16384
static char g_dir[MAX_PATH];
static char g_path[MAX_PATH + 64];
static char g_in[BUF_SIZE];
static char g_out[BUF_SIZE];

static const char *path_in_dir(const char *name)
{
    int n = 0;
    for (const char *s = g_dir; *s; s++) g_path[n++] = *s;
    for (const char *s = name; *s; s++) g_path[n++] = *s;
    g_path[n] = 0;
    return g_path;
}

static long read_file(const char *path, char *buf, long cap)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, (DWORD)cap, &got, NULL);
    CloseHandle(h);
    return ok ? (long)got : -1;
}

static void write_file(const char *path, const char *buf, long len)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD put = 0;
    WriteFile(h, buf, (DWORD)len, &put, NULL);
    CloseHandle(h);
}

static void save_scalar(int centi)
{
    const char *path = path_in_dir(FIX_INI);
    long len = read_file(path, g_in, BUF_SIZE);
    if (len < 0) len = 0;
    long out = rewrite_scalar(g_in, len, g_out, BUF_SIZE, centi);
    if (out > 0) write_file(path, g_out, out);
}

static int same_bytes(const unsigned char *a, const unsigned char *b, int n)
{
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

/* Base of the fix when it is exactly the build this plugin knows, else NULL. */
static unsigned char *known_fix(void)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA(FIX_ASI);
    if (!base) return NULL;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;
    if (nt->FileHeader.TimeDateStamp != FIX_TIMESTAMP) return NULL;
    if (nt->OptionalHeader.SizeOfImage != FIX_IMAGE_SIZE) return NULL;
    if (!same_bytes(base + RVA_SCALAR_WRITE, SIG_WRITE, 8)) return NULL;
    if (!same_bytes(base + RVA_SCALAR_READ, SIG_READ, 8)) return NULL;
    return base;
}

#ifdef FOVKEYS_SIM
/* test build only: the test harness sets the key that is "held" */
__declspec(dllexport) volatile int FovKeys_SimKey;
static int game_has_focus(void) { return 1; }
static int key_down(int vk) { return vk > 0 && FovKeys_SimKey == vk; }
#else
static int game_has_focus(void)
{
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    if (!w) return 0;
    GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

static int key_down(int vk) { return vk > 0 && (GetAsyncKeyState(vk) & 0x8000) != 0; }
#endif

#define TICK_MS 15
#define REPEAT_DELAY_MS 400
#define REPEAT_RATE_MS 60

static DWORD WINAPI worker(LPVOID unused)
{
    (void)unused;

    /* settings */
    long len = read_file(path_in_dir(OWN_INI), g_in, BUF_SIZE);
    if (len < 0) len = 0;
    int key_more  = read_centi(g_in, len, "IncreaseKey", 0x6B * 100) / 100; /* Numpad + */
    int key_less  = read_centi(g_in, len, "DecreaseKey", 0x6D * 100) / 100; /* Numpad - */
    int key_reset = read_centi(g_in, len, "ResetKey", 0x6A * 100) / 100;    /* Numpad * */
    int step      = clamp(read_centi(g_in, len, "Step", 1), 1, 50);
    int lo        = clamp(read_centi(g_in, len, "Min", 60), 10, 100);
    int hi        = clamp(read_centi(g_in, len, "Max", 140), 100, 200);
    int reset     = clamp(read_centi(g_in, len, "ResetFOVScalar", 100), lo, hi);

    /* wait for the fix to be loaded and to have read its ini */
    unsigned char *base = NULL;
    for (int waited = 0; !base && waited < 120000; waited += 250) {
        base = known_fix();
        if (!base) Sleep(250);
    }
    if (!base) return 0;
    /* MSVC static-init guard: 0 = not yet, -1 = being initialised, anything else = done */
    volatile int *guard = (volatile int *)(base + RVA_SCALAR_GUARD);
    while (*guard == 0 || *guard == -1) Sleep(250);
    volatile float *scalar = (volatile float *)(base + RVA_SCALAR);

    int centi = (int)(*scalar * 100.0f + 0.5f);
    if (centi < 10 || centi > 300) return 0; /* not a sane FOV scalar: leave it alone */

    int held = 0;      /* key currently acted on: 0 none, 1 more, 2 less, 3 reset */
    int held_ms = 0;   /* how long it has been down */
    int next_ms = 0;   /* when the next repeat fires */
    for (;;) {
        Sleep(TICK_MS);
        int now = 0;
        if (game_has_focus()) {
            if (key_down(key_reset)) now = 3;
            else if (key_down(key_more)) now = 1;
            else if (key_down(key_less)) now = 2;
        }
        int fire = 0;
        if (now != held) {
            held = now; held_ms = 0; next_ms = REPEAT_DELAY_MS;
            fire = now != 0;
        } else if (now == 1 || now == 2) {
            held_ms += TICK_MS;
            if (held_ms >= next_ms) { fire = 1; next_ms += REPEAT_RATE_MS; }
        }
        if (!fire) continue;

        int want = now == 3 ? reset : clamp(centi + (now == 1 ? step : -step), lo, hi);
        if (want == centi) continue;
        centi = want;
        *scalar = (float)centi / 100.0f;
        save_scalar(centi);
    }
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        DWORD n = GetModuleFileNameA(self, g_dir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return TRUE;
        while (n && g_dir[n - 1] != '\\' && g_dir[n - 1] != '/') n--;
        g_dir[n] = 0;
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}

#endif /* FOVKEYS_TEST */
