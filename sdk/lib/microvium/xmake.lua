-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

includes("../freestanding", "../string")

library("microvium")
  set_default(false)
  add_deps("freestanding", "string")
  add_files("../../third_party/microvium/dist-c/microvium.c")
  add_includedirs("../../include/microvium", ".")
  add_defines("CHERIOT_NO_AMBIENT_MALLOC")
