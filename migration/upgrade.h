/*
 * QEMU live upgrade
 *
 * Authors:
 *  Shenming Lu   <lushenming@bytedance.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef QEMU_UPGRADE_H
#define QEMU_UPGRADE_H

enum LIVE_UPGRADE_FD_TYPE {
    LIVE_UPGRADE_RAM_FD,
};

enum LIVE_UPGRADE_SYNC_EVENT {
    LIVE_UPGRADE_NEW_PREPARED = 7,
};

int live_upgrade_setup_parameter(int argc, char **argv);
int live_upgrade_save_fd(char *name, int fd, enum LIVE_UPGRADE_FD_TYPE type);

#endif
