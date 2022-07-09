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

int argc_i;
char **argv_i;

int live_upgrade_setup_parameter(int argc, char **argv)
{
    int opt;

    argc_i = argc;
    for (opt = 1; opt < argc; opt++) {
        /* new qemu don't inherit the -incoming option */
        if (!strcmp(argv[opt], "-incoming"))
            argc_i -= 2;
    }

    argv_i = g_malloc0_n(sizeof(char *), argc_i);
    argc_i = 1;
    for (opt = 1; opt < argc; opt++) {
        if (!strcmp(argv[opt], "-incoming")) {
            opt++;
            continue;
        }
        argv_i[argc_i++] = g_strdup(argv[opt]);
    }

    return 0;
}

static void live_upgrade_update_parameter(const char *binary)
{
    argv_i[0] = g_strdup(binary);
}

char *qmp_live_upgrade(const char *binary, Error **errp)
{
    char *ret = NULL;

    if (!strlen(binary)) {
        ret = g_strdup("Invalid arguments");
        return ret;
    }

    live_upgrade_update_parameter(binary);

    ret = g_strdup("success");
    return ret;
}
