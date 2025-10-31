-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("cheriot test suite")
local scriptdir = os.scriptdir()

sdkdir = "../sdk"
includes(sdkdir)

set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

option("most-tests")
    set_description("Build with most tests by default")
    set_showmenu(true)
    set_default("true")

-- All targets here gain the build directory as an include path, since we
-- generate the test prototype header therein.
rule("cheriot.test.ibuilddir")
    on_load(function (target)
        import("core.project.config")
        target:add("cxflags", config.builddir and "-I$(builddir)" or "-I$(buildir)")
    end)
target()
    add_rules("cheriot.test.ibuilddir")

rule("cheriot.test.phony")
    on_load(function (target)
        target:set("kind", "phony")
        target:set("cheriot.type", "test-phony")
    end)

-- Helper to add a C++ test
local defined_tests = {}
local test_options = {}
function test(name, opts)
    opts = opts or {}

    local option_name = "test-" .. name

    table.insert(defined_tests, name)
    test_options[name] = opts

    option(option_name)
        set_category("tests")
        add_deps("most-tests")

        -- https://github.com/xmake-io/xmake/issues/6419#issuecomment-2872975584
        on_check(function (option)
            if opts.not_most ~= true then
                try { function ()
                    option:set_value(option:dep("most-tests"):enabled())
                end }
            end
        end)

    target(name .. "_test")
        set_default(false)
        add_rules(
            (opts.compartment == false)
            and "cheriot.test.phony"
            or "cheriot.compartment")
        if opts.file ~= false then
            add_files(name .. "-test.cc")
        end
        if get_config("print-floats") then
            add_defines("CHERIOT_PRINT_FLOATS")
            add_deps("softfloat")
        end
        if get_config("print-doubles") then
            add_defines("CHERIOT_PRINT_DOUBLES")
            add_deps("softfloat")
        end
        on_load(function(target)
          target:set("cheriot.test.option", option_name)
        end)
end

-- Helper for creating the different variants of the FreeRTOS compile tests.
function freertos_compile_test(name, defines)
target("freertos-compile-" .. name)
    set_default(false)
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
test("ccompile", { file = false, prototype = false, name = false })
    add_files("ccompile-test.c")
    add_deps("freertos-compile-semaphore-only",
    "freertos-compile-mutex-only",
    "freertos-compile-recursive-mutex-only",
    "freertos-compile-all-options")

-- Fictitious test that just adds dependencies to compile
test("extra_compiles", { file = false, compartment = false, prototype = false, name = false })
    add_deps("atomic_fixed")

-- Fictitious test for the preamble of test-runner.cc:/run_tests
test("preflight", { file = false, compartment = false, prototype = false, name = false } )

-- Test the debug helpers.
test("debug_cxx", { name = "Debug helpers (C++)" })
test("debug_c", { file = false, name = "Debug helpers (C)" })
    add_files("debug_c-test.c")

test("isa", {name="ISA"})

-- Test MMIO access
test("mmio", { name = "MMIO" })

-- Test unwind cleanup
test("unwind_cleanup", { name = "Unwind cleanup" })
    add_deps("unwind_error_handler")

-- Smoke tests for softfloat
test("softfloat")
    add_deps("softfloat")

-- Test minimal stdio implementation
test("stdio")
    add_deps("stdio")

test("bigdata", { not_most = true, conflicts = { "allocator" } })

-- Test the static sealing types
compartment("static_sealing_inner")
    add_files("static_sealing_inner.cc")
    set_default(false)
test("static_sealing", { name = "Static sealing" })
    add_deps("static_sealing_inner")

-- Test crash recovery.
compartment("crash_recovery_inner")
    add_files("crash_recovery_inner.cc")
    set_default(false)
compartment("crash_recovery_outer")
    add_files("crash_recovery_outer.cc")
    set_default(false)
test("crash_recovery", { name = "Crash recovery" })
    add_deps("crash_recovery_inner", "crash_recovery_outer")

-- Test cross-compartment calls
compartment("compartment_calls_inner")
    add_files("compartment_calls_inner.cc")
    set_default(false)
test("compartment_calls")
    add_deps("compartment_calls_inner")

-- Test check_pointer
test("check_pointer")

-- Test various APIs that are too small to deserve their own test file
test("misc")
    add_deps("string", "strtol")
    on_load(function(target)
        target:values_set("shared_objects", { exampleK = 1024, test_word = 4 }, {expand = false})
    end)

-- Test stacks
compartment("stack_integrity_thread")
    add_files("stack_integrity_thread.cc")
    set_default(false)
test("stack", { name = "Switcher stack handling" })
    add_deps("stack_integrity_thread")

-- Test the thread pool
test("thread_pool", { name = "Thread pool" })
    add_deps("cxxrt")

-- Test our support for global constructors
test("global_constructors", { compartment = false } )
    add_files("global_constructors-test.cc")

-- Test queues
test("queue", { name = "Queue" })
    add_deps("message_queue")

-- Test the futex implementation
test("futex", { name = "Futex" })

-- Test locks built on top of the futex
test("locks", { name = "Locks" })
    add_deps("cxxrt")

-- Test the generic linked list from ds/
test("list", { name = "List" })

-- Test that the event groups APIs work
test("eventgroup", { name = "Event groups" })
    add_deps("cxxrt", "event_group")

-- Test the multiwaiter
test("multiwaiter", { name = "Multiwatier" })
    add_deps("cxxrt", "message_queue")

-- Test the allocator and the revoker.
test("allocator", { name = "Allocator" })
    add_deps("cxxrt")

includes(path.join(sdkdir, "lib"))

rule("cheriot.tests")
    on_load(function (target)
        local test_header_lines = {}
        local test_calls = {}
        for _, test in ipairs(defined_tests) do
            local opts = test_options[test]
            local test_enabled = get_config("test-" .. test)
            local test_compartment = test .. "_test"

            -- For tests that don't want a prototype, generate an enabled flag
            if opts.prototype == false then
                table.insert(test_header_lines, format(
                    "static const int TestEnabled_%s = %d;",
                    test, test_enabled and 1 or 0))
            end

            if test_enabled then
                -- Even tests without compartments still have targets
                target:add("deps", test_compartment)

                if opts.prototype ~= false then
                    table.insert(test_header_lines, format("%s%s",
                        (opts.compartment ~= false)
                        and format("__cheri_compartment(%q)", test_compartment)
                        or "",
                    format("int test_%s();", test)))
                end

                if opts.name ~= false then
                    table.insert(test_calls,
                        format("\t\trun_timed(%q, test_%s);",
                            opts.name or test, test))
                end
            end
        end

        target:set("configvar", "test_header",
            table.concat(test_header_lines, "\n"))
        target:set("configvar", "test_calls",
            table.concat(test_calls, "\n"))

        target:add("configfiles", path.join(scriptdir, "tests-prototypes.h.in"),
            {pattern = "@(.-)@", filename = "tests-prototypes.h"})
        target:add("configfiles", path.join(scriptdir, "tests-all.inc.in"),
            {pattern = "@(.-)@", filename = "tests-all.inc"})
    end)

    after_load(function (target)
        -- Generically absorb phony test targets' files into ours.  This makes
        -- the definitions above slightly more uniform.
        for _, d in table.orderpairs(target:deps()) do
            if d:get("cheriot.type") == "test-phony"
               and get_config(d:get("cheriot.test.option"))
            then
                target:add("files", d:get("files"))
            end
        end

        for _, test in ipairs(defined_tests) do
            local opts = test_options[test]
            if get_config("test-" .. test) and opts.conflicts then
              for _, conflict in ipairs(opts.conflicts) do
                assert(not get_config("test-" .. conflict),
                 ("Enabled test %s conflicts with also enabled %s"):format(test, conflict))
              end
            end
        end
    end)

-- Compartment for the test entry point.
compartment("test_runner")
    add_files("test-runner.cc")
    add_files("test-version.cc", { rules = { "cheriot.define-rtos-git-description" } } )

    add_rules("cheriot.tests")

-- Firmware image for the test suite.
firmware("test-suite")
    -- Main entry points
    add_deps("test_runner", "thread_pool")
    -- Helper libraries not implicitly included by the RTOS and needed by the
    -- runner framework itself
    add_deps("debug", "freestanding")
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
