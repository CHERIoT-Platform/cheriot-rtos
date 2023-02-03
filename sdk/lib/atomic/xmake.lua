-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

library("atomic")
  add_files("atomic1.cc",
            "atomic2.cc",
            "atomic4.cc",
            "atomic8.cc",
            "atomicn.cc")

library("atomic-fixed")
  add_files("atomic1.cc",
            "atomic2.cc",
            "atomic4.cc",
            "atomic8.cc")
