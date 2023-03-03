-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

library("microvium")
  add_files("../../third_party/microvium/dist-c/microvium.c")
  add_includedirs("../../include/microvium", ".")
  add_defines("CHERIOT_NO_AMBIENT_MALLOC")
