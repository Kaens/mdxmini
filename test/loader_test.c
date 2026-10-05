#include "mdxmini.h"
#include "lzx042.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sus;
#define TEST(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++sus; } } while (0)

static void vectors(void) {
    unsigned char input[64] = {0};
    unsigned char *out = NULL;
    size_t size = 0, i;
    memcpy(input + 4, "LZX 0.42", 8);
    memcpy(input + 0x26, "\x7f\xff\xff\x4c", 4);
    /* A literal, overlapping short match of length 5 at distance 1,
       then the long-form end marker: 1 0 0 1 1 0 1. */
    input[42] = 0x9a;
    input[43] = 'A'; input[44] = 255;
    input[45] = 255; input[46] = 248; input[47] = 0;
    TEST(mdx_lzx042(input, 48, &out, &size) == 1);
    TEST(size == 6 && out && !memcmp(out, "AAAAAA", 6));
    free(out);
    for (i = 12; i < 48; ++i) {
        TEST(mdx_lzx042(input, i, &out, &size) == -1);
        TEST(!out && !size);
    }
    /* Long match, then extended match, both overlapping at distance 1. */
    input[42] = 0xa8; input[43] = 'A';
    input[44] = 255; input[45] = 255;
    input[46] = 255; input[47] = 248; input[48] = 0;
    TEST(mdx_lzx042(input, 49, &out, &size) == 1);
    TEST(size == 10 && out && !memcmp(out, "AAAAAAAAAA", 10));
    free(out);
    input[45] = 248; input[46] = 10;
    input[47] = 255; input[48] = 248; input[49] = 0;
    TEST(mdx_lzx042(input, 50, &out, &size) == 1);
    TEST(size == 12 && out && !memcmp(out, "AAAAAAAAAAAA", 12));
    free(out);
    input[42] = 0x9a; input[43] = 'A';
    input[45] = 255; input[46] = 248; input[47] = 0;
    input[44] = 254; /* before the start of output */
    TEST(mdx_lzx042(input, 48, &out, &size) == -1);
    input[4] = 0;
    TEST(mdx_lzx042(input, 48, &out, &size) == 0);
}

int main(int argc, char **argv) {
    int i, reject = 0;
    vectors();
    mdx_set_rate(48000);
    for (i = 1; i < argc; ++i) {
        t_mdxmini song = {0};
        short samples[9600];
        unsigned long long energy = 0;
        int block, j, opened, length;
        if (!strcmp(argv[i], "--reject")) { reject = 1; continue; }
        opened = mdx_open(&song, argv[i], NULL) == 0;
        if (reject) {
            TEST(!opened);
            /* Failed opens and repeated close must be safe. */
            mdx_close(&song);
            mdx_close(&song);
            printf("REJECT %s\n", argv[i]);
            continue;
        }
        TEST(opened);
        if (!opened) continue;
        mdx_set_max_loop(&song, 1);
        length = mdx_get_length(&song);
        TEST(length > 0);
        for (block = 0; block < 20; ++block) {
            memset(samples, 0, sizeof(samples));
            mdx_calc_sample(&song, samples, 4800);
            for (j = 0; j < 4800 * song.channels; ++j)
                energy += (long long)samples[j] * samples[j];
        }
        TEST(energy > 0); /* some audio is being played */
        printf("PLAY length=%d energy=%llu %s\n", length, energy, argv[i]);
        mdx_close(&song);
        mdx_close(&song);
    }
    return sus ? 1 : 0;
}
