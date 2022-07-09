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

char *qmp_live_upgrade(const char *binary, Error **errp)
{
    char *ret = NULL;

    if (!strlen(binary)) {
        ret = g_strdup("Invalid arguments");
        return ret;
    }

    ret = g_strdup("success");
    return ret;
}
