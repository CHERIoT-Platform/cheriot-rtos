-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

library("clock_helpers")
  set_default(false)
  add_files("clock-helpers.cc")
  on_load(function(target)
    target:values_set("shared_objects", { wall_clock_time = 24 }, {expand = false})
  end)

-- Write contents to path if it would create or update the contents
-- Copied from sdk/xmake.lua, because functions there aren't exported.
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


compartment("wall_clock")
  set_default(false)
  add_files("wall_clock.cc")
  add_deps("unwind_error_handler", "clock_helpers")
  on_prepare(function(target)
    local template = io.readfile(path.join(target:scriptdir(), "rtc_sources.hh.in"))
    local includes = ""
    for _,include in ipairs(target:get("cheriot.clock_source_includes")) do
      includes = includes .. "#include <" .. include .. ">\n"
    end
    local types = ""
    for _,typeName in ipairs(target:get("cheriot.clock_source_types")) do
      types = types .. typeName .. ", "
    end
    local contents = template:gsub("@include_implementations@", includes)
    contents = contents:gsub("@clock_sources@", types)
    local outPath = path.join(target:targetdir(), "rtc_sources.hh");
    try
    {
      function()
        -- Try reading the file and comparing
        local oldContents = io.readfile(outPath)
        if oldContents == contents then return end
        io.writefile(outPath, contents)
      end
    , { catch = function()
          -- If that threw an exception, just write the file
          io.writefile(outPath, contents)
        end
      }
    }
  end)
  on_load(function(target)
    target:values_set("shared_objects", { wall_clock_time = 24 }, {expand = false})
    target:add('cxflags', format("-I%s", target:targetdir()), {force =
    true})
  end)

