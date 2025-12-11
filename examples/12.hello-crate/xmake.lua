-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Hello World")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board", function()
	set_default("sail")
end)

compartment("run_rust")
	add_rules("cheriot.compartment")
	add_rules("cheriot.rust.crate")

	add_deps("freestanding", "string", "crt", "cxxrt", "atomic_fixed", "compartment_helpers", "debug", "softfloat")
	add_deps("message_queue", "locks", "event_group", "cheriot.allocator")
	add_deps("stdio")
	add_deps("strtol")
	add_files("run-rust.cc")
	add_files("./hello_world/Cargo.toml", { sourcekind = "cheriot_rust_crate" })

-- Firmware image for the example.
firmware("rust-runner")
  add_deps("run_rust")
  on_load(function(target)
  	target:values_set("board", "$(board)")
  	target:values_set("threads", {
  		{
  			compartment = "run_rust",
  			priority = 1,
  			entry_point = "run_rust",
            stack_size = 0x1F00,
  			trusted_stack_frames = 1,
  		},
  	}, { expand = false })
  end)
