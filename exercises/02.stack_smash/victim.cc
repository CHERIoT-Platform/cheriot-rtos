#include "victim.h"
#include <cstring>
#include <debug.hh>
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;
using Debug = ConditionalDebug<true, "victim">;

// super secret value chosen by fair die roll
uint64_t secret = 42;

int guess_secret(uint64_t guess) {
    if (guess == secret) {
        Debug::log("Secret guessed correctly! {}", guess);
        return 0;
    } else {
        Debug::log("Incorrect guess: {}", guess);
        return -1;
    }
}

int insecure_memcpy(void* dest, const void* src, size_t n) {
    memcpy(dest, src, n);
    return 0;
}

int secure_memcpy(void* dest, const void* src, size_t n) {
    if (!check_pointer<PermissionSet{Permission::Store}>(dest, n) || 
        !check_pointer<PermissionSet{Permission::Load}>(src, n)) {
        Debug::log("Pointer check failed in secure_memcpy");
        return -EINVAL;
    }

    memcpy(dest, src, n);
    return 0;
}