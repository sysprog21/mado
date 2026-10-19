/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024-2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _LINUX_VT_H__
#define _LINUX_VT_H__

#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "twin_private.h"

#define VT_DEV_TTY_MAX 16 /* "/dev/tty" and any int */
#define SIG_SWITCH_FROM SIGUSR1
#define SIG_SWITCH_TO SIGUSR2

static volatile sig_atomic_t *vt_fd;

/* Set while the VT is switched away from us */
static volatile sig_atomic_t *vt_away_flag;

static inline int twin_vt_open(int vt_num)
{
    char vt_dev[VT_DEV_TTY_MAX] = {0};
    snprintf(vt_dev, VT_DEV_TTY_MAX, "/dev/tty%d", vt_num);

    int fd = open(vt_dev, O_RDWR);
    if (fd < 0) {
        log_error("Failed to open %s", vt_dev);
    }

    return fd;
}

static inline int twin_vt_mode(int fd, int mode)
{
    return ioctl(fd, KDSETMODE, mode);
}

static void twin_vt_process_switch_signal(int signum)
{
    if (signum == SIG_SWITCH_FROM) {
        /* Notify kernel to release current virtual terminal */
        ioctl(*vt_fd, VT_RELDISP, 1);

        /* Set the virtual terminal display in text mode */
        ioctl(*vt_fd, KDSETMODE, KD_TEXT);

        *vt_away_flag = true;
    } else if (signum == SIG_SWITCH_TO) {
        /* Switch complete */
        ioctl(*vt_fd, VT_RELDISP, VT_ACKACQ);

        /* Restore the virtual terminal display in graphics mode */
        ioctl(*vt_fd, KDSETMODE, KD_GRAPHICS);

        *vt_away_flag = false;
    }
}

static inline void twin_vt_setup_signal_handler()
{
    struct sigaction tty_sig = {.sa_handler = twin_vt_process_switch_signal};
    sigfillset(&tty_sig.sa_mask);

    /* Set the signal handler for leaving the current TTY */
    sigaction(SIG_SWITCH_FROM, &tty_sig, NULL);

    /* Set the signal handler for returning to the previous TTY */
    sigaction(SIG_SWITCH_TO, &tty_sig, NULL);

    /* Deliver them only while waiting for input: twin_linux_input_poll()
     * unblocks them atomically, so the wait sees every switch */
    sigset_t sw;
    sigemptyset(&sw);
    sigaddset(&sw, SIG_SWITCH_FROM);
    sigaddset(&sw, SIG_SWITCH_TO);
    sigprocmask(SIG_BLOCK, &sw, NULL);
}

/* Give the terminal back: the saved switching mode and text display */
static inline void twin_vt_restore(int fd, const struct vt_mode *old_vtm)
{
    ioctl(fd, VT_SETMODE, old_vtm);
    twin_vt_mode(fd, KD_TEXT);
    sigset_t sw;
    sigemptyset(&sw);
    sigaddset(&sw, SIG_SWITCH_FROM);
    sigaddset(&sw, SIG_SWITCH_TO);
    sigprocmask(SIG_UNBLOCK, &sw, NULL);
}

static inline bool twin_vt_setup(int *fd_ptr,
                                 struct vt_mode *old_vtm,
                                 volatile sig_atomic_t *vt_away)
{
    /* Open VT0 to inquire information */
    int vt0 = twin_vt_open(0);
    if (vt0 < 0) {
        log_error("Failed to open VT0");
        return false;
    }

    /* Inquire for current VT number */
    struct vt_stat vt;
    int got = ioctl(vt0, VT_GETSTATE, &vt);
    close(vt0);
    if (got == -1) {
        log_error("Failed to get VT number");
        return false;
    }

    int vt_num = vt.v_active;

    /* Open the VT */
    if ((*fd_ptr = twin_vt_open(vt_num)) < 0)
        return false;

    /* Attempt to activate the specified virtual terminal by its number */
    if (ioctl(*fd_ptr, VT_ACTIVATE, vt_num) < 0 ||
        /* and wait until it becomes the active VT */
        ioctl(*fd_ptr, VT_WAITACTIVE, vt_num) < 0) {
        log_error("Failed to activate VT %d", vt_num);
        goto bail;
    }

    /* Save the virtual terminal mode */
    if (ioctl(*fd_ptr, VT_GETMODE, old_vtm) < 0) {
        log_error("Failed to get VT mode");
        goto bail;
    }

    /* Set the virtual terminal mode */
    struct vt_mode vtm = {
        .mode = VT_PROCESS, .relsig = SIGUSR1, .acqsig = SIGUSR2};

    if (ioctl(*fd_ptr, VT_SETMODE, &vtm) < 0) {
        log_error("Failed to set VT mode");
        goto bail;
    }

    /* Set the virtual terminal display mode to graphics mode */
    if (twin_vt_mode(*fd_ptr, KD_GRAPHICS) < 0) {
        log_error("Failed to set KD_GRAPHICS mode");
        twin_vt_restore(*fd_ptr, old_vtm);
        goto bail;
    }

    vt_fd = fd_ptr;
    vt_away_flag = vt_away;

    return true;

bail:
    close(*fd_ptr);
    *fd_ptr = -1;
    return false;
}
#endif
