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

-- Helper to add a C test
function test_c(name)
    compartment(name .. "_test")
         add_files(name .. "-test.c")
end

-- Test the allocator and the revoker.
-- test("allocator")
-- Test the thread pool
test("thread_pool")
-- -- Test the futex implementation
-- test("futex")
-- -- Test locks built on top of the futex
-- test("queue")
-- -- Test queues
-- test("locks")
-- -- Test the static sealing types
-- test("static_sealing")
-- compartment("static_sealing_inner")
-- 	add_files("static_sealing_inner.cc")
-- -- Test crash recovery.
-- compartment("crash_recovery_inner")
-- 	add_files("crash_recovery_inner.cc")
-- compartment("crash_recovery_outer")
-- 	add_files("crash_recovery_outer.cc")
-- test("crash_recovery")
-- -- Test the multiwaiter
-- test("multiwaiter")
-- -- Test that C code can compile
-- test_c("ccompile")
-- -- Test stacks
-- compartment("stack_integrity_thread")
--     add_files("stack_integrity_thread.cc")
-- test("stack")
-- compartment("compartment_calls_inner")
--     add_files("compartment_calls_inner.cc")
-- compartment("compartment_calls_inner_with_handler")
--     add_files("compartment_calls_inner_with_handler.cc")
-- test("compartment_calls")
compartment("dma")
    add_files(path.join(sdkdir, "core/dma-v2/dma_compartment.cc"))

test("dma")

includes(path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/cxxrt"),
         path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/string"),
         path.join(sdkdir, "lib/crt"),
         path.join(sdkdir, "lib/thread_pool"))

-- Compartment for the test entry point.
compartment("test_runner")
    add_files("test-runner.cc", "global_constructors-test.cc")

-- Firmware image for the test suite.
firmware("test-suite")
    -- Main entry points
    add_deps("test_runner", "thread_pool")
    -- Helper libraries
    add_deps("freestanding", "string", "crt", "cxxrt", "atomic_fixed")
    -- Tests
    -- add_deps("allocator_test")
    add_deps("thread_pool_test")
    -- add_deps("futex_test")
    -- add_deps("queue_test")
    -- add_deps("locks_test")
    -- add_deps("static_sealing_test", "static_sealing_inner")
    -- add_deps("crash_recovery_test", "crash_recovery_inner", "crash_recovery_outer")
    -- add_deps("multiwaiter_test")
    -- add_deps("ccompile_test")
    -- add_deps("stack_test", "stack_integrity_thread")
    -- add_deps("compartment_calls_test", "compartment_calls_inner", "compartment_calls_inner_with_handler")
    add_deps("dma_test")
    add_deps("dma")
    -- Set the thread entry point to the test runner.
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "test_runner",
                priority = 3,
                entry_point = "run_tests",
                stack_size = 0x600,
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
            }
            -- {
            --     compartment = "thread_pool",
            --     priority = 1,
            --     entry_point = "thread_pool_run",
            --     stack_size = 0x600,
            --     trusted_stack_frames = 8
            -- }
        }, {expand = false})
    end)
