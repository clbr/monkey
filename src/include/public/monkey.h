/*  Monkey HTTP Daemon
 *  ------------------
 *  Copyright (C) 2001-2012, Eduardo Silva P. <edsiper@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef MK_MONKEYLIB_H
#define MK_MONKEYLIB_H

#include <pthread.h>

#define MONKEY__
#define MONKEY_MINOR__
#define MONKEY_PATCHLEVEL__

/* ---------------------------------
 *               Data
 * --------------------------------- */

/* Opaque pointer, not for use for the apps */
typedef struct mklib_ctx_t* mklib_ctx;

enum {
    MKLIB_FALSE = 0,
    MKLIB_TRUE = 1
};

/* Supported plugins, OR'ed in the init call */
enum {
    MKLIB_LIANA = 0x1,
    MKLIB_LIANA_SSL = 0x2
};

/* Config options for the main config call */
enum {
    MKC_WORKERS,
    MKC_TIMEOUT,
    MKC_PIDFILE,
    MKC_USERDIR,
    MKC_INDEXFILE,
    MKC_HIDEVERSION,
    MKC_RESUME,
    MKC_USER,
    MKC_KEEPALIVE,
    MKC_KEEPALIVETIMEOUT,
    MKC_MAXKEEPALIVEREQUEST,
    MKC_MAXREQUESTSIZE,
    MKC_SYMLINK,
    MKC_DEFAULTMIMETYPE
};

/* Config options for the vhost config call */
enum {
    MKV_SERVERNAME,
    MKV_DOCUMENTROOT
};

/* struct session_request need not be exposed */
typedef void mklib_session;

/* Called when a new connection arrives. Return MKLIB_FALSE to reject this connection. */
typedef int (*ipcheck)(const char *ip);

/* Called when the URL is known. Return MKLIB_FALSE to reject this connection. */
typedef int (*urlcheck)(const char *url);

/* The data callback. Return MKLIB_FALSE if you don't want to handle this URL
 * (it will be checked against real files at this vhost's DocumentRoot).
 *
 * Set *content to point to the content memory (NULL-terminated). It must
 * stay available until the close callback is called.
 *
 * *header has static storage of 34 bytes for any custom headers. */
typedef int (*data)(const mklib_session *, const char *vhost, const char *url,
                    unsigned int *status, char **content, char *header);

/* This will be called after the content has been served. If you allocated
 * any memory, you can match that memory to the mklib_session pointer and free
 * it in this callback. */
typedef void (*close)(const mklib_session *, const char *);


/* ---------------------------------
 *                API
 * --------------------------------- */

/* Returns NULL on error. All pointer arguments may be NULL and the port/plugins
 * may be 0 for the defaults in each case.
 *
 * With no address, bind to all.
 * With no port, use 2001.
 * With no plugins, default to MKLIB_LIANA only.
 * With no documentroot, the default vhost won't access files.
 */
struct mklib_ctx mklib_init(const char *address, unsigned int port, unsigned int plugins,
                            const char *documentroot,
                            ipcheck, urlcheck, data, close);

/* NULL-terminated config call, consisting of pairs of config item and argument.
 * Returns MKLIB_FALSE on failure. */
int mklib_config(mklib_ctx, ...);

/* NULL-terminated config call creating a vhost with *name. Returns MKLIB_FALSE
 * on failure. */
int mklib_vhost_config(mklib_ctx, char *name, ...);

/* Start the server. */
int mklib_start(mklib_ctx);

/* Stop the server and free mklib_ctx. */
int mklib_stop(mklib_ctx);

#endif
