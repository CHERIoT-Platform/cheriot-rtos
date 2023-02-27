-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

library("atomic")
  set_default(false)
  add_files("atomic1.cc",
            "atomic2.cc",
            "atomic4.cc",
            "atomic8.cc",
            "atomicn.cc")

library("atomic_fixed")
  set_default(false)
  add_files("atomic1.cc",
            "atomic2.cc",
            "atomic4.cc",
            "atomic8.cc")
