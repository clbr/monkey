/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Monkey HTTP Daemon
 * ------------------
 * Copyright (C) 2012, Lauri Kasanen <cand@gmx.com>
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

static struct host *mk_lib_host_find(const char *name)
{
    struct host *entry_host;
    struct mk_list *head_vhost;

    mk_list_foreach(head_vhost, &config->hosts) {
        entry_host = mk_list_entry(head_vhost, struct host, _head);

        if (strcmp(name, entry_host->file) == 0) return entry_host;
    }

    return NULL;
}


static void mklib_run(void *p)
{
    int remote_fd, ret;
    mklib_ctx ctx = p;

    mk_utils_worker_rename("libmonkey");
    mk_socket_set_tcp_defer_accept(config->server_fd);

    while (1) {

        if (!ctx->lib_running) {
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
        dlclose(handle);
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
    if (!a) return NULL;

    config = mk_mem_malloc_z(sizeof(struct server_config));
    if (!config) return NULL;

    a->ipf = ipf;
    a->urlf = urlf;
    a->dataf = dataf;
    a->closef = closef;

    config->serverconf = strdup(MONKEY_PATH_CONF);
    mk_config_set_init_values();
    mk_sched_init();
    mk_plugin_init();

    if (plugins & MKLIB_LIANA_SSL) {
        config->transport_layer = strdup("liana_ssl");
        if (!load_networking(PLUGDIR"/monkey-liana_ssl.so")) return NULL;
    }
    else {
        config->transport_layer = strdup("liana");
        if (!load_networking(PLUGDIR"/monkey-liana.so")) return NULL;
    }

    if (!plg_netiomap) return NULL;
    mk_plugin_preworker_calls();

    if (port) config->serverport = port;
    if (address) config->listen_addr = strdup(address);
    else config->listen_addr = strdup(config->listen_addr);

    unsigned long len;
    struct host *host = mk_mem_malloc_z(sizeof(struct host));
    /* We hijack this field for the vhost name */
    host->file = strdup("default");
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
        host->documentroot.data = strdup("/dev/null");
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
    mk_clock_sequential_init();
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
    if (!ctx || ctx->lib_running) return MKLIB_FALSE;

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

                free(sched_list);
                mk_sched_init();
            break;
            case MKC_TIMEOUT:
                i = va_arg(va, int);
                config->timeout = i;
            break;
            case MKC_USERDIR:
                s = va_arg(va, char *);
                if (config->user_dir) free(config->user_dir);
                config->user_dir = strdup(s);
            break;
            case MKC_INDEXFILE:
                s = va_arg(va, char *);
                if (config->index_files) mk_string_split_free(config->index_files);
                config->index_files = mk_string_split_line(s);
            break;
            case MKC_HIDEVERSION:
                i = va_arg(va, int);
                config->server_software.data = NULL;

                /* Basic server information */
                if (!i) {
                    mk_string_build(&config->server_software.data,
                                    &len, "libmonkey/%s (%s)", VERSION, OS);
                }
                else {
                    mk_string_build(&config->server_software.data, &len, "libmonkey");
                }
                config->server_software.len = len;

                /* Mark it so for the default vhost */
                struct mk_list *hosts = &config->hosts;
                struct host *def = mk_list_entry_first(hosts, struct host, _head);
                free(def->host_signature);
                free(def->header_host_signature.data);
                def->header_host_signature.data = NULL;

                def->host_signature = strdup(config->server_software.data);
                mk_string_build(&def->header_host_signature.data,
                                &def->header_host_signature.len,
                                "Server: %s", def->host_signature);
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
            case MKC_SYMLINK:
                i = va_arg(va, int);
                config->symlink = i ? MK_TRUE : MK_FALSE;
            break;
            case MKC_DEFAULTMIMETYPE:
                s = va_arg(va, char *);
                free(config->default_mimetype);
                config->default_mimetype = NULL;
                mk_string_build(&config->default_mimetype, &len, "%s\r\n", s);
                mk_pointer_set(&mimetype_default->type, config->default_mimetype);
            break;
            default:
                mk_warn("Unknown config option");
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

    /* Does it exist already? */
    struct host *h = mk_lib_host_find(name);
    if (h) return MKLIB_FALSE;

    const struct host *defaulth = mk_lib_host_find("default");
    if (!defaulth) return MKLIB_FALSE;


    h = mk_mem_malloc_z(sizeof(struct host));
    h->file = strdup(name);

    h->documentroot.data = strdup("/dev/null");
    h->documentroot.len = sizeof("/dev/null") - 1;

    mk_list_init(&h->error_pages);
    mk_list_init(&h->server_names);

    char *s;
    int i;
    va_list va;

    va_start(va, name);

    i = va_arg(va, int);
    while (i) {
        const enum mklib_mkv e = i;

        switch(e) {
            case MKV_SERVERNAME:
                s = va_arg(va, char *);

                struct mk_list *head, *list = mk_string_split_line(s);

                mk_list_foreach(head, list) {
                    struct mk_string_line *entry = mk_list_entry(head,
                                                                 struct mk_string_line,
                                                                 _head);
                    if (entry->len > MK_HOSTNAME_LEN - 1) {
                        continue;
                    }

                    struct host_alias *alias = mk_mem_malloc_z(sizeof(struct host_alias));
                    alias->name = mk_string_tolower(entry->val);
                    alias->len = entry->len;
                    mk_list_add(&alias->_head, &h->server_names);
                }

                mk_string_split_free(list);
            break;
            case MKV_DOCUMENTROOT:
                s = va_arg(va, char *);
                free(h->documentroot.data);
                h->documentroot.data = strdup(s);
                h->documentroot.len = strlen(s);
            break;
            default:
                mk_warn("Unknown config option");
            break;
        }

        i = va_arg(va, int);
    }

    h->host_signature = strdup(defaulth->host_signature);
    h->header_host_signature.data = strdup(defaulth->header_host_signature.data);
    h->header_host_signature.len = defaulth->header_host_signature.len;

    mk_list_add(&h->_head, &config->hosts);
    config->nhosts++;

    va_end(va);
    return MKLIB_TRUE;
}

/* Start the server. */
int mklib_start(mklib_ctx ctx)
{
    if (!ctx || ctx->lib_running) return MKLIB_FALSE;

    ctx->workers = mk_mem_malloc_z(sizeof(pthread_t) * config->workers);

    int i;
    for (i = 0; i < config->workers; i++) {
        mk_sched_launch_thread(config->worker_capacity, &ctx->workers[i], ctx);
    }

    ctx->lib_running = 1;
    ctx->tid = mk_utils_worker_spawn_arg(mklib_run, ctx);

    return MKLIB_TRUE;
}

/* Stop the server and free mklib_ctx. */
int mklib_stop(mklib_ctx ctx)
{
    if (!ctx || !ctx->lib_running) return MKLIB_FALSE;

    ctx->lib_running = 0;
    pthread_cancel(ctx->tid);

    int i;
    for (i = 0; i < config->workers; i++) {
        pthread_cancel(ctx->workers[i]);
    }

    mk_plugin_exit_all();

#ifdef SAFE_FREE
    mk_config_free_all();
#else
    free(config);
#endif

    free(ctx);

    return MKLIB_TRUE;
}

#endif
