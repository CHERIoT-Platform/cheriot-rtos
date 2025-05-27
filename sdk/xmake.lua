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

local function option_disable_unless_dep(option, dep)
	if not option:dep(dep):value() then option:enable(false) end
end

local function option_check_dep(raise, option, dep)
	if option:value() and not option:dep(dep):value() then
		return raise("Option " .. option:name() .. " depends on " .. dep)
	end
end

option("board")
	set_description("Board JSON description file")
	set_showmenu(true)
	set_category("board")

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

-- Force -Oz irrespective of build config.  At -O0, we blow out our stack and
-- require much stronger alignment.
set_optimize("smallest")

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
			"riscv32cheriot-unknown-cheriotrtos",
			"-mcpu=cheriot",
			"-mabi=cheriot",
			"-mxcheri-rvc",
			"-mrelax",
			"-fshort-wchar",
			"-nostdinc",
			"-Oz",
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
			"-I" .. path.join(include_directory, "c++-config"),
			"-I" .. path.join(include_directory, "libc++"),
			"-I" .. include_directory,
		}
		-- C/C++ flags
		toolchain:add("cxflags", default_flags)
		toolchain:add("cxxflags", "-std=c++23")
		toolchain:add("cflags", "-std=c23")
		-- Assembly flags
		toolchain:add("asflags", default_flags)
	end)
toolchain_end()

-- Override cxflags and cflags for the cheriot-clang toolchain to use the
-- baremetal ABI and target triple instead.
--
-- For xmake reasons, these get appended to the toolchain parameters, so we're
-- relying on the tools having a "last one wins" policy, with nothing in the
-- middle being interpreted relative to an earlier value.
rule("cheriot.baremetal-abi")
	on_load(function (target)
		for _, flags in ipairs({"cxflags", "asflags"}) do
			target:add(flags,
				{ "-target", "riscv32cheriot-unknown-unknown" },
				{ expand = false, force = true })
			target:add(flags,
				{ "-mabi=cheriot-baremetal" },
				{ expand = false, force = true })
		end
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

set_defaultarchs("cheriot")
set_defaultplat("cheriot")
set_languages("c23", "cxx23")


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


-- Common rules for any CHERI MCU component (library or compartment)
rule("cheriot.component")

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

-- Helper to find a board file given either the name of a board file or a path.
local function board_file_for_name(boardName, searchDir)
	-- ${sdkboards} for absolute references
	local boardfile = string.gsub(boardName, "${(%w*)}",
		{ sdkboards=path.join(scriptdir, "boards") })
	-- The directory containing the board file.
	local boarddir = path.directory(boardfile);
	-- If this isn't a path, look in searchDir
	if not os.isfile(boardfile) then
		boarddir = searchDir
		local fullBoardPath = path.join(boarddir, boardfile .. '.json')
		if not os.isfile(fullBoardPath) then
			fullBoardPath = path.join(boarddir, boardfile .. '.patch')
		end
		if not os.isfile(fullBoardPath) then
			return nil
		end
		boardfile = fullBoardPath
	end
	return boarddir, boardfile
end

-- If a string value is a number, return it as number, otherwise return it
-- in its original form.
local function asNumberIfNumber(value)
	if tostring(tonumber(value)) == value then
		return tonumber(value)
	end
	return value
end

-- Heuristic to tell a Lua table is probably an array in Lua
-- This is O(n), but n is usually very small, and this happens once per
-- build so this doesn't really matter.
--
-- The generality and minimality of Lua tables results in some subtlety.  While
-- Lua has a notion of "borders" within the integer keys of a table t (values b
-- s.t. "(b == 0 or t[b] ~= nil) and t[b+1] == nil"), atop which it defines a
-- "sequence", a table with only a single border, we mean something stronger: a
-- sequence with only positive integer keys densely packed from 1.
local function isarray(t)
	local border = nil

	-- Iteration order is undefined, even for numeric keys.  Each visited key
	-- has non-nil value.
	for k, _ in pairs(t) do
		-- A non-positive-integral key means this isn't an array
		-- (and since lua integers are finite, exclude anything for which
		-- successor would be ill-defined)
		if type(k) ~= "number" or
		   k <= 0 or
		   k >= math.maxinteger or
		   math.tointeger(k) ~= k then
			return false
		end

		if t[k+1] == nil then
			-- More than one border means this isn't a sequence
			if border ~= nil then return false end
			border = k
		end
	end

	-- An empty table (in which no border will be found) is an array.
	-- Otherwise, t is an array if all of the above and t[1] is populated.
	return (border == nil) or (t[1] ~= nil)
end


local function patch_board(base, patches, xmakeJson)
	for _, p in ipairs(patches) do
		if not p.op then
			print("missing op in "..xmakeJson.encode(p))
			return nil
		end
		if not p.path or (type(p.path) ~= "string") then
			print("missing or invalid path in "..xmakeJson.encode(p))
			return nil
		end

		-- Parse the JSON Pointer into an array of filed names, converting
		-- numbers into Lua numbers if we see them.  This is not quite right,
		-- because it doesn't handle field names with / in them, but we don't
		-- currently use those for anything.  It also assumes that we really do
		-- mean array indexes when we say numbers.  If we have an object with
		-- "3" as the key and try to replace 3, it will currently not do the
		-- right thing.  
		local objectPath = {}
		for entry in string.gmatch(p.path, "/([^/]+)") do
			table.insert(objectPath, asNumberIfNumber(entry))
		end

		if #objectPath < 1 then
			print("invalid path in "..xmakeJson.encode(p))
			return nil
		end

		-- JSON arrays are indexed from 0, Lua's are from 1.  If someone says
		-- array index 0, we need to map that to 1, and so on.

		-- Last path object is the name of the key we're going to modify.
		local nodeName = table.remove(objectPath)
		-- Walk the path to find the object that we're going to modify.
		local nodeToModify = base
		for _, pathComponent in ipairs(objectPath) do
			if isarray(nodeToModify) then
				if type(pathComponent) ~= "number" then
					print("invalid non-numeric index into array in "..xmakeJson.encode(p))
					return nil
				end
				pathComponent = pathComponent + 1
			end
			nodeToModify = nodeToModify[pathComponent]
		end

		local isArrayOperation = false
		if isarray(nodeToModify) then
			if type(nodeName) == "number" then
				nodeName = nodeName + 1
				isArrayOperation = true
			elseif p.op == "add" and nodeName == "-" then
				-- The string "-" at the end of an "add"'s path means "append"
				nodeName = #nodeToModify + 1
				isArrayOperation = true
			end
		end

		-- Handle the operation
		if (p.op == "replace") or (p.op == "add") then
			if not p.value then
				print(tostring(p.op).. " requires a value, missing in ", xmakeJson.encode(p))
				return nil
			end
			if isArrayOperation and p.op == "add" then
				table.insert(nodeToModify, nodeName, p.value)
			else
				nodeToModify[nodeName] = p.value
			end
		elseif p.op == "remove" then
			nodeToModify[nodeName] = nil
		else
			print(tostring(p.op) .. " is not a valid operation in ", xmakeJson.encode(p))
			return nil
		end
	end
end

-- Helper to load a board file.  This must be passed the json object provided
-- by import("core.base.json") because import does not work in helper
-- functions at the top level.
local function load_board_file_inner(boardDir, boardFile, xmakeJson)
	if path.extension(boardFile) == ".json" then
		return xmakeJson.loadfile(boardFile)
	end
	if path.extension(boardFile) ~= ".patch" then
		print("unknown extension for board file: " .. boardFile)
		return nil
	end
	local patch = xmakeJson.loadfile(boardFile)
	if not patch.base then
		print("Board file " .. boardFile .. " does not specify a base")
		return nil
	end
	local baseDir, baseFile = board_file_for_name(patch.base, boardDir)
	if not baseDir then
		print("unable to find board file " .. patch.name .. ".  Try specifying a full path")
		return nil
	end
	local base = load_board_file_inner(baseDir, baseFile, xmakeJson)

	patch_board(base, patch.patch, xmakeJson)

	return base
end

-- Load a board (patch) file (recursively) and then apply the configuration's
-- mixins as well.
local function load_board_file(boardDir, boardFile, xmakeJson, xmakeConfig)
	local base = load_board_file_inner(boardDir, boardFile, xmakeJson)

	local mixinString = xmakeConfig.get("board-mixins")
	if not mixinString or mixinString == "" then
		return base
	end

	for mixinName in mixinString:gmatch("([^,]*),?") do
		-- Initially, look next to the board file
		local mixinDir, mixinFile = board_file_for_name(mixinName, boardDir)
		if not mixinDir then
			-- Fall back to looking in the SDK/ boards dir (which might be the same thing)
			mixinDir, mixinFile = board_file_for_name(mixinName, path.join(scriptdir, "boards"))
		end
		if not mixinDir then
			print("unable to find board mixin " .. mixinName .. ".  Try specifying a full path")
			return nil
		end

		-- XXX this *ought* to return nil, error on error, but it just throws.
		local mixinTree, err = xmakeJson.loadfile(mixinFile)
		if not mixinTree then
			error ("Could not process mixin %q: %s"):format(mixinName, err)
		end

		print(("Patching board with %q"):format(mixinFile))

		patch_board(base, mixinTree, xmakeJson)
	end

	return base
end

target("cheriot.board")
	set_kind("phony")
	set_default(false)

	on_load(function (target)
		import("core.base.json")
		import("core.project.config")

		local boarddir, boardfile = board_file_for_name(get_config("board"), path.join(scriptdir, "boards"))
		if not boarddir then
			raise("unable to find board file " .. get_config("board") .. ".  Try specifying a full path")
		end
		local board = load_board_file(boarddir, boardfile, json, config)

		-- Normalize memory extents within the board to have either both an end
		-- and length or neither.
		local function normalize_extent(jsonpath, extent)
			local start = extent.start
			local stop = extent["end"]
			local length = extent.length
			if not stop and not length then
				raise(table.concat({
					"Memory extent",
					table.concat(jsonpath,"."),
					"does not specify a length or an end"}, " "))
			elseif not stop then
				extent["end"] = start + length
			elseif not length then
				extent.length = stop - start
			elseif start + length ~= stop then
				raise(table.concat({
					"Memory extent",
					table.concat(jsonpath,"/"),
					"specifies inconsistent length and end"}, " "))
			end
		end
		local jsonpath = {"devices"}
		for name, extent in table.orderpairs(board.devices) do
			jsonpath[2] = name
			normalize_extent(jsonpath, extent)
		end
		normalize_extent({"instruction_memory"}, board.instruction_memory)
		if board.data_memory then
			normalize_extent({"data_memory"}, board.data_memory)
		end
		local jsonpath = {"ldscript_fragments"}
		for name, fragment in table.orderpairs(board.ldscript_fragments) do
			-- XXX: ldscript fragments don't always know where they end?
			if type(name) == "string" and (fragment["end"] or fragment.length) then
				jsonpath[2] = name
				normalize_extent(jsonpath, fragment)
			end
		end

		target:set("cheriot.board_dir", boarddir)
		target:set("cheriot.board_file", boardfile)
		target:set("cheriot.board_info", { board })
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
		local board_target = target:deps()["cheriot.board"]
		local board = board_target:get("cheriot.board_info")
		local boarddir = board_target:get("cheriot.board_dir")

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
				-- Allow ${sdk} to refer to the SDK directory, like includes
				fragment.srcpath = string.gsub(fragment.srcpath, "${(%w*)}", { sdk=scriptdir })
				if not path.is_absolute(fragment.srcpath) then
					fragment.srcpath = path.join(boarddir, fragment.srcpath);
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

		local board_target = target:deps()["cheriot.board"]
		local boarddir = board_target:get("cheriot.board_dir")
		local board = board_target:get("cheriot.board_info")

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
				include_path = string.gsub(include_path, "${(%w*)}", { sdk=scriptdir })
				if not path.is_absolute(include_path) then
					include_path = path.join(boarddir, include_path);
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

-- Rule for defining a firmware image.
rule("cheriot.firmware")
	-- Firmwares are reachability roots.
	add_deps("cheriot.reachability_root")

	-- Firmware targets want board-driven ldscript processing
	add_deps("cheriot.board.ldscript.conf")

	-- Firmware targets want board-driven all-dependent-target configuration
	add_deps("cheriot.board.targets.conf")

	add_deps("cheriot.firmware.link")

	add_deps("cheriot.firmware.scheduler.threads")

	add_imports("core.project.config")

	on_run(function (target)
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

	before_config(function (target)
		local function visit_all_dependencies(callback)
			visit_all_dependencies_of(target, callback)
		end

		local board = target:deps()["cheriot.board"]:get("cheriot.board_info")

		local software_revoker = board.revoker == "software"

		local loader = target:deps()['cheriot.loader'];

		if not board.stack_high_water_mark then
			-- If we don't have the stack high watermark, the trusted stack is smaller.
			loader:set('loader_trusted_stack_size', 176)
		end

		-- Generate our top-level linker script as a configfile
		do
			local top_ldscript_genname = target:name() .. "-firmware.ldscript"
			target:set("cheriot.ldscript", path.join(target:configdir(), top_ldscript_genname))
			target:add("configfiles", path.join(scriptdir, "firmware.ldscript.in"),
				{pattern = "@(.-)@", filename = top_ldscript_genname })
		end

		-- The heap, by default, starts immediately after globals and static shared objects
		local heap_start = '.'
		if board.heap.start then
			heap_start = format("0x%x", board.heap.start)
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

		-- Stacks must be less than this size or we cannot express them in the
		-- loader metadata.  The checks in lld for valid stacks currently
		-- reject anything larger than this, so provide a helpful error here,
		-- rather than an unhelpful one later.
		local stack_size_limit = 65280

		-- Initial pass through thread sequence to derive values within each
		for i, thread in ipairs(threads) do
			thread.mangled_entry_point = string.format("\"__export_%s__Z%d%sv\"", thread.compartment, string.len(thread.entry_point), thread.entry_point)
			thread.thread_id = i
			-- Trusted stack frame is 24 bytes.  If this size is too small, the
			-- loader will fail.  If it is too big, we waste space.
			thread.trusted_stack_size = loader_trusted_stack_size + (24 * thread.trusted_stack_frames)

			if thread.stack_size > stack_size_limit then
				raise("thread " .. i .. " requested a " .. thread.stack_size ..
				" stack.  Stacks over " .. stack_size_limit ..
				" are not yet supported in the compartment switcher.")
			end
		end

		-- Pass through thread sequence, generating linker directives
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

		local shared_objects = {
			-- 32-bit counter for the hazard-pointer epoch.
			allocator_epoch = 4,
			-- Two hazard pointers per thread.
			allocator_hazard_pointers = #(threads) * 8 * 2
			}
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

-- Build the loader.  The firmware rule will set the flags required for
-- this to create threads.
target("cheriot.loader")
	set_default(false)
	add_rules("cheriot.component-debug", "cheriot.baremetal-abi", "cheriot.subobject-bounds")
	set_kind("object")
	-- FIXME: We should be setting this based on a board config file.
	add_files(path.join(coredir, "loader/boot.S"), path.join(coredir, "loader/boot.cc"),  {force = {cxflags = "-O1"}})
	add_defines("CHERIOT_AVOID_CAPRELOCS")

	add_deps("cheriot.board")

	after_load(function (target)
		target:set('cheriot.debug-name', "loader")
		local config = {
			-- Size in bytes of the trusted stack.
			loader_trusted_stack_size = 192,
			-- Size in bytes of the loader's stack.
			loader_stack_size = 1024
		}
		target:add('defines', "CHERIOT_LOADER_STACK_SIZE=" .. config.loader_stack_size)
		target:add("defines", "CHERIOT_NO_AMBIENT_MALLOC")
		target:set('cheriot_loader_config', config)
		for k, v in pairs(config) do
			target:set(k, v)
		end

		local board = target:dep("cheriot.board"):get("cheriot.board_info")
		target:add("defines", board.rtos_defines and board.rtos_defines.loader)
	end)

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

includes("lib/")

