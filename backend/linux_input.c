/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <twin.h>
#include <unistd.h>

#include "linux_input.h"
#include "twin_private.h"

#define EVDEV_CNT_MAX 32
#define EVDEV_NAME_SIZE_MAX 50

/* Coordinate smoothing configuration */
/* Weight for smoothing (0=off, 1-7=strength) */
#ifndef TWIN_INPUT_SMOOTH_WEIGHT
#define TWIN_INPUT_SMOOTH_WEIGHT 3
#endif

/* Compile-time bounds check for smoothing weight */
#if TWIN_INPUT_SMOOTH_WEIGHT < 0 || TWIN_INPUT_SMOOTH_WEIGHT > 7
#error "TWIN_INPUT_SMOOTH_WEIGHT must be in range [0, 7]"
#endif

struct twin_linux_input {
    twin_screen_t *screen;
    struct udev *udev;
    struct udev_monitor *mon;
    /* [0]: the udev monitor (fd -1 without one, which poll() skips), then
     * one entry per open event device */
    struct pollfd pfds[1 + EVDEV_CNT_MAX];
    int evdev_idx[EVDEV_CNT_MAX]; /* /dev/input/eventN of pfds[1 + i] */
    int evdev_cnt;
    int btns;
    int x, y;
    int abs_x_min, abs_y_min;   /* Minimum value for ABS_X/ABS_Y from device */
    int abs_x_max, abs_y_max;   /* Maximum value for ABS_X/ABS_Y from device */
    bool abs_range_initialized; /* Whether ABS range has been queried */
#if TWIN_INPUT_SMOOTH_WEIGHT > 0
    int smooth_x, smooth_y;  /* Smoothed coordinates for ABS events */
    bool smooth_initialized; /* Whether smoothing has been initialized */
#endif
};

static void check_mouse_bounds(twin_linux_input_t *tm)
{
    /* Guard against zero-sized screens */
    if (tm->screen->width == 0 || tm->screen->height == 0) {
        tm->x = tm->y = 0;
        return;
    }

    if (tm->x < 0)
        tm->x = 0;
    if (tm->x >= tm->screen->width)
        tm->x = tm->screen->width - 1;
    if (tm->y < 0)
        tm->y = 0;
    if (tm->y >= tm->screen->height)
        tm->y = tm->screen->height - 1;
}

#if TWIN_INPUT_SMOOTH_WEIGHT > 0
/* Apply weighted moving average smoothing to reduce jitter
 * Formula: smooth = (smooth * weight + raw) / (weight + 1)
 * Higher weight = more smoothing but more latency
 */
static inline void smooth_abs_coords(twin_linux_input_t *tm,
                                     int raw_x,
                                     int raw_y)
{
    if (!tm->smooth_initialized) {
        /* Cold start: use raw values directly */
        tm->smooth_x = raw_x;
        tm->smooth_y = raw_y;
        tm->smooth_initialized = true;
    } else {
        /* Weighted moving average with 64-bit intermediates to prevent overflow
         */
        int64_t acc_x =
            (int64_t) tm->smooth_x * TWIN_INPUT_SMOOTH_WEIGHT + raw_x;
        int64_t acc_y =
            (int64_t) tm->smooth_y * TWIN_INPUT_SMOOTH_WEIGHT + raw_y;
        tm->smooth_x = (int) (acc_x / (TWIN_INPUT_SMOOTH_WEIGHT + 1));
        tm->smooth_y = (int) (acc_y / (TWIN_INPUT_SMOOTH_WEIGHT + 1));
    }

    tm->x = tm->smooth_x;
    tm->y = tm->smooth_y;
}
#endif

/* Send the pointer state as an event of @kind */
static void _pointer(twin_linux_input_t *tm, twin_event_kind_t kind)
{
    twin_event_t tev = {.kind = kind};
    check_mouse_bounds(tm);
    tev.u.pointer.screen_x = tm->x;
    tev.u.pointer.screen_y = tm->y;
    tev.u.pointer.button = tm->btns;
    twin_screen_dispatch(tm->screen, &tev);
}

/* Map a device coordinate in [min, max] to [0, size - 1]; false for a
 * device range that cannot be scaled */
static bool _scale_abs(int value, int min, int max, int size, int *out)
{
    int range = max - min;
    if (range <= 0)
        return false;
    *out = (int) (((int64_t) (value - min) * (size - 1)) / range);
    return true;
}

static void twin_linux_input_events(struct input_event *ev,
                                    twin_linux_input_t *tm)
{
    int v;

    switch (ev->type) {
    case EV_REL:
        if (ev->code == REL_X)
            tm->x += ev->value;
        else if (ev->code == REL_Y)
            tm->y += ev->value;
        else
            break;
        _pointer(tm, TwinEventMotion);
        break;
    case EV_ABS:
        if (ev->code == ABS_X) {
            if (!_scale_abs(ev->value, tm->abs_x_min, tm->abs_x_max,
                            tm->screen->width, &v)) {
                log_warn("Ignoring ABS_X event: invalid range [%d,%d]",
                         tm->abs_x_min, tm->abs_x_max);
                break;
            }
#if TWIN_INPUT_SMOOTH_WEIGHT > 0
            smooth_abs_coords(tm, v, tm->y);
#else
            tm->x = v;
#endif
        } else if (ev->code == ABS_Y) {
            if (!_scale_abs(ev->value, tm->abs_y_min, tm->abs_y_max,
                            tm->screen->height, &v)) {
                log_warn("Ignoring ABS_Y event: invalid range [%d,%d]",
                         tm->abs_y_min, tm->abs_y_max);
                break;
            }
#if TWIN_INPUT_SMOOTH_WEIGHT > 0
            smooth_abs_coords(tm, tm->x, v);
#else
            tm->y = v;
#endif
        } else {
            break;
        }
        _pointer(tm, TwinEventMotion);
        break;
    case EV_KEY:
        if (ev->code == BTN_LEFT) {
            tm->btns = ev->value > 0 ? 1 : 0;
            _pointer(tm, tm->btns ? TwinEventButtonDown : TwinEventButtonUp);
        }
        break;
    }
}

static int twin_linux_udev_init(struct udev **udev, struct udev_monitor **mon)
{
    /* Create udev object */
    *udev = udev_new();
    if (!*udev) {
        log_error("Failed to create udev object");
        return -1;
    }

    /* Create a monitor for kernel events */
    *mon = udev_monitor_new_from_netlink(*udev, "udev");
    if (!*mon) {
        log_error("Failed to create udev monitor");
        udev_unref(*udev);
        return -1;
    }

    /* Filter for input subsystem */
    udev_monitor_filter_add_match_subsystem_devtype(*mon, "input", NULL);
    udev_monitor_enable_receiving(*mon);

    /* File descriptor for the monitor */
    return udev_monitor_get_fd(*mon);
}

static bool twin_linux_udev_update(struct udev_monitor *mon)
{
    struct udev_device *dev = NULL;

    /* Get the device that caused the event */
    dev = udev_monitor_receive_device(mon);
    if (dev) {
        const char *action = udev_device_get_action(dev);
        const char *dev_node = udev_device_get_devnode(dev);

        if (action && dev_node) {
            const char *keyboard =
                udev_device_get_property_value(dev, "ID_INPUT_KEYBOARD");
            const char *mouse =
                udev_device_get_property_value(dev, "ID_INPUT_MOUSE");

            /* Ensure udev event is for mouse or keyboard */
            if (!keyboard && !mouse) {
                udev_device_unref(dev);
                return false;
            }

            /* Capture only add and remove events */
            if (!strcmp(action, "add") || !strcmp(action, "remove")) {
                log_info("udev: %s: %s", action, dev_node);
                udev_device_unref(dev);
                return true;
            }
        }
    }

    /* No event is caputured */
    return false;
}

/* Query absolute axis information from an input device */
static void twin_linux_input_query_abs(int fd, twin_linux_input_t *tm)
{
    struct input_absinfo abs_info;

    /* Query ABS_X range (minimum and maximum) */
    if (ioctl(fd, EVIOCGABS(ABS_X), &abs_info) == 0) {
        /* Validate range: maximum must be greater than minimum */
        if (abs_info.maximum > abs_info.minimum) {
            int range = abs_info.maximum - abs_info.minimum;
            int current_range = tm->abs_x_max - tm->abs_x_min;

            /* Accept first device range unconditionally, or larger ranges from
             * subsequent devices
             */
            if (!tm->abs_range_initialized || range > current_range) {
                log_info("ABS_X range updated: [%d,%d] (device fd=%d)",
                         abs_info.minimum, abs_info.maximum, fd);
                tm->abs_x_min = abs_info.minimum;
                tm->abs_x_max = abs_info.maximum;
                tm->abs_range_initialized = true;
            }
        } else {
            log_warn("Device fd=%d: ABS_X range invalid [%d,%d]", fd,
                     abs_info.minimum, abs_info.maximum);
        }
    }

    /* Query ABS_Y range (minimum and maximum) */
    if (ioctl(fd, EVIOCGABS(ABS_Y), &abs_info) == 0) {
        /* Validate range: maximum must be greater than minimum */
        if (abs_info.maximum > abs_info.minimum) {
            int range = abs_info.maximum - abs_info.minimum;
            int current_range = tm->abs_y_max - tm->abs_y_min;

            /* Accept first device range unconditionally, or larger ranges from
             * subsequent devices
             */
            if (!tm->abs_range_initialized || range > current_range) {
                log_info("ABS_Y range updated: [%d,%d] (device fd=%d)",
                         abs_info.minimum, abs_info.maximum, fd);
                tm->abs_y_min = abs_info.minimum;
                tm->abs_y_max = abs_info.maximum;
            }
        } else {
            log_warn("Device fd=%d: ABS_Y range invalid [%d,%d]", fd,
                     abs_info.minimum, abs_info.maximum);
        }
    }
}

/* Open the event devices present now, keeping those already open and
 * closing those that went away */
static void twin_linux_edev_open(twin_linux_input_t *tm)
{
    struct pollfd pfds[EVDEV_CNT_MAX];
    int idx[EVDEV_CNT_MAX];
    int cnt = 0;
    bool changed = false;

    char evdev_name[EVDEV_NAME_SIZE_MAX];
    for (int i = 0; i < EVDEV_CNT_MAX; i++) {
        snprintf(evdev_name, sizeof(evdev_name), "/dev/input/event%d", i);
        if (access(evdev_name, F_OK) != 0)
            continue;

        /* Keep the descriptor of a device already open */
        int fd = -1;
        for (int j = 0; j < tm->evdev_cnt; j++) {
            if (tm->evdev_idx[j] == i) {
                fd = tm->pfds[1 + j].fd;
                tm->pfds[1 + j].fd = -1;
                break;
            }
        }
        if (fd < 0) {
            fd = open(evdev_name, O_RDWR | O_NONBLOCK);
            if (fd < 0)
                continue;
            twin_linux_input_query_abs(fd, tm);
            changed = true;
        }
        pfds[cnt] = (struct pollfd) {.fd = fd, .events = POLLIN};
        idx[cnt++] = i;
    }

    /* Whatever is left was disconnected */
    for (int j = 0; j < tm->evdev_cnt; j++) {
        if (tm->pfds[1 + j].fd >= 0) {
            close(tm->pfds[1 + j].fd);
            changed = true;
        }
    }

    memcpy(&tm->pfds[1], pfds, sizeof(pfds[0]) * cnt);
    memcpy(tm->evdev_idx, idx, sizeof(idx[0]) * cnt);
    tm->evdev_cnt = cnt;

#if TWIN_INPUT_SMOOTH_WEIGHT > 0
    /* Reset smoothing on reconnection to prevent coordinate jumps */
    if (changed)
        tm->smooth_initialized = false;
#else
    (void) changed;
#endif
}

void twin_linux_input_poll(twin_linux_input_t *tm, int timeout_ms)
{
    if (poll(tm->pfds, 1 + tm->evdev_cnt, timeout_ms) <= 0)
        return; /* timed out, or a signal (a VT switch) woke us */

    bool rescan = (tm->pfds[0].revents & POLLIN) && tm->mon &&
                  twin_linux_udev_update(tm->mon);

    struct input_event ev[64];
    for (int i = 0; i < tm->evdev_cnt; i++) {
        short revents = tm->pfds[1 + i].revents;
        if (revents & (POLLERR | POLLHUP | POLLNVAL))
            rescan = true;
        if (!(revents & POLLIN))
            continue;
        ssize_t n;
        while ((n = read(tm->pfds[1 + i].fd, ev, sizeof(ev))) > 0)
            for (size_t k = 0; k < (size_t) n / sizeof(ev[0]); k++)
                twin_linux_input_events(&ev[k], tm);
    }

    if (rescan)
        twin_linux_edev_open(tm);
}

twin_linux_input_t *twin_linux_input_create(twin_screen_t *screen)
{
    twin_linux_input_t *tm = calloc(1, sizeof(*tm));
    if (!tm)
        return NULL;

    tm->screen = screen;

    /* Initialize ABS axis ranges to common touchscreen default.
     * These will be updated to actual device values when devices are opened.
     */
    tm->abs_x_min = tm->abs_y_min = 0;
    tm->abs_x_max = tm->abs_y_max = 32767;
    log_info(
        "Input system initialized: screen=%dx%d, default ABS range=[%d,%d]",
        screen->width, screen->height, tm->abs_x_min, tm->abs_x_max);

    /* Centering the cursor position */
    tm->x = screen->width / 2;
    tm->y = screen->height / 2;

#if TWIN_INPUT_SMOOTH_WEIGHT > 0
    /* Initialize smoothing from center position */
    tm->smooth_x = tm->x, tm->smooth_y = tm->y;
    tm->smooth_initialized = true;
#endif

    /* Hotplug notifications, when udev is there */
    int udev_fd = twin_linux_udev_init(&tm->udev, &tm->mon);
    if (udev_fd < 0) {
        /* A monitor without a descriptor is no use; earlier failures
         * already released what they made */
        if (tm->mon) {
            udev_monitor_unref(tm->mon);
            udev_unref(tm->udev);
        }
        tm->udev = NULL;
        tm->mon = NULL;
    }
    tm->pfds[0] = (struct pollfd) {.fd = udev_fd, .events = POLLIN};

    twin_linux_edev_open(tm);
    if (tm->evdev_cnt == 0 && udev_fd < 0) {
        log_error("Failed to open udev and evdev");
        twin_linux_input_destroy(tm);
        return NULL;
    }
    return tm;
}

void twin_linux_input_destroy(twin_linux_input_t *tm)
{
    if (!tm)
        return;
    for (int i = 0; i < tm->evdev_cnt; i++)
        close(tm->pfds[1 + i].fd);
    if (tm->mon)
        udev_monitor_unref(tm->mon);
    if (tm->udev)
        udev_unref(tm->udev);
    free(tm);
}
