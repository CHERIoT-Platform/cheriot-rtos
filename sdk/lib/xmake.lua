option("print-floats")
	set_default(false)
	set_description("Enable printing float values in the debug and stdio libraries")
	set_showmenu(true)

option("print-doubles")
	set_default(false)
	set_description("Enable printing double values in the debug and stdio libraries")
	set_showmenu(true)

includes(
	"atomic",
	"compartment_helpers",
	"crt",
	"cxxrt",
	"debug",
	"event_group",
	"freestanding",
	"locks",
	"microvium",
	"queue",
	"softfloat",
	"stdio",
	"string",
	"strtol",
	"thread_pool",
	"unwind_error_handler")
