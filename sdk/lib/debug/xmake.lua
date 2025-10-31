library("debug")
  set_default(false)
  add_options("print-floats", "print-doubles")
  add_files("debug.cc")
  on_load(function(target)
    if get_config("print-floats") then
      target:add('deps', "softfloat32compare")
      target:add('deps', "softfloat32convert")
      target:add('deps', "softfloat32add")
      target:add('deps', "softfloat32sub")
      target:add('deps', "softfloat32mul")
    else
      target:add('defines', "CHERIOT_NO_FLOAT_PRINTING")
    end
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
