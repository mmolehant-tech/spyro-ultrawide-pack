#define FOVKEYS_TEST
#include "../fovkeys.c"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static void show(const char *t, const char *in, int centi) {
    char out[4096]; long n = rewrite_scalar(in, (long)strlen(in), out, sizeof out, centi);
    assert(n >= 0); out[n] = 0;
    printf("--- %s (%d)\n%s<<<\n", t, centi, out);
}
int main(void) {
    const char *orig = "[MAIN]\r\nOverrideFOV = -1.0f                      ; Set your FOV override here. (Less than 0 = Disabled, More than 0 = FOV value in degrees)\r\nFOVScalar = 1.07f                        ; Set your FOV scalar here.";
    long L = (long)strlen(orig);
    assert(read_centi(orig, L, "FOVScalar", -1) == 107);
    assert(read_centi(orig, L, "OverrideFOV", 0) == -100);
    assert(read_centi(orig, L, "Missing", 42) == 42);
    const char *own = "[Hotkeys]\r\n; comment IncreaseKey = 0x01\r\nIncreaseKey = 0x6B   ; Numpad +\r\nDecreaseKey=109\r\n  ResetKey = 0x6a\r\n[FOV]\r\nStep = 0.01\r\nMin = 0.60\r\nMax = 1.4\r\nResetFOVScalar = 1.075f\r\nBad = \r\n";
    long O = (long)strlen(own);
    assert(read_centi(own, O, "IncreaseKey", 0) / 100 == 0x6B);
    assert(read_centi(own, O, "DecreaseKey", 0) / 100 == 109);
    assert(read_centi(own, O, "ResetKey", 0) / 100 == 0x6A);
    assert(read_centi(own, O, "Step", 0) == 1);
    assert(read_centi(own, O, "Min", 0) == 60);
    assert(read_centi(own, O, "Max", 0) == 140);
    assert(read_centi(own, O, "ResetFOVScalar", 0) == 108);
    assert(read_centi(own, O, "Bad", 7) == 7);
    char b[32]; int n;
    n = format_centi(b, 107); b[n] = 0; assert(!strcmp(b, "1.07f"));
    n = format_centi(b, 100); b[n] = 0; assert(!strcmp(b, "1.00f"));
    n = format_centi(b, 60);  b[n] = 0; assert(!strcmp(b, "0.60f"));
    n = format_centi(b, 140); b[n] = 0; assert(!strcmp(b, "1.40f"));
    /* rewriting keeps everything else byte for byte */
    char out[4096]; long m = rewrite_scalar(orig, L, out, sizeof out, 107);
    assert(m == L && !memcmp(out, orig, L));
    m = rewrite_scalar(orig, L, out, sizeof out, 112); out[m] = 0;
    assert(read_centi(out, m, "FOVScalar", 0) == 112 && read_centi(out, m, "OverrideFOV", 0) == -100 && m == L);
    assert(strstr(out, "FOVScalar = 1.12f                        ; Set your FOV scalar here."));
    show("normal", orig, 95);
    show("no comment, LF", "[MAIN]\nOverrideFOV = -1.0f\nFOVScalar=1.0\nOther = 3\n", 123);
    show("key missing", "[MAIN]\r\nOverrideFOV = -1.0f\r\n", 110);
    show("empty file", "", 100);
    assert(rewrite_scalar(orig, L, out, 50, 100) == -1);
    puts("ALL NATIVE TESTS PASSED");
    return 0;
}
