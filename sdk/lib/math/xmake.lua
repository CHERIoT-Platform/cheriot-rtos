-- Copyright CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

function mathlib(name)
    library(name)
        set_default(false)
        add_includedirs("../../third_party/msun/include", ".")
        add_cxflags("-include " .. path.join(os.scriptdir(), "math.h"))
        add_cxflags("-include " .. path.join(os.scriptdir(), "compat.h"))
end

function add_msun_file(file)
    add_files(path.join("../../third_party/msun", file))
end

mathlib("math32")
    add_msun_file("e_expf.c")
    add_msun_file("e_logf.c")
    add_msun_file("e_log2f.c")
    add_msun_file("e_log10f.c")
    add_msun_file("e_sqrtf.c")
    add_msun_file("s_exp2f.c")
    add_msun_file("s_floorf.c")
    add_msun_file("s_fmaximum_numf.c")
    add_msun_file("s_fminimum_numf.c")
    add_msun_file("s_roundf.c")
    add_msun_file("s_ceilf.c") -- untested

mathlib("math64")
    add_msun_file("e_exp.c")
    add_msun_file("e_log.c")
    add_msun_file("e_log2.c")
    add_msun_file("e_log10.c")
    add_msun_file("e_sqrt.c")
    add_msun_file("s_exp2.c")
    add_msun_file("s_floor.c")
    add_msun_file("s_fmaximum_num.c") -- untested
    add_msun_file("s_fminimum_num.c") -- untested
    add_msun_file("s_round.c")
    add_msun_file("s_ceil.c")

target("math")
    set_default(false)
	set_kind("phony")
    add_deps("math32")
    add_deps("math64")
