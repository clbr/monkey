/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

#ifndef MONKEY_PLUGIN_H
#define MONKEY_PLUGIN_H

/* General Headers */
#include <errno.h>

/* Monkey Headers */
#include "mk_plugin.h"
#include "mk_list.h"
#include "mk_http.h"
#include "mk_file.h"
#include "mk_socket.h"
#include "mk_macros.h"

/* global vars */
struct plugin_api MK_EXPORT *mk_api;
struct plugin_info MK_EXPORT _plugin_info;

mk_plugin_key_t MK_EXPORT _mkp_data;

#define MONKEY_PLUGIN(a, b, c, d)                   \
    struct plugin_info MK_EXPORT _plugin_info = {a, b, c, d}

#ifdef TRACE
#define PLUGIN_TRACE(...) \
    mk_api->trace(_plugin_info.shortname,     \
                  MK_TRACE_PLUGIN,            \
                  __FUNCTION__,               \
                  __FILE__,                   \
                  __LINE__,                   \
                  __VA_ARGS__)
#else
#define PLUGIN_TRACE(...) do {} while(0)
#endif

/* 
 * Redefine messages macros 
 */

#undef  mk_info
#define mk_info(...) mk_api->_error(MK_INFO, __VA_ARGS__)

#undef  mk_err
#define mk_err(...) mk_api->_error(MK_ERR, __VA_ARGS__)

#undef  mk_warn
#define mk_warn(...) mk_api->_error(MK_WARN, __VA_ARGS__)

#undef  mk_bug
#define mk_bug(condition) do {                  \
        if (mk_unlikely((condition)!=0)) {         \
            mk_api->_error(MK_BUG, "[%s] Bug found in %s() at %s:%d",    \
                           _plugin_info.shortname, __FUNCTION__, __FILE__, __LINE__); \
            abort();                                                    \
        }                                                               \
    } while(0)

#endif
