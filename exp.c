/**
 * lzs_decompress.c
 *
 * LZS decompressor for ROM-0 / ZyXEL config blobs.
 *
 * Original authors: alguien, KinG Of PiraTeS
 * Rewritten for clarity, safety, and portability.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------------- */

#define HISTORY_SIZE   2048u   /* LZS sliding-window / output buffer size */
#define READ_SIZE      2048u   /* bytes read from input file               */

/* -------------------------------------------------------------------------
 * Bit-stream reader
 * ---------------------------------------------------------------------- */

typedef struct {
    const uint8_t *data;   /* compressed input buffer          */
    size_t         size;   /* total bytes in data              */
    size_t         index;  /* current byte position            */
    int            offset; /* current bit position within byte (0-7) */
} bitstream_t;

/**
 * Read a single bit from the stream.
 * Returns the bit value (0 or 1), or -1 on end-of-data.
 */
static int bs_read_bit(bitstream_t *bs)
{
    if (bs->index >= bs->size)
        return -1;

    int bit = (bs->data[bs->index] >> (7 - bs->offset)) & 1;
    if (++bs->offset == 8) {
        bs->offset = 0;
        bs->index++;
    }
    return bit;
}

/**
 * Read @nbits bits from the stream, MSB first, into a uint32_t.
 * Returns false if the stream runs out of data.
 */
static bool bs_read_bits(bitstream_t *bs, int nbits, uint32_t *out)
{
    uint32_t value = 0;
    for (int i = 0; i < nbits; i++) {
        int bit = bs_read_bit(bs);
        if (bit < 0)
            return false;
        value = (value << 1) | (uint32_t)bit;
    }
    *out = value;
    return true;
}

/**
 * Peek ahead to check for the LZS end-of-stream marker (2-bit 0b11 followed
 * by 7-bit 0x00) without consuming bits from the stream.
 */
static bool bs_is_end_marker(bitstream_t *bs)
{
    bitstream_t saved = *bs;  /* snapshot */
    uint32_t    v;

    if (!bs_read_bits(bs, 2, &v) || v != 3) {
        *bs = saved;
        return false;
    }
    if (!bs_read_bits(bs, 7, &v) || v != 0) {
        *bs = saved;
        return false;
    }

    *bs = saved;  /* peek — do not consume */
    return true;
}

/* -------------------------------------------------------------------------
 * LZS decompressor
 * ---------------------------------------------------------------------- */

/**
 * Decode the variable-length match-length field.
 * Returns the decoded length, or -1 on read error.
 */
static int decode_length(bitstream_t *bs)
{
    uint32_t v;

    /* First 2-bit group */
    if (!bs_read_bits(bs, 2, &v))
        return -1;
    if (v != 3)
        return (int)v + 2;   /* length 2..4 */

    /* Second 2-bit group */
    if (!bs_read_bits(bs, 2, &v))
        return -1;
    if (v != 3)
        return (int)v + 5;   /* length 5..7 */

    /* Extended: consume 4-bit groups until one is != 15 */
    int length = 8;
    do {
        if (!bs_read_bits(bs, 4, &v))
            return -1;
        length += (int)v;
    } while (v == 15);

    return length;
}

/**
 * Decompress LZS-encoded @src_size bytes from @src into a
 * heap-allocated buffer.  The output is NUL-terminated for
 * convenience when the content is text.
 *
 * @out_size is set to the number of decompressed bytes on success.
 * Returns a pointer the caller must free(), or NULL on failure.
 */
static uint8_t *lzs_decompress(const uint8_t *src, size_t src_size,
                                size_t *out_size)
{
    uint8_t *history = calloc(1, HISTORY_SIZE + 1);  /* +1 for NUL */
    if (!history) {
        fprintf(stderr, "lzs_decompress: calloc failed\n");
        return NULL;
    }

    bitstream_t bs = { .data = src, .size = src_size };
    size_t      hpos = 0;

    while (hpos < HISTORY_SIZE) {

        /* Check end-of-stream *before* reading the next token */
        if (bs_is_end_marker(&bs)) {
            /* Consume the marker bits we peeked at */
            uint32_t dummy;
            bs_read_bits(&bs, 2, &dummy);
            bs_read_bits(&bs, 7, &dummy);
            break;
        }

        int flag = bs_read_bit(&bs);
        if (flag < 0) {
            fprintf(stderr, "lzs_decompress: unexpected end of input\n");
            break;
        }

        if (flag == 0) {
            /* ---- Literal byte ---- */
            uint32_t byte;
            if (!bs_read_bits(&bs, 8, &byte)) {
                fprintf(stderr, "lzs_decompress: truncated literal\n");
                break;
            }
            history[hpos++] = (uint8_t)byte;

        } else {
            /* ---- Back-reference ---- */
            int      use_short;
            uint32_t offset_bits;

            use_short = bs_read_bit(&bs);
            if (use_short < 0) break;

            if (!bs_read_bits(&bs, use_short ? 7 : 11, &offset_bits)) {
                fprintf(stderr, "lzs_decompress: truncated offset\n");
                break;
            }

            size_t sp_offset = (size_t)offset_bits;

            if (sp_offset == 0 || sp_offset > hpos) {
                fprintf(stderr,
                    "lzs_decompress: invalid back-reference offset %zu "
                    "(history pos=%zu)\n", sp_offset, hpos);
                break;
            }

            int sp_length = decode_length(&bs);
            if (sp_length < 0) {
                fprintf(stderr, "lzs_decompress: truncated length\n");
                break;
            }

            /* Copy match bytes one at a time to handle overlapping refs */
            for (int i = 0; i < sp_length && hpos < HISTORY_SIZE; i++) {
                history[hpos] = history[hpos - sp_offset];
                hpos++;
            }
        }
    }

    if (out_size)
        *out_size = hpos;

    return history;
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <data.lzs>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* --- Read input file --- */
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    uint8_t *src = malloc(READ_SIZE);
    if (!src) {
        fprintf(stderr, "malloc failed\n");
        fclose(f);
        return EXIT_FAILURE;
    }

    size_t src_size = fread(src, 1, READ_SIZE, f);
    fclose(f);

    if (src_size == 0) {
        fprintf(stderr, "error: file is empty or unreadable\n");
        free(src);
        return EXIT_FAILURE;
    }

    /* --- Decompress --- */
    size_t   out_size = 0;
    uint8_t *out      = lzs_decompress(src, src_size, &out_size);
    free(src);

    if (!out)
        return EXIT_FAILURE;

    /* --- Emit decompressed bytes to stdout --- */
    fwrite(out, 1, out_size, stdout);
    free(out);

    return EXIT_SUCCESS;
}
