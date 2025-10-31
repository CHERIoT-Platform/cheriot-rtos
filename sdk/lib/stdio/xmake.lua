library("stdio")
  add_deps("string")
  set_default(false)
  add_files("printf.cc")
  add_options("print-doubles")
  on_load(function(target)
    if get_config("print-doubles") then
      target:add('deps', "softfloat64compare")
      target:add('deps', "softfloat64convert")
      target:add('deps', "softfloat64add")
      target:add('deps', "softfloat64sub")
      target:add('deps', "softfloat64mul")
    else
      target:add('defines', "CHERIOT_NO_DOUBLE_PRINTING")
    end
  end)
