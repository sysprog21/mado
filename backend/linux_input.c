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

typedef struct {
    twin_screen_t *screen;
    pthread_t udev_thread;
    pthread_t evdev_thread;
    int fd;
    int btns;
    int x, y;
} twin_linux_input_t;

static int evdev_fd[EVDEV_CNT_MAX];
static int evdev_cnt;

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

static void *twin_linux_udev_thread(void *arg)
{
    struct udev *udev;
    struct udev_monitor *mon;
    struct udev_device *dev;
    int fd;

    /* Create udev object */
    udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Failed to create udev object\n");
        return NULL;
    }

    /* Create a monitor for kernel events */
    mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        fprintf(stderr, "Failed to create udev monitor\n");
        udev_unref(udev);
        return NULL;
    }

    /* Filter for input subsystem (keyboard/mouse) */
    udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
    udev_monitor_enable_receiving(mon);

    /* File descriptor for the monitor */
    fd = udev_monitor_get_fd(mon);

    printf("Listening for keyboard and mouse events...\n");

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        int ret = select(fd + 1, &fds, NULL, NULL, NULL);
        if (ret <= 0) {
            continue;
        }

        /* Get the device that caused the event */
        dev = udev_monitor_receive_device(mon);
        if (dev) {
            const char *action = udev_device_get_action(dev);
            const char *devnode = udev_device_get_devnode(dev);
            const char *subsystem = udev_device_get_subsystem(dev);
            // const char *devtype = udev_device_get_devtype(dev);

            if (action && devnode && subsystem &&
                strcmp(subsystem, "input") == 0) {
                /* Detect if it’s a keyboard or mouse */
                const char *name =
                    udev_device_get_property_value(dev, "ID_INPUT_KEYBOARD");
                const char *mouse =
                    udev_device_get_property_value(dev, "ID_INPUT_MOUSE");

                if (name || mouse) {
                    if (strcmp(action, "add") == 0) {
                        printf("Device added: %s\n", devnode);

                        /* Open device file to get file descriptor */
                        int device_fd = open(devnode, O_RDONLY | O_NONBLOCK);
                        if (device_fd >= 0) {
                            printf("Opened device %s with file descriptor %d\n",
                                   devnode, device_fd);
                            /* You can keep the fd open for further use */
                            close(device_fd); /* Close for now */
                        } else {
                            perror("Error opening device");
                        }
                    } else if (strcmp(action, "remove") == 0) {
                        printf("Device removed: %s\n", devnode);
                    }
                }
            }

            udev_device_unref(dev);
        }
    }

    /* Clean up */
    udev_monitor_unref(mon);
    udev_unref(udev);

    return NULL;
}

static void *twin_linux_evdev_thread(void *arg)
{
    twin_linux_input_t *tm = arg;

    /* Open all event devices */
    char evdev_name[EVDEV_NAME_SIZE_MAX] = {0};
    for (int i = 0; i < EVDEV_CNT_MAX; i++) {
        snprintf(evdev_name, EVDEV_NAME_SIZE_MAX, "/dev/input/event%d", i);
        int fd = open(evdev_name, O_RDWR | O_NONBLOCK);
        if (fd > 0) {
            evdev_fd[evdev_cnt] = fd;
            evdev_cnt++;
        }
    }

    /* Initialize pollfd array */
    struct pollfd pfds[EVDEV_CNT_MAX];
    for (int i = 0; i < evdev_cnt; i++) {
        pfds[i].fd = evdev_fd[i];
        pfds[i].events = POLLIN;
    }

    /* Event polling */
    struct input_event ev;
    while (1) {
        /* Wait until any event is available */
        if (poll(pfds, evdev_cnt, -1) <= 0)
            continue;

        /* Iterate through all file descriptors */
        for (int i = 0; i < evdev_cnt; i++) {
            /* Try reading events */
            ssize_t n = read(pfds[i].fd, &ev, sizeof(ev));
            if (n == sizeof(ev)) {
                /* Handle events */
                twin_linux_input_events(&ev, tm);
            }
        }
    }

    return NULL;
}

static bool dummy(int file, twin_file_op_t ops, void *closure)
{
    return true;
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

#if 1
    /* FIXME: Need to fix the unexpected termination of the program.
     * Hooking a dummy function here is only a hack*/

    /* Set file handler for reading input device file */
    twin_set_file(dummy, tm->fd, TWIN_READ, tm);
#endif

    /* Start event handling thread */
    if (pthread_create(&tm->udev_thread, NULL, twin_linux_udev_thread, tm)) {
        log_error("Failed to create udev thread");
        return NULL;
    }

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
