-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

-- Put 4-byte atomics in a separate library because locks all use 32-bit
-- futexes but don't need anything else
library("atomic1")
  set_default(false)
  add_files("atomic1.cc")
library("atomic2")
  set_default(false)
  add_files("atomic2.cc")
library("atomic4")
  set_default(false)
  add_files("atomic4.cc")
library("atomic8")
  set_default(false)
  add_files("atomic8.cc")

target("atomic_fixed")
  set_default(false)
  set_kind("phony")
  add_deps("atomic1", "atomic2", "atomic4", "atomic8")

library("atomic")
  set_default(false)
  add_deps("atomic_fixed")
  add_files("atomicn.cc")

