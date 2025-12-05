#pragma once
#include <platform/concepts/entropy.h>

using EntropySource = TrivialInsecureEntropySource;

static_assert(IsEntropySource<EntropySource>,
              "EntropySource must be an entropy source");
