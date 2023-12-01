includes("../atomic")

library("locks")
  add_deps("atomic4")
  add_files("locks.cc", "semaphore.cc")
