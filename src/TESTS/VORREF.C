/* Host reference: libvorbisfile honors single-page and chained granule limits. */
#include <stdio.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>
int main(int argc, char **argv)
{
    OggVorbis_File file;
    char buffer[4096];
    int stream;
    long count;
    if (argc != 2 || ov_fopen(argv[1], &file)) return 1;
    ogg_int64_t expected = ov_pcm_total(&file, -1), total = 0;
    /* Opening a seekable chain probes its final link. Explicitly reset it. */
    if (ov_pcm_seek(&file, 0)) { ov_clear(&file); return 2; }
    while ((count = ov_read(&file, buffer, sizeof(buffer), 0, 2, 1, &stream))) {
        if (count == OV_HOLE) continue;
        if (count < 0) { ov_clear(&file); return 3; }
        vorbis_info *info = ov_info(&file, stream);
        if (!info || info->channels != 1 || info->rate != 48000) return 4;
        if (fwrite(buffer, 1, count, stdout) != (size_t)count) return 5;
        total += count / 2;
    }
    ov_clear(&file);
    if (expected != total) return 6;
    return fflush(stdout) ? 7 : 0;
}
