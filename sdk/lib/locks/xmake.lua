includes("../atomic")

debugOption("locks")

library("locks")
  add_rules("cheriot.component-debug")
  add_deps("atomic4")
  add_files("locks.cc", "semaphore.cc")
  on_load(function (target)
	target:set('cheriot.debug-name', "locks")
  end)
