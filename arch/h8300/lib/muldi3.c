/* More subroutines needed by GCC output code on some machines.  */
/* Compile this one with gcc.  */
/* Copyright (C) 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004, 2005  Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.  */

#include "libgcc.h"

#define __ll_B ((UWtype) 1 << (W_TYPE_SIZE / 2))
#define __ll_lowpart(t) ((UWtype) (t) & (__ll_B - 1))
#define __ll_highpart(t) ((UWtype) (t) >> (W_TYPE_SIZE / 2))

#define umul_ppmm(w1,w0,u,v) \
  do { \
    UWtype __x0, __x1, __x2, __x3; \
    UHWtype __ul, __vl, __uh, __vh; \
    __ul = __ll_lowpart (u); \
    __uh = __ll_highpart (u); \
    __vl = __ll_lowpart (v); \
    __vh = __ll_highpart (v); \
    __x0 = (UWtype) __ul * __vl; \
    __x1 = (UWtype) __ul * __vh; \
    __x2 = (UWtype) __uh * __vl; \
    __x3 = (UWtype) __uh * __vh; \
    __x1 += __ll_highpart (__x0); \
    __x1 += __x2; \
    if (__x1 < __x2) \
      __x3 += __ll_B; \
    (w1) = __x3 + __ll_highpart (__x1); \
    (w0) = __ll_lowpart (__x1) * __ll_B + __ll_lowpart (__x0); \
  } while (0)

#define __umulsidi3(u,v) ( 			\
    { 						\
      DWunion __w; 				\
      umul_ppmm (__w.s.high, __w.s.low, u, v); 	\
      __w.ll; } 				\
    )

DWtype
__muldi3 (DWtype u, DWtype v)
{
  const DWunion uu = {.ll = u};
  const DWunion vv = {.ll = v};
  DWunion w = {.ll = __umulsidi3 (uu.s.low, vv.s.low)};

  w.s.high += ((UWtype) uu.s.low * (UWtype) vv.s.high
	       + (UWtype) uu.s.high * (UWtype) vv.s.low);

  return w.ll;
}
