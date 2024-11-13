/*
 * Twin - A Tiny Window System
 * Copyright (c) 2024 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

#ifndef _LINUX_VT_H__
#define _LINUX_VT_H__

#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "twin_private.h"

#define VT_DEV_TTY_MAX 11
static inline int twin_vt_open(int vt_num)
{
    int fd;

    char vt_dev[VT_DEV_TTY_MAX] = {0};
    snprintf(vt_dev, VT_DEV_TTY_MAX, "/dev/tty%d", vt_num);

    fd = open(vt_dev, O_RDWR);
    if (fd < 0) {
        log_error("Failed to open %s", vt_dev);
    }

    return fd;
}

static inline int twin_vt_mode(int fd, int mode)
{
    return ioctl(fd, KDSETMODE, mode);
}

static inline bool twin_vt_setup(int *fd_ptr)
{
    /* Open VT0 to inquire information */
    if ((*fd_ptr = twin_vt_open(0)) < -1) {
        log_error("Failed to open VT0");
        return false;
    }

    /* Inquire for current VT number */
    struct vt_stat vt;
    if (ioctl(*fd_ptr, VT_GETSTATE, &vt) == -1) {
        log_error("Failed to get VT number");
        return false;
    }

    int vt_num = vt.v_active;

    /* Open the VT */
    if ((*fd_ptr = twin_vt_open(vt_num)) < -1) {
        return false;
    }

    /* Set VT to graphics mode to inhibit command-line text */
    if (twin_vt_mode(*fd_ptr, KD_GRAPHICS) < 0) {
        log_error("Failed to set KD_GRAPHICS mode");
        return false;
    }

    return true;
}
#endif
