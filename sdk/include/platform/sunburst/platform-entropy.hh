#pragma once
#include <interrupt.h>
#include <platform/concepts/entropy.h>

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(ethernetInterruptEntropy,
                                        InterruptName::EthernetInterrupt,
                                        true,
                                        false)

/**
 * A simple entropy source.  This wraps a few weak entropy sources to seed a
 * PRNG.  It is absolutely not secure and should not be used for anything that
 * depends on cryptographically secure random numbers!  Unfortunately, there is
 * nothing on the Sonata instantiation of CHERIoT SAFE that can be used as a
 * secure entropy source.
 */
struct EntropySource : public TrivialInsecureEntropySource
{
	/// Constructor, tries to generate an independent sequence of random numbers
	EntropySource()
	{
		reseed();
	}

	/// Reseed the PRNG
	void reseed()
	{
		uint32_t interrupts =
		  *interrupt_futex_get(STATIC_SEALED_VALUE(ethernetInterruptEntropy));
		TrivialInsecureEntropySource::reseed(interrupts);
	}
};

static_assert(IsEntropySource<EntropySource>,
              "EntropySource must be an entropy source");
