-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

debugOption("cxxrt")

library("cxxrt")
  add_rules("cherimcu.component-debug")
  add_files("guard.cc")
