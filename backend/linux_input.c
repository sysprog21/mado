/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
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

typedef struct {
    twin_screen_t *screen;
    pthread_t evdev_thread;
    struct evdev_info evdevs[EVDEV_CNT_MAX];
    size_t evdev_cnt;
    size_t udev_cnt;
    int fd;
    int btns;
    int x, y;
} twin_linux_input_t;

static void check_mouse_bounds(twin_linux_input_t *tm)
{
    if (tm->x < 0)
        tm->x = 0;
    if (tm->x > tm->screen->width)
        tm->x = tm->screen->width;
    if (tm->y < 0)
        tm->y = 0;
    if (tm->y > tm->screen->height)
        tm->y = tm->screen->height;
}

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
        if (ev->code == ABS_X) {
            tm->x = ev->value;
            check_mouse_bounds(tm);
            tev.kind = TwinEventMotion;
            tev.u.pointer.screen_x = tm->x;
            tev.u.pointer.screen_y = tm->y;
            tev.u.pointer.button = tm->btns;
            twin_screen_dispatch(tm->screen, &tev);
        } else if (ev->code == ABS_Y) {
            tm->y = ev->value;
            check_mouse_bounds(tm);
            tev.kind = TwinEventMotion;
            tev.u.pointer.screen_x = tm->x;
            tev.u.pointer.screen_y = tm->y;
            tev.u.pointer.button = tm->btns;
            twin_screen_dispatch(tm->screen, &tev);
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
        if (fd > 0 && !opened) {
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

    /* Centering the cursor position */
    tm->x = screen->width / 2;
    tm->y = screen->height / 2;

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
