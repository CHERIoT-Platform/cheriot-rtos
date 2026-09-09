compartment("cancellation_token")
  set_default(false)
  add_deps("atomic4", "cancellation_token_fast_path")
  add_files("cancellation.cc")

library("cancellation_token_fast_path")
  set_default(false)
  add_deps("atomic4")
  add_files("cancellation-fast-path.cc")
