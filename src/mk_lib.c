/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Monkey HTTP Daemon
 * ------------------
 * Copyright (C) 2001-2012, Eduardo Silva P. <edsiper@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Only built for the library */
#ifdef SHAREDLIB

#include <stdio.h>
#include <mk_lib.h>
#include <mk_utils.h>
#include <mk_memory.h>
#include <mk_config.h>
#include <mk_info.h>
#include <mk_string.h>
#include <mk_plugin.h>
#include <dlfcn.h>
#include <mk_clock.h>
#include <mk_mimetype.h>

static int lib_running = 0;

static void mklib_run(void *p)
{
    mk_utils_worker_rename("libmonkey");

    while (1) {

        if (!lib_running) {
            sleep(1);
            continue;
        }

        puts("Hey pretty!");
        sleep(1);
    }
}

static int load_networking(char *path)
{
    void *handle;
    struct plugin *p;
    int ret;

    handle = mk_plugin_load(path);
    if (!handle) return MKLIB_FALSE;

    p = mk_plugin_alloc(handle, path);
    if (!p) {
        dlclose(handle);
        return MKLIB_FALSE;
    }

    ret = p->init(&api, "");
    if (ret < 0) {
        mk_plugin_free(p);
        return MKLIB_FALSE;
    }

    mk_plugin_register(p);
    return MKLIB_TRUE;
}

/* Returns NULL on error. All pointer arguments may be NULL and the port/plugins
 * may be 0 for the defaults in each case.
 *
 * With no address, bind to all.
 * With no port, use 2001.
 * With no plugins, default to MKLIB_LIANA only.
 * With no documentroot, the default vhost won't access files.
 */
mklib_ctx mklib_init(const char *address, unsigned int port,
                   unsigned int plugins, const char *documentroot,
                   ipcheck_f ipf, urlcheck_f urlf, data_f dataf, close_f closef)
{
    mklib_ctx a = mk_mem_malloc_z(sizeof(struct mklib_ctx_t));
    if (!a) return MKLIB_FALSE;

    config = mk_mem_malloc_z(sizeof(struct server_config));
    if (!config) return MKLIB_FALSE;

    config->serverconf = MONKEY_PATH_CONF;
    mk_config_set_init_values();
    mk_sched_init();
    mk_plugin_init();

    if (plugins & MKLIB_LIANA_SSL) {
        config->transport_layer = strdup("liana_ssl");
        if (!load_networking(PLUGDIR"/monkey-liana_ssl.so")) return MKLIB_FALSE;
    }
    else {
        config->transport_layer = strdup("liana");
        if (!load_networking(PLUGDIR"/monkey-liana.so")) return MKLIB_FALSE;
    }

    if (!plg_netiomap) return MKLIB_FALSE;
    mk_plugin_preworker_calls();

    if (port) config->serverport = port;
    if (address) config->listen_addr = strdup(address);

    config->server_software.data = "";
    config->server_software.len = 0;
    config->default_mimetype = mk_string_dup(MIMETYPE_DEFAULT_TYPE);
    mk_mimetype_read_config();

    /* Server listening socket */
    config->server_fd = mk_socket_server(config->serverport, config->listen_addr);

    /* Clock thread */
    a->clock = mk_utils_worker_spawn((void *) mk_clock_worker_init);

    mk_mem_pointers_init();
    mk_thread_keys_init();

    mk_plugin_core_process();

    return a;
}

/* NULL-terminated config call, consisting of pairs of config item and argument.
 * Returns MKLIB_FALSE on failure. */
int mklib_config(mklib_ctx ctx, ...)
{
    if (!ctx) return MKLIB_FALSE;

    unsigned long len;
    /* Basic server information */
    if (config->hideversion == MK_FALSE) {
        mk_string_build(&config->server_software.data,
                        &len, "Monkey/%s (%s)", VERSION, OS);
        config->server_software.len = len;
    }
    else {
        mk_string_build(&config->server_software.data, &len, "Monkey Server");
        config->server_software.len = len;
    }


    return MKLIB_TRUE;
}

/* NULL-terminated config call creating a vhost with *name. Returns MKLIB_FALSE
 * on failure. */
int mklib_vhost_config(mklib_ctx ctx, char *name, ...)
{
    if (!ctx) return MKLIB_FALSE;

    return MKLIB_TRUE;
}

/* Start the server. */
int mklib_start(mklib_ctx ctx)
{
    if (!ctx || lib_running) return MKLIB_FALSE;

    lib_running = 1;
    ctx->tid = mk_utils_worker_spawn(mklib_run);

    return MKLIB_TRUE;
}

/* Stop the server and free mklib_ctx. */
int mklib_stop(mklib_ctx ctx)
{
    if (!ctx || !lib_running) return MKLIB_FALSE;

    lib_running = 0;
    pthread_cancel(ctx->tid);

    // TODO: kill the workers here

    free(ctx);
    free(config);

    return MKLIB_TRUE;
}

#endif
