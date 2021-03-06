/* Fortify macros for strings.h functions.
   Copyright (C) 2017 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef __STRINGS_FORTIFIED
# define __STRINGS_FORTIFIED 1

__fortify_function void
__NTH (bcopy (const void *__src, void *__dest, size_t __len))
{
  (void) __builtin___memmove_chk (__dest, __src, __len, __bos0 (__dest));
}

__fortify_function void
__NTH (bzero (void *__dest, size_t __len))
{
  (void) __builtin___memset_chk (__dest, '\0', __len, __bos0 (__dest));
}

void __explicit_bzero_chk (void *__dest, size_t __len, size_t __destlen)
  __THROW __nonnull ((1));

__fortify_function void
__NTH (explicit_bzero (void *__dest, size_t __len))
{
  __explicit_bzero_chk (__dest, __len, __bos0 (__dest));
}

#endif
