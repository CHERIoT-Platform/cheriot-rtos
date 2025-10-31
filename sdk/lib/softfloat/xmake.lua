-- Copyright CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

local softfloatdir = os.scriptdir()
local softfloatheader = path.join(softfloatdir, "softfloat.h")

function softfloat_library(size, name)
	library("softfloat" .. tostring(size) .. name)
		set_default(false)
		add_cxflags("-include " .. softfloatheader)
		add_files("../../third_party/compiler-rt/fp_mode.c")
		add_defines("__SOFTFP__")
end

function softfloat_op_library(op)
	softfloat_library(32, op)
		add_files("../../third_party/compiler-rt/"..op.."sf3.c")
	softfloat_library(64, op)
		add_files("../../third_party/compiler-rt/"..op.."df3.c")
end


softfloat_op_library("add")
softfloat_op_library("sub")
softfloat_op_library("mul")
softfloat_op_library("div")

softfloat_library(32, "neg")
	add_files("../../third_party/compiler-rt/negsf2.c")
softfloat_library(64, "neg")
	add_files("../../third_party/compiler-rt/negdf2.c")

softfloat_library(32, "compare")
	add_files("../../third_party/compiler-rt/comparesf2.c")
softfloat_library(64, "compare")
	add_files("../../third_party/compiler-rt/comparedf2.c")


-- Conversions between 32-bit and 64-bit floats.
-- Not part of either the 32-bit or 64-bit combined library because it doesn't
-- make sense unless you use both.
softfloat_library(3264, "convert")
	add_files("../../third_party/compiler-rt/truncdfsf2.c")
	add_files("../../third_party/compiler-rt/extendsfdf2.c")
	add_files("../../third_party/compiler-rt/fixunsdfsi.c")
	add_files("../../third_party/compiler-rt/floatunsidf.c")

softfloat_library(32, "convert")
	add_files("../../third_party/compiler-rt/fixsfsi.c")
	add_files("../../third_party/compiler-rt/fixsfdi.c")
	add_files("../../third_party/compiler-rt/fixunssfsi.c")
	add_files("../../third_party/compiler-rt/fixunssfdi.c")
	add_files("../../third_party/compiler-rt/floatsisf.c")
	add_files("../../third_party/compiler-rt/floatdisf.c")
	add_files("../../third_party/compiler-rt/floattisf.c")
	add_files("../../third_party/compiler-rt/floatunsisf.c")
	add_files("../../third_party/compiler-rt/floatundisf.c")
	add_files("../../third_party/compiler-rt/floatuntisf.c")
	add_deps("softfloat64mul")
	add_deps("softfloat64add")
	add_deps("softfloat3264convert")


softfloat_library(64, "convert")
	add_files("../../third_party/compiler-rt/fixdfsi.c")
	add_files("../../third_party/compiler-rt/fixdfdi.c")
	add_files("../../third_party/compiler-rt/fixunsdfdi.c")
	add_files("../../third_party/compiler-rt/floatsidf.c")
	add_files("../../third_party/compiler-rt/floatdidf.c")
	add_files("../../third_party/compiler-rt/floattidf.c")
	add_files("../../third_party/compiler-rt/floatundidf.c")
	add_files("../../third_party/compiler-rt/floatuntidf.c")
	add_deps("softfloat3264convert")

softfloat_library(32, "pow")
	add_files("../../third_party/compiler-rt/powisf2.c")
softfloat_library(64, "pow")
	add_files("../../third_party/compiler-rt/powidf2.c")


softfloat_library(32, "complex")
	add_files("../../third_party/compiler-rt/mulsc3.c")
	add_files("../../third_party/compiler-rt/divsc3.c")
softfloat_library(64, "complex")
	add_files("../../third_party/compiler-rt/muldc3.c")
	add_files("../../third_party/compiler-rt/divdc3.c")


target("softfloat32")
	set_default(false)
	set_kind("phony")
	add_deps("softfloat32add")
	add_deps("softfloat32sub")
	add_deps("softfloat32mul")
	add_deps("softfloat32div")
	add_deps("softfloat32neg")
	add_deps("softfloat32convert")
	add_deps("softfloat32compare")

target("softfloat64")
	set_default(false)
	set_kind("phony")
	add_deps("softfloat64add")
	add_deps("softfloat64sub")
	add_deps("softfloat64mul")
	add_deps("softfloat64div")
	add_deps("softfloat64neg")
	add_deps("softfloat64convert")
	add_deps("softfloat64compare")

library("softfloat")
	set_default(false)
	add_cxflags("-include " .. softfloatheader)
	add_files("../../third_party/compiler-rt/fp_mode.c")
	add_deps("softfloat32")
	add_deps("softfloat64")
	add_deps("softfloat3264convert")

target("softfloatall")
	set_default(false)
	set_kind("phony")
	add_deps("softfloat")
	add_deps("softfloat32pow")
	add_deps("softfloat64pow")
	add_deps("softfloat32complex")
	add_deps("softfloat64complex")
