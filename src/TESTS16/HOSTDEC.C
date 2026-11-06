/* Host regression: decode a file with SRC16/DECODE.C and write raw s16le. */
#include <windows.h>
#include "DECODE.H"
int main(int argc, char **argv)
{
    static RPSTREAM s; static short buf[4096 * 2]; FILE *out; int n; long total = 0;
    if (argc != 3) return 2;
    if (!RpOpen(&s, argv[1])) { fprintf(stderr, "open: %s\n", s.error); return 1; }
    if (argc > 3) {}
    out = fopen(argv[2], "wb");
    printf("codec=%s rate=%lu channels=%u frames=%lu\n", RpCodecName(&s), (unsigned long)s.rate, s.channels, (unsigned long)s.total_frames);
    while ((n = RpDecode(&s, buf, 4096)) > 0) { fwrite(buf, 2 * s.channels, n, out); total += n; }
    if (n < 0) { fprintf(stderr, "decode: %s\n", s.error); return 1; }
    printf("decoded=%ld\n", total); fclose(out); RpClose(&s); return 0;
}
