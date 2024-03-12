-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("cheriot test suite")
sdkdir = "../sdk"
includes(sdkdir)

set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

-- Helper to add a C++ test
function test(name)
    compartment(name .. "_test")
         add_files(name .. "-test.cc")
end

-- Helper for creating the different variants of the FreeRTOS compile tests.
function freertos_compile_test(name, defines)
target("freertos-compile-" .. name)
	set_kind("object")
	add_files("ccompile-freertos-test.c")
	add_defines("CHERIOT_CUSTOM_DEFAULT_MALLOC_CAPABILITY")
	add_defines(defines)
end

-- Try compiling the FreeRTOS compat layer with different combinations of
-- semaphore options enabled.
freertos_compile_test("semaphore-only", {"CHERIOT_EXPOSE_FREERTOS_SEMAPHORE"})
freertos_compile_test("mutex-only", {"CHERIOT_EXPOSE_FREERTOS_MUTEX"})
freertos_compile_test("recursive-mutex-only", {"CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX"})
freertos_compile_test("all-options", {"CHERIOT_EXPOSE_FREERTOS_SEMAPHORE", "CHERIOT_EXPOSE_FREERTOS_MUTEX", "CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX"})

-- Fake compartment that owns all C-compile-only tests
compartment("ccompile_test")
	add_files("ccompile-test.c")
	add_deps("freertos-compile-semaphore-only",
	"freertos-compile-mutex-only",
	"freertos-compile-recursive-mutex-only",
	"freertos-compile-all-options")

-- Test MMIO access
test("mmio")
-- Test the allocator and the revoker.
test("allocator")
-- Test the thread pool
test("thread_pool")
-- Test the futex implementation
test("futex")
-- Test locks built on top of the futex
test("queue")
-- Test queues
test("locks")
-- Test the static sealing types
test("static_sealing")
compartment("static_sealing_inner")
	add_files("static_sealing_inner.cc")
-- Test crash recovery.
compartment("crash_recovery_inner")
	add_files("crash_recovery_inner.cc")
compartment("crash_recovery_outer")
	add_files("crash_recovery_outer.cc")
test("crash_recovery")
-- Test the multiwaiter
test("multiwaiter")
-- Test that the event groups APIs work
test("eventgroup")
-- Test stacks
compartment("stack_integrity_thread")
    add_files("stack_integrity_thread.cc")
test("stack")
compartment("compartment_calls_inner")
    add_files("compartment_calls_inner.cc")
compartment("compartment_calls_inner_with_handler")
    add_files("compartment_calls_inner_with_handler.cc")
test("compartment_calls")
test("check_pointer")
-- Test various APIs that are too small to deserve their own test file
test("misc")

includes(path.join(sdkdir, "lib"))

-- Compartment for the test entry point.
compartment("test_runner")
    add_files("test-runner.cc", "global_constructors-test.cc")

-- Firmware image for the test suite.
firmware("test-suite")
    -- Main entry points
    add_deps("test_runner", "thread_pool")
    -- Helper libraries
    add_deps("freestanding", "string", "crt", "cxxrt", "atomic_fixed", "compartment_helpers", "debug")
    add_deps("message_queue", "locks", "event_group")
    -- Tests
    add_deps("mmio_test")
    add_deps("eventgroup_test")
    add_deps("allocator_test")
    add_deps("thread_pool_test")
    add_deps("futex_test")
    add_deps("queue_test")
    add_deps("locks_test")
    add_deps("static_sealing_test", "static_sealing_inner")
    add_deps("crash_recovery_test", "crash_recovery_inner", "crash_recovery_outer")
    add_deps("multiwaiter_test")
    add_deps("ccompile_test")
    add_deps("stack_test", "stack_integrity_thread")
    add_deps("compartment_calls_test", "compartment_calls_inner", "compartment_calls_inner_with_handler")
    add_deps("check_pointer_test")
    add_deps("misc_test")
    -- Set the thread entry point to the test runner.
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "test_runner",
                priority = 3,
                entry_point = "run_tests",
                stack_size = 0x800,
                -- This must be an odd number for the trusted stack exhaustion
                -- test to fail in the right compartment.
                trusted_stack_frames = 9
            },
            {
                compartment = "thread_pool",
                priority = 2,
                entry_point = "thread_pool_run",
                stack_size = 0x600,
                trusted_stack_frames = 8
            },
            {
                compartment = "thread_pool",
                priority = 1,
                entry_point = "thread_pool_run",
                stack_size = 0x600,
                trusted_stack_frames = 8
            }
        }, {expand = false})
    end)
