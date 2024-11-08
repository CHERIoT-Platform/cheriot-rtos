#!/usr/bin/env lua5.3
-- Copyright CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

local infile = io.input()

local labels = {}     -- label |-> style array
local label_outs = {} -- label |-> label |-> style
local label_ins = {}  -- label |-> label |-> ()

local label_irq_assume  = {}  -- label |-> { "deferred", "enabled" }
local label_irq_require = {}  -- label |-> { "deferred", "enabled" }

local exports = {}    -- label |-> ()

local lastlabels = {}

local function debug() end
if true then
  function debug(...)
    io.stderr:write(string.format("DBG %s\n",
                                  table.concat(table.pack(...), " ")))
  end
end

local function end_label(clear)
  -- At the end of a lable, if we haven't been told that we've assumed a
  -- different IRQ disposition than required on the way in, then inherit
  -- assumptions from requirements
  local lastlabel = lastlabels[#lastlabels]
  if label_irq_assume[lastlabel] == nil then
    label_irq_assume[lastlabel] =
     assert(label_irq_require[lastlabel],
      "Missing IRQ requirement for prior label, cannot inherit assumption")
  end

  if clear then lastlabels = {} end
end

local function found_label(label, where)
  assert(label)
  assert(where)

  assert(labels[label] == nil, label)
  labels[label] = { ("tooltip=%s"):format(where) }

  if label_ins[label] == nil then
    label_ins[label] = {}
  end

  if label_outs[label] == nil then
    label_outs[label] = {}
  end

  if #lastlabels > 0 then end_label(false) end

  table.insert(lastlabels, label)
  if #lastlabels > 2 then table.remove(lastlabels, 1) end
  assert(#lastlabels <= 2)
end

local function found_edge_from(from, to, style)
  assert(from)
  assert(to)

  debug("", "EDGE FROM", from, to)

  if label_outs[from] == nil then label_outs[from] = {} end
  label_outs[from][to] = {style}
end

local function found_edge_to(from, to)
  assert(from)
  assert(to)

  debug("", "EDGE TO", from, to)

  if label_ins[to] == nil then label_ins[to] = {} end
  label_ins[to][from] = true
end

local lineix = 1

-- Read and discard until we get to the good stuff
for line in infile:lines("*l") do
  debug("HUNT", line)
  if line:match(".globl __Z26compartment_switcher_entryz") then break end
  lineix = lineix + 1
end

local IRQ_dispositions =
  { ["any"] = true
  , ["deferred"] = true
  , ["enabled"] = true
  }

-- And here we go
for line in infile:lines("*l") do
  local label

  debug("LINE", line)

  -- numeric labels are suppresed
  label = line:match("^(%d+):$")
  if label then
    debug("", "Numeric label")
    goto nextline
  end

  -- local labels
  label = line:match("^(%.L.*):$")
  if label then
    debug("", "Local label")
    found_label(label, lineix)
    if #lastlabels > 1 then
      found_edge_to(lastlabels[#lastlabels-1], lastlabels[#lastlabels])
    end
    goto nextline
  end

  -- documentation-only labels
  label = line:match("^//(%.L.*):$")
  if label then
    debug("", "Documentation label", #lastlabels)
    found_label(label, lineix)

    -- Documentation labels are presumed to be fall-through and do not need the
    -- clutter of "FROM: above"
    assert(#lastlabels > 1)
    found_edge_from(lastlabels[#lastlabels-1], lastlabels[#lastlabels])
    found_edge_to(lastlabels[#lastlabels-1], lastlabels[#lastlabels])

    -- Documentation labels are presumed to inherit the IRQ disposition from
    -- "above" as well.
    label_irq_require[lastlabels[#lastlabels]] =
      assert(label_irq_assume[lastlabels[#lastlabels-1]],
             "Missing IRQ disposition for prior label")

    goto nextline
  end

  -- other global labels
  label = line:match("^([%w_]*):$")
  if label then
    debug("", "global label")
    found_label(label, lineix)
    if #lastlabels > 1 then
      found_edge_to(lastlabels[#lastlabels-1], lastlabels[#lastlabels])
    end
    exports[label] = true
    goto nextline
  end

  -- [cm]ret clear the last label, preventing fallthru
  if line:match("^%s+[cm]ret$") then
    debug("", "[cm]ret")
    end_label(true)
    goto nextline
  end

  -- unconditonal jumps add an edge and clear the last label, since we cannot
  -- be coming "FROM: above"
  label = line:match("^%s+j%s+(%g*)$")
  if label then
    debug("", "Jump")
    assert(#lastlabels > 0)
    found_edge_to(lastlabels[#lastlabels], label)
    end_label(true)
    goto nextline
  end

  -- branches add edges to local labels
  label = line:match("^%s+b.*,%s*(%.L%g*)$")
  if label then
    debug("", "Branch")
    assert(#lastlabels > 0)
    found_edge_to(lastlabels[#lastlabels], label)
    goto nextline
  end

  -- OK, now hunt for structured comments.
  line = line:match("^%s*%*%s*(%S.*)$") or line:match("^%s*//%s*(%S.*)$")
  if not line then goto nextline end

  -- "FROM: malice" annotations promote lastlabel to being exported
  label = line:match("^FROM:%s+malice%s*")
  if label then
    debug("", "Malice", #lastlabels)
    assert(#lastlabels > 0)
    exports[lastlabels[#lastlabels]] = true
    goto nextline
  end

  -- "FROM: above"
  label = line:match("^FROM:%s+above%s*")
  if label then
    debug("", "Above")
    assert(#lastlabels > 1)
    found_edge_from(lastlabels[#lastlabels-1], lastlabels[#lastlabels])

    -- "FROM: above" implies IRQ requirements, too
    label_irq_require[lastlabels[#lastlabels]] =
      assert(label_irq_assume[lastlabels[#lastlabels-1]],
             "Missing IRQ disposition for prior label")

    goto nextline
  end

  -- "IFROM: above"
  label = line:match("^IFROM:%s+above%s*")
  if label then
    debug("", "Above")
    assert(#lastlabels > 1)
    found_edge_from(lastlabels[#lastlabels-1],
                    lastlabels[#lastlabels],
                    "style=dashed")

    -- "IFROM: above" implies IRQ requirements, too
    label_irq_require[lastlabels[#lastlabels]] =
      assert(label_irq_assume[lastlabels[#lastlabels-1]],
             "Missing IRQ disposition for prior label")

    goto nextline
  end

  -- "FROM: cross-call" no-op
  label = line:match("^FROM:%s+cross%-call%s*")
  if label then
    goto nextline
  end

  -- "FROM: interrupt" no-op
  label = line:match("^FROM:%s+interrupt%s*")
  if label then
    goto nextline
  end

  -- "FROM: error" no-op
  label = line:match("^FROM:%s+interrupt%s*")
  if label then
    goto nextline
  end

  -- "FROM: $symbol"
  label = line:match("^FROM:%s+(%S+)%s*")
  if label then
    debug("", "FROM", lastlabels[#lastlabels], label)
    assert(#lastlabels > 0)
    found_edge_from(label, lastlabels[#lastlabels])
    goto nextline
  end

  -- "IFROM: $symbol"
  label = line:match("^IFROM:%s+(%S+)%s*")
  if label then
    debug("", "IFROM", lastlabels[#lastlabels], label)
    assert(#lastlabels > 0)
    found_edge_from(label, lastlabels[#lastlabels], "style=dashed")
    goto nextline
  end

  -- "IFROM: $symbol"
  label = line:match("^ITO:%s+(%S+)%s*")
  if label then
    debug("", "ITO", lastlabels[#lastlabels], label)
    assert(#lastlabels > 0)
    found_edge_to(lastlabels[#lastlabels], label, "style=dashed")
    goto nextline
  end

  -- "IRQ ASSUME: {deferred,enabled}"
  label = line:match("^IRQ ASSUME:%s+(%S+)%s*")
  if label then
    debug("", "IRQ ASSUME", lastlabels[#lastlabels], label)
    assert (IRQ_dispositions[label])
    label_irq_assume[lastlabels[#lastlabels]] = label
    goto nextline
  end

  -- "IRQ REQURE: {deferred,enabled}"
  label = line:match("^IRQ REQUIRE:%s+(%S+)%s*")
  if label then
    debug("", "IRQ", lastlabels[#lastlabels], label)
    assert (IRQ_dispositions[label])
    label_irq_require[lastlabels[#lastlabels]] = label
    goto nextline
  end

  -- Stop reading when we get to the uninteresting library exports
  if line:match("Switcher%-exported library functions%.$") then
    debug("", "Break")
    break
  end

  ::nextline::
  lineix = lineix + 1
end

-- Take adjacency matrix representation and add lists.
label_inls = {}
label_outls = {}
for focus, _ in pairs(labels) do
  label_inls[focus] = {}
  label_outls[focus] = {}

  for from, _ in pairs(label_ins[focus]) do
    assert(labels[from])
    assert(label_outs[from][focus],
      string.format("%s in from %s but no out edge", focus, from))
    assert(   label_irq_require[focus] == "any"
           or label_irq_assume[from] == label_irq_require[focus],
           string.format("IRQ-invalid arc from %s (%s) to %s (%s)",
             from, label_irq_assume[from], focus, label_irq_require[focus]))

    table.insert(label_inls[focus], from)
  end
  for to, _ in pairs(label_outs[focus]) do
    assert(labels[to])
    assert(label_ins[to][focus],
      string.format("%s out to %s but no in edge", focus, to))
    assert(   label_irq_require[to] == "any"
           or label_irq_assume[focus] == label_irq_require[to],
           string.format("IRQ-invalid arc from %s (%s) to %s (%s)",
             focus, label_irq_assume[focus], to, label_irq_require[to]))

    table.insert(label_outls[focus], to)
  end
end

local function render_exports(...)
  local args = {...}

  local nexports = 0
  for export, _ in pairs(exports) do nexports = nexports + 1 end
  assert(nexports == #args,
         ("Wrong number of exports: %d != %d"):format(nexports, #args))

  print(" { rank=min; edge [style=invis]; ")

  for _, export in ipairs(args) do
    assert(exports[export], "Purported export isn't")
    print("", ("%q"):format(export), ";")
  end

  for i = 1, #args-1 do
    print("", ("%q -> %q ;"):format(args[i], args[i+1]))
  end

  print(" }")
end

print("digraph switcher {")

-- Put all our exports at the top of the graph, in a fixed order.
render_exports(".Lhandle_error_handler_return",
               "exception_entry_asm",
               "switcher_after_compartment_call",
               "__Z26compartment_switcher_entryz")

for from, from_params in pairs(labels) do

  if exports[from] then
    table.insert(from_params, "shape=box")
  elseif #label_inls[from] == 1 then
    -- Indegree 1, this is either an exit, a decision node, or just a waypoint
    if #label_outls[from] == 0 then
      -- Exit
      table.insert(from_params, "shape=octagon")
    elseif #label_outls[from] == 1 then
      -- Waypoint
      table.insert(from_params, "shape=oval")
    else
      -- Decision
      table.insert(from_params, "shape=trapezium")
    end
  else
    if #label_outls[from] == 0 then
      -- Exit
      table.insert(from_params, "shape=octagon")
    elseif #label_outls[from] == 1 then
      table.insert(from_params, "shape=invtrapezium")
    else
      table.insert(from_params, "shape=hexagon")
    end
  end

  table.insert(from_params,
    ({ ["any"] = "fontname=\"Times\""
    , ["deferred"] = "fontname=\"Times-Bold\""
    , ["enabled"] = "fontname=\"Times-Italic\""
    })[label_irq_assume[from]])

  if    from:match("^%.?L?switch")
     or from == "__Z26compartment_switcher_entryz" then
    table.insert(from_params, "style=filled")
    table.insert(from_params, "fillcolor=cyan")
  elseif from:match("^%.?L?exception") then
    table.insert(from_params, "style=filled")
    table.insert(from_params, "fillcolor=red")
  elseif from:match("^%.?L?handle") then
    table.insert(from_params, "style=filled")
    table.insert(from_params, "fillcolor=orange")
  end

  print("", ("%q [%s];"):format(from, table.concat(from_params,",")))

  for to, style in pairs(label_outs[from]) do
    print("", ("%q"):format(from),
              ("-> %q [%s];"):format(to, table.concat(style,",")))
  end
  print("")
end

print("}")
