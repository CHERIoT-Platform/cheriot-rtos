#pragma once
#include <compartment-macros.h>
#include <ds/xoroshiro.h>
#include <interrupt.h>
#include <platform/concepts/entropy.h>
#include <riscvreg.h>

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(revokerInterruptEntropy,
                                        InterruptName::EthernetReceiveInterrupt,
                                        true,
                                        false)

/**
 * A simple entropy source.  This wraps a few weak entropy sources to seed a
 * PRNG.  It is absolutely not secure and should not be used for anything that
 * depends on cryptographically secure random numbers!  Unfortunately, there is
 * nothing on the Arty A7 instantiation of CHERIoT SAFE that can be used as a
 * secure entropy source.
 */
class EntropySource
{
	ds::xoroshiro::P128R64 prng;

	public:
	using ValueType = uint64_t;

	/// Definitely not secure!
	static constexpr bool IsSecure = false;

	/// Constructor, tries to generate an independent sequence of random numbers
	EntropySource()
	{
		reseed();
	}

	/// Reseed the PRNG
	void reseed()
	{
		// Start from a not very random seed
		uint64_t seed = rdcycle64();
		prng.set_state(seed, seed >> 24);
		uint32_t interrupts =
		  *interrupt_futex_get(STATIC_SEALED_VALUE(revokerInterruptEntropy));
		// Permute it with another not-very-random number
		for (uint32_t i = 0; i < ((interrupts & 0xff00) >> 8); i++)
		{
			prng.long_jump();
		}
		for (uint32_t i = 0; i < (interrupts & 0xff); i++)
		{
			prng.jump();
		}
		// At this point, our random number is in a fairly predictable state,
		// but with a fairly low probability of being the same predictable
		// state as before.
	}

	ValueType operator()()
	{
		return prng();
	}
};

static_assert(IsEntropySource<EntropySource>,
              "EntropySource must be an entropy source");
