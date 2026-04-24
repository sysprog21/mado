/*
 * Twin - A Tiny Window System
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <twin.h>

#include "../backend/headless-shm.h"

/* Get current timestamp in microseconds */
static uint64_t get_timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Format timestamp for display */
static void format_timestamp(uint64_t timestamp, char *buffer, size_t size)
{
    time_t sec = timestamp / 1000000;
    int usec = timestamp % 1000000;
    struct tm *tm = localtime(&sec);
    snprintf(buffer, size, "%02d:%02d:%02d.%06d", tm->tm_hour, tm->tm_min,
             tm->tm_sec, usec);
}

/* Calculate time difference in milliseconds */
static double time_diff_ms(uint64_t start, uint64_t end)
{
    return (double) (end - start) / 1000.0;
}

/* Writes PNG files without external dependencies */

/* Portable byte swap for 32-bit values */
#if defined(__GNUC__) || defined(__clang__)
#define BSWAP32(x) __builtin_bswap32(x)
#else
static inline uint32_t bswap32(uint32_t x)
{
    return ((x & 0x000000ff) << 24) | ((x & 0x0000ff00) << 8) |
           ((x & 0x00ff0000) >> 8) | ((x & 0xff000000) >> 24);
}
#define BSWAP32(x) bswap32(x)
#endif

/* CRC32 table for PNG chunk verification */
static const uint32_t crc32_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4,
    0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
};

/* Calculate CRC32 checksum */
static uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        crc = (crc >> 4) ^ crc32_table[crc & 0x0f];
        crc = (crc >> 4) ^ crc32_table[crc & 0x0f];
    }
    return ~crc;
}

/* Write PNG file with minimal dependencies */
static int save_png(const char *filename,
                    const twin_argb32_t *pixels,
                    int width,
                    int height)
{
    if (width <= 0 || height <= 0)
        return -1;

    /* Guard against integer overflow in size calculations.
     * Check each multiplication step before it executes, so wrapped
     * results never feed into downstream arithmetic.
     */
    if ((size_t) width > (SIZE_MAX - 1) / 4)
        return -1;
    size_t row_size = 1 + (size_t) width * 4;
    if ((size_t) height > SIZE_MAX / row_size)
        return -1;
    size_t raw_sz = (size_t) height * row_size;
    /* Uncompressed DEFLATE: 2 (zlib hdr) + 5 per block + raw + 4 (adler).
     * Ensure the total fits in a 32-bit PNG chunk length field.
     */
    size_t nblocks = (raw_sz + 65534) / 65535;
    size_t idat_bound = 2 + nblocks * 5 + raw_sz + 4;
    if (idat_bound > UINT32_MAX)
        return -1;

    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return -1;

    /* PNG magic bytes */
    static const uint8_t png_sig[8] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    };
    fwrite(png_sig, 1, 8, fp);

/* Helper macros for writing PNG chunks */
#define PUT_U32(u)           \
    do {                     \
        uint8_t b[4];        \
        b[0] = (u) >> 24;    \
        b[1] = (u) >> 16;    \
        b[2] = (u) >> 8;     \
        b[3] = (u);          \
        fwrite(b, 1, 4, fp); \
    } while (0)

#define PUT_BYTES(buf, len) fwrite(buf, 1, len, fp)

    /* Write IHDR chunk */
    uint8_t ihdr[13];
    uint32_t *p32 = (uint32_t *) ihdr;
    p32[0] = BSWAP32(width);
    p32[1] = BSWAP32(height);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 6;  /* color type: RGBA */
    ihdr[10] = 0; /* compression */
    ihdr[11] = 0; /* filter */
    ihdr[12] = 0; /* interlace */

    PUT_U32(13); /* chunk length */
    PUT_BYTES("IHDR", 4);
    PUT_BYTES(ihdr, 13);
    uint32_t crc = crc32(0, (uint8_t *) "IHDR", 4);
    crc = crc32(crc, ihdr, 13);
    PUT_U32(crc);

    /* Write IDAT chunk (sizes validated above) */
    size_t raw_size = raw_sz;
    size_t max_deflate_size = idat_bound;

    uint8_t *idat = malloc(max_deflate_size);
    if (!idat) {
        fclose(fp);
        return -1;
    }

    /* Simple uncompressed DEFLATE block */
    size_t idat_size = 0;
    idat[idat_size++] = 0x78; /* ZLIB header */
    idat[idat_size++] = 0x01;

    /* Write uncompressed blocks */
    uint8_t *raw_data = malloc(raw_size);
    if (!raw_data) {
        free(idat);
        fclose(fp);
        return -1;
    }

    /* Convert ARGB to RGBA with filter bytes */
    size_t raw_pos = 0;
    for (int y = 0; y < height; y++) {
        raw_data[raw_pos++] = 0; /* filter type: none */
        for (int x = 0; x < width; x++) {
            twin_argb32_t pixel = pixels[y * width + x];
            raw_data[raw_pos++] = (pixel >> 16) & 0xff; /* R */
            raw_data[raw_pos++] = (pixel >> 8) & 0xff;  /* G */
            raw_data[raw_pos++] = pixel & 0xff;         /* B */
            raw_data[raw_pos++] = (pixel >> 24) & 0xff; /* A */
        }
    }

    /* Write as uncompressed DEFLATE blocks */
    size_t pos = 0;
    while (pos < raw_size) {
        size_t chunk = raw_size - pos;
        if (chunk > 65535)
            chunk = 65535;

        /* final block flag */
        idat[idat_size++] = (pos + chunk >= raw_size) ? 1 : 0;
        idat[idat_size++] = chunk & 0xff;
        idat[idat_size++] = (chunk >> 8) & 0xff;
        idat[idat_size++] = ~chunk & 0xff;
        idat[idat_size++] = (~chunk >> 8) & 0xff;

        memcpy(idat + idat_size, raw_data + pos, chunk);
        idat_size += chunk;
        pos += chunk;
    }

    /* ADLER32 checksum: A = 1 + sum(bytes) mod 65521,
     * B = sum(running A) mod 65521, result = (B << 16) | A
     */
    uint32_t adler_a = 1, adler_b = 0;
    for (size_t i = 0; i < raw_size; i++) {
        adler_a = (adler_a + raw_data[i]) % 65521;
        adler_b = (adler_b + adler_a) % 65521;
    }
    uint32_t adler = (adler_b << 16) | adler_a;
    idat[idat_size++] = (adler >> 24) & 0xff;
    idat[idat_size++] = (adler >> 16) & 0xff;
    idat[idat_size++] = (adler >> 8) & 0xff;
    idat[idat_size++] = adler & 0xff;

    PUT_U32(idat_size);
    PUT_BYTES("IDAT", 4);
    PUT_BYTES(idat, idat_size);
    crc = crc32(0, (uint8_t *) "IDAT", 4);
    crc = crc32(crc, idat, idat_size);
    PUT_U32(crc);

    /* Write IEND chunk */
    PUT_U32(0);
    PUT_BYTES("IEND", 4);
    PUT_U32(crc32(0, (uint8_t *) "IEND", 4));

#undef PUT_U32
#undef PUT_BYTES

    free(raw_data);
    free(idat);
    fclose(fp);
    return 0;
}

static twin_headless_shm_t *connect_to_backend(int *fd_out, size_t *size_out)
{
    int fd = shm_open(TWIN_HEADLESS_SHM_NAME, O_RDWR, 0666);
    if (fd < 0) {
        fprintf(stderr, "Failed to open shared memory: %s\n", strerror(errno));
        fprintf(stderr, "Is the headless backend running?\n");
        return NULL;
    }

    /* Get size */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "Failed to get shared memory size: %s\n",
                strerror(errno));
        close(fd);
        return NULL;
    }

    /* Map shared memory */
    void *addr =
        mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Failed to map shared memory: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }

    twin_headless_shm_t *shm = (twin_headless_shm_t *) addr;

    /* Verify magic and version */
    if (shm->magic != TWIN_HEADLESS_MAGIC) {
        fprintf(stderr, "Invalid shared memory magic\n");
        munmap(addr, st.st_size);
        close(fd);
        return NULL;
    }

    if (shm->version != TWIN_HEADLESS_VERSION) {
        fprintf(stderr, "Version mismatch (expected %d, got %d)\n",
                TWIN_HEADLESS_VERSION, shm->version);
        munmap(addr, st.st_size);
        close(fd);
        return NULL;
    }

    *fd_out = fd;
    *size_out = st.st_size;
    return shm;
}

static int send_command(twin_headless_shm_t *shm,
                        twin_headless_cmd_t cmd,
                        const char *data)
{
    static uint32_t seq = 0;

    shm->command = cmd;
    if (data)
        strncpy(shm->command_data, data, sizeof(shm->command_data) - 1);
    else
        shm->command_data[0] = '\0';

    shm->command_seq = ++seq;

    /* Wait for response (with timeout) */
    int timeout = 30; /* 3 seconds total */
    while (shm->response_seq != seq && timeout > 0) {
        usleep(100000); /* 100ms */
        timeout--;
    }

    if (timeout == 0) {
        fprintf(stderr, "Command timeout (backend may be unresponsive)\n");
        return -1;
    }

    if (shm->response_status != 0) {
        fprintf(stderr, "Command failed: %s\n", shm->response_data);
        return -1;
    }

    printf("%s\n", shm->response_data);
    return 0;
}

static int inject_mouse_event(twin_headless_shm_t *shm,
                              const char *type,
                              int x,
                              int y,
                              int button)
{
    /* Validate coordinates */
    if (x < 0 || y < 0 || x >= shm->width || y >= shm->height) {
        fprintf(stderr, "Invalid coordinates (%d, %d) - screen size is %dx%d\n",
                x, y, shm->width, shm->height);
        return -1;
    }

    /* Check for event queue overflow */
    uint32_t pending = shm->event_write_idx - shm->event_read_idx;
    if (pending >= TWIN_HEADLESS_MAX_EVENTS) {
        fprintf(stderr,
                "Event queue full (%u pending) - backend may be stalled\n",
                pending);
        return -1;
    }

    uint32_t idx = shm->event_write_idx % TWIN_HEADLESS_MAX_EVENTS;
    twin_headless_event_t *evt = &shm->events[idx];

    if (strcmp(type, "move") == 0) {
        evt->kind = TwinEventMotion;
    } else if (strcmp(type, "down") == 0) {
        evt->kind = TwinEventButtonDown;
    } else if (strcmp(type, "up") == 0) {
        evt->kind = TwinEventButtonUp;
    } else {
        fprintf(stderr, "Unknown mouse event type: %s (use: move/down/up)\n",
                type);
        return -1;
    }

    evt->u.pointer.x = x;
    evt->u.pointer.y = y;
    evt->u.pointer.button = button;

    shm->event_write_idx++;
    return 0;
}


static void print_usage(const char *prog)
{
    printf("Usage: %s [options] command\n", prog);
    printf("\nCommands:\n");
    printf("  status              Get backend status\n");
    printf("  shot FILE           Save screenshot to FILE.png\n");
    printf("  shutdown            Shutdown the backend\n");
    printf("  mouse TYPE X Y [B]  Inject mouse event (TYPE: move/down/up)\n");
    printf("  monitor             Monitor backend activity\n");
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help\n");
    printf("\nExamples:\n");
    printf("  %s status\n", prog);
    printf("  %s shot output.png\n", prog);
    printf("  %s mouse move 100 200\n", prog);
    printf("  %s mouse down 100 200 1\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* Parse options */
    int opt;
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0},
    };

    while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[optind];

    /* Connect to backend */
    int shm_fd;
    size_t shm_size;
    twin_headless_shm_t *shm = connect_to_backend(&shm_fd, &shm_size);
    if (!shm)
        return 1;

    int ret = 0;

    /* Support aliases for shot command */
    if (strcmp(command, "s") == 0 || strcmp(command, "screenshot") == 0) {
        command = "shot";
    }

    if (strcmp(command, "status") == 0) {
        ret = send_command(shm, TWIN_HEADLESS_CMD_GET_STATE, NULL);
        if (ret == 0) {
            uint64_t now = get_timestamp();
            char last_activity_str[32];
            format_timestamp(shm->last_activity_timestamp, last_activity_str,
                             sizeof(last_activity_str));

            printf("Screen: %dx%d\n", shm->width, shm->height);
            printf("Rendering: %u FPS, %u frames total\n",
                   shm->frames_per_second, shm->frames_rendered_total);
            printf("Events: %u total (%u/sec, %u mouse)\n",
                   shm->events_processed_total, shm->events_processed_second,
                   shm->mouse_events_total);
            printf("Commands: %u\n", shm->commands_processed_total);

            /* Event queue status */
            uint32_t pending = shm->event_write_idx - shm->event_read_idx;
            if (pending > 0) {
                printf("Event Queue: %u pending\n", pending);
            }

            printf("Last Activity: %s\n", last_activity_str);

            double age_ms = time_diff_ms(shm->last_activity_timestamp, now);
            if (age_ms < 1000) {
                printf("Status: Active (%.1fms ago)\n", age_ms);
            } else if (age_ms < 5000) {
                printf("Status: Idle (%.1fs ago)\n", age_ms / 1000.0);
            } else {
                printf("Status: Stale (%.1fs ago)\n", age_ms / 1000.0);
            }
        }
    } else if (strcmp(command, "shot") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Missing filename argument\n");
            ret = 1;
        } else {
            const char *filename = argv[optind + 1];

            /* Get front buffer pointer (first buffer after header) */
            size_t header_size = sizeof(twin_headless_shm_t);
            twin_argb32_t *fb = (twin_argb32_t *) ((char *) shm + header_size);

            /* Save as PNG */
            if (save_png(filename, fb, shm->width, shm->height) < 0) {
                fprintf(stderr, "Failed to save screenshot: %s\n",
                        strerror(errno));
                ret = 1;
            } else {
                printf("Screenshot saved to %s\n", filename);
            }
        }
    } else if (strcmp(command, "shutdown") == 0) {
        ret = send_command(shm, TWIN_HEADLESS_CMD_SHUTDOWN, NULL);
    } else if (strcmp(command, "mouse") == 0) {
        if (optind + 3 >= argc) {
            fprintf(stderr, "Usage: mouse TYPE X Y [BUTTON]\n");
            ret = 1;
        } else {
            const char *type = argv[optind + 1];
            int x = atoi(argv[optind + 2]);
            int y = atoi(argv[optind + 3]);
            int button = (optind + 4 < argc) ? atoi(argv[optind + 4]) : 0;

            ret = inject_mouse_event(shm, type, x, y, button);
            if (ret == 0)
                printf("Mouse event injected\n");
        }
    } else if (strcmp(command, "monitor") == 0) {
        printf("Monitoring backend activity (Ctrl+C to stop)...\n");
        printf("Time             Events  Frames   FPS  Status\n");
        printf("---------------  ------  ------  ----  --------\n");

        uint32_t last_events = shm->events_processed_total;
        uint32_t last_frames = shm->frames_rendered_total;
        uint64_t last_timestamp = shm->last_activity_timestamp;
        uint64_t last_print_time = get_timestamp();

        while (1) {
            uint64_t now = get_timestamp();
            bool events_updated = (shm->events_processed_total != last_events);
            bool frames_updated = (shm->frames_rendered_total != last_frames);
            bool timestamp_updated =
                (shm->last_activity_timestamp != last_timestamp);

            /* Print update when something changes or every second */
            uint64_t since_print = now - last_print_time;
            if (events_updated || frames_updated || timestamp_updated ||
                since_print >= 1000000) {
                char time_str[20];
                format_timestamp(shm->last_activity_timestamp, time_str,
                                 sizeof(time_str));

                double age_ms = time_diff_ms(shm->last_activity_timestamp, now);
                const char *status;
                if (age_ms < 100)
                    status = "ACTIVE";
                else if (age_ms < 1000)
                    status = "IDLE";
                else
                    status = "STALE";

                printf("%-15s  %6u  %6u  %4u  %s\n", time_str,
                       shm->events_processed_total, shm->frames_rendered_total,
                       shm->frames_per_second, status);

                last_events = shm->events_processed_total;
                last_frames = shm->frames_rendered_total;
                last_timestamp = shm->last_activity_timestamp;
                last_print_time = now;
            }

            usleep(100000); /* 100ms */
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        ret = 1;
    }

    /* Cleanup */
    munmap(shm, shm_size);
    close(shm_fd);

    return ret;
}
