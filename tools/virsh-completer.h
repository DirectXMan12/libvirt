/*
 * virsh-completer.h: Common custom completion utilities
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

#ifndef VIRSH_COMPLETER_H
# define VIRSH_COMPLETER_H

# include "virsh.h"

/* Utils - General */
/* __VA_NARGS__:
 *
 * This macro determine the length (up to 63) of
 * __VA_ARGS__ arguments passed to a macro.
 */

/* inspired by
 * https://groups.google.com/forum/#!topic/comp.std.c/d-6Mj5Lko_s */
# define __VA_NARGS__(...) \
    __VA_NARGS_FLATTEN__(__VA_ARGS__,INV_NUM_SEQ())
# define __VA_NARGS_FLATTEN__(...) \
    __VA_NARGS_IMPL__(__VA_ARGS__)
# define __VA_NARGS_IMPL__( \
    _1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16, \
    _17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
    _31,_32,_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44, \
    _45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58, \
    _59,_60,_61,_62,_63, N, ...) N
# define INV_NUM_SEQ() \
    63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46, \
    45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28, \
    27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10, \
    9,8,7,6,5,4,3,2,1,0

/* VSH_STRING_COMPLETER:
 *
 * @ctl: a vshControl* or NULL
 * @name: the name of the completer (unquoted)
 * @__VA_ARGS__: the options as strings
 *
 * This macro creates a vshComplete[name] function
 * suitable to for use as a custom option completer.
 * The completer will return an array of strings with
 * the values specified.
 */
# define VSH_STRING_COMPLETER(ctl, name, ...) \
    static char ** \
    vshComplete ## name (unsigned int flags) \
    { \
        virCheckFlags(0, NULL); \
        return vshVarArgsToStringList(ctl, __VA_NARGS__(__VA_ARGS__), \
                                      __VA_ARGS__); \
    }

char ** vshVarArgsToStringList(vshControl *ctl, unsigned int count, ...);

#endif /* VIRSH_COMPLETER_H */
