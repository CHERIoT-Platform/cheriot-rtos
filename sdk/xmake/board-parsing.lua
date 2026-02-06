local json = import("core.base.json", { anonymous = true })

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


local function patch_board(base, patches)
	for _, p in ipairs(patches) do
		if not p.op then
			print("missing op in " .. json.encode(p))
			return nil
		end
		if not p.path or (type(p.path) ~= "string") then
			print("missing or invalid path in " .. json.encode(p))
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
			print("invalid path in " .. json.encode(p))
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
					print("invalid non-numeric index into array in " .. json.encode(p))
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
				print(tostring(p.op).. " requires a value, missing in ", json.encode(p))
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
			print(tostring(p.op) .. " is not a valid operation in ", json.encode(p))
			return nil
		end
	end
end

-- Helper to load a board file.  This must be passed the json object provided
-- by import("core.base.json") because import does not work in helper
-- functions at the top level.
local function load_board_file_inner(boardDir, boardFile)
	if path.extension(boardFile) == ".json" then
		return json.loadfile(boardFile)
	end
	if path.extension(boardFile) ~= ".patch" then
		print("unknown extension for board file: " .. boardFile)
		return nil
	end
	local patch = json.loadfile(boardFile)
	if not patch.base then
		print("Board file " .. boardFile .. " does not specify a base")
		return nil
	end
	local baseDir, baseFile = board_file_for_name(patch.base, boardDir)
	if not baseDir then
		print("unable to find board file " .. patch.name .. ".  Try specifying a full path")
		return nil
	end
	local base = load_board_file_inner(baseDir, baseFile)

	patch_board(base, patch.patch)

	return base
end

-- Load a board (patch) file (recursively) and then apply the configuration's
-- mixins as well.
local function load_board_file(boardDir, boardFile, mixinString)
	local base = load_board_file_inner(boardDir, boardFile)

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
		local mixinTree, err = json.loadfile(mixinFile)
		if not mixinTree then
			error ("Could not process mixin %q: %s"):format(mixinName, err)
		end

		print(("Patching board with %q"):format(mixinFile))

		patch_board(base, mixinTree)
	end

	return base
end

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

function main(defaultPath, boardName, boardMixins)
	local boarddir, boardfile = board_file_for_name(boardName, defaultPath)
	if not boarddir then
		raise("unable to find board file " .. boardName .. ".  Try specifying a full path")
	end
	local board = load_board_file(boarddir, boardfile, boardMixins)

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

	-- The size of a register spill frame in a trusted stack is a function
	-- of the board.  While *most* of the system gets this from
	-- core/switcher/trusted-stack-assembly.h, we need it when sizing
	-- thread trusted stacks over in the generated linker scripts.  The
	-- loader component asserts that this value matches what the rest of
	-- the system sees.
	board.trusted_spill_size = board.stack_high_water_mark and 192 or 176

	return { info = board, dir = boarddir, file = boardfile }
end
