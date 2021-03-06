/* Elided pthread mutex lock.
   Copyright (C) 2014-2017 Free Software Foundation, Inc.
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

#include <pthread.h>
#include <pthreadP.h>
#include <lowlevellock.h>
#include <htm.h>
#include <elision-conf.h>
#include <stdint.h>

#if !defined(LLL_LOCK) && !defined(EXTRAARG)
/* Make sure the configuration code is always linked in for static
   libraries.  */
#include "elision-conf.c"
#endif

#ifndef EXTRAARG
#define EXTRAARG
#endif
#ifndef LLL_LOCK
#define LLL_LOCK(a,b) lll_lock(a,b), 0
#endif

#define aconf __elision_aconf

/* Adaptive lock using transactions.
   By default the lock region is run as a transaction, and when it
   aborts or the lock is busy the lock adapts itself.  */

int
__lll_lock_elision (int *futex, short *adapt_count, EXTRAARG int private)
{
  /* adapt_count can be accessed concurrently; these accesses can be both
     inside of transactions (if critical sections are nested and the outer
     critical section uses lock elision) and outside of transactions.  Thus,
     we need to use atomic accesses to avoid data races.  However, the
     value of adapt_count is just a hint, so relaxed MO accesses are
     sufficient.
     Do not begin a transaction if another cpu has locked the
     futex with normal locking.  If adapt_count is zero, it remains and the
     next pthread_mutex_lock call will try to start a transaction again.  */
    if (atomic_load_relaxed (futex) == 0
	&& atomic_load_relaxed (adapt_count) <= 0 && aconf.try_tbegin > 0)
    {
      int status = __libc_tbegin_retry ((void *) 0, aconf.try_tbegin - 1);
      if (__builtin_expect (status == _HTM_TBEGIN_STARTED,
			    _HTM_TBEGIN_STARTED))
	{
	  /* Check the futex to make sure nobody has touched it in the
	     mean time.  This forces the futex into the cache and makes
	     sure the transaction aborts if some other cpu uses the
	     lock (writes the futex).  */
	  if (__builtin_expect (atomic_load_relaxed (futex) == 0, 1))
	    /* Lock was free.  Return to user code in a transaction.  */
	    return 0;

	  /* Lock was busy.  Fall back to normal locking.  */
	  if (__builtin_expect (__libc_tx_nesting_depth () <= 1, 1))
	    {
	      /* In a non-nested transaction there is no need to abort,
		 which is expensive.  Simply end the started transaction.  */
	      __libc_tend ();
	      /* Don't try to use transactions for the next couple of times.
		 See above for why relaxed MO is sufficient.  */
	      if (aconf.skip_lock_busy > 0)
		atomic_store_relaxed (adapt_count, aconf.skip_lock_busy);
	    }
	  else /* nesting depth is > 1 */
	    {
	      /* A nested transaction will abort eventually because it
		 cannot make any progress before *futex changes back to 0.
		 So we may as well abort immediately.
		 This persistently aborts the outer transaction to force
		 the outer mutex use the default lock instead of retrying
		 with transactions until the try_tbegin of the outer mutex
		 is zero.
		 The adapt_count of this inner mutex is not changed,
		 because using the default lock with the inner mutex
		 would abort the outer transaction.  */
	      __libc_tabort (_HTM_FIRST_USER_ABORT_CODE | 1);
	      __builtin_unreachable ();
	    }
	}
      else if (status != _HTM_TBEGIN_TRANSIENT)
	{
	  /* A persistent abort (cc 1 or 3) indicates that a retry is
	     probably futile.  Use the normal locking now and for the
	     next couple of calls.
	     Be careful to avoid writing to the lock.  See above for why
	     relaxed MO is sufficient.  */
	  if (aconf.skip_lock_internal_abort > 0)
	    atomic_store_relaxed (adapt_count,
				  aconf.skip_lock_internal_abort);
	}
      else
	{
	  /* The transaction failed for some retries with
	     _HTM_TBEGIN_TRANSIENT.  Use the normal locking now and for the
	     next couple of calls.  */
	  if (aconf.skip_lock_out_of_tbegin_retries > 0)
	    atomic_store_relaxed (adapt_count,
				  aconf.skip_lock_out_of_tbegin_retries);
	}
    }

  /* Use normal locking as fallback path if transaction does not succeed.  */
  return LLL_LOCK ((*futex), private);
}
