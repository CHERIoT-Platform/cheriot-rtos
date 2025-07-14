#include <vector>
#include <debug.hh>
#include "microvium-ffi.hh"
#include "js.hh"

using Debug = ConditionalDebug<true, "JS compartment">;

int run_js (uint8_t *bytecode, size_t sz) {
	Debug::log("{} bytes of heap available",
		heap_quota_remaining(MALLOC_CAPABILITY));

    ////////////////////////////////////////////////////////////////////////
    // We've now read the bytecode into a buffer.  Spin up the JavaScript
    // VM to execute it.
    ////////////////////////////////////////////////////////////////////////

    // Allocate the space for the VM capability registers on the stack and
    // record its location.
    // **Note**: This must be on the stack and in same compartment as the
    // JavaScript interpreter, so that the callbacks can re-derive it from
    // csp.
    AttackerRegisterState state;
    attackerRegisterStateAddress = Capability{&state}.address();

    mvm_TeError                         err;
    std::unique_ptr<mvm_VM, MVMDeleter> vm;
    // Create a Microvium VM from the bytecode.
    {
        mvm_VM *rawVm;
        err = mvm_restore(
            &rawVm,            /* Out pointer to the VM */
            bytecode,   /* Bytecode data */
            sz,   /* Bytecode length */
            MALLOC_CAPABILITY, /* Capability used to allocate memory */
            ::resolve_import); /* Callback used to resolve FFI imports */
        // If this is not valid bytecode, give up.
        Debug::Assert(
            err == MVM_E_SUCCESS, "Failed to parse bytecode: {}", err);
        vm.reset(rawVm);
    }

    // Get a handle to the JavaScript `run` function.
    mvm_Value run;
    err = mvm_resolveExports(vm.get(), &ExportRun, &run, 1);
    if (err != MVM_E_SUCCESS)
    {
        Debug::log("Failed to get run function: {}", err);
		return -2;
    }
    else
    {
        // Call the function:
        err = mvm_call(vm.get(), run, nullptr, nullptr, 0);
        if (err != MVM_E_SUCCESS)
        {
            Debug::log("Failed to call run function: {}", err);
			return -3;
        }
    }
	return 0;
}
