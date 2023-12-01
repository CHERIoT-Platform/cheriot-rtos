-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

debugOption("cxxrt")

library("cxxrt")
  add_rules("cheriot.component-debug")
  add_files("guard.cc")
