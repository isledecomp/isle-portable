// this is the only way to stop miniaudio atomics error when building isle.rpx and lego1.rpx

#include <stdint.h>

uint64_t __atomic_load_8(const uint64_t* p, int memorder) {
    (void)memorder;
    return *p;
}

void __atomic_store_8(uint64_t* p, uint64_t v, int memorder) {
    (void)memorder;
    *p = v;
}

uint64_t __atomic_exchange_8(uint64_t* p, uint64_t v, int memorder) {
    (void)memorder;
    uint64_t old = *p;
    *p = v;
    return old;
}

uint64_t __atomic_fetch_add_8(uint64_t* p, uint64_t v, int memorder) {
    (void)memorder;
    uint64_t old = *p;
    *p += v;
    return old;
}

uint64_t __atomic_fetch_sub_8(uint64_t* p, uint64_t v, int memorder) {
    (void)memorder;
    uint64_t old = *p;
    *p -= v;
    return old;
}

uint64_t __atomic_fetch_and_8(uint64_t* p, uint64_t v, int memorder) {
    (void)memorder;
    uint64_t old = *p;
    *p &= v;
    return old;
}

uint64_t __atomic_fetch_or_8(uint64_t* p, uint64_t v, int memorder) {
    (void)memorder;
    uint64_t old = *p;
    *p |= v;
    return old;
}

uint64_t __atomic_fetch_xor_8(uint64_t* p, uint64_t v, int memorder) {
    (void)memorder;
    uint64_t old = *p;
    *p ^= v;
    return old;
}
