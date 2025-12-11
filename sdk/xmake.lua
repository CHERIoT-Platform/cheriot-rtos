-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

--Capture the directory containing this script for later use.  We need this to
--find the location of the linker scripts and so on.
local scriptdir = os.scriptdir()
-- The directory where we will find the core components
local coredir = path.join(scriptdir, "core")

-- xmake has started refusing to pass flags that it doesn't recognise, so tell
-- it to stop doing that for now.
set_policy("check.auto_ignore_flags", false)

add_rules("mode.release", "mode.debug")

-- Disallow any modes other than release and debug.  The only difference is the
-- value of the `NDEBUG` macro: We always enable debug info and optimise for
-- size in both modes, most things should use the --debug-{option}= flags for
-- finer-grained control.
set_allowedmodes("release", "debug")

-- More work arounds for xmake's buggy flag detection.
if is_mode("release") then
    add_defines("NDEBUG", {force = true})
end

local function option_disable_unless_dep(option, dep)
	if not option:dep(dep):value() then option:enable(false) end
end

local function option_check_dep(raise, option, dep)
	if option:value() and not option:dep(dep):value() then
		return raise("Option " .. option:name() .. " depends on " .. dep)
	end
end

option("board", function ()
	set_description("Board JSON description file")
	set_showmenu(true)
	set_category("board")
	add_deps("board-mixins")

	after_check(function (self)
		if type(self:value()) ~= "string" then
			raise("Bad value for required --board option")
		end

		local project_config = import("core.project.config", { anonymous = true })
		local board_parsing = import("board-parsing",
			{ anonymous = true
			, rootdir = path.join(os.scriptdir(), "xmake")
			})

		-- Several paths in board JSON can contain ${variable} expansions:
		-- * ${sdk} to refer to the SDK directory,
		-- * ${sdkboards} to refer to the SDK's boards/ directory
		-- * ${project} for the root source tree.
		--
		-- The board handling goo will also add ${board} for the --board file's
		-- own directory, once that has been resolved.
		local board_path_substitutes = {
			sdk = scriptdir,
			sdkboards = path.join(scriptdir, "boards"),
			project = os.projectdir(),
		}

		local board_conf = board_parsing(
			board_path_substitutes,
			self:value(),
			self:dep("board-mixins"):value())

		project_config.set("cheriot.board", board_conf)
	end)
end)

option("board-mixins")
	set_default("")
	set_description("Comma separated list of board mixin patch files");
	set_showmenu(true)
	set_category("board")

option("allocator")
	set_description("Build with the shared heap allocator compartment")
	set_default(true)
	set_showmenu(true)

option("allocator-rendering")
	set_default(false)
	set_description("Include heap_render() functionality in the allocator")
	set_showmenu(true)

	add_deps("allocator")
	after_check(function (option)
		option_check_dep(raise, option, "allocator")
	end)

option("scheduler-accounting")
	set_default(false)
	set_description("Track per-thread cycle counts in the scheduler");
	set_showmenu(true)

option("scheduler-multiwaiter")
	set_default(true)
	set_description("Enable multiwaiter support in the scheduler.  Disabling this can reduce code size if multiwaiters are not used.");
	set_showmenu(true)

	add_deps("allocator")
	before_check(function (option)
		option_disable_unless_dep(option, "allocator")
	end)
	after_check(function (option)
		option_check_dep(raise, option, "allocator")
	end)

function debugOption(name)
	option("debug-" .. name)
		set_default(false)
		set_description("Enable verbose output and assertions in the " .. name)
		set_showmenu(true)
		set_category("Debugging")
	option_end()
end

function debugLevelOption(name)
	option("debug-" .. name)
		set_default("none")
		set_description("Specify verbose output level (none|information|warning|error|critical) in the " .. name)
		set_showmenu(true)
		set_category("Debugging")
		set_values("none", "information", "warning", "error", "critical")
		before_check(function (option)
			-- For some reason, xmake calls this with a nil option sometimes.
			-- Just pretend it makes sense.
			if option == nil then
				return
			end
			-- Map possible options to the define values that we want:
			local values = {
				none = "None",
				information = "Information",
				warning = "Warning",
				error = "Error",
				critical = "Critical"
			}
			local value = values[tostring(option:value())]
			-- Even though we've specified the allowed values, xmake doesn't
			-- enforce this, so do it ourselves.
			if not value then
				raise("Invalid value " .. tostring(option:value()) .. " for option "..option:name())
			end
		end)
	option_end()
end

debugOption("loader")
debugOption("scheduler")
debugLevelOption("allocator")
debugOption("token_library")

function stackCheckOption(name)
	option("stack-usage-check-" .. name)
		set_default(false)
		set_description("Enable dynamic stack usage checks in " .. name .. ". Do not enable this in debug builds!")
		set_showmenu(true)
		set_category("Debugging")
	option_end()
end

stackCheckOption("allocator")
stackCheckOption("scheduler")

function testCheckOption(name)
	option("testing-" .. name)
		set_default(false)
		set_description("Enable testing feature " .. name .. ". Do not enable this in builds that don't produce a UART log!")
		set_showmenu(true)
		set_category("Debugging")
	option_end()
end

testCheckOption("model-output")

-- Set up our llvm configuration.
toolchain("cheriot-clang", function ()
	set_kind("standalone")
	set_toolset("cc", "clang")
	set_toolset("cxx", "clang++")
	set_toolset("ld", "ld.lld")
	set_toolset("objdump", "llvm-objdump")
	set_toolset("strip", "llvm-strip")
	set_toolset("as", "clang")

	--Set up the flags that we need.
	on_load(function (self)
		local board = get_config("cheriot.board").info
		local include_directory = path.join(scriptdir, "include")

		local target = self:config("target")
			or "riscv32cheriot-unknown-cheriotrtos"
		local cpu = board["cpu"] or "cheriot"
		local abi = self:config("abi") or "cheriot"

		-- Flags used for C/C++ and assembly
		local default_clang_flags = {
			"-target", target,
			"-mcpu=" .. cpu,
			"-mabi=" .. abi,
			"-mxcheri-rvc",
			"-mrelax",
			"-fshort-wchar",
			"-g",
			"-ffunction-sections",
			"-fdata-sections",
			"-fomit-frame-pointer",
			"-fno-builtin-setjmp",
			"-fno-builtin-longjmp",
			"-fno-builtin-printf",
			"-fno-exceptions",
			"-fno-asynchronous-unwind-tables",
			"-fno-c++-static-destructors",
			"-fno-rtti",
			"-nostdinc",
			"-I" .. path.join(include_directory, "c++-config"),
			"-I" .. path.join(include_directory, "libc++"),
			"-I" .. include_directory,
		}
		-- C/C++ flags
		self:add("cxflags", default_clang_flags)
		-- Assembly flags
		self:add("asflags", default_clang_flags)

		-- Rust flags
		local default_rc_flags = {
			"--target=" .. target,
			"-Ctarget-cpu=" .. cpu,
		}
		self:add("rcflags", default_rc_flags)
	end)
end)

-- Pass configuration to the cheriot-clang toolchain to use the baremetal ABI
-- and target triple instead of its defaults.
rule("cheriot.baremetal-abi")
	on_load(function (self)
		self:set("toolchains", "cheriot-clang",
			{ target = "riscv32cheriot-unknown-unknown"
			, abi = "cheriot-baremetal"
			})
	end)
rule_end()

rule("cheriot.subobject-bounds")
	on_config(function (target)
		import("lib.detect.check_cxsnippets")

		local versionCheckString = "_Static_assert(__CHERIOT__ >= 20250812);"
		local ok = target:check_cxxsnippets(versionCheckString)
		if ok then
			print("Enabling sub-object bounds for ".. target:name())
			target:add("cxflags",
				"-Xclang -cheri-bounds=subobject-safe",
				{ expand = false, force = true })
		end
	end)
rule_end()

-- Helper to visit all dependencies of a specified target exactly once and call
-- a callback.
local function visit_all_dependencies_of(target, callback)
	local visited = {}
	local function visit(target)
		if not visited[target:name()] then
			visited[target:name()] = true
			callback(target)
			for _, d in table.orderpairs(target:deps()) do
				visit(d)
			end
		end
	end
	visit(target)
end

-- Write contents to path if it would create or update the contents
local function maybe_writefile(xmake_io, xmake_try, path, contents)
	xmake_try
	{ function()
		-- Try reading the file and comparing
		local old_contents = xmake_io.readfile(path)
		if old_contents == contents then return end
		xmake_io.writefile(path, contents)
	  end
	, { catch = function()
		-- If that threw an exception, just write the file
		xmake_io.writefile(path, contents)
	  end
	} }
end

-- Common rules for most CHERIoT builds
rule("cheriot.toolchain", function ()
	on_load(function (target)
		-- Use our toolchain and build for the CHERIoT architecture and platform
		target:set("toolchains", "cheriot-clang")
		target:set("arch", "cheriot")
		target:set("plat", "cheriot")

		-- Default to -Oz irrespective of build config.  At -O0, we blow out our
		-- stack and require much stronger alignment.
		target:set("optimize", "smallest")

		-- Default to modern versions of supported languages
		target:set("languages", "c23", "cxx23")
	end)
end)

-- Common rules for any CHERIoT component (library or compartment)
rule("cheriot.component")
	add_deps("cheriot.toolchain")

	add_deps("cheriot.rust", "cheriot.rust.crate")

	-- Set some default config values for all cheriot components.
	on_load(function (target)
		-- Treat this as a static library, though we will replace the default linking steps.
		target:set("kind", "static")
		-- We don't want a lib prefix or equivalent.
		target:set("prefixname", "")
	end)
	before_build(function (target)
		if not target:get("cheriot.reachable") then
			raise("target " .. target:name() .. " is being built but does not " ..
			"appear to be connected to a firmware image.  Please either use " ..
			"add_deps(\"" .. target:name() .. "\" to add it or use set_default(false) " ..
			"prevent it from being built when not linked")
		end
	end)

	-- Custom link step, link this as a compartment, with the linker script
	-- that will be provided in the specialisation of this rule.
	on_linkcmd(function (target, batchcmds, opt)
		-- Get a specified linker script
		local linkerscript_name = target:get("cheriot.ldscript")
		local linkerscript = path.join(scriptdir, linkerscript_name)
		-- Link using the compartment's linker script.
		batchcmds:show_progress(opt.progress, "linking " .. target:get("cheriot.type") .. ' ' .. target:filename())
		batchcmds:mkdir(target:targetdir())
		batchcmds:vrunv(target:tool("ld"), table.join({"--script=" .. linkerscript, "--compartment", "--gc-sections", "--relax", "-o", target:targetfile()}, target:objectfiles()), opt)
		-- This depends on all of the object files and the linker script.
		batchcmds:add_depfiles(linkerscript)
		batchcmds:add_depfiles(target:objectfiles())
	end)

-- Rule for marking all reflexive, transitive dependencies of a target as
-- reachable.  See the check for "cheriot.reachable" in the "cheriot.component"
-- rule's before_build hook, above.
rule("cheriot.reachability_root")
	-- Run in on_config, specifically after after_load, so that other rules
	-- on this target get a chance to add to the dependency graph.
	on_config(function (target)
		visit_all_dependencies_of(target, function (target)
			target:set("cheriot.reachable", true)
		end)
	end)

-- CHERI MCU libraries are currently built as compartments, without a
-- `-cheri-compartment` flag.  They should gain that flag once the compiler
-- supports more than one library.
rule("cheriot.library")
	add_deps("cheriot.component")
	on_load(function (target)
		-- Mark this target as a CHERI MCU library.
		target:set("cheriot.type", "library")
		-- Libraries have a .library extension
		target:set("extension", ".library")
		-- Link with the library linker script, which drops .data* sections.
		target:set("cheriot.ldscript", "library.ldscript")

		target:add("defines", "CHERIOT_NO_AMBIENT_MALLOC")
	end)

-- CHERI MCU compartments have an explicit compartment name passed to the
-- compiler.
rule("cheriot.compartment")
	add_deps("cheriot.component")
	on_load(function (target)
		-- Mark this target as a CHERI MCU compartment.
		target:set("cheriot.type", "compartment")
		target:set("cheriot.ldscript", "compartment.ldscript")
		target:set("extension", ".compartment")
	end)
	-- Add the compartment name flag.  This defaults to the target's name but
	-- can be overridden by setting the cheriot.compartment property.
	after_load(function (target)
		local compartment = target:get("cheriot.compartment") or target:name()
		target:add("cxflags", "-cheri-compartment=" .. compartment, {force=true})
	end)

-- Privileged compartments are built as compartments, but with a slightly
-- different linker script.
rule("cheriot.privileged-compartment")
	add_deps("cheriot.compartment")
	on_load(function (target)
		target:set("cheriot.ldscript", "privileged-compartment.ldscript")
		target:set("cheriot.type", "privileged compartment")
		target:add("defines", "CHERIOT_AVOID_CAPRELOCS")
	end)

rule("cheriot.privileged-library")
	add_deps("cheriot.library")
	on_load(function (target)
		target:set("cheriot.type", "privileged library")
		target:set("cheriot.ldscript", "privileged-compartment.ldscript")
		target:add("defines", "CHERIOT_NO_AMBIENT_MALLOC")
	end)

rule("cheriot.generated-source")
	on_load(function(target)
		target:set("cheriot.type", "generated source")

		-- Generated source targets get used during compilation, and not just
		-- linking, phases of dependent targets, so tell xmake about that.
		--
		-- XXX: If/when we can mandate xmake v3 or later, apparently the
		-- preferred mechanism is that such generated code targets use the newer
		-- {before,on,after}_prepare phase rather than ..._build.
		target:set("policy", "build.fence", true)
	end)

-- Build the switcher as an object file that we can import into the final
-- linker script.  The switcher is independent of the firmware image
-- configuration and so can be built as a single target.
target("cheriot.switcher")
	add_rules("cheriot.toolchain", "cheriot.baremetal-abi")
	set_default(false)
	set_kind("object")
	add_files(path.join(coredir, "switcher/entry.S"))

-- Build the allocator as a privileged compartment. The allocator is
-- independent of the firmware image configuration and so can be built as a
-- single target.
-- TODO: We should provide a mechanism for firmware images to either opt out of
-- having an allocator (or into providing a different allocator for a
-- particular application)
target("cheriot.allocator")
	set_default(false)
	add_rules("cheriot.privileged-compartment",
		"cheriot.component-debug",
		"cheriot.component-stack-checks",
		"cheriot.subobject-bounds",
		"cheriot.board.define.revokable_memory")
	add_files(path.join(coredir, "allocator/main.cc"))
	add_deps("locks")
	add_deps("compartment_helpers")
	add_deps("cheriot.board")
	set_default(false)
	on_load(function (target)
		target:set("cheriot.compartment", "allocator")
		target:set('cheriot.debug-name', "allocator")
		target:add('defines', "HEAP_RENDER=" .. tostring(get_config("allocator-rendering")))
	end)
	after_load(function (target)
		local board = target:dep("cheriot.board"):get("cheriot.board_info")
		if board.revoker and board.revoker ~= "software" then
			target:add("deps", "cheriot.board.interrupts")
		end
		target:add("defines", board.rtos_defines and board.rtos_defines.allocator)
	end)

-- Add the allocator to the firmware image if enabled.
--
-- Do this as a rule and not directly in on_load() because the actual firmware
-- build scripts (that is, the things that indcludes() us) use the firwmare
-- target()'s on_load() hook themselves.
rule("cheriot.conditionally_link_allocator")
	on_load(function (target)
		if get_config("allocator") then
			target:add("deps", "cheriot.allocator")
		end
	end)

target("cheriot.token_library")
	set_default(false)
	add_rules("cheriot.privileged-library", "cheriot.component-debug")
	add_files(path.join(coredir, "token_library/token_unseal.S"))
	on_load(function (target)
		target:set('cheriot.debug-name', "token_library")
	end)

target("cheriot.software_revoker")
	set_default(false)
	add_files(path.join(coredir, "software_revoker/revoker.cc"))
	add_rules("cheriot.privileged-compartment")
	on_load(function (target)
		target:set("cheriot.compartment", "software_revoker")
		target:set("cheriot.ldscript", "software_revoker.ldscript")
		target:add("defines", "CHERIOT_NO_AMBIENT_MALLOC")
	end)

-- XXX This can probably go away now that we're ing option()/`config`.
target("cheriot.board")
	set_kind("phony")
	set_default(false)
	add_options("cheriot.board")

	on_load(function (self)
		local board_conf = get_config("cheriot.board") or {}
		self:set("cheriot.board_dir", board_conf.dir)
		self:set("cheriot.board_file", board_conf.file)
		self:set("cheriot.board_info", { board_conf.info })
		self:set("cheriot.trusted_spill_size",
			board_conf.info.trusted_spill_size)
	end)

target("cheriot.board.file")
	set_kind("binary")
	set_default(false)
	set_targetdir("$(buildir)")
	set_filename("board.json")

	add_rules("cheriot.generated-source")

	add_deps("cheriot.board")

	on_build(function(target)
		import("core.base.json")

		print(format("Patched board file will be saved as %q",
			path.absolute(target:targetfile())))

		maybe_writefile(io, try, target:targetfile(),
			json.encode(target:dep("cheriot.board"):get("cheriot.board_info")))
	end)

	on_link(function (target) end)

target("cheriot.board.ldscript.mmio")
	set_kind("binary")
	set_default(false)
	add_deps("cheriot.board")
	set_targetdir("$(buildir)")
	set_filename("mmio.ldscript")

	add_rules("cheriot.generated-source")

	on_build(function(target)
		local board = target:dep("cheriot.board"):get("cheriot.board_info")

		-- Build the MMIO space for the board
		local mmios = {}
		local mmio_start = 0xffffffff
		local mmio_end = 0
		-- Add start and end markers for all MMIO devices.
		for name, range in table.orderpairs(board.devices) do
			local start = range.start
			local stop = range["end"]
			mmio_start = math.min(mmio_start, start)
			mmio_end = math.max(mmio_end, stop)
			table.insert(mmios, format("__export_mem_%s = 0x%x", name, start))
			table.insert(mmios, format("__export_mem_%s_end = 0x%x", name, stop))
		end

		-- For the built-in, named memory segments, provide starts and ends
		-- See the ldscript generation for others
		local spans = {}
		do
			local imem = board.instruction_memory
			spans["instruction_memory"] = { imem.start, imem["end"] }
		end
		if board.data_memory then
			local dmem = board.data_memory
			spans["data_memory"] = { dmem.start, dmem["end"] }
		end

		-- For named ldscript fragments with starts and ends, do the same.
		for name, fragment in table.orderpairs(board.ldscript_fragments) do
			if type(name) == "string" and fragment.start and fragment["end"] then
				spans[name] = { fragment.start, fragment["end"] }
			end
		end

		local span_text = {}
		for name, range in pairs(spans) do
			table.insert(span_text,
				format("__memspan__%s = 0x%x;\n__memspan__%s_end = 0x%x;",
					name, range[1], name, range[2]))
		end

		-- Provide the range of the MMIO space and the heap.
		maybe_writefile(io, try, target:targetfile(),
			table.concat({
				format("__mmio_region_start = 0x%x", mmio_start),
				table.concat(mmios, ";\n"),
				format("__mmio_region_end = 0x%x", mmio_end),
				format("__export_mem_heap_end = 0x%x", board.heap["end"]),
				table.concat(span_text, "\n")
			}, ";\n"))
	end)

	on_link(function (target) end)

-- Add a target's configuration directory to its 'cxflags' in a way that xmake
-- propagates to things that depend on this target.
rule("cheriot.cxflags.interface.iquote.targetdir")
	after_load(function (target)
		target:add('cxflags', format("-iquote%s", target:targetdir()),
		  {force = true, interface = true})
	end)

target("cheriot.board.interrupts")
	set_kind("binary")
	set_default(false)
	add_rules("cheriot.cxflags.interface.iquote.targetdir")
	add_deps("cheriot.board")
	set_targetdir("$(buildir)")
	set_filename("board-interrupts.h")

	add_rules("cheriot.generated-source")

	on_build(function (target)
		local board = target:dep("cheriot.board"):get("cheriot.board_info")

		local interrupt_names_numbers = {}
		if board.interrupts then
			for _, interrupt in ipairs(board.interrupts) do
				table.insert(interrupt_names_numbers, interrupt.name .. "=" .. math.floor(interrupt.number))
			end
		else
			-- don't generate an emtpy enum
			table.insert(interrupt_names_numbers, "DummyInterrupt=0")
		end

		local template = io.readfile(path.join(scriptdir, "board-interrupts.h.in"))
		maybe_writefile(io, try, target:targetfile(),
			template:gsub("@board_interrupt_enum_body@",
				table.concat(interrupt_names_numbers, ",\n")))
	end)

	on_link(function (target) end)

rule("cheriot.board.define.revokable_memory")
	on_load(function (target)
		target:add("deps", "cheriot.board")
	end)

	after_load(function (target)
		local board = target:deps()["cheriot.board"]:get("cheriot.board_info")

		-- Set the start of memory that can be revoked.
		-- By default, this is the start of code memory but it can be
		-- explicitly overwritten.
		local revokable_memory_start = board.instruction_memory.start
		if board.data_memory then
			revokable_memory_start = board.data_memory.start
		end
		if board.revokable_memory_start then
			revokable_memory_start = board.revokable_memory_start
		end
		target:add("defines",
			"REVOKABLE_MEMORY_START=" .. format("0x%x", revokable_memory_start))
	end)

rule("cheriot.board.ldscript.conf")
	on_load(function (target)
		target:add("deps", "cheriot.board")
	end)

	after_load(function (target)
		local board_config = get_config("cheriot.board")
		local board = board_config.info

		local function path_subst(p)
			return p:gsub("${(%w*)}", board_config.path_substitutes)
		end

		local ldscript_fragments = {}
		do
			local fragment = target:get("cheriot.ldfragment.rocode")
			assert(fragment, "Target must specify cheriot.ldfragment.rocode")
			assert(fragment.start == nil)
			assert(fragment.start_string == nil)
			fragment.start = board.instruction_memory.start
			table.insert(ldscript_fragments, fragment)
		end
		do
			local fragment = target:get("cheriot.ldfragment.rwdata")
			if fragment then
				assert(fragment.start == nil)
				assert(fragment.start_string == nil)
				if board.data_memory then
					fragment.start = board.data_memory.start
				else
					fragment.start = ldscript_fragments[#ldscript_fragments].start + 1
					fragment.start_string = "."
				end
				table.insert(ldscript_fragments, fragment)
			end
		end

		-- Use a pairs() iterator here so that the board file can choose to
		-- use arrays or maps.
		for _, fragment in pairs(board.ldscript_fragments or {}) do
			if fragment.srcpath then
				fragment.srcpath = path_subst(fragment.srcpath)
				if not path.is_absolute(fragment.srcpath) then
					fragment.srcpath = path.join(board_config.dir, fragment.srcpath);
				end
			end
			table.insert(ldscript_fragments, fragment)
		end

		for _, fragment in ipairs(ldscript_fragments) do
			fragment.start_string =
				fragment.start_string or format("0x%x", fragment.start)
		end

		-- Sort the various ldscripts and work out the top-level PHDRs and SECTIONs
		table.sort(ldscript_fragments, function(a,b) return a.start < b.start end)
		local ldscript_top_phdrs = {}
		local ldscript_top_sections = {}
		local ldscript_top_phdr_template = "\n\tload${ix} PT_${phtype} ;"
		-- Sections inherit the most recently set program header, so we can
		-- create a stub section with no contents to set all subsequent
		-- sections to map into the right program header.  (The included linker
		-- scripts do not assign program headers on their sections.)
		local ldscript_top_section_template =
			"\n\t.load${ix}_pre_stub : { } :load${ix}" ..
			"\n\t. = ${start_string} ;" ..
			"\n\t${body}" ..
			"\n\t.load${ix}_post_stub : ALIGN(4) { }"
		for ix, v in ipairs(ldscript_fragments) do
			v.ix = ix
			v.phtype = v.phtype or "LOAD"

			-- If this fragment is a configfile, then work out its generated
			-- path from its name, generate the body for the top-level script,
			-- add it to the target's configfiles and "cheriot.ldscripts" list.
			if v.srcpath then
				if v.genname then
					assert(v.body == nil, "ldscript with srcpath and body")
					local genpath = path.join(target:configdir(), v.genname)
					v.body = format("INCLUDE %q", genpath)

					target:add("cheriot.ldscripts", genpath)
					target:add("configfiles", v.srcpath,
						{ pattern = "@(.-)@"
						, filename = v.genname
						})
				else
					target:add("cheriot.ldscripts", v.srcpath)
					v.body = format("INCLUDE %q", v.srcpath)
				end
			else
				assert(v.body, "ldscript with neither srcpath nor body")
				v.body = v.body:gsub("@(.-)@", v)
			end

			-- The ()s here are not superfluous: gsub returns multiple results,
			-- and since it's the last argument here, Lua defaults to passing
			-- all of them to the enclosing call, which isn't what we want.
			-- (See 5.3's manual's section "3.4 – Expressions".)
			table.insert(ldscript_top_phdrs,
				(ldscript_top_phdr_template:gsub("${([_%w]*)}", v)))
			table.insert(ldscript_top_sections,
				(ldscript_top_section_template:gsub("${([_%w]*)}", v)))
		end

		-- Define configuration variables for the target's top-level ldscript.
		target:set("configvar", "ldscript_top_phdrs", table.concat(ldscript_top_phdrs))
		target:set("configvar", "ldscript_top_sections", table.concat(ldscript_top_sections, "\n"))
	end)

-- Do board-directed whole-dependency-tree configuration (defines &c)
rule("cheriot.board.targets.conf")
	after_load(function (target)
		target:add("deps", "cheriot.board")
	end)

	-- Run this after all the after_load()s have completed, so we have a full
	-- view of the dependency tree
	on_config(function (target)
		local function visit_all_dependencies(callback)
			visit_all_dependencies_of(target, callback)
		end

		-- Add defines to all dependencies.
		local add_defines_each_dependency = function (defines)
			visit_all_dependencies(function (target)
				target:add('defines', defines)
			end)
		end

		-- Add cxflags to all dependencies.
		local add_cxflags = function (cxflags)
			visit_all_dependencies(function (target)
				target:add('cxflags', cxflags, {force = true})
			end)
		end

		local board_config = get_config("cheriot.board")
		local board = board_config.info
		local function path_subst(p)
			return p:gsub("${(%w*)}", board_config.path_substitutes)
		end

		if board.revoker then
			local temporal_defines = { "TEMPORAL_SAFETY" }
			if board.revoker == "software" then
				temporal_defines[#temporal_defines+1] = "SOFTWARE_REVOKER"
			end
			add_defines_each_dependency(temporal_defines)
		end

		if board.driver_includes then
			for _, include_path in ipairs(board.driver_includes) do
				-- Allow ${sdk} to refer to the SDK directory, so that external
				-- board includes can include generic platform bits.
				include_path = path_subst(include_path)
				if not path.is_absolute(include_path) then
					include_path = path.join(board_config.dir, include_path);
				end
				visit_all_dependencies(function (target)
					target:add('includedirs', include_path)
				end)
			end
		end

		-- If this board defines any macros, add them to all targets
		if board.defines then
			add_defines_each_dependency(board.defines)
		end

		-- If this board defines any cxflags, add them to all targets
		if board.cxflags then
			add_cxflags(board.cxflags)
		end

		add_defines_each_dependency("CPU_TIMER_HZ=" .. math.floor(board.timer_hz))
		add_defines_each_dependency("TICK_RATE_HZ=" .. math.floor(board.tickrate_hz))

		if board.simulation then
			add_defines_each_dependency("SIMULATION")
		end

		if board.stack_high_water_mark then
			add_defines_each_dependency("CONFIG_MSHWM")
		end

		-- Build the MMIO space for the board
		for name, _ in table.orderpairs(board.devices) do
			add_defines_each_dependency("DEVICE_EXISTS_" .. name)
		end
	end)

-- Rule for linking targets that are top-level firmware image like
rule("cheriot.firmware.linkcmd")

	after_load(function (target)
		target:add("deps", "cheriot.board.ldscript.mmio")
	end)

	-- Perform the final link step for a firmware image.
	on_linkcmd(function (target, batchcmds, opt)
		-- Get a specified linker script, or set the default to the compartment
		-- linker script.
		local linkerscript_mmio = target:dep("cheriot.board.ldscript.mmio"):targetfile()
		local linkerscript1 = target:get("cheriot.ldscript")
		-- Link using the firmware's linker script.
		batchcmds:show_progress(opt.progress, "linking firmware " .. target:targetfile())
		batchcmds:mkdir(target:targetdir())
		local objects = target:objectfiles()

		local depfilter = target:extraconf("rules", "cheriot.firmware.linkcmd", "dependency_filter")
		                  or function() return true end
		visit_all_dependencies_of(target, function (dep)
			if not dep:targetfile() then return end
			if depfilter(dep) then
				table.insert(objects, dep:targetfile())
			end
		end)

		local ldargs = {
			"-n",
			"--script=" .. linkerscript_mmio,
			"--script=" .. linkerscript1,
			"--relax",
			"-o", target:targetfile()
		}
		if target:extraconf("rules", "cheriot.firmware.linkcmd", "compartment_report") then
			batchcmds:show_progress(opt.progress, "Creating firmware report " .. target:targetfile() .. ".json")
			table.insert(ldargs, "--compartment-report=" .. target:targetfile() .. ".json")
		end

		batchcmds:vrunv(target:tool("ld"), table.join(ldargs, objects), opt)
		batchcmds:show_progress(opt.progress, "Creating firmware dump " .. target:targetfile() .. ".dump")
		batchcmds:vexecv(target:tool("objdump"), {"-glxsdrS", "--demangle", target:targetfile()}, table.join(opt, {stdout = target:targetfile() .. ".dump"}))
		batchcmds:add_depfiles(linkerscript_mmio, linkerscript1)
		batchcmds:add_depfiles(target:get("cheriot.ldscripts"))
		batchcmds:add_depfiles(objects)
	end)

-- Specialize the above specifically for a RTOS firmware target
rule ("cheriot.firmware.link")
	add_deps("cheriot.firmware.linkcmd")
	before_link(function(target)
		-- add_deps(), as used in cheriot.firmware below, doesn't set rule
		-- extraconfigs, and we don't want to make the firmware targets have to
		-- add the rule by hand, so... do that here before the
		-- "cheriot.firmware.linkcmd" on_linkcmd script fires.  Even though we
		-- control most of the firmware target definition, it's rude to steal
		-- all the script hooks for something we expect the user to touch, so
		-- do this in a rule, too.
		local cr = target:extraconf("rules", "cheriot.firmware.link", "compartment_report")
		target:extraconf_set("rules", "cheriot.firmware.linkcmd", "compartment_report",
			cr == nil and true or cr)
		target:extraconf_set("rules", "cheriot.firmware.linkcmd", "dependency_filter",
			target:extraconf("rules", "cheriot.firmware.link", "dependency_filter") or
			function(dep)
				return (dep:get("cheriot.type") == "library") or
					(dep:get("cheriot.type") == "compartment") or
					(dep:get("cheriot.type") == "privileged compartment") or
					(dep:get("cheriot.type") == "privileged library")
			end)
	end)

-- Rule for setting thread count for the per-firmware scheduler target
rule("cheriot.firmware.scheduler.threads")
	after_load(function (target)
		local scheduler = target:deps()[target:name() .. ".scheduler"]
		local threads = target:values("threads")

		local thread_priorities_set = {}
		for _, thread in ipairs(threads) do
			if type(thread.priority) ~= "number" or thread.priority < 0 then
				raise(("thread %d has malformed priority %q"):format(i, thread.priority))
			end
			thread_priorities_set[thread.priority] = true
		end

		-- Repack thread priorities into a contiguous span starting at 0.
		local thread_priorities = {}
		for p, _ in pairs(thread_priorities_set) do
			table.insert(thread_priorities, p)
		end
		table.sort(thread_priorities)
		local thread_priority_remap = {}
		for ix, v in ipairs(thread_priorities) do
			thread_priority_remap[v] = ix - 1
		end
		for i, thread in ipairs(threads) do
			if thread.priority ~= thread_priority_remap[thread.priority] then
				print(("Remapping priority of thread %d from %d to %d"):format(
					i, thread.priority, thread_priority_remap[thread.priority]
				))
				thread.priority = thread_priority_remap[thread.priority]
			end
		end

		local thread_max_priority = 0
		for _, thread in ipairs(threads) do
			thread_max_priority = math.max(thread_max_priority, thread.priority)
		end

		scheduler:add('defines', "CONFIG_THREADS_NUM=" .. #(threads))
		scheduler:add('defines', "CONFIG_THREAD_MAX_PRIORITY=" .. thread_max_priority)
	end)

rule("cheriot.firmware.common_shared_objects")
	after_load(function (target)
		local threads = target:values("threads")
		target:values_set("shared_objects", {
			-- 32-bit counter for the hazard-pointer epoch.
			allocator_epoch = 4,
			-- Two hazard pointers per thread.
			allocator_hazard_pointers = #(threads) * 8 * 2
		}, { expand = false })
	end)

-- Set configuration variables for firmware images' ldscripts base on board and
-- target (thread) information
rule("cheriot.firmware.ldscript.conf")
	before_config(function (target)
		local function visit_all_dependencies(callback)
			visit_all_dependencies_of(target, callback)
		end

		local board_target = target:deps()["cheriot.board"]
		local board = board_target:get("cheriot.board_info")

		local software_revoker = board.revoker == "software"

		-- The heap, by default, starts immediately after globals and static shared objects
		local heap_start = '.'
		if board.heap.start then
			heap_start = format("0x%x", board.heap.start)
		end

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

		-- Stacks must be less than this size or we cannot express them in the
		-- loader metadata.  The checks in lld for valid stacks currently
		-- reject anything larger than this, so provide a helpful error here,
		-- rather than an unhelpful one later.
		local stack_size_limit = 65280

		-- Initial pass through thread sequence to derive values within each
		local trusted_spill_size = board_target:get("cheriot.trusted_spill_size")
		for i, thread in ipairs(threads) do
			thread.mangled_entry_point = string.format("\"__export_%s__Z%d%sv\"", thread.compartment, string.len(thread.entry_point), thread.entry_point)
			thread.thread_id = i
			-- Trusted stack frame is 24 bytes.  If this size is too small, the
			-- loader will fail.  If it is too big, we waste space.
			thread.trusted_stack_size = trusted_spill_size + (24 * thread.trusted_stack_frames)

			if thread.stack_size > stack_size_limit then
				raise("thread " .. i .. " requested a " .. thread.stack_size ..
				" stack.  Stacks over " .. stack_size_limit ..
				" are not yet supported in the compartment switcher.")
			end
		end

		-- Pass through thread sequence, generating linker directives
		local thread_headers = ""
		local thread_trusted_stacks = ""
		local thread_stacks = ""
		for i, thread in ipairs(threads) do
			thread_stacks = thread_stacks .. string.gsub(thread_stack_template, "${([_%w]*)}", thread)
			thread_trusted_stacks = thread_trusted_stacks .. string.gsub(thread_trusted_stack_template, "${([_%w]*)}", thread)
			thread_headers = thread_headers .. string.gsub(thread_template, "${([_%w]*)}", thread)
		end

		-- Next set up the substitutions for the linker scripts.

		-- Templates for parts of the linker script that are instantiated per compartment
		local compartment_templates = {
			-- sdk/core/loader/types.h:/CompartmentHeader
			compartment_headers =
				"\n\t\tLONG(\".${compartment}_code_start\");" ..
				"\n\t\tSHORT((SIZEOF(.${compartment}_code) + 7) / 8);" ..
				"\n\t\tSHORT(\".${compartment}_imports_end\" - \".${compartment}_code_start\");" ..
				"\n\t\tLONG(\".${compartment}_export_table\");" ..
				"\n\t\tSHORT(\".${compartment}_export_table_end\" - \".${compartment}_export_table\");" ..
				"\n\t\tLONG(\".${compartment}_globals\");" ..
				"\n\t\tASSERT((SIZEOF(\".${compartment}_globals\") % 4) == 0, \"${compartment}'s globals oddly sized\");" ..
				"\n\t\tASSERT(SIZEOF(\".${compartment}_globals\") < 0x40000, \"${compartment}'s globals are too large\");" ..
				"\n\t\tSHORT(SIZEOF(\".${compartment}_globals\") / 4);" ..
				"\n\t\tSHORT(\".${compartment}_bss_start\" - \".${compartment}_globals\");" ..
				"\n\t\tLONG(\".${compartment}_cap_relocs_start\");" ..
				"\n\t\tSHORT(\".${compartment}_cap_relocs_end\" - \".${compartment}_cap_relocs_start\");" ..
				"\n\t\tLONG(\".${compartment}_sealed_objects_start\");" ..
				"\n\t\tSHORT(\".${compartment}_sealed_objects_end\" - \".${compartment}_sealed_objects_start\");\n",
			pcc_ld =
				"\n\t\".${compartment}_code\" : CAPALIGN" ..
				"\n\t{" ..
				"\n\t\t\".${compartment}_code_start\" = .;" ..
				"\n\t\t\"${obj}\"(\".compartment_import_table\");" ..
				"\n\t\t\".${compartment}_imports_end\" = .;" ..
				"\n\t\t\"${obj}\"(.text);" ..
				"\n\t\t\"${obj}\"(.init_array);" ..
				"\n\t\t\"${obj}\"(.rodata);" ..
				"\n\t\t. = ALIGN(8);" ..
				"\n\t}\n",
			gdc_ld =
				"\n\t\".${compartment}_globals\" : CAPALIGN" ..
				"\n\t{" ..
				"\n\t\t\".${compartment}_globals\" = .;" ..
				"\n\t\t\"${obj}\"(.data);" ..
				"\n\t\t\".${compartment}_bss_start\" = .;" ..
				"\n\t\t\"${obj}\"(.bss)" ..
				"\n\t\t. = ALIGN(4);" ..
				"\n\t}\n",
			compartment_exports =
				"\n\t\t. = ALIGN(8); \".${compartment}_export_table\" = .;" ..
				"\n\t\t\"${obj}\"(.compartment_export_table);" ..
				"\n\t\t\".${compartment}_export_table_end\" = .;\n",
			cap_relocs =
				"\n\t\t\".${compartment}_cap_relocs_start\" = .;" ..
				"\n\t\t\"${obj}\"(__cap_relocs);\n\t\t\".${compartment}_cap_relocs_end\" = .;",
			sealed_objects =
				"\n\t\t\".${compartment}_sealed_objects_start\" = .;" ..
				"\n\t\t\"${obj}\"(.sealed_objects);\n\t\t\".${compartment}_sealed_objects_end\" = .;"
		}
		--Library headers are almost identical to compartment headers, except
		--that they don't have any globals.
		local library_templates = {
			compartment_headers =
				"\n\t\tLONG(\".${compartment}_code_start\");" ..
				"\n\t\tSHORT((SIZEOF(.${compartment}_code) + 7) / 8);" ..
				"\n\t\tSHORT(\".${compartment}_imports_end\" - \".${compartment}_code_start\");" ..
				"\n\t\tLONG(\".${compartment}_export_table\");" ..
				"\n\t\tSHORT(\".${compartment}_export_table_end\" - \".${compartment}_export_table\");" ..
				"\n\t\tLONG(0);" ..
				"\n\t\tSHORT(0);" ..
				"\n\t\tSHORT(0);" ..
				"\n\t\tLONG(\".${compartment}_cap_relocs_start\");" ..
				"\n\t\tSHORT(\".${compartment}_cap_relocs_end\" - \".${compartment}_cap_relocs_start\");" ..
				"\n\t\tLONG(\".${compartment}_sealed_objects_start\");" ..
				"\n\t\tSHORT(\".${compartment}_sealed_objects_end\" - \".${compartment}_sealed_objects_start\");\n",
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
			heap_start=heap_start,
			thread_count=#(threads),
			thread_headers=thread_headers,
			thread_trusted_stacks=thread_trusted_stacks,
			thread_stacks=thread_stacks,
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
				"\t\t*/cheriot.software_revoker.compartment(.text .text.* .rodata .rodata.* .data.rel.ro);\n" ..
				"\t}\n" ..
				"\t.software_revoker_end = .;\n\n"
			ldscript_substitutions.software_revoker_globals =
				"\n\t.software_revoker_globals : CAPALIGN" ..
				"\n\t{" ..
				"\n\t\t.software_revoker_globals = .;" ..
				"\n\t\t*/cheriot.software_revoker.compartment(.data .data.* .sdata .sdata.*);" ..
				"\n\t\t.software_revoker_bss_start = .;" ..
				"\n\t\t*/cheriot.software_revoker.compartment(.sbss .sbss.* .bss .bss.*)" ..
				"\n\t\t. = ALIGN(8);" ..
				"\n\t}" ..
				"\n\t.software_revoker_globals_end = .;\n"
			ldscript_substitutions.compartment_exports =
				"\n\t\t. = ALIGN(8); .software_revoker_export_table = .;" ..
				"\n\t\t*/cheriot.software_revoker.compartment(.compartment_export_table);" ..
				"\n\t\t.software_revoker_export_table_end = .;\n" ..
				ldscript_substitutions.compartment_exports
			-- sdk/core/loader/types.h:/PrivilegedCompartment
			ldscript_substitutions.software_revoker_header =
				"\n\t\tLONG(.software_revoker_start);" ..
				"\n\t\tSHORT(.software_revoker_end - .software_revoker_start);" ..
				"\n\t\tLONG(.software_revoker_globals);" ..
				"\n\t\tASSERT((SIZEOF(.software_revoker_globals) % 4) == 0, \"Software revoker globals oddly sized\");" ..
				"\n\t\tSHORT(SIZEOF(.software_revoker_globals) / 4);" ..
				"\n\t\tLONG(0)" ..
				"\n\t\tSHORT(0)" ..
				"\n\t\tLONG(.software_revoker_export_table);" ..
				"\n\t\tSHORT(.software_revoker_export_table_end - .software_revoker_export_table);\n" ..
				"\n\t\tLONG(0);" ..
				"\n\t\tSHORT(0);\n"
		end


		-- Process all of the library dependencies.
		local library_count = 0
		visit_all_dependencies(function (target)
			if target:get("cheriot.type") == "library" then
				library_count = library_count + 1
				add_dependency(target:name(), target, library_templates)
			end
		end)

		-- Process all of the compartment dependencies.
		local compartment_count = 0
		visit_all_dependencies(function (target)
			if target:get("cheriot.type") == "compartment" then
				compartment_count = compartment_count + 1
				add_dependency(target:name(), target, compartment_templates)
			end
		end)

		local shared_objects = { }
		visit_all_dependencies(function (target)
			local globals = target:values("shared_objects")
			if globals then
				for name, size in pairs(globals) do
					if not (name == "__wrap_locked__") then
						if shared_objects[global] and (not (shared_objects[global] == size)) then
							raise("Global " .. global .. " is declared with different sizes.")
						end
						shared_objects[name] = size
					end
				end
			end
		end)
		-- TODO: We should sort pre-shared globals by size to minimise padding.
		-- Each global is emitted as a separate section so that we can use
		-- CAPALIGN and let the linker insert the required padding.
		local shared_objects_template =
			"\n\t\t. = ALIGN(MIN(${size}, 8));" ..
			"\n\t\t__cheriot_shared_object_section_${global} : CAPALIGN" ..
			"\n\t\t{" ..
			"\n\t\t\t__cheriot_shared_object_${global} = .;" ..
			"\n\t\t\t. += ${size};" ..
			"\n\t\t\t__cheriot_shared_object_${global}_end = .;" ..
			"\n\t\t}\n"
		local shared_objects_section = ""
		for global, size in table.orderpairs(shared_objects) do
			shared_objects_section = shared_objects_section .. string.gsub(shared_objects_template, "${([_%w]*)}", {global=global, size=size})
		end
		ldscript_substitutions.shared_objects = shared_objects_section

		-- Add the counts of libraries and compartments to the substitution list.
		ldscript_substitutions.compartment_count = compartment_count
		ldscript_substitutions.library_count = library_count

		-- Set the each of the substitutions.
		for key, value in pairs(ldscript_substitutions) do
			target:set("configvar", key, value)
		end

	end)

rule("cheriot.firmware.ldscript.files")
	-- Text and data segments are configfiles for each firmware target.  Set
	-- those here and let the "cheriot.board.ldscript.conf" rule handle working
	-- out the rest.  This must be done in on_load() because the bulk of that
	-- rule's work is done in after_load().
	on_load(function (target)
		target:set("cheriot.ldfragment.rocode", {
			{ srcpath = path.join(scriptdir, "firmware.rocode.ldscript.in")
			, genname = target:name() .. "-firmware.rocode.ldscript"
			}})

		target:set("cheriot.ldfragment.rwdata", {
			{ srcpath = path.join(scriptdir, "firmware.rwdata.ldscript.in")
			, genname = target:name() .. "-firmware.rwdata.ldscript"
			}})
	end)

	after_load(function (target)
		-- Generate our top-level linker script as a configfile
		do
			local top_ldscript_genname = target:name() .. "-firmware.ldscript"
			target:set("cheriot.ldscript", path.join(target:configdir(), top_ldscript_genname))
			target:add("configfiles", path.join(scriptdir, "firmware.ldscript.in"),
				{pattern = "@(.-)@", filename = top_ldscript_genname })
		end
	end)

rule("cheriot.firmware.run")
	on_run(function (target)
		import("core.project.config")

		local board = target:deps()["cheriot.board"]:get("cheriot.board_info")

		if (not board.run_command) and (not board.simulator) then
			raise("board description does not define a run command")
		end

		local simulator = board.run_command or board.simulator
		simulator = string.gsub(simulator, "${(%w*)}", { sdk=scriptdir, board=boarddir })
		local firmware = target:targetfile()
		local directory = path.directory(firmware)
		firmware = path.filename(firmware)
		local run = function(simulator)
			local simargs = { firmware }
			os.execv(simulator, simargs, { curdir = directory })
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

-- Rule for defining a firmware image.
rule("cheriot.firmware")
	-- Firmwares build using our toolchain
	add_deps("cheriot.toolchain")

	-- Firmwares are reachability roots.
	add_deps("cheriot.reachability_root")

	-- Firmware targets want board-driven ldscript processing
	add_deps("cheriot.board.ldscript.conf")

	-- Firmware targets want board-driven all-dependent-target configuration
	add_deps("cheriot.board.targets.conf")

	add_deps("cheriot.firmware.link")

	add_deps("cheriot.firmware.scheduler.threads")

	add_deps("cheriot.firmware.ldscript.conf")
	add_deps("cheriot.firmware.ldscript.files")

	add_deps("cheriot.firmware.run")

	-- Set up the thread defines and further information for the linker script.
	-- This must be after load so that dependencies are resolved.
	after_load(function (target)

		-- Pick up the dependency on the board information, which has been
		-- processed already by virtue of that target being *loaded*.
		target:add("deps", "cheriot.board")

		local board = target:deps()["cheriot.board"]:get("cheriot.board_info")
		if board.revoker == "software" then
			target:add('deps', "cheriot.software_revoker")
		end
	end)


-- Rule for conditionally enabling debug for a component.
rule("cheriot.component-debug")
	after_load(function (target)
		local name = target:get("cheriot.debug-name") or target:name()
		local value = get_config("debug-"..name)
		if type(value) == "nil" then
			error ("No debug configuration for %q; missing xmake debugOption()?"):format(name)
		elseif type(value) == "boolean" then
			value = tostring(value)
		else
			-- Initial capital
			value = "DebugLevel::" .. string.sub(value, 1, 1):upper() .. string.sub(value, 2)
		end
		target:add('options', "debug-" .. name)
		target:add('defines', "DEBUG_" .. name:upper() .. "=" .. value);
	end)

-- Rule for conditionally enabling stack checks for a component.
rule("cheriot.component-stack-checks")
	after_load(function (target)
		local name = target:get("cheriot.debug-name") or target:name()
		target:add('options', "stack-usage-check-" .. name)
		target:add('defines', "CHERIOT_STACK_CHECKS_" .. name:upper() .. "=" .. tostring(get_config("stack-usage-check-"..name)))
	end)

-- Rule for making RTOS git revision information available to a build target.
--
-- Because this value is definitionally quite volatile, we jump through some
-- hoops to allow it to be set per file rather than per xmake target, minimizing
-- splash damage (necessitating only recompiling the necessary files and
-- relinking revdepwards to the firmware image).  That is, rather than using
-- add_rules at the target scope, you can add this rule as part of add_files:
--
--   add_files("version.cc", {rules = {"cheriot.define-rtos-git-description"}})
local sdk_git_description = nil
rule("cheriot.define-rtos-git-description")
	before_build_file(function(target, sourcefile, opt)
		sdk_git_description = sdk_git_description or try {
			function()
				return os.iorunv("git", {"-C", scriptdir, "describe", "--always", "--dirty"}):gsub("[\r\n]", "")
			end
		}
		sdk_git_description = sdk_git_description or "unknown"

		local fileconfig = target:fileconfig(sourcefile) or {}
		fileconfig.defines = fileconfig.defines or {}
		table.insert(fileconfig.defines, ("CHERIOT_RTOS_GIT_DESCRIPTION=%q"):format(sdk_git_description))
		target:fileconfig_set(sourcefile, fileconfig)
	end)

-- Common aspects of the CHERIoT loader target
rule("cheriot.loader.base")
	add_deps("cheriot.toolchain",
	         "cheriot.component-debug",
	         "cheriot.baremetal-abi",
	         "cheriot.subobject-bounds")

	on_load(function (target)
		target:set("kind", "object")
		target:set("default", false)

		target:add("deps", "cheriot.board")

		target:add("defines",
		           "CHERIOT_AVOID_CAPRELOCS",
		           "CHERIOT_NO_AMBIENT_MALLOC")

		target:set('cheriot.debug-name', "loader")

		target:set("optimize", "fast")
		target:set("languages", "c23", "cxx23")
	end)

	after_load(function (target)
		local board_target = target:dep("cheriot.board")
		local board = board_target:get("cheriot.board_info")
		target:add("defines", board.rtos_defines and board.rtos_defines.loader)
		target:add('defines',
			"CHERIOT_LOADER_TRUSTED_SPILL_SIZE=" .. board_target:get("cheriot.trusted_spill_size"))
	end)

-- Build the loader.
target("cheriot.loader")
	add_rules("cheriot.loader.base")

	-- FIXME: We should be setting this based on a board config file.
	add_files(path.join(coredir, "loader/boot.S"),
	          path.join(coredir, "loader/boot.cc"))

-- Helper function to define firmware.  Used as `target`.
function firmware(name)
	-- Build the scheduler.  The firmware rule will set the flags required for
	-- this to create threads.
	target(name .. ".scheduler")
		add_rules("cheriot.privileged-compartment", "cheriot.component-debug", "cheriot.component-stack-checks", "cheriot.subobject-bounds")
		add_deps("locks", "crt", "atomic1")
		add_deps("compartment_helpers")
		add_deps("cheriot.board.interrupts")
		add_deps("cheriot.board")
		on_load(function (target)
			target:set("cheriot.compartment", "scheduler")
			target:set('cheriot.debug-name', "scheduler")
			target:add('defines', "SCHEDULER_ACCOUNTING=" .. tostring(get_config("scheduler-accounting")))
			target:add('defines', "SCHEDULER_MULTIWAITER=" .. tostring(get_config("scheduler-multiwaiter")))
		end)
		after_load(function (target)
			local board = target:dep("cheriot.board"):get("cheriot.board_info")
			target:add("defines", board.rtos_defines and board.rtos_defines.scheduler)

			if board.interrupts then
				-- Define the macro that's used to initialise the scheduler's interrupt configuration.
				local interruptConfiguration = "CHERIOT_INTERRUPT_CONFIGURATION="
				for _, interrupt in ipairs(board.interrupts) do
					interruptConfiguration = interruptConfiguration .. "{"
						.. math.floor(interrupt.number) .. ","
						.. math.floor(interrupt.priority) .. ","
						.. (interrupt.edge_triggered and "true" or "false")
						.. "},"
				end
				target:add('defines', interruptConfiguration)
			end
		end)
		add_files(path.join(coredir, "scheduler/main.cc"))

	-- Create the firmware target.  This target remains open on return and so
	-- the caller can add more rules to it.
	target(name)
		set_kind("binary")
		add_deps("cheriot.board.file")
		add_rules("cheriot.firmware")
		add_rules("cheriot.firmware.common_shared_objects")
		add_rules("cheriot.conditionally_link_allocator")
		add_deps(name .. ".scheduler", "cheriot.loader", "cheriot.switcher")
		add_deps("cheriot.token_library")
end

-- Helper to create a library.
function library(name)
	target(name)
		add_rules("cheriot.library")
end

-- Helper to create a compartment.
function compartment(name)
	target(name)
		add_rules("cheriot.compartment")

		-- It's a good guess that application compartments depend on the
		-- interrupt configuration.  Let's bake that assumption in, on the
		-- grounds that the few users that don't want to pick up that build
		-- dependency (which is, recall, just a generated include file and
		-- contributes no bytes to the final image if not used) can use the
		-- compartment rule instead.
		add_deps("cheriot.board.interrupts")
end

-- Rules for standalone Rust source files.
-- Note that to make the implementation of this rule not too complex single source files are compiled to static libraries,
-- which will also contain the the parts of Rust's standard library that CHERIoT supports.
--
-- It expects the user to pass a CHERIoT-enabled `rustc` with the `--rc` flag to `xmake config`.
rule("cheriot.rust", function()
	set_extensions(".rs")
	on_build_file(function(target, sourcefile, opt)
		-- imports
		import("core.base.option")
		import("core.base.hashset")
		import("core.tool.compiler")
		import("core.project.depend")
		import("utils.progress")
		import("core.base.json")
		import("core.project.config")

		local dependfile = target:dependfile(sourcefile)

		-- path/to/rust.rs -> libpath_to_rust
		local libname = "lib" .. string.gsub(sourcefile:sub(1, #sourcefile - 3), "//", "_")
		local targetfile = target:objectfile(libname)
		-- <buildir>/libpath_to_rust.o -> <buildir>/libpath_to_rust.a
		local targetfile = targetfile:sub(1, #targetfile - 1) .. "a"
		table.insert(target:objectfiles(), targetfile)

		local compinst = compiler.load("rc", { target = target })
		local compflags = compinst:compflags({ target = target })

		local dependinfo = option.get("rebuild") and {} or (depend.load(dependfile) or {})

		local depvalues = { compinst:program(), compflags, sourcefile }
		if not depend.is_changed(dependinfo, { lastmtime = os.mtime(targetfile), values = depvalues }) then
			return
		end

		progress.show(opt.progress, "${color.build.object}compiling %s", path.filename(sourcefile))

		io.flush()

		dependinfo.files = {}
		local flags = table.join("-Copt-level=z", "--crate-type=staticlib", compflags, "-o", targetfile, sourcefile)

		vprint("%s %s", compinst:program(), table.concat(flags, " "))
		local outdata, errdata = os.iorunv(compinst:program(), flags)

		assert(errdata == nil or errdata == "" or errdata:match("Finished") or (not errdata:match("error")),
			"failed to compile  " .. sourcefile .. ":\n" .. errdata)

		dependinfo.values = depvalues
		table.insert(dependinfo.files, sourcefile)
		depend.save(dependinfo, dependfile)
	end)
end)

-- Hack to override the default rule for Rust, which is not sensible for our use-case.
rule("rust", function()
	add_deps("cheriot.rust")
end)

-- Rule for Rust crates. As for standalone Rust source files, it instructs the Rust compiler
-- to produce static libraries, so that all the relevant parts of Rust's standard library
-- that CHERIoT supports are bundled.
--
-- It requires the user to have `cargo` in the $PATH and to provide a CHERIoT-enabled `rustc`
-- with  the `--rc` flag to `xmake config`.
--
-- Internally, it "just" calls `cargo` passing the path to the Cargo.toml relative to this rule,
-- makes it compile the crate to a static library and wires up the target to add the resulting
-- library to the object files to link to produce the final target.
rule("cheriot.rust.crate", function()
	set_sourcekinds("cheriot_rust_crate")

	-- Invoke `cargo` to build the crate with the given manifest path using the selected `rustc` compiler.
	-- Then, add the resulting static library file as an object file for the target, which will be picked up during linking.
	on_build_file(function(target, sourcefile, opt)
		import("lib.detect.find_tool")
		import("utils.progress")
		import("core.base.json")
		import("core.project.config")

		local cargo = find_tool("cargo")
		assert(
			cargo,
			"No `cargo` binary was found. Please install `cargo`: https://doc.rust-lang.org/cargo/getting-started/installation.html"
		)

		local rc = target:compiler("rc")
		assert(rc, "No `rustc` compiler was set.")

		-- We won't check for depfiles here: cargo will take care of that for us.
		local manifest_path = path.absolute(sourcefile)

		local flags = { "metadata", "--no-deps", "--format-version=1", "--manifest-path=" .. manifest_path }
		local cmd = cargo.program .. table.concat(flags, " ")
		local crate_metadata, errdata = os.iorunv(cargo.program, flags)

		assert(not errdata or errdata == "", "failed to run `" .. cmd .. " :\n" .. errdata)

		local crate_metadata = json.decode(crate_metadata:trim())
		assert(crate_metadata["packages"], "Invalid Cargo.toml format. Does it have a `[package]` section?")

		local crate_name = nil
		for _, v in ipairs(crate_metadata["packages"]) do
			local name = v["name"]
			if name then
				crate_name = name
				break
			end
		end

		assert(
			crate_name,
			"Invalid Cargo.toml format. Does it have a `[package]` section with a `name = ` for the package?"
		)
		local build_dir = path.join(path.directory(target:objectfile("foo")), crate_name)

		progress.show(opt.progress, "${color.build.object}compiling.$(mode) crate %s", crate_name)

		local rustflags = rc:compflags()
		local cargoflags = { "build", "--lib", "--target-dir=" .. build_dir, "--manifest-path=" .. manifest_path }
		local crate_build_mode

		if is_mode("release") then
			table.insert(cargoflags, "--release")
			crate_build_mode = "release"
		else
			crate_build_mode = "debug"
		end

		local rustflags_str = table.concat(rustflags, " ")
		local cmd = string.format(
			'RUSTC="%s" RUSTFLAGS="%s" %s %s',
			rc:program(),
			rustflags_str,
			cargo.program,
			table.concat(cargoflags, " ")
		)

		vprint(cmd)

		local outdata, errdata =
				os.iorunv(cargo.program, cargoflags, { envs = { RUSTC = rc:program(), RUSTFLAGS = rustflags_str } })

		assert(
			errdata == nil or errdata == "" or errdata:match("Finished") or (not errdata:match("error")),
			"failed to compile  " .. sourcefile .. ":\n" .. errdata
		)

		-- Add the resulting static library to the objectfiles() of the target and remove any Cargo.toml.o that might appear there.
		local library_path = path.join(build_dir, crate_build_mode, "lib" .. crate_name .. ".a")
		table.insert(target:objectfiles(), library_path)
		for i, v in ipairs(target:objectfiles()) do
			if v:match("Cargo.toml.o") then
				target:objectfiles()[i] = nil
			end
		end
	end)
end)

includes("lib/")
