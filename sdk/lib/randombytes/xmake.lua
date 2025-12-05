-- Copyright CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

compartment("randombytes")
	add_deps("cxxrt")
	set_default(false)
	add_files("randombytes.cc")
