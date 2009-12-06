#ifndef _URCU_UATOMIC_ARCH_S390_H
#define _URCU_UATOMIC_ARCH_S390_H

/*
 * Atomic exchange operations for the S390 architecture. Based on information
 * taken from the Principles of Operation Appendix A "Conditional Swapping
 * Instructions (CS, CDS)".
 *
 * Copyright (c) 2009 Novell, Inc.
 * Author: Jan Blunck <jblunck@suse.de>
 * Copyright (c) 2009 Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <urcu/compiler.h>
#include <urcu/system.h>

#ifndef __SIZEOF_LONG__
#ifdef __s390x__
#define __SIZEOF_LONG__ 8
#else
#define __SIZEOF_LONG__ 4
#endif
#endif

#ifndef BITS_PER_LONG
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#endif

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 2)
#define COMPILER_HAVE_SHORT_MEM_OPERAND
#endif

/*
 * MEMOP assembler operand rules:
 * - op refer to MEMOP_IN operand
 * - MEMOP_IN can expand to more than a single operand. Use it at the end of
 *   operand list only.
 */

#ifdef COMPILER_HAVE_SHORT_MEM_OPERAND

#define MEMOP_OUT(addr)	"=Q" (*(addr))
#define MEMOP_IN	"Q" (*(addr))
#define MEMOP_REF(op)	#op		/* op refer to MEMOP_IN operand */

#else /* !COMPILER_HAVE_SHORT_MEM_OPERAND */

#define MEMOP_OUT(addr)	"=m" (*(addr))
#define MEMOP_IN	"a" (addr), "m" (*(addr))
#define MEMOP_REF(op)	"0(" #op ")"	/* op refer to MEMOP_IN operand */

#endif /* !COMPILER_HAVE_SHORT_MEM_OPERAND */

#define uatomic_set(addr, v)	STORE_SHARED(*(addr), (v))
#define uatomic_read(addr)	LOAD_SHARED(*(addr))

/* xchg */

static inline __attribute__((always_inline))
unsigned long _uatomic_exchange(volatile void *addr, unsigned long val, int len)
{
	switch (len) {
	case 4:
	{
		unsigned int old_val;

		__asm__ __volatile__(
			"0:	cs %0,%2," MEMOP_REF(%3) "\n"
			"	brc 4,0b\n"
			: "=&r" (old_val), MEMOP_OUT (addr)
			: "r" (val), MEMOP_IN (addr)
			: "memory", "cc");
		return old_val;
	}
#if (BITS_PER_LONG == 64)
	case 8:
	{
		unsigned long old_val;

		__asm__ __volatile__(
			"0:	csg %0,%2," MEMOP_REF(%3) "\n"
			"	brc 4,0b\n"
			: "=&r" (old_val), MEMOP_OUT (addr)
			: "r" (val), MEMOP_IN (addr)
			: "memory", "cc");
		return old_val;
	}
#endif
	default:
		__asm__ __volatile__(".long	0xd00d00");
	}

	return 0;
}

#define uatomic_xchg(addr, v)						    \
	(__typeof__(*(addr))) _uatomic_exchange((addr), (unsigned long)(v), \
					       sizeof(*(addr)))

/* cmpxchg */

static inline __attribute__((always_inline))
unsigned long _uatomic_cmpxchg(void *addr, unsigned long old,
			       unsigned long _new, int len)
{
	switch (len) {
	case 4:
	{
		unsigned int old_val = (unsigned int)old;

		__asm__ __volatile__(
			"	cs %0,%2," MEMOP_REF(%3) "\n"
			: "+r" (old_val), MEMOP_OUT (addr)
			: "r" (_new), MEMOP_IN (addr)
			: "memory", "cc");
		return old_val;
	}
#if (BITS_PER_LONG == 64)
	case 8:
	{
		__asm__ __volatile__(
			"	csg %0,%2," MEMOP_REF(%3) "\n"
			: "+r" (old), MEMOP_OUT (addr)
			: "r" (_new), MEMOP_IN (addr)
			: "memory", "cc");
		return old;
	}
#endif
	default:
		__asm__ __volatile__(".long	0xd00d00");
	}

	return 0;
}

#define uatomic_cmpxchg(addr, old, _new)				\
	(__typeof__(*(addr))) _uatomic_cmpxchg((addr),			\
					       (unsigned long)(old),	\
					       (unsigned long)(_new),	\
					       sizeof(*(addr)))

/* uatomic_add_return */

static inline __attribute__((always_inline))
unsigned long _uatomic_add_return(void *addr, unsigned long val, int len)
{
	switch (len) {
	case 4:
	{
		unsigned int old, oldt;

		oldt = uatomic_read((unsigned int *)addr);
		do {
			old = oldt;
			oldt = _uatomic_cmpxchg(addr, old, old + val, 4);
		} while (oldt != old);

		return old + val;
	}
#if (BITS_PER_LONG == 64)
	case 8:
	{
		unsigned long old, oldt;

		oldt = uatomic_read((unsigned long *)addr);
		do {
			old = oldt;
			oldt = _uatomic_cmpxchg(addr, old, old + val, 8);
		} while (oldt != old);

		return old + val;
	}
#endif
	}
	__builtin_trap();
	return 0;
}

#define uatomic_add_return(addr, v)					\
	((__typeof__(*(addr))) _uatomic_add_return((addr),		\
						  (unsigned long)(v),	\
						  sizeof(*(addr))))

/* uatomic_sub_return, uatomic_add, uatomic_sub, uatomic_inc, uatomic_dec */

#define uatomic_sub_return(addr, v)	uatomic_add_return((addr), -(v))

#define uatomic_add(addr, v)		(void)uatomic_add_return((addr), (v))
#define uatomic_sub(addr, v)		(void)uatomic_sub_return((addr), (v))

#define uatomic_inc(addr)		uatomic_add((addr), 1)
#define uatomic_dec(addr)		uatomic_add((addr), -1)

#define compat_uatomic_cmpxchg(ptr, old, _new)	uatomic_cmpxchg(ptr, old, _new)

#endif /* _URCU_UATOMIC_ARCH_S390_H */
