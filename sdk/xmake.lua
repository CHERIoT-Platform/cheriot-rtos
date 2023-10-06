-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

-- xmake has started refusing to pass flags that it doesn't recognise, so tell
-- it to stop doing that for now.
set_policy("check.auto_ignore_flags", false)

add_rules("mode.release", "mode.debug")

-- Disallow any modes other than release and debug.  The only difference is the
-- value of the `NDEBUG` macro: We always enable debug info and optimise for
-- size in both modes, most things should use the --debug-{option}= flags for
-- finer-grained control.
set_allowedmodes("release", "debug")

set_allowedarchs("cheriot")

-- More work arounds for xmake's buggy flag detection.
if is_mode("release") then
    add_defines("NDEBUG", {force = true})
end

option("scheduler-accounting")
	set_default(false)
	set_description("Track per-thread cycle counts in the scheduler");
	set_showmenu(true)

function debugOption(name)
	option("debug-" .. name)
		set_default(false)
		set_description("Enable verbose output and assertions in the " .. name)
		set_showmenu(true)
		set_category("Debugging")
	option_end()
end

debugOption("loader")
debugOption("scheduler")
debugOption("allocator")
debugOption("token_library")

-- Force -Oz irrespective of build config.  At -O0, we blow out our stack and
-- require much stronger alignment.
set_optimize("Oz")

--Capture the directory containing this script for later use.  We need this to
--find the location of the linker scripts and so on.
local scriptdir = os.scriptdir()
-- The directory where we will find the core components
local coredir = path.join(scriptdir, "core")

-- Set up our llvm configuration.
toolchain("cheriot-clang")
	set_kind("standalone")
	set_toolset("cc", "clang")
	set_toolset("cxx", "clang++")
	set_toolset("ld", "ld.lld")
	set_toolset("objdump", "llvm-objdump")
	set_toolset("strip", "llvm-strip")
	set_toolset("as", "clang")

	--Set up the flags that we need.
	on_load(function (toolchain)
		local core_directory = scriptdir
		local include_directory = path.join(core_directory, "include")
		-- Flags used for C/C++ and assembly
		local default_flags = {
			"-target",
			"riscv32-unknown-unknown",
			"-mcpu=cheriot",
			"-mabi=cheriot",
			"-mxcheri-rvc",
			"-mrelax",
			"-fshort-wchar",
			"-nostdinc",
			"-Oz",
			"-g",
			"-fomit-frame-pointer",
			"-fno-builtin",
			"-fno-exceptions",
			"-fno-asynchronous-unwind-tables",
			"-fno-c++-static-destructors",
			"-fno-rtti",
			"-Werror",
			"-I" .. path.join(include_directory, "c++-config"),
			"-I" .. path.join(include_directory, "libc++"),
			"-I" .. include_directory,
		}
		-- C/C++ flags
		toolchain:add("cxflags", default_flags, {force = true})
		toolchain:add("cflags", default_flags)
		toolchain:add("cxxflags", "-std=c++20")
		toolchain:add("cflags", "-std=c2x", {force = true})
		-- Assembly flags
		toolchain:add("asflags", default_flags)
	end)
toolchain_end()


set_defaultarchs("cheriot")
set_defaultplat("cheriot")
set_languages("c2x", "cxx20")

-- Common rules for any CHERI MCU component (library or compartment)
rule("cherimcu.component")

	-- Set some default config balues for all cherimcu components.
	on_load(function (target)
		-- Treat this as a static library, though we will replace the default linking steps.
		target:set("kind", "static")
		-- We don't want a lib prefix or equivalent.
		target:set("prefixname", "")
	end)

	-- Custom link step, link this as a compartment, with the linker script
	-- that will be provided in the specialisation of this rule.
	on_linkcmd(function (target, batchcmds, opt)
		-- Get a specified linker script
		local linkerscript_name = target:get("cherimcu.ldscript")
		local linkerscript = path.join(scriptdir, linkerscript_name)
		-- Link using the compartment's linker script.
		batchcmds:show_progress(opt.progress, "linking " .. target:get("cherimcu.type") .. ' ' .. target:filename())
		batchcmds:mkdir(target:targetdir())
		batchcmds:vrunv(target:tool("ld"), table.join({"--script=" .. linkerscript, "--compartment", "--relax", "-o", target:targetfile()}, target:objectfiles()), opt)
		-- This depends on all of the object files and the linker script.
		batchcmds:add_depfiles(linkerscript)
		batchcmds:add_depfiles(target:objectfiles())
	end)

-- CHERI MCU libraries are currently built as compartments, without a
-- `-cheri-compartment` flag.  They should gain that flag once the compiler
-- supports more than one library.
rule("cherimcu.library")
	add_deps("cherimcu.component")
	on_load(function (target)
		-- Mark this target as a CHERI MCU library.
		target:set("cherimcu.type", "library")
		-- Libraries have a .library extension
		target:set("extension", ".library")
		-- Link with the library linker script, which drops .data* sections.
		target:set("cherimcu.ldscript", "library.ldscript")
	end)

-- CHERI MCU compartments have an explicit compartment name passed to the
-- compiler.
rule("cherimcu.compartment")
	add_deps("cherimcu.component")
	on_load(function (target)
		-- Mark this target as a CHERI MCU compartment.
		target:set("cherimcu.type", "compartment")
		target:set("cherimcu.ldscript", "compartment.ldscript")
		target:set("extension", ".compartment")
	end)
	-- Add the compartment name flag.  This defaults to the target's name but
	-- can be overridden by setting the cherimcu.compartment property.
	after_load(function (target)
		local compartment = target:get("cherimcu.compartment") or target:name()
		target:add("cxflags", "-cheri-compartment=" .. compartment, {force=true})
	end)

-- Privileged compartments are built as compartments, but with a slightly
-- different linker script.
rule("cherimcu.privileged-compartment")
	add_deps("cherimcu.compartment")
	on_load(function (target)
		target:set("cherimcu.ldscript", "privileged-compartment.ldscript")
		target:set("cherimcu.type", "privileged compartment")
		target:add("defines", "CHERIOT_AVOID_CAPRELOCS")
	end)

rule("cherimcu.privileged-library")
	add_deps("cherimcu.library")
	on_load(function (target)
		target:set("cherimcu.type", "privileged library")
		target:set("cherimcu.ldscript", "privileged-compartment.ldscript")
	end)

-- Build the switcher as an object file that we can import into the final
-- linker script.  The switcher is independent of the firmware image
-- configuration and so can be built as a single target.
target("cherimcu.switcher")
	set_kind("object")
	add_files(path.join(coredir, "switcher/entry.S"))

-- Build the allocator as a privileged compartment. The allocator is
-- independent of the firmware image configuration and so can be built as a
-- single target.
-- TODO: We should provide a mechanism for firmware images to either opt out of
-- having an allocator (or into providing a different allocator for a
-- particular application)
target("cherimcu.allocator")
	add_rules("cherimcu.privileged-compartment", "cherimcu.component-debug")
	add_files(path.join(coredir, "allocator/main.cc"))
	on_load(function (target)
		target:set("cherimcu.compartment", "alloc")
		target:set('cherimcu.debug-name', "allocator")
	end)

target("cheriot.token_library")
	add_rules("cherimcu.privileged-library", "cherimcu.component-debug")
	add_files(path.join(coredir, "token_library/token_unseal.S"))
	on_load(function (target)
		target:set('cherimcu.debug-name', "token_library")
	end)

target("cherimcu.software_revoker")
	set_default(false)
	add_files(path.join(coredir, "software_revoker/revoker.cc"))
	add_rules("cherimcu.privileged-compartment")
	on_load(function (target)
		target:set("cherimcu.compartment", "software_revoker")
		target:set("cherimcu.ldscript", "software_revoker.ldscript")
	end)

-- Helper to get the board file for a given target
local board_file = function(target)
	local boardfile = target:values("board")
	if not boardfile then
		raise("target " .. target:name() .. " does not define a board name")
	end
	-- The directory containing the board file.
	local boarddir = path.directory(boardfile);
	if path.basename(boardfile) == boardfile then
		boarddir = path.join(scriptdir, "boards")
		boardfile = path.join(boarddir, boardfile .. '.json')
	end
	return boarddir, boardfile
end

-- Rule for defining a firmware image.
rule("firmware")
	on_run(function (target)
		import("core.base.json")
		import("core.project.config")
		local boarddir, boardfile = board_file(target)
		local board = json.loadfile(boardfile)
		if not board.simulator then
			raise("board description " .. boardfile .. " does not define a run command")
		end
		local simulator = board.simulator
		simulator = string.gsub(simulator, "${(%w*)}", { sdk=scriptdir, board=boarddir })
		local firmware = target:targetfile()
		local directory = path.directory(firmware)
		firmware = path.basename(firmware)
		local run = function(simulator)
			os.execv(simulator, { firmware }, { curdir = directory })
		end
		-- Try executing the simulator from the sdk directory, if it's there.
		local tools_directory = config.get("sdk")
		local simpath = path.join(tools_directory, simulator)
		if os.isexec(simpath) then
			run(simpath)
			return
		end
		simpath = path.join(path.join(tools_directory, "bin"), simulator)
		if os.isexec(simpath) then
			run(simpath)
			return
		end
		-- Otherwise, hope that it's in the path
		run(simulator)
	end)

	-- Set up the thread defines and the information for the linker script.
	-- This must be after load so that dependencies are resolved.
	after_load(function (target)
		import("core.base.json")

		local boarddir, boardfile = board_file(target);
		local board = json.loadfile(boardfile)

		local add_defines = function (defines)
			for _, d in table.orderpairs(target:deps()) do
				d:add('defines', defines)
			end
		end

		local software_revoker = false
		if board.revoker then
			local temporal_defines = { "TEMPORAL_SAFETY" }
			if board.revoker == "software" then
				temporal_defines[#temporal_defines+1] = "SOFTWARE_REVOKER"
				software_revoker = true
				target:add('deps', "cherimcu.software_revoker")
			end
			add_defines(temporal_defines)
		end

		if board.driver_includes then
			for _, include_path in ipairs(board.driver_includes) do
				-- Allow ${sdk} to refer to the SDK directory, so that external
				-- board includes can include generic platform bits.
				include_path = string.gsub(include_path, "${(%w*)}", { sdk=scriptdir })
				if not path.is_absolute(include_path) then
					include_path = path.join(boarddir, include_path);
				end
				for _, d in table.orderpairs(target:deps()) do
					d:add('includedirs', include_path)
				end
			end
		end

		-- If this board defines any macros, add them to all targets
		if board.defines then
			add_defines(board.defines)
		end

		add_defines("CPU_TIMER_HZ=" .. math.floor(board.timer_hz))
		add_defines("TICK_RATE_HZ=" .. math.floor(board.tickrate_hz))

		if board.simulation then
			add_defines("SIMULATION")
		end

		local loader = target:deps()['cheriot.loader'];

		if board.stack_high_water_mark then
			add_defines("CONFIG_MSHWM")
		else
			-- If we don't have the stack high watermark, the trusted stack is smaller.
			loader:set('loader_trusted_stack_size', 168)
		end

		-- Build the MMIO space for the board
		local mmio = ""
		local mmio_start = 0xffffffff
		local mmio_end = 0
		-- Add start and end markers for all MMIO devices.
		for name, range in table.orderpairs(board.devices) do
			local start = range.start
			local stop = range["end"]
			if not stop then
				if not range.length then
					raise("Device " .. name .. " does not specify a length or an end)")
				end
				stop = start + range.length
			end
			add_defines("DEVICE_EXISTS_" .. name)
			mmio_start = math.min(mmio_start, start)
			mmio_end = math.max(mmio_end, stop)
			mmio = format("%s__export_mem_%s = 0x%x;\n__export_mem_%s_end = 0x%x;\n",
				mmio, name, start, name, stop);
		end
		-- Provide the range of the MMIO space and the heap.
		mmio = format("__mmio_region_start = 0x%x;\n%s__mmio_region_end = 0x%x;\n__export_mem_heap_end = 0x%x;\n",
			mmio_start, mmio, mmio_end, board.heap["end"])

		local code_start = format("0x%x", board.instruction_memory.start)

		-- Set the start of memory that can be revoked.
		-- By default, this is the start of code memory but it can be
		-- explicitly overwritten.
		local revokable_memory_start = code_start;
		if board.revokable_memory_start then
			revokable_memory_start = format("0x%x", board.revokable_memory_start);
		end
		add_defines("REVOKABLE_MEMORY_START=" .. revokable_memory_start);

		local heap_start = '.'
		if board.heap.start then
			heap_start = format("0x%x", board.heap.start)
		end
		
		if board.interrupts then
			-- The macro used to provide the interrupt enumeration in the public header
			local interruptNames = "CHERIOT_INTERRUPT_NAMES="
			-- Define the macro that's used to initialise the scheduler's interrupt configuration.
			local interruptConfiguration = "CHERIOT_INTERRUPT_CONFIGURATION="
			for _, interrupt in ipairs(board.interrupts) do
				interruptNames = interruptNames .. interrupt.name .. "=" .. math.floor(interrupt.number) .. ", "
				interruptConfiguration = interruptConfiguration .. "{"
					.. math.floor(interrupt.number) .. ","
					.. math.floor(interrupt.priority) .. ","
					.. (interrupt.edge_triggered and "true" or "false")
					.. "},"
			end
			add_defines(interruptNames)
			target:deps()[target:name() .. ".scheduler"]:add('defines', interruptConfiguration)
		end

		local loader_stack_size = loader:get('loader_stack_size')
		local loader_trusted_stack_size = loader:get('loader_trusted_stack_size')
		loader:add('defines', "CHERIOT_LOADER_TRUSTED_STACK_SIZE=" .. loader_trusted_stack_size)

		-- Get the threads config and prepare the predefined macros that describe them
		local threads = target:values("threads")

		-- Declare space and start and end symbols for a thread's C stack
		local thread_stack_template =
			"\n\t. = ALIGN(16);" ..
			"\n\t.thread_stack_${thread_id} : CAPALIGN" ..
			"\n\t{" ..
			"\n\t\t.thread_${thread_id}_stack_start = .;" ..
			"\n\t\t. += ${stack_size};" ..
			"\n\t\t.thread_${thread_id}_stack_end = .;" ..
			"\n\t}\n"
		-- Declare space and start and end symbols for a thread's trusted stack
		local thread_trusted_stack_template =
			"\n\t. = ALIGN(8);" ..
			"\n\t.thread_trusted_stack_${thread_id} : CAPALIGN" ..
			"\n\t{" ..
			"\n\t\t.thread_${thread_id}_trusted_stack_start = .;" ..
			"\n\t\t. += ${trusted_stack_size};" ..
			"\n\t\t.thread_${thread_id}_trusted_stack_end = .;" ..
			"\n\t}\n"
		-- Build a `class ThreadConfig` for a thread
		local thread_template =
				"\n\t\tSHORT(${priority});" ..
				"\n\t\tLONG(${mangled_entry_point});" ..
				"\n\t\tLONG(.thread_${thread_id}_stack_start);" ..
				"\n\t\tSHORT(.thread_${thread_id}_stack_end - .thread_${thread_id}_stack_start);" ..
				"\n\t\tLONG(.thread_${thread_id}_trusted_stack_start);" ..
				"\n\t\tSHORT(.thread_${thread_id}_trusted_stack_end - .thread_${thread_id}_trusted_stack_start);" ..
				"\n\n"

		--Pass the declared threads as macros when building the loader and the
		--scheduler.
		local thread_headers = ""
		local thread_trusted_stacks =
			"\n\t. = ALIGN(8);" ..
			"\n\t.loader_trusted_stack : CAPALIGN" ..
			"\n\t{" ..
			"\n\t\tbootTStack = .;" ..
			"\n\t\t. += " .. loader_trusted_stack_size .. ";" ..
			"\n\t}\n"
		local thread_stacks =
			"\n\t. = ALIGN(16);" ..
			"\n\t.loader_stack : CAPALIGN" ..
			"\n\t{" ..
			"\n\t\tbootStack = .;" ..
			"\n\t\t. += " .. loader_stack_size .. ";" ..
			"\n\t}\n"
		-- Stacks must be less than this size or truncating them in compartment
		-- switch may encounter precision errors.
		local stack_size_limit = 8176
		for i, thread in ipairs(threads) do
			thread.mangled_entry_point = string.format("__export_%s__Z%d%sv", thread.compartment, string.len(thread.entry_point), thread.entry_point)
			thread.thread_id = i
			-- Trusted stack frame is 24 bytes.  If this size is too small, the
			-- loader will fail.  If it is too big, we waste space.
			thread.trusted_stack_size = loader_trusted_stack_size + (24 * thread.trusted_stack_frames)

			if thread.stack_size > stack_size_limit then
				raise("thread " .. i .. " requested a " .. thread.stack_size ..
				" stack.  Stacks over " .. stack_size_limit ..
				" are not yet supported in the compartment switcher.")
			end

			thread_stacks = thread_stacks .. string.gsub(thread_stack_template, "${([_%w]*)}", thread)
			thread_trusted_stacks = thread_trusted_stacks .. string.gsub(thread_trusted_stack_template, "${([_%w]*)}", thread)
			thread_headers = thread_headers .. string.gsub(thread_template, "${([_%w]*)}", thread)

		end
		local add_defines = function(compartment, option_name)
			target:deps()[compartment]:add('defines', "CONFIG_THREADS_NUM=" .. #(threads))
		end
		add_defines(target:name() .. ".scheduler", "scheduler")

		-- Next set up the substitutions for the linker scripts.

		-- Templates for parts of the linker script that are instantiated per compartment
		local compartment_templates = {
			compartment_headers =
				"\n\t\tLONG(.${compartment}_code_start);" ..
				"\n\t\tSHORT((SIZEOF(.${compartment}_code) + 7) / 8);" ..
				"\n\t\tSHORT(.${compartment}_imports_end - .${compartment}_code_start);" ..
				"\n\t\tLONG(.${compartment}_export_table);" ..
				"\n\t\tSHORT(.${compartment}_export_table_end - .${compartment}_export_table);" ..
				"\n\t\tLONG(.${compartment}_globals);" ..
				"\n\t\tSHORT(SIZEOF(.${compartment}_globals));" ..
				"\n\t\tSHORT(.${compartment}_bss_start - .${compartment}_globals);" ..
				"\n\t\tLONG(.${compartment}_cap_relocs_start);" ..
				"\n\t\tSHORT(.${compartment}_cap_relocs_end - .${compartment}_cap_relocs_start);" ..
				"\n\t\tLONG(.${compartment}_sealed_objects_start);" ..
				"\n\t\tSHORT(.${compartment}_sealed_objects_end - .${compartment}_sealed_objects_start);\n",
			pcc_ld =
				"\n\t.${compartment}_code : CAPALIGN" ..
				"\n\t{" ..
				"\n\t\t.${compartment}_code_start = .;" ..
				"\n\t\t${obj}(.compartment_import_table);" ..
				"\n\t\t.${compartment}_imports_end = .;" ..
				"\n\t\t${obj}(.text);" ..
				"\n\t\t${obj}(.init_array);" ..
				"\n\t\t${obj}(.rodata);" ..
				"\n\t\t. = ALIGN(8);" ..
				"\n\t}\n",
			gdc_ld =
				"\n\t.${compartment}_globals : CAPALIGN" ..
				"\n\t{" ..
				"\n\t\t.${compartment}_globals = .;" ..
				"\n\t\t${obj}(.data);" ..
				"\n\t\t.${compartment}_bss_start = .;" ..
				"\n\t\t${obj}(.bss)" ..
				"\n\t}\n",
			compartment_exports =
				"\n\t\t.${compartment}_export_table = ALIGN(8);" ..
				"\n\t\t${obj}(.compartment_export_table);" ..
				"\n\t\t.${compartment}_export_table_end = .;\n",
			cap_relocs =
				"\n\t\t.${compartment}_cap_relocs_start = .;" ..
				"\n\t\t${obj}(__cap_relocs);\n\t\t.${compartment}_cap_relocs_end = .;",
			sealed_objects =
				"\n\t\t.${compartment}_sealed_objects_start = .;" ..
				"\n\t\t${obj}(.sealed_objects);\n\t\t.${compartment}_sealed_objects_end = .;"
		}
		--Library headers are almost identical to compartment headers, except
		--that they don't have any globals.
		local library_templates = {
			compartment_headers =
				"\n\t\tLONG(.${compartment}_code_start);" ..
				"\n\t\tSHORT((SIZEOF(.${compartment}_code) + 7) / 8);" ..
				"\n\t\tSHORT(.${compartment}_imports_end - .${compartment}_code_start);" ..
				"\n\t\tLONG(.${compartment}_export_table);" ..
				"\n\t\tSHORT(.${compartment}_export_table_end - .${compartment}_export_table);" ..
				"\n\t\tLONG(0);" ..
				"\n\t\tSHORT(0);" ..
				"\n\t\tSHORT(0);" ..
				"\n\t\tLONG(.${compartment}_cap_relocs_start);" ..
				"\n\t\tSHORT(.${compartment}_cap_relocs_end - .${compartment}_cap_relocs_start);" ..
				"\n\t\tLONG(.${compartment}_sealed_objects_start);" ..
				"\n\t\tSHORT(.${compartment}_sealed_objects_end - .${compartment}_sealed_objects_start);\n",
			pcc_ld = compartment_templates.pcc_ld,
			gdc_ld = "",
			library_exports = compartment_templates.compartment_exports,
			cap_relocs = compartment_templates.cap_relocs,
			sealed_objects = compartment_templates.sealed_objects
		}
		-- The substitutions that we're going to have in the final linker
		-- script.  Initialised as empty strings.
		local ldscript_substitutions = {
			compartment_exports="",
			library_exports="",
			cap_relocs="",
			compartment_headers="",
			pcc_ld="",
			gdc_ld="",
			software_revoker_code="",
			software_revoker_globals="",
			software_revoker_header="",
			sealed_objects="",
			mmio=mmio,
			code_start=code_start,
			heap_start=heap_start,
			thread_count=#(threads),
			thread_headers=thread_headers,
			thread_trusted_stacks=thread_trusted_stacks,
			thread_stacks=thread_stacks,
			loader_stack_size=loader:get('loader_stack_size'),
			loader_trusted_stack_size=loader:get('loader_trusted_stack_size')
		}
		-- Helper function to add a dependency to the linker script
		local add_dependency = function (name, dep, templates)
			local obj = path.relative(dep:targetfile(), "$(projdir)")
			local obj = dep:targetfile()
			-- Helper to substitute the current compartment name and object file into a template.
			local substitute = function (str)
				return string.gsub(str, "${(%w*)}", { obj=obj, compartment=name })
			end
			for key, template in table.orderpairs(templates) do
				ldscript_substitutions[key] = ldscript_substitutions[key] .. substitute(template)
			end
		end

		-- If this board requires the software revoker, add it as a dependency
		-- and add the relevant bits to the linker script.
		if software_revoker then
			ldscript_substitutions.software_revoker_code =
				"\tsoftware_revoker_code : CAPALIGN\n" ..
				"\t{\n" ..
				"\t\t.software_revoker_start = .;\n" ..
				"\t\t.software_revoker_import_end = .;\n" ..
				"\t\tsoftware_revoker.compartment(.text .text.* .rodata .rodata.* .data.rel.ro);\n" ..
				"\t\t*/cherimcu.software_revoker.compartment(.text .text.* .rodata .rodata.* .data.rel.ro);\n" ..
				"\t}\n" ..
				"\t.software_revoker_end = .;\n\n"
			ldscript_substitutions.software_revoker_globals =
				"\n\t.software_revoker_globals : CAPALIGN" ..
				"\n\t{" ..
				"\n\t\t.software_revoker_globals = .;" ..
				"\n\t\t*/cherimcu.software_revoker.compartment(.data .data.* .sdata .sdata.*);" ..
				"\n\t\t.software_revoker_bss_start = .;" ..
				"\n\t\t*/cherimcu.software_revoker.compartment(.sbss .sbss.* .bss .bss.*)" ..
				"\n\t}" ..
				"\n\t.software_revoker_globals_end = .;\n"
			ldscript_substitutions.compartment_exports =
				"\n\t\t.software_revoker_export_table = ALIGN(8);" ..
				"\n\t\t*/cherimcu.software_revoker.compartment(.compartment_export_table);" ..
				"\n\t\t.software_revoker_export_table_end = .;\n" ..
				ldscript_substitutions.compartment_exports
			ldscript_substitutions.software_revoker_header =
				"\n\t\tLONG(.software_revoker_start);" ..
				"\n\t\tSHORT(.software_revoker_end - .software_revoker_start);" ..
				"\n\t\tLONG(.software_revoker_globals);" ..
				"\n\t\tSHORT(SIZEOF(.software_revoker_globals));" ..
				-- The import table offset is computed from the start by code
				-- that assumes that the first two words are space for sealing
				-- keys, so we set it to 16 here to provide a computed size of
				-- 0.
				"\n\t\tSHORT(16)" ..
				"\n\t\tLONG(.software_revoker_export_table);" ..
				"\n\t\tSHORT(.software_revoker_export_table_end - .software_revoker_export_table);\n" ..
				"\n\t\tLONG(0);" ..
				"\n\t\tSHORT(0);\n"
		end


		-- Process all of the library dependencies.
		local library_count = 0
		for name, dep in table.orderpairs(target:deps()) do
			if dep:get("cherimcu.type") == "library" then
				library_count = library_count + 1
				add_dependency(name, dep, library_templates)
			end
		end

		-- Process all of the compartment dependencies.
		local compartment_count = 0
		for name, dep in table.orderpairs(target:deps()) do
			if dep:get("cherimcu.type") == "compartment" then
				compartment_count = compartment_count + 1
				add_dependency(name, dep, compartment_templates)
			end
		end

		-- Add the counts of libraries and compartments to the substitution list.
		ldscript_substitutions.compartment_count = compartment_count
		ldscript_substitutions.library_count = library_count

		-- Set the each of the substitutions.
		for key, value in pairs(ldscript_substitutions) do
			target:set("configvar", key, value)
		end
	end)

	-- Perform the final link step for a firmware image.
	on_linkcmd(function (target, batchcmds, opt)
		import("core.project.config")
		-- Get a specified linker script, or set the default to the compartment
		-- linker script.
		local linkerscript = path.join(config.buildir(), target:name() .. "-firmware.ldscript")
		-- Link using the firmware's linker script.
		batchcmds:show_progress(opt.progress, "linking firmware " .. target:targetfile())
		batchcmds:mkdir(target:targetdir())
		local objects = target:objectfiles()
		for name, dep in table.orderpairs(target:deps()) do
			if (dep:get("cherimcu.type") == "library") or
				(dep:get("cherimcu.type") == "compartment") or
				(dep:get("cherimcu.type") == "privileged compartment") or
				(dep:get("cherimcu.type") == "privileged library") then
				table.insert(objects, dep:targetfile())
			end
		end
		batchcmds:vrunv(target:tool("ld"), table.join({"--script=" .. linkerscript, "--relax", "-o", target:targetfile(), "--compartment-report=" .. target:targetfile() .. ".json" }, objects), opt)
		batchcmds:show_progress(opt.progress, "Creating firmware report " .. target:targetfile() .. ".json")
		batchcmds:show_progress(opt.progress, "Creating firmware dump " .. target:targetfile() .. ".dump")
		batchcmds:vexecv(target:tool("objdump"), {"-glxsdrS", "--demangle", target:targetfile()}, table.join(opt, {stdout = target:targetfile() .. ".dump"}))
		batchcmds:add_depfiles(linkerscript)
		batchcmds:add_depfiles(objects)
	end)

-- Rule for conditionally enabling debug for a component.
rule("cherimcu.component-debug")
	after_load(function (target)
		local name = target:get("cherimcu.debug-name") or target:name()
		target:add('options', "debug-" .. name)
		target:add('defines', "DEBUG_" .. name:upper() .. "=" .. tostring(get_config("debug-"..name)))
	end)


-- Build the loader.  The firmware rule will set the flags required for
-- this to create threads.
target("cheriot.loader")
	add_rules("cherimcu.component-debug")
	set_kind("object")
	-- FIXME: We should be setting this based on a board config file.
	add_files(path.join(coredir, "loader/boot.S"), path.join(coredir, "loader/boot.cc"),  {force = {cxflags = "-O1"}})
	add_defines("CHERIOT_AVOID_CAPRELOCS")
	on_load(function (target)
		target:set('cherimcu.debug-name', "loader")
		local config = {
			-- Size in bytes of the trusted stack.
			loader_trusted_stack_size = 184,
			-- Size in bytes of the loader's stack.
			loader_stack_size = 1024
		}
		target:add('defines', "CHERIOT_LOADER_STACK_SIZE=" .. config.loader_stack_size)
		target:set('cheriot_loader_config', config)
		for k, v in pairs(config) do
			target:set(k, v)
		end
	end)

-- Helper function to define firmware.  Used as `target`.
function firmware(name)
	-- Build the scheduler.  The firmware rule will set the flags required for
	-- this to create threads.
	target(name .. ".scheduler")
		add_rules("cherimcu.privileged-compartment", "cherimcu.component-debug")
		on_load(function (target)
			target:set("cherimcu.compartment", "sched")
			target:set('cherimcu.debug-name', "scheduler")
			target:add('defines', "SCHEDULER_ACCOUNTING=" .. tostring(get_config("scheduler-accounting")))
		end)
		add_files(path.join(coredir, "scheduler/main.cc"))

	-- Create the firmware target.  This target remains open on return and so
	-- the caller can add more rules to it.
	target(name)
		set_kind("binary")
		add_rules("firmware")
		-- TODO: Make linking the allocator optional.
		add_deps(name .. ".scheduler", "cheriot.loader", "cherimcu.switcher", "cherimcu.allocator")
		add_deps("cheriot.token_library")
		-- The firmware linker script will be populated based on the set of
		-- compartments.
		add_configfiles(path.join(scriptdir, "firmware.ldscript.in"), {pattern = "@(.-)@", filename = name .. "-firmware.ldscript"})
end

-- Helper to create a library.
function library(name)
	target(name)
		add_rules("cherimcu.library")
end

-- Helper to create a compartment.
function compartment(name)
	target(name)
		add_rules("cherimcu.compartment")
end
