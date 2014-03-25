/*
 * virsh-completer.c: Common custom completion utilities
 *
 * Copyright (C) 2005, 2007-2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "virsh-completer.h"

#include <stdarg.h>

#include "conf/domain_conf.h"
#include "viralloc.h"

/* Utils - General */
char **
vshVarArgsToStringList(vshControl *ctl, unsigned int count, ...)
{
    va_list ap;
    char **strs;
    size_t i;

    if (count == 0)
        return NULL;

    va_start(ap, count);

    strs = vshCalloc(ctl, count, sizeof(char*));
    for (i = 0; i < count; i++)
        strs[i] = vshStrdup(ctl, va_arg(ap, char*));

    va_end(ap);

    return strs;
};
