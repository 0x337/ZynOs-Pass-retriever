/**
 * ZyNOS ROM-0 Config Password Retriever
 *
 * Unpacks LZS-compressed ROM-0 configuration files from ZyXEL routers
 * and extracts the stored password.
 *
 * Based on:
 *   - http://git.kopf-tisch.de/?p=zyxel-revert;a=summary
 *   - https://github.com/OmerMor/SciStudio/tree/master/scistudio
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Types & Constants
 * ---------------------------------------------------------------------- */

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define ROM_BASE_OFFSET   0x2000
#define ROM_PASS_OFFSET   0x14
#define DEST_SIZE_FACTOR  50   /* decompress buffer = filesize * factor */
#define ROM_NAME_TARGET   "autoexec.net"
#define LZS_DATA_SKIP     (0xC + 4)  /* bytes to skip before LZS payload */

#if defined(_MSC_VER)
  #pragma pack(push, 1)
  #define PACKED
#elif defined(__GNUC__)
  #define PACKED __attribute__((packed))
#else
  #error "Unknown compiler: manual struct packing required."
#endif

typedef struct PACKED rom_header {
    u16  version;
    u16  size;
    u16  offset;
    char name[14];
} rom_header_t;

#if defined(_MSC_VER)
  #pragma pack(pop)
#endif

/* -------------------------------------------------------------------------
 * Bit-stream reader
 * ---------------------------------------------------------------------- */

typedef struct {
    const u8 *src;
    u8       *dst;
    u8       *dst_end;  /* pointer past last written byte after unpack */
    u32       src_pos;  /* current bit position in src */
} lzs_ctx_t;

static inline u16 be16(u16 x)
{
    const u8 *b = (const u8 *)&x;
    return (u16)((b[0] << 8) | b[1]);
}

/**
 * Read @num_bits from the bit-stream.
 * Reads up to 3 bytes ahead so @num_bits must be <= 16.
 */
static u32 lzs_read_bits(lzs_ctx_t *ctx, int num_bits)
{
    if (num_bits <= 0)
        return 0;

    int byte_pos = ctx->src_pos / 8;
    int bit_pos  = ctx->src_pos % 8;

    u32 window = ((u32)ctx->src[byte_pos    ] << 16)
               | ((u32)ctx->src[byte_pos + 1] <<  8)
               | ((u32)ctx->src[byte_pos + 2]);

    ctx->src_pos += num_bits;
    return (window >> (24 - num_bits - bit_pos)) & ((1u << num_bits) - 1);
}

/**
 * Decode the variable-length match length field.
 */
static int lzs_read_length(lzs_ctx_t *ctx)
{
    int length = 2;
    int bits;

    do {
        bits    = lzs_read_bits(ctx, 2);
        length += bits;
    } while (bits == 3 && length < 8);

    if (length == 8) {
        do {
            bits    = lzs_read_bits(ctx, 4);
            length += bits;
        } while (bits == 15);
    }

    return length;
}

/* -------------------------------------------------------------------------
 * LZS decompressor
 * ---------------------------------------------------------------------- */

/**
 * Decompress LZS data from ctx->src into ctx->dst.
 *
 * Returns true on success (end-of-stream marker found),
 * false on error (dictionary underflow or other problem).
 */
static bool lzs_decompress(lzs_ctx_t *ctx, u8 *dst_buf, size_t dst_buf_size)
{
    u8 *d = dst_buf;
    u8 *dst_limit = dst_buf + dst_buf_size;

    while (true) {
        /* Literal or reference? */
        if (lzs_read_bits(ctx, 1) == 0) {
            /* Literal byte */
            if (d >= dst_limit) {
                fprintf(stderr, "lzs: output buffer overflow\n");
                return false;
            }
            *d++ = (u8)lzs_read_bits(ctx, 8);
            continue;
        }

        /* Back-reference */
        int  use_short  = lzs_read_bits(ctx, 1);
        int  offset     = lzs_read_bits(ctx, use_short ? 7 : 11);

        if (use_short && offset == 0) {
            /* End-of-stream marker */
            printf("LZS: end-of-stream marker found.\n");
            break;
        }

        u8 *ref = d - offset;
        if (ref < dst_buf) {
            fprintf(stderr, "lzs: dictionary underflow (offset=%d)\n", offset);
            return false;
        }

        int length = lzs_read_length(ctx);
        if (d + length > dst_limit) {
            fprintf(stderr, "lzs: output buffer overflow during copy\n");
            return false;
        }

        while (length--)
            *d++ = *ref++;
    }

    ctx->dst_end = d;
    return true;
}

/* -------------------------------------------------------------------------
 * ROM-0 file processing
 * ---------------------------------------------------------------------- */

/**
 * Read the entire contents of @path into a heap-allocated buffer.
 * Caller is responsible for free()ing the returned pointer.
 * Sets *out_size on success; returns NULL on failure.
 */
static u8 *read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "read_file: invalid file size %ld\n", file_size);
        fclose(f);
        return NULL;
    }

    u8 *buf = malloc((size_t)file_size);
    if (!buf) {
        fprintf(stderr, "read_file: malloc(%ld) failed\n", file_size);
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)file_size, f) != (size_t)file_size) {
        fprintf(stderr, "read_file: short read on \"%s\"\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = (size_t)file_size;
    return buf;
}

/**
 * Write @len bytes from @data to the file at @path.
 * Returns true on success.
 */
static bool write_file(const char *path, const u8 *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return false;
    }

    bool ok = (fwrite(data, 1, len, f) == len);
    fclose(f);
    return ok;
}

/**
 * Parse the ROM-0 file at @in_path, decompress the autoexec.net block,
 * print the embedded password, and write the decompressed data to
 * @in_path + ".dat".
 *
 * Returns 0 on success, -1 on failure.
 */
static int process_rom0(const char *in_path)
{
    size_t file_size = 0;
    u8 *src = read_file(in_path, &file_size);
    if (!src)
        return -1;

    size_t dst_size = file_size * DEST_SIZE_FACTOR;
    u8 *dst = calloc(1, dst_size);
    if (!dst) {
        fprintf(stderr, "process_rom0: calloc(%zu) failed\n", dst_size);
        free(src);
        return -1;
    }

    const u8 *base       = src + ROM_BASE_OFFSET;
    const u8 *src_end    = src + file_size;
    int        block_idx = 0;
    bool       found     = false;
    lzs_ctx_t  ctx       = {0};

    while (true) {
        if (base + sizeof(rom_header_t) > src_end) {
            printf("End of file reached.\n");
            break;
        }

        rom_header_t hdr;
        memcpy(&hdr, base, sizeof(hdr));
        hdr.size   = be16(hdr.size);
        hdr.offset = be16(hdr.offset);

        if (hdr.name[0] == '\0') {
            printf("End of ROM table (empty name).\n");
            break;
        }

        printf("[%02d] offset=0x%04x  size=0x%04x  name=%s\n",
               block_idx++, hdr.offset, hdr.size, hdr.name);

        if (strcmp(hdr.name, ROM_NAME_TARGET) == 0) {
            const u8 *lzs_data = src + ROM_BASE_OFFSET + hdr.offset + LZS_DATA_SKIP;
            if (lzs_data >= src_end) {
                fprintf(stderr, "process_rom0: LZS data pointer out of bounds\n");
                break;
            }

            ctx.src     = lzs_data;
            ctx.src_pos = 0;

            if (lzs_decompress(&ctx, dst, dst_size)) {
                const char *password = (const char *)(dst + ROM_PASS_OFFSET);
                printf("\n>>> Password: %s\n\n", password);
                found = true;
            }
        } else {
            printf("    (skipping)\n");
        }

        base += sizeof(rom_header_t);
    }

    if (!found)
        fprintf(stderr, "Warning: \"%s\" block not found; password not extracted.\n",
                ROM_NAME_TARGET);

    /* Write decompressed output */
    if (ctx.dst_end) {
        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s.dat", in_path);
        size_t written = (size_t)(ctx.dst_end - dst);

        if (write_file(out_path, dst, written))
            printf("Wrote %zu decompressed bytes to \"%s\"\n", written, out_path);
        else
            fprintf(stderr, "process_rom0: failed to write output file\n");
    }

    free(dst);
    free(src);
    return found ? 0 : -1;
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    puts("----------------------------------------------");
    puts(" ZyNOS ROM-0 Config Password Retriever");
    puts("----------------------------------------------\n");

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path_to_rom0_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("Processing \"%s\" ...\n\n", argv[1]);
    int result = process_rom0(argv[1]);

    puts(result == 0 ? "Done." : "Finished with errors.");
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
