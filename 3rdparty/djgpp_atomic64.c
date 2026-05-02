/*
 * Non-atomic 64-bit __atomic_*_8 stubs for DJGPP / DOS.
 *
 * DOS is single-threaded so real atomics are unnecessary.  GCC emits calls to
 * these helper functions when targeting i486 (or when __i586__ is undefined)
 * because the ISA lacks a native 64-bit atomic instruction.  Normally libatomic
 * provides them, but DJGPP doesn't ship libatomic.
 *
 * Every function simply performs a plain (non-atomic) load/store/exchange/CAS
 * which is perfectly safe in a single-threaded environment.
 */

#include <stdint.h>
#include <string.h>

uint64_t __atomic_load_8(const volatile void *ptr, int memorder)
{
	(void)memorder;
	uint64_t val;
	memcpy(&val, (const void *)ptr, sizeof(val));
	return val;
}

void __atomic_store_8(volatile void *ptr, uint64_t val, int memorder)
{
	(void)memorder;
	memcpy((void *)ptr, &val, sizeof(val));
}

uint64_t __atomic_exchange_8(volatile void *ptr, uint64_t val, int memorder)
{
	(void)memorder;
	uint64_t old;
	memcpy(&old, (void *)ptr, sizeof(old));
	memcpy((void *)ptr, &val, sizeof(val));
	return old;
}

int __atomic_compare_exchange_8(
	volatile void *ptr,
	void *expected,
	uint64_t desired,
	int success_memorder,
	int failure_memorder
)
{
	(void)success_memorder;
	(void)failure_memorder;
	uint64_t current;
	memcpy(&current, (void *)ptr, sizeof(current));
	uint64_t exp;
	memcpy(&exp, expected, sizeof(exp));
	if (current == exp) {
		memcpy((void *)ptr, &desired, sizeof(desired));
		return 1;
	}
	memcpy(expected, &current, sizeof(current));
	return 0;
}

uint64_t __atomic_fetch_add_8(volatile void *ptr, uint64_t val, int memorder)
{
	(void)memorder;
	uint64_t old;
	memcpy(&old, (void *)ptr, sizeof(old));
	uint64_t new_val = old + val;
	memcpy((void *)ptr, &new_val, sizeof(new_val));
	return old;
}

uint64_t __atomic_fetch_sub_8(volatile void *ptr, uint64_t val, int memorder)
{
	(void)memorder;
	uint64_t old;
	memcpy(&old, (void *)ptr, sizeof(old));
	uint64_t new_val = old - val;
	memcpy((void *)ptr, &new_val, sizeof(new_val));
	return old;
}

uint64_t __atomic_fetch_and_8(volatile void *ptr, uint64_t val, int memorder)
{
	(void)memorder;
	uint64_t old;
	memcpy(&old, (void *)ptr, sizeof(old));
	uint64_t new_val = old & val;
	memcpy((void *)ptr, &new_val, sizeof(new_val));
	return old;
}

uint64_t __atomic_fetch_or_8(volatile void *ptr, uint64_t val, int memorder)
{
	(void)memorder;
	uint64_t old;
	memcpy(&old, (void *)ptr, sizeof(old));
	uint64_t new_val = old | val;
	memcpy((void *)ptr, &new_val, sizeof(new_val));
	return old;
}

uint64_t __atomic_fetch_xor_8(volatile void *ptr, uint64_t val, int memorder)
{
	(void)memorder;
	uint64_t old;
	memcpy(&old, (void *)ptr, sizeof(old));
	uint64_t new_val = old ^ val;
	memcpy((void *)ptr, &new_val, sizeof(new_val));
	return old;
}
