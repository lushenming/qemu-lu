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
#include "qemu/error-report.h"

/* only support -qemu-upgrade-shmfd option now */
#define LIVE_UPGRADE_ARG_NUM        2
#define LIVE_UPGRADE_SHM_SIZE       (16 * 1024 * 1024)
#define LIVE_UPGRADE_HEADER_MAGIC   (0x95273E5F)
/* hugepage backing RAM */
#define MAX_RAM_FD                  4
#define MAX_NAME_LEN                32

struct live_upgrade_header {
    uint32_t magic;
    uint32_t version;
};

struct live_upgrade_fd {
    int ram_fds[MAX_RAM_FD];
    char ram_fd_names[MAX_RAM_FD][MAX_NAME_LEN];
    int ram_fd_index;
};

struct live_upgrade_state {
    struct live_upgrade_header head;
    struct live_upgrade_fd fds;
};

static int argc_i;
static char **argv_i;
static struct live_upgrade_state curr_state;

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

static int _live_upgrade_save_fd(int *fds, char **names, int *fd_idx,
                                 int fd, char *name)
{
    fds[*fd_idx] = fd;
    strcpy(names[*fd_idx], name);
    *fd_idx++;
    return 0;
}

int live_upgrade_save_fd(char *name, int fd, enum LIVE_UPGRADE_FD_TYPE type)
{
    int *fds, *fd_idx;
    char **names;

    if (!name || strlen(name) >= MAX_NAME_LEN || fd <= 0)
        return -EINVAL;

    switch (type) {
    case LIVE_UPGRADE_RAM_FD:
        {
            fds = curr_state.fds.ram_fds;
            names = curr_state.fds.ram_fd_names;
            fd_idx = &curr_state.fds.ram_fd_index;
            if (*fd_idx >= MAX_RAM_FD)
                return -ENOSPC;
        }
        break;
    default:
        return -ENOSYS;
    }

    return _live_upgrade_save_fd(fds, names, fd_idx, fd, name);
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

static void live_upgrade_init_state_header(void)
{
    curr_state.head.magic = LIVE_UPGRADE_HEADER_MAGIC;
    curr_state.head.version = 1;
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

    live_upgrade_init_state_header();

    memcpy(shm_ptr, &curr_state, sizeof(curr_state));

    ret = g_strdup("success");
    return ret;
}
