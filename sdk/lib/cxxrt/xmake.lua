-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

debugOption("cxxrt")

library("cxxrt")
  set_default(false)
  add_deps("locks")
  add_rules("cheriot.component-debug")
  add_files("guard.cc")
