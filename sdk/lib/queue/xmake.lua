-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

library("message_queue_library")
  set_default(false)
  add_files("queue.cc")

compartment("message_queue")
  set_default(false)
  add_files("queue_compartment.cc")
