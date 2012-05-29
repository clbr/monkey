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
#include <mk_server.h>
#include <stdarg.h>

static int lib_running = 0;

static void mklib_run(void *p)
{
    int remote_fd, ret;

    mk_utils_worker_rename("libmonkey");
    mk_socket_set_tcp_defer_accept(config->server_fd);

    while (1) {

        if (!lib_running) {
            sleep(1);
            continue;
        }

        remote_fd = mk_socket_accept(config->server_fd);
        if (remote_fd == -1) continue;

        ret = mk_sched_add_client(remote_fd);
        if (ret == -1) close(remote_fd);
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

    unsigned long len;
    struct host *host = mk_mem_malloc_z(sizeof(struct host));
    mk_list_init(&host->error_pages);
    mk_list_init(&host->server_names);
    mk_string_build(&host->host_signature, &len, "libmonkey");
    mk_string_build(&host->header_host_signature.data,
                    &host->header_host_signature.len,
                    "Server: %s", host->host_signature);

    struct host_alias *alias = mk_mem_malloc_z(sizeof(struct host_alias));
    mk_string_build(&alias->name, &len, config->listen_addr);
    alias->len = strlen(config->listen_addr);
    mk_list_add(&alias->_head, &host->server_names);

    mk_list_add(&host->_head, &config->hosts);
    config->nhosts++;

    if (documentroot) {
        host->documentroot.data = strdup(documentroot);
        host->documentroot.len = strlen(documentroot);
    }
    else {
        host->documentroot.data = "/dev/null";
        host->documentroot.len = sizeof("/dev/null") - 1;
    }

    config->server_software.data = "";
    config->server_software.len = 0;
    config->default_mimetype = mk_string_dup(MIMETYPE_DEFAULT_TYPE);
    mk_mimetype_read_config();

    config->worker_capacity = mk_server_worker_capacity(config->workers);
    config->max_load = (config->worker_capacity * config->workers);

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
    int i;
    char *s;
    va_list va;

    va_start(va, ctx);

    i = va_arg(va, int);
    while (i) {
        const enum mklib_mkc e = i;

        switch(e) {
            case MKC_WORKERS:
                i = va_arg(va, int);
                config->workers = i;
                config->worker_capacity = mk_server_worker_capacity(config->workers);
                config->max_load = (config->worker_capacity * config->workers);
            break;
            case MKC_TIMEOUT:
                i = va_arg(va, int);
                config->timeout = i;
            break;
            case MKC_USERDIR:
                s = va_arg(va, char *);
                config->user_dir = strdup(s);
            break;
            case MKC_INDEXFILE:
                s = va_arg(va, char *);
                config->index_files = mk_string_split_line(s);
            break;
            case MKC_HIDEVERSION:
                i = va_arg(va, int);

                /* Basic server information */
                if (!i) {
                    mk_string_build(&config->server_software.data,
                                    &len, "libmonkey/%s (%s)", VERSION, OS);
                }
                else {
                    mk_string_build(&config->server_software.data, &len, "libmonkey");
                }
                config->server_software.len = len;
            break;
            case MKC_RESUME:
                i = va_arg(va, int);
                config->resume = i ? MK_TRUE : MK_FALSE;
            break;
            case MKC_KEEPALIVE:
                i = va_arg(va, int);
                config->keep_alive = i ? MK_TRUE : MK_FALSE;
            break;
            case MKC_KEEPALIVETIMEOUT:
                i = va_arg(va, int);
                config->keep_alive_timeout = i;
            break;
            case MKC_MAXKEEPALIVEREQUEST:
                i = va_arg(va, int);
                config->max_keep_alive_request = i;
            break;
            case MKC_MAXREQUESTSIZE:
                i = va_arg(va, int);
                config->max_request_size = i;
            break;
        }

        i = va_arg(va, int);
    }

    va_end(va);
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

    ctx->workers = mk_mem_malloc_z(sizeof(pthread_t) * config->workers);

    int i;
    for (i = 0; i < config->workers; i++) {
        mk_sched_launch_thread(config->worker_capacity, &ctx->workers[i]);
    }

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

    int i;
    for (i = 0; i < config->workers; i++) {
        pthread_cancel(ctx->workers[i]);
    }

    free(ctx);
    free(config);

    return MKLIB_TRUE;
}

#endif
