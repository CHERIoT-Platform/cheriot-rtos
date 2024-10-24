-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

includes("../freestanding", "../compartment_helpers", "../atomic")

library("message_queue_library")
  set_default(false)
  add_deps("freestanding", "compartment_helpers", "atomic4")
  add_files("queue.cc")

compartment("message_queue")
  add_deps("unwind_error_handler")
  add_deps("message_queue_library")
  set_default(false)
  add_files("queue_compartment.cc")
