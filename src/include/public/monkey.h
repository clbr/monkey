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

#define __MONKEY__
#define __MONKEY_MINOR__
#define __MONKEY_PATCHLEVEL__


/* Data */

/* Opaque pointer, not for use for the apps */
typedef struct mklib_ctx_t* mklib_ctx;

enum {
    MKLIB_FALSE = 0,
    MKLIB_TRUE = 1
};

/* struct session_request need not be exposed */
typedef void mklib_session;

typedef int (*ipcheck)(const char *);
typedef int (*urlcheck)(const char *);
typedef int (*data)(const mklib_session *, const char *, const char *, unsigned int *, char *);
typedef void (*close)(const mklib_session *, const char *);


/* API */

struct mklib_ctx mklib_init(const char *address, unsigned int port, unsigned int plugins,
                            ipcheck, urlcheck, data, close);

#endif
