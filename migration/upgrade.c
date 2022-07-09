/*
 * QEMU live upgrade
 *
 * Authors:
 *  Shenming Lu   <lushenming@bytedance.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include "upgrade.h"

/* only support -qemu-upgrade-shmfd option now */
#define LIVE_UPGRADE_ARG_NUM	2
#define LIVE_UPGRADE_SHM_SIZE	(16 * 1024 * 1024)

int argc_i;
char **argv_i;

int live_upgrade_setup_parameter(int argc, char **argv)
{
    int opt;

    argc_i = argc;
    for (opt = 1; opt < argc; opt++) {
        /*
         * new qemu don't inherit the -incoming option
         * add the -shmfd option in qmp_live_upgrade
         */
        if (!strcmp(argv[opt], "-incoming") ||
            !strcmp(argv[opt], "-qemu-upgrade-shmfd"))
            argc_i -= 2;
    }
    argc_i += LIVE_UPGRADE_ARG_NUM;

    argv_i = g_malloc0_n(sizeof(char *), argc_i);
    argc_i = 1;
    for (opt = 1; opt < argc; opt++) {
        if (!strcmp(argv[opt], "-incoming") ||
            !strcmp(argv[opt], "-qemu-upgrade-shmfd")) {
            opt++;
            continue;
        }
        argv_i[argc_i++] = g_strdup(argv[opt]);
    }

    return 0;
}

static void live_upgrade_update_parameter(const char *binary, int shm_fd)
{
    argv_i[0] = g_strdup(binary);
    argv_i[argc_i++] = g_strdup("-qemu-upgrade-shmfd");
    argv_i[argc_i++] = g_strdup_printf("%d", shm_fd);
}

static int live_upgrade_setup_shm(uint8_t **shm_ptr)
{
    char *shm_path = NULL, *buf = NULL;
    int shm_fd, ret;

    shm_path = g_strdup_printf("/dev/shm/qemu-upgrade-%d.XXXXXX",
                               getpid());
    shm_fd = mkstemp(shm_path);
    if (shm_fd < 0) {
        error_report("%s: mkstemp failed: %s", __func__, strerror(errno));
        ret = -errno;
        goto out;
    }

    if (unlink(shm_path) < 0) {
        error_report("%s: unlink failed: %s", __func__, strerror(errno));
        ret = -errno;
        goto out;
    }

    buf = g_malloc0(LIVE_UPGRADE_SHM_SIZE);
    if (!buf) {
        error_report("%s: malloc buf failed", __func__);
        ret = -ENOMEM;
        goto out;
    }

    if (write(shm_fd, buf, LIVE_UPGRADE_SHM_SIZE) !=
        LIVE_UPGRADE_SHM_SIZE) {
        error_report("%s: init shm to 0 failed: %s", __func__,
                     strerror(errno));
        ret = -errno;
        goto out;
    }

    *shm_ptr = mmap(0, LIVE_UPGRADE_SHM_SIZE, PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);
    if (*shm_ptr == MAP_FAILED) {
        error_report("%s: mmap failed: %s", __func__, strerror(errno));
        ret = -errno;
        goto out;
    }

    ret = shm_fd;

out:
    if (shm_path)
        g_free(shm_path);
    if (shm_fd > 0 && ret < 0)
        close(shm_fd);
    if (buf)
        g_free(buf);
    return ret;
}

char *qmp_live_upgrade(const char *binary, Error **errp)
{
    char *ret = NULL;
    int shm_fd;
    uint8_t *shm_ptr = NULL;

    if (!strlen(binary)) {
        ret = g_strdup("Invalid arguments");
        return ret;
    }

    shm_fd = live_upgrade_setup_shm(&shm_ptr);
    if (shm_fd < 0) {
        ret = g_strdup("Setup shm failed");
        return ret;
    }

    live_upgrade_update_parameter(binary, shm_fd);

    ret = g_strdup("success");
    return ret;
}
