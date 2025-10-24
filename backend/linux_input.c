/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <twin.h>
#include <unistd.h>

#include "linux_input.h"
#include "twin_private.h"

#define EVDEV_CNT_MAX 32
#define EVDEV_NAME_SIZE_MAX 50
#define UDEV_RESERVED_CNT 1
#define UDEV_EVDEV_FD_CNT (EVDEV_CNT_MAX + UDEV_RESERVED_CNT)

struct evdev_info {
    int idx;
    int fd;
};

/* Coordinate smoothing configuration */
/* Weight for smoothing (0=off, 1-7=strength) */
#ifndef TWIN_INPUT_SMOOTH_WEIGHT
#define TWIN_INPUT_SMOOTH_WEIGHT 3
#endif

/* Compile-time bounds check for smoothing weight */
#if TWIN_INPUT_SMOOTH_WEIGHT < 0 || TWIN_INPUT_SMOOTH_WEIGHT > 7
#error "TWIN_INPUT_SMOOTH_WEIGHT must be in range [0, 7]"
#endif

typedef struct {
    twin_screen_t *screen;
    pthread_t evdev_thread;
    struct evdev_info evdevs[EVDEV_CNT_MAX];
    size_t evdev_cnt;
    size_t udev_cnt;
    int fd;
    int btns;
    int x, y;
    int abs_x_min, abs_y_min; /* Minimum value for ABS_X/ABS_Y from device */
    int abs_x_max, abs_y_max; /* Maximum value for ABS_X/ABS_Y from device */
#if TWIN_INPUT_SMOOTH_WEIGHT > 0
    int smooth_x, smooth_y;  /* Smoothed coordinates for ABS events */
    bool smooth_initialized; /* Whether smoothing has been initialized */
#endif
} twin_linux_input_t;

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

static void twin_linux_input_events(struct input_event *ev,
                                    twin_linux_input_t *tm)
{
    /* TODO: twin_screen_dispatch() should be protect by mutex_lock, but the
     * counterpart piece of code with mutex is not yet sure */

    twin_event_t tev;

    switch (ev->type) {
    case EV_REL:
        if (ev->code == REL_X) {
            tm->x += ev->value;
            check_mouse_bounds(tm);
            tev.kind = TwinEventMotion;
            tev.u.pointer.screen_x = tm->x;
            tev.u.pointer.screen_y = tm->y;
            tev.u.pointer.button = tm->btns;
            twin_screen_dispatch(tm->screen, &tev);
        } else if (ev->code == REL_Y) {
            tm->y += ev->value;
            check_mouse_bounds(tm);
            tev.kind = TwinEventMotion;
            tev.u.pointer.screen_x = tm->x;
            tev.u.pointer.screen_y = tm->y;
            tev.u.pointer.button = tm->btns;
            twin_screen_dispatch(tm->screen, &tev);
        }
        break;
    case EV_ABS:
        /* Scale absolute coordinates to screen resolution.
         * The range is dynamically queried from each device using EVIOCGABS.
         * Formula: scaled = ((value - min) * (screen_size - 1)) / (max - min)
         * This correctly maps device coordinates [min, max] to screen [0,
         * size-1]
         */
        if (ev->code == ABS_X) {
            int range = tm->abs_x_max - tm->abs_x_min;
            if (range > 0) {
                int raw_x = ((int64_t) (ev->value - tm->abs_x_min) *
                             (tm->screen->width - 1)) /
                            range;
#if TWIN_INPUT_SMOOTH_WEIGHT > 0
                /* Apply smoothing to X axis, keep Y unchanged */
                smooth_abs_coords(tm, raw_x, tm->y);
#else
                tm->x = raw_x;
#endif
                check_mouse_bounds(tm);
                tev.kind = TwinEventMotion;
                tev.u.pointer.screen_x = tm->x;
                tev.u.pointer.screen_y = tm->y;
                tev.u.pointer.button = tm->btns;
                twin_screen_dispatch(tm->screen, &tev);
            } else {
                log_warn("Ignoring ABS_X event: invalid range [%d,%d]",
                         tm->abs_x_min, tm->abs_x_max);
            }
        } else if (ev->code == ABS_Y) {
            int range = tm->abs_y_max - tm->abs_y_min;
            if (range > 0) {
                int raw_y = ((int64_t) (ev->value - tm->abs_y_min) *
                             (tm->screen->height - 1)) /
                            range;
#if TWIN_INPUT_SMOOTH_WEIGHT > 0
                /* Apply smoothing to Y axis, keep X unchanged */
                smooth_abs_coords(tm, tm->x, raw_y);
#else
                tm->y = raw_y;
#endif
                check_mouse_bounds(tm);
                tev.kind = TwinEventMotion;
                tev.u.pointer.screen_x = tm->x;
                tev.u.pointer.screen_y = tm->y;
                tev.u.pointer.button = tm->btns;
                twin_screen_dispatch(tm->screen, &tev);
            } else {
                log_warn("Ignoring ABS_Y event: invalid range [%d,%d]",
                         tm->abs_y_min, tm->abs_y_max);
            }
        }
        break;
    case EV_KEY:
        if (ev->code == BTN_LEFT) {
            tm->btns = ev->value > 0 ? 1 : 0;
            tev.kind = tm->btns ? TwinEventButtonDown : TwinEventButtonUp;
            tev.u.pointer.screen_x = tm->x;
            tev.u.pointer.screen_y = tm->y;
            tev.u.pointer.button = tm->btns;
            twin_screen_dispatch(tm->screen, &tev);
        }
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

            /* Update global range if this device has a larger range */
            if (range > current_range) {
                log_info("ABS_X range updated: [%d,%d] (device fd=%d)",
                         abs_info.minimum, abs_info.maximum, fd);
                tm->abs_x_min = abs_info.minimum;
                tm->abs_x_max = abs_info.maximum;
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

            /* Update global range if this device has a larger range */
            if (range > current_range) {
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

static void twin_linux_edev_open(struct pollfd *pfds, twin_linux_input_t *tm)
{
    /* New event device list */
    struct evdev_info evdevs[EVDEV_CNT_MAX];
    int new_evdev_cnt = 0;
    memset(evdevs, 0, sizeof(evdevs));

    /* Open all event devices */
    char evdev_name[EVDEV_NAME_SIZE_MAX] = {0};
    for (int i = 0; i < EVDEV_CNT_MAX; i++) {
        /* Check if the file exists */
        snprintf(evdev_name, EVDEV_NAME_SIZE_MAX, "/dev/input/event%d", i);
        if (access(evdev_name, F_OK) != 0)
            continue;

        /* Match device with the old device list  */
        bool opened = false;
        for (size_t j = 0; j < tm->evdev_cnt; j++) {
            /* Copy the fd if the device is already on the list */
            if (tm->evdevs[j].idx == i) {
                evdevs[new_evdev_cnt].idx = tm->evdevs[j].idx;
                evdevs[new_evdev_cnt].fd = tm->evdevs[j].fd;
                tm->evdevs[j].fd = -1;
                new_evdev_cnt++;
                opened = true;
                break;
            }
        }

        /* Open the file if it is not on the list */
        int fd = open(evdev_name, O_RDWR | O_NONBLOCK);
        if (fd >= 0 && !opened) {
            /* Query absolute axis info for newly opened devices */
            twin_linux_input_query_abs(fd, tm);
            evdevs[new_evdev_cnt].idx = i;
            evdevs[new_evdev_cnt].fd = fd;
            new_evdev_cnt++;
        }
    }

    /* Close disconnected devices */
    for (size_t i = 0; i < tm->evdev_cnt; i++) {
        if (tm->evdevs[i].fd > 0)
            close(tm->evdevs[i].fd);
    }

    /* Overwrite the evdev list */
    memcpy(tm->evdevs, evdevs, sizeof(tm->evdevs));
    tm->evdev_cnt = new_evdev_cnt;

    /* Initialize evdev poll file descriptors */
    for (size_t i = tm->udev_cnt; i < tm->evdev_cnt + tm->udev_cnt; i++) {
        pfds[i].fd = tm->evdevs[i - 1].fd;
        pfds[i].events = POLLIN;
    }
}

static void *twin_linux_evdev_thread(void *arg)
{
    twin_linux_input_t *tm = arg;

    struct udev *udev = NULL;
    struct udev_monitor *mon = NULL;

    /* Open Linux udev (user space device manager) */
    int udev_fd = twin_linux_udev_init(&udev, &mon);
    if (udev_fd >= 0)
        tm->udev_cnt = 1;

    /* Place the udev fd into the poll fds */
    struct pollfd pfds[UDEV_EVDEV_FD_CNT];
    pfds[0].fd = udev_fd;
    pfds[0].events = POLLIN;

    /* Open event devices */
    twin_linux_edev_open(pfds, tm);

    /* Accessing to input devices is impossible, terminate the thread */
    if (tm->evdev_cnt == 0 && tm->udev_cnt == 0) {
        log_error("Failed to open udev and evdev");
        pthread_exit(NULL);
    }

    /* Event polling */
    struct input_event ev;
    while (1) {
        /* Wait until any event is available */
        if (poll(pfds, tm->evdev_cnt + tm->udev_cnt, -1) <= 0)
            continue;

        /* Iterate through all file descriptors */
        for (size_t i = 0; i < tm->evdev_cnt + tm->udev_cnt; i++) {
            if (i < tm->udev_cnt) {
                /* Check udev event */
                if (twin_linux_udev_update(mon)) {
                    /* Re-open event devices */
                    twin_linux_edev_open(pfds, tm);
                    break;
                }
                continue;
            }
            /* Try reading events */
            ssize_t n = read(pfds[i].fd, &ev, sizeof(ev));
            if (n == sizeof(ev)) {
                /* Handle events */
                twin_linux_input_events(&ev, tm);
            }
        }
    }

    /* Clean up */
    udev_monitor_unref(mon);
    udev_unref(udev);

    return NULL;
}

void *twin_linux_input_create(twin_screen_t *screen)
{
    /* Create object for handling Linux input system */
    twin_linux_input_t *tm = calloc(1, sizeof(twin_linux_input_t));
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

    /* Start event handling thread */
    if (pthread_create(&tm->evdev_thread, NULL, twin_linux_evdev_thread, tm)) {
        log_error("Failed to create evdev thread");
        return NULL;
    }

    return tm;
}

void twin_linux_input_destroy(void *_tm)
{
    twin_linux_input_t *tm = _tm;
    close(tm->fd);
    free(tm);
}
