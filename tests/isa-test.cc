// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "ISA"
#include "tests.hh"
#include <debug.hh>
#include <optional>
#include <priv/riscv.h>
#include <timeout.hh>
#include <token.h>
#include <unwind.h>

using namespace CHERI;

std::optional<volatile size_t>         expectedMCause;
std::optional<volatile CauseCode>      expectedCauseCode;
std::optional<volatile ptraddr_t>      expectedErrorPC;
std::optional<volatile RegisterNumber> expectedRegisterNumber;
volatile int                           crashes = 0;

/* Value of mcause on CHERI exception */
constexpr size_t MCauseCheri = priv::MCAUSE_CHERI;

extern "C" enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Test saw error for PCC {} mcause {} mtval {}",
	          frame->pcc,
	          mcause,
	          mtval);
	TEST(expectedMCause.has_value(), "Unexpected Crash!");
	TEST_EQUAL(mcause, *expectedMCause, "wrong mcause");
	if (mcause == MCauseCheri)
	{
		auto [cheriCause, registerNumber] = extract_cheri_mtval(mtval);
		debug_log("CHERI cause: {} {}", cheriCause, registerNumber);
		TEST(expectedCauseCode.has_value(),
		     "CHERI mcause without expectedCauseCode (test error)");
		TEST_EQUAL(cheriCause, *expectedCauseCode, "wrong cheri cause");
		/* Reset expectedCauseCode */
		expectedCauseCode = std::nullopt;

		/* We generally do not know the expected register number because it is
		 * chosen by compiler. The exception is PCC. */
		if (expectedRegisterNumber.has_value())
		{
			TEST_EQUAL(
			  registerNumber, *expectedRegisterNumber, "wrong register num");
			expectedRegisterNumber = std::nullopt;
		}
	}
	/* Reset expectedMCause so that we fail on any unexpected faults */
	expectedMCause = std::nullopt;

	/* If we know the expected error PC, check it is reported correctly. */
	if (expectedErrorPC.has_value())
	{
		Capability framePCCCap = {frame->pcc};
		TEST_EQUAL(framePCCCap.address(), *expectedErrorPC, "wrong ErrorPC");
		expectedErrorPC = std::nullopt;
	}

	crashes = crashes + 1;

	cleanup_unwind();
	TEST(false, "should be unreachable");
	return ForceUnwind;
}

namespace
{
	/**
	 * A global used in various tests when need to construct a pointer to a
	 * global.
	 */
	int myGlobalInt = 0;
	/**
	 * A global int* used in various tests when we need a global location that
	 * can store a capability. We make it an array of two so that we can use it
	 * for misaligned tests and remain in bounds.
	 */
	int *myGlobalIntPointer[2] = {NULL, NULL};
	/**
	 * We will set this to a value returned by `token_key_new` so that we have
	 * a sealing capability for use in tests.
	 */
	SKey sealingCapability = NULL;
	/**
	 * We will set this to a value returned by `token_sealed_alloc` so that we
	 * have a sealed data capability for use in tests.
	 */
	CHERI_SEALED(void *) sealedDataCapability = NULL;

	/*
	 * The usual set of permissions associated with globals.
	 */
	constexpr PermissionSet GlobalPermissions = {
	  Permission::Global,
	  Permission::Load,
	  Permission::Store,
	  Permission::LoadMutable,
	  Permission::LoadGlobal,
	  Permission::LoadStoreCapability};

	class SetBoundsTestCase
	{
		const ptraddr_t RequestedBase;
		const ptraddr_t RequestedLength;
		const ptraddr_t ExpectedBase;
		const ptraddr_t ExpectedTop;
		const ptraddr_t ExpectedTopRoundDown;

		public:
		SetBoundsTestCase(ptraddr_t aRequestedBase,
		                  ptraddr_t aRequestedLength,
		                  ptraddr_t anExpectedBase,
		                  ptraddr_t anExpectedTop,
		                  ptraddr_t anExpectedTopRoundDown)
		  : RequestedBase(aRequestedBase),
		    RequestedLength(aRequestedLength),
		    ExpectedBase(anExpectedBase),
		    ExpectedTop(anExpectedTop),
		    ExpectedTopRoundDown(anExpectedTopRoundDown)
		{
		}

		void run_test()
		{
			debug_log("Set bounds test reqBase={} reqLen={}",
			          RequestedBase,
			          RequestedLength);
			// Sanity checks on the test case TODO check statically?
			TEST(ExpectedBase <= ExpectedTop,
			     "Invalid test case: exp_base > exp_top");
			TEST(ExpectedBase <= RequestedBase,
			     "Invalid test case: exp_base > req_base");
			TEST(ExpectedTop >= RequestedBase + RequestedLength,
			     "Invalid test case:  exp_top < req_top");
			TEST(ExpectedTopRoundDown <= (RequestedBase + RequestedLength),
			     "Invalid test case: exp_top_rndwn > req_base+req_len");

			/*
			 * Attempt set bounds with requested bounds. By starting with NULL
			 * we can check that we get the bounds we expect but can't validate
			 * that bounds checks / tag clearing are performed correctly. To do
			 * that we'd need a very large capability to start with.
			 */
			Capability<uint8_t> c{NULL};
			c += RequestedBase; // Technically UB, but seems to work.
			c.bounds().set_inexact(RequestedLength);
			debug_log("Validating set bounds result: {}", c);
			TEST_EQUAL(c.base(), ExpectedBase, "Unexpected base");
			TEST_EQUAL(c.top(), ExpectedTop, "Unexpected top");
			Capability<uint8_t> c2{NULL};
			c2 += RequestedBase;
			c2.bounds().set_inexact_at_most(RequestedLength);
			debug_log("Validating set bounds rnddwn: {}", c2);
			TEST_EQUAL(c2.base(), RequestedBase, "Unexpected base2");
			TEST_EQUAL(c2.top(), ExpectedTopRoundDown, "Unexpected top2");
		}
	};

	SetBoundsTestCase setBoundsCases[] = {
	  SetBoundsTestCase(0x100, 0x1a, 0x100, 0x11a, 0x11a),  // easy case
	  SetBoundsTestCase(0x700, 0x1aa, 0x700, 0x8aa, 0x8aa), // easy case
	  SetBoundsTestCase(0xd01,
	                    0x3ff,
	                    0xd00,
	                    0x1100, // exact top, inexact base, e bump
	                    0xd01 + 0x1ff),
	  SetBoundsTestCase(0x9f3,
	                    0x7ff,
	                    0x000009F0,
	                    0x000011F8,
	                    0x9F3 + 0x1ff), // T-B = 0x201
	  SetBoundsTestCase(0xbeef9793,
	                    0x3fb,
	                    0xBEEF9792,
	                    0xBEEF9B8E,
	                    0xbeef9793 +
	                      0x1ff), // monotonicity failure regression part i
	  SetBoundsTestCase(0xbeef9792,
	                    0x3fc,
	                    0xBEEF9792,
	                    0xBEEF9B8E,
	                    0xBEEF9B8E), // monotonicity failure regression part ii
	  SetBoundsTestCase(0x4fffe1ff,
	                    0x3fe,
	                    0x4FFFE1FC,
	                    0x4FFFE600,
	                    0x4fffe1ff + 0x1ff), // requires T increment
	  SetBoundsTestCase(0xe7e96c2f,
	                    0x3ff,
	                    0xE7E96C2C,
	                    0xE7E97030,
	                    0xE7E96C2f + 0x1ff), // requires e bump and T increment
	};

	void test_set_bounds()
	{
		int x = 0;
		for (auto testCase : setBoundsCases)
		{
			testCase.run_test();
		}
	}

	template<typename T>
	void test_restrict_capability(Capability<T> capability, PermissionSet mask)
	{
		PermissionSet requestedPermissions(mask);
		requestedPermissions &= capability.permissions();
		PermissionSet expectedPermissions =
		  requestedPermissions.to_representable();
		capability.permissions() &= mask;
		TEST_EQUAL(PermissionSet(capability.permissions()),
		           expectedPermissions,
		           "permissions did not match expected");
	}

	void test_and_perms()
	{
		int              myLocal = 0;
		const Capability CapabilityToLocal{&myLocal};
		const Capability CapabilityToGlobal{myGlobalIntPointer};
		const Capability CapabilityToFunction{
		  reinterpret_cast<const void *>(&test_and_perms)};
		const Capability SealingCapability{sealingCapability};
		debug_log("Checking and perms results...");
		// Test all possible masks on our 'root' capabilities. Our sealing
		// capability doesn't include the user perm so we exclude that for now
		// meaning we only test the low 11 bits of the permissions field.
		for (uint32_t p = 0; p <= 0x7ff; p++)
		{
			auto permissionMask = PermissionSet::from_raw(p);
			test_restrict_capability(CapabilityToLocal, permissionMask);
			test_restrict_capability(CapabilityToGlobal, permissionMask);
			test_restrict_capability(CapabilityToFunction, permissionMask);
			test_restrict_capability(SealingCapability, permissionMask);
		}
	}

	/**
	 * Returns the return capability passed to this function by the caller. To
	 * ensure this works we need to disable inlining and tail call optimisation.
	 */
	__noinline __attribute__((not_tail_called)) void *get_return_address()
	{
		return __builtin_return_address(0);
	}

	/**
	 * Returns an interrupt disabling return sentry by calling the above helper
	 * from an interrupts disabled context.
	 */
	[[cheri::interrupt_state(disabled)]] void *
	get_interrupts_disabled_return_sentry()
	{
		return get_return_address();
	}

	/**
	 * Returns an interrupt enabling return sentry by calling the above helper
	 * from an interrupts enabled context.
	 */
	[[cheri::interrupt_state(enabled)]] void *
	get_interrupts_enabled_return_sentry()
	{
		return get_return_address();
	}

	void test_sentries()
	{
		// XXX the following produces a linker error:
		// error: ld.lld: error: Compartment 'isa_test' imports library function
		// '__library_export_isa_test__ZN12_GLOBAL__N_129get_interrupts_enabled_sentryEv'
		// as cross-compartment call possible compiler / linker bug?

		// Capability interruptsEnabledSentry =
		// {get_interrupts_enabled_return_sentry}; debug_log("interrupts enabled
		// sentry {}", reinterpret_cast<void*>(interruptsEnabledSentry.get()));
		// TEST(interruptsEnabledSentry.type() == 3,
		//      "Expected type 3 but got {}",
		//      interruptsEnabledSentry.type());

		// Capability interruptsDisabledSentry =
		// {&get_interrupts_disabled_sentry}; debug_log("interrupts disabled
		// sentry {}", reinterpret_cast<void*>(interruptsDisabledSentry.get()));
		// TEST(interruptsDisabledSentry.type() == 2,
		//      "Expected type 2 but got {}",
		//      interruptsDisabledSentry.type());

		Capability interruptsEnabledReturnSentry =
		  get_interrupts_enabled_return_sentry();
		debug_log("interrupts enabled return sentry {}",
		          interruptsEnabledReturnSentry);
		TEST_EQUAL(interruptsEnabledReturnSentry.type(),
		           CheriSealTypeReturnSentryEnabling,
		           "Wrong type for enabling return sentry.");

		Capability interruptsDisabledReturnSentry =
		  get_interrupts_disabled_return_sentry();
		debug_log("interrupts disabled return sentry {}",
		          interruptsDisabledReturnSentry);
		TEST_EQUAL(interruptsDisabledReturnSentry.type(),
		           CheriSealTypeReturnSentryDisabling,
		           "Wrong type for disabling return sentry.");
	}

	/**
	 * Function that performs a store of capability `value` through `location`.
	 * We call this with values we expect to fault and want to know the address
	 * of the faulting instruction. Since it is so simple it will have no
	 * prelude so we can use the address of the function as the address of the
	 * store. Obviously we can't allow it to be inlined.
	 */
	__noinline void perform_store(int *value, int **location)
	{
		*location = value;
	}

	/**
	 * Function that performs a load of a capability through `location`. We call
	 * this with a capability that we expect to fault and want to know the
	 * address of the faulting instruction. Since it is so simple it will
	 * probably have no prelude so we can use the address of the function as the
	 * address of the load. Obviously we can't allow it to be inlined.
	 */
	__noinline int *perform_load(int *volatile *location)
	{
		return *location;
	}

	/**
	 * Type of (pointer to) Capability -> Capability function that 'filters' a
	 * capability. `do_load_test`, `do_store_test` and `do_jalr_test` take
	 * arguments of this type to allow modifying capabilities operands in
	 * flexible ways before attempting tests.
	 */
	template<class T>
	using CapabilityFilter = Capability<T> (*)(Capability<T>);

	/**
	 * A CapabilityFilter that simply returns its input i.e. a no-op / identity
	 * function. Used as default.
	 */
	template<class T>
	Capability<T> identity_filter(Capability<T> input)
	{
		return input;
	}

	/**
	 * A CapabilityFilter that returns its input with the tag cleared.
	 */
	template<class T>
	Capability<T> clear_tag_filter(Capability<T> input)
	{
		input.invalidate();
		return input;
	}

	/**
	 * A CapabilityFilter that returns a sealed capability. Since we don't have
	 * access to a sealing capability it can't actually seal its input so just
	 * returns one we made earlier using token_sealed_alloc. This is fine for
	 * our purposes as we only want it to trigger SealViolations.
	 */
	template<class T>
	Capability<T> get_sealed_capability_filter(Capability<T> inputCapability)
	{
		return reinterpret_cast<T *>(sealedDataCapability);
	}

	/**
	 * A CapabilityFilter that returns the input capability with permissions
	 * restricted to the given PermissionSet.
	 */
	template<PermissionSet Permissions, class T>
	Capability<T> permission_filter(Capability<T> inputCapability)
	{
		inputCapability.permissions() &= Permissions;
		return inputCapability;
	}

	/**
	 * A CapabilityFilter that returns its input with length set to one.
	 * This should be sufficient to trigger a BoundsViolation when we use it.
	 */
	template<class T>
	Capability<T> restrict_bounds_filter(Capability<T> inputCapability)
	{
		inputCapability.bounds() = 1;
		return inputCapability;
	}

	/**
	 * A CapabilityFilter that returns its input with the address incremented by
	 * one so that it will be misaligned.
	 */
	template<class T>
	Capability<T> misalign_filter(Capability<T> inputCapability)
	{
		Capability<uint8_t> byteCap = {
		  reinterpret_cast<uint8_t *>(inputCapability.get())};
		byteCap += 1;
		return {reinterpret_cast<T *>(byteCap.get())};
	}

	/**
	 * Run a test where we attempt a capability load via a capability to a
	 * global int* after passing it through `baseFilter`. Asserts that we
	 * receive CHERI fault `expectedFault`.
	 */
	void do_load_test(CauseCode               expectedFault,
	                  CapabilityFilter<int *> baseFilter,
	                  size_t                  anExpectedMCause = MCauseCheri)
	{
		Capability capToIntPointer = baseFilter(myGlobalIntPointer);
		expectedMCause             = anExpectedMCause;
		expectedCauseCode          = expectedFault;
		expectedErrorPC            = Capability{perform_load}.address();
		int previousCrashes        = crashes;
		// on_error with no handler will just skip the rest of the lambda
		// after an unwind, which is what we want.
		on_error([&]() { perform_load(capToIntPointer); });
		TEST(crashes == previousCrashes + 1,
		     "Expected load via {} to crash",
		     capToIntPointer);
	}

	void test_load_faults()
	{
		debug_log("Attempting to load via untagged capability");
		do_load_test(CauseCode::TagViolation, clear_tag_filter);

		debug_log("Attempting to load via sealed capability");
		do_load_test(CauseCode::SealViolation, get_sealed_capability_filter);

		debug_log("Attempting to load via store-only capability");
		do_load_test(CauseCode::PermitLoadViolation,
		             permission_filter<PermissionSet{Permission::Store}>);

		debug_log("Attempting to load via capability with too small bounds.");
		do_load_test(CauseCode::BoundsViolation, restrict_bounds_filter);

		debug_log("Attempting to load via misaligned capability.");
		do_load_test(
		  CauseCode::Invalid, misalign_filter, priv::MCAUSE_LOAD_MISALIGNED);
	}

	/**
	 * Run a test where we attempt a capability store of a local capability to a
	 * int into a global int*. The capability base for the store is passed
	 * through `baseFilter` and the stored capability through `dataFilter`.
	 * Optionally assert that we receive CHERI fault `expectedFault`. The fault
	 * is optional because certain faults should not occur if the stored
	 * capability is untagged. Because we are storing a local capability into a
	 * global it can only succeed if the stored capability has the tag cleared.
	 */
	void do_store_test(std::optional<CauseCode> expectedFault,
	                   CapabilityFilter<int *>  baseFilter = identity_filter,
	                   CapabilityFilter<int>    dataFilter = identity_filter,
	                   std::optional<size_t>    anExpectedMCause = MCauseCheri)
	{
		int        myLocalInt      = 0;
		Capability capToIntPointer = baseFilter(myGlobalIntPointer);
		Capability storeData       = dataFilter(&myLocalInt);
		bool       expectCrash     = anExpectedMCause.has_value();
		// set target memory to NULL so we can detect if a store took place
		myGlobalIntPointer[0] = NULL;
		if (expectCrash)
		{
			expectedMCause    = anExpectedMCause;
			expectedCauseCode = expectedFault;
			expectedErrorPC   = Capability{perform_store}.address();
		}
		int previousCrashes = crashes;
		on_error([&]() { perform_store(storeData, capToIntPointer); });
		TEST(crashes == previousCrashes + (expectCrash ? 1 : 0),
		     "{} store of {} via {} to crash",
		     expectCrash ? "Expected" : "Did not expect",
		     storeData,
		     capToIntPointer);
		// Check whether the store happened and, if it did, what it stored.
		Capability targetAfterStore {myGlobalIntPointer[0]};
		if (expectCrash) {
			TEST_EQUAL(targetAfterStore, NULL, "target memory modified after faulting store");
		} else {
			// all tests expect the tag to be cleared even if the store doesn't
			// fault, so invalidate storeData before comparing with the result.
			storeData.invalidate();
			TEST_EQUAL(targetAfterStore, storeData, "target memory did not match expected after non-faulting store");
		}
	}

	void test_store_faults()
	{
		// Faulting cases
		debug_log("Attempting to store via untagged capability");
		do_store_test(CauseCode::TagViolation, clear_tag_filter);

		debug_log("Attempting to store via sealed capability");
		do_store_test(CauseCode::SealViolation, get_sealed_capability_filter);

		debug_log("Attempting to store via read-only capability");
		do_store_test(CauseCode::PermitStoreViolation,
		              permission_filter<PermissionSet{Permission::Load}>);

		debug_log("Attempting to store capability via data only pointer");
		do_store_test(CauseCode::PermitStoreCapabilityViolation,
		              permission_filter<PermissionSet{Permission::Load,
		                                              Permission::Store}>);

		debug_log("Attempt to store capability via too small bounds");
		do_store_test(CauseCode::BoundsViolation, restrict_bounds_filter,
		              clear_tag_filter);

		debug_log("Attempt to store capability via misaligned pointer");
		do_store_test(std::nullopt,
		              misalign_filter,
		              clear_tag_filter,
		              priv::MCAUSE_STORE_MISALIGNED);

		// Non-faulting but special cases

		// Storing a local capability via a cap without store-local is
		// expected to succeed, but will clear the tag of the stored data
		debug_log("Attempting to store local capability in global");
		do_store_test(
		  std::nullopt,
		  permission_filter<PermissionSet{Permission::Load,
		                                  Permission::Store,
		                                  Permission::LoadStoreCapability}>,
		  identity_filter,
		  std::nullopt);

		debug_log(
		  "Attempting to store untagged capability via data only pointer");
		// Storing untagged cap via data-only cap is allowed.
		// Contrast with PermitStoreCapabilityViolation case above.
		do_store_test(
		  std::nullopt,
		  permission_filter<PermissionSet{Permission::Load, Permission::Store}>,
		  clear_tag_filter,
		  std::nullopt);
	}

	void test_restricted_load(bool          invalidateData,
	                          PermissionSet capPermissions,
	                          bool          expectValid,
	                          PermissionSet expectedPermissions)
	{
		Capability capToIntPointer = {myGlobalIntPointer};
		capToIntPointer.permissions() &= capPermissions;
		Capability capToGlobalInt = &myGlobalInt;
		if (invalidateData)
		{
			capToGlobalInt.invalidate();
		}
		myGlobalIntPointer[0] = capToGlobalInt;
		Capability loadedCap  = *capToIntPointer;
		TEST(loadedCap.is_valid() == expectValid,
		     "Unexpected tag value on loaded cap: {}",
		     loadedCap);
		TEST(loadedCap.permissions() == expectedPermissions,
		     "Unexpected permissions on loaded cap: {}",
		     loadedCap);
	}

	void test_restricted_loads()
	{
		// This test used to expect reduced permissions in the result but this
		// was changed in an ISA update.
		debug_log("Attempting to load capability via not-load-cap cap.");
		test_restricted_load(
		  false, {Permission::Load}, false, GlobalPermissions);

		debug_log(
		  "Attempting to load untagged capability via not-load-mutable cap.");
		test_restricted_load(true,
		                     {Permission::Load,
		                      Permission::LoadStoreCapability,
		                      Permission::LoadGlobal},
		                     false,
		                     GlobalPermissions);

		debug_log(
		  "Attempting to load untagged capability via not-load-global cap.");
		test_restricted_load(true,
		                     {Permission::Load,
		                      Permission::LoadStoreCapability,
		                      Permission::LoadMutable},
		                     false,
		                     GlobalPermissions);

		debug_log("Attempting to load capability via not-load-mutable cap.");
		test_restricted_load(false,
		                     {Permission::Load,
		                      Permission::LoadStoreCapability,
		                      Permission::LoadGlobal},
		                     true,
		                     GlobalPermissions.without(
		                       Permission::Store, Permission::LoadMutable));

		debug_log("Attempting to load capability via not-load-global cap.");
		test_restricted_load(false,
		                     {Permission::Load,
		                      Permission::LoadStoreCapability,
		                      Permission::LoadMutable},
		                     true,
		                     GlobalPermissions.without(Permission::Global,
		                                               Permission::LoadGlobal));
		// XXX get hold of a sealed data capability
	}

	/**
	 * Function stub used to get an executable capability for use in jalr tests.
	 */
	void test_function() {}

	void do_jalr_test(CauseCode                exception,
	                  CapabilityFilter<void()> targetFilter)
	{
		Capability capToTestFunction = test_function;
		Capability capToFunction     = targetFilter(capToTestFunction);
		expectedMCause               = MCauseCheri;
		expectedCauseCode            = exception;
		int previousCrashes          = crashes;
		on_error([&]() { capToFunction.get()(); });
		TEST(crashes == previousCrashes + 1,
		     "Expected jalr to {} to crash",
		     capToFunction);
	}

	void test_jalr_faults()
	{
		debug_log("Attempting to jump to untagged capability");
		do_jalr_test(CauseCode::TagViolation, clear_tag_filter);

		debug_log("Attempting to jump to non-executable capability");
		do_jalr_test(CauseCode::PermitExecuteViolation,
		             permission_filter<PermissionSet(Permission::Load)>);

		debug_log("Attempting to jump to capability with too small bounds");
		do_jalr_test(CauseCode::BoundsViolation, restrict_bounds_filter);
	}

	/*
	 * A convenient place to test PCC related errors. Note that we use 4-byte
	 * nops so that we can test things like half of an instruction being in
	 * bounds. Clang doesn't seem to emit a prelude for this function. If it
	 * does (e.g. at -O0) our expected error PC calculations will be wrong due
	 * to alignment.
	 */
	__attribute__((aligned(4))) void aligned_nop_slide()
	{
		asm volatile(".rept 5\n"
		             "add x0, x15, x15\n"
		             ".endr\n");
	}

	/*
	 * Same as aligned_nop_slide except that we try to ensure the 4-byte nops
	 * are unaligned by using a c.nop. Depending on how hw fetches instructions
	 * this could conceivable make a difference.
	 */
	__attribute__((aligned(4))) void unaligned_nop_slide()
	{
		asm volatile("c.nop\n"
		             ".rept 5\n"
		             "add x0, x15, x15\n"
		             ".endr\n");
	}

	/*
	 * Same as aligned_nop_slide except that we use compressed the 2-byte nops.
	 */
	__attribute__((aligned(4))) void compressed_nop_slide()
	{
		asm volatile(".rept 10\n"
		             "c.nop\n"
		             ".endr\n");
	}

	/**
	 * Call `target` with PCC bounds set to `size`, which should be small enough
	 * to cause it to fault. On failure the error handler will derive a new PCC
	 * with compartment bounds and continue.
	 */
	void
	test_pcc_bounds_crash(void (*target)(), size_t size, size_t errorOffset)
	{
		Capability capToTarget = target;
		capToTarget.bounds()   = size;
		expectedMCause         = MCauseCheri;
		expectedErrorPC        = capToTarget.address() + errorOffset;
		expectedCauseCode      = CauseCode::BoundsViolation;
		expectedRegisterNumber = RegisterNumber::PCC;
		debug_log("Calling function with too small PCC bounds {}", capToTarget);
		int previousCrashes = crashes;
		on_error([&]() { (*capToTarget)(); });
		TEST(crashes == previousCrashes + 1,
		     "Call with too small PCC bounds did not crash.");
	}

	/**
	 * Tests that check that executing off the end of PCC will fault.
	 */
	void test_pcc_bounds()
	{
		// Call aligned_nop_slide with different bounds.
		// Note that the error PC is always the first byte of the instruction
		// that exceeds PCC bounds so depends on alignment.
		test_pcc_bounds_crash(aligned_nop_slide, 8, 8);
		test_pcc_bounds_crash(aligned_nop_slide, 9, 8);
		test_pcc_bounds_crash(aligned_nop_slide, 10, 8);
		test_pcc_bounds_crash(aligned_nop_slide, 11, 8);

		// Repeat the above tests with an unaligned_nop_slide.
		test_pcc_bounds_crash(unaligned_nop_slide, 8, 6);
		test_pcc_bounds_crash(unaligned_nop_slide, 9, 6);
		test_pcc_bounds_crash(unaligned_nop_slide, 10, 10);
		test_pcc_bounds_crash(unaligned_nop_slide, 11, 10);

		// And finally compressed_nop_slide.
		test_pcc_bounds_crash(compressed_nop_slide, 8, 8);
		test_pcc_bounds_crash(compressed_nop_slide, 9, 8);
		test_pcc_bounds_crash(compressed_nop_slide, 10, 10);
		test_pcc_bounds_crash(compressed_nop_slide, 11, 10);
	}
} // anonymous namespace

int test_isa()
{
	// get some sealing and sealed capabilities for use in tests
	sealingCapability    = token_key_new();
	sealedDataCapability = blocking_forever<token_sealed_alloc>(
	  MALLOC_CAPABILITY, sealingCapability, sizeof(void *));
	// Do some sanity checks on above
	TEST(Capability{sealingCapability}.permissions().contains(Permission::Seal),
	     "sealing key dosen't have seal permission");
	TEST(Capability{sealedDataCapability}.is_sealed(),
	     "sealedDataCap is not sealed");
	test_set_bounds();
	test_and_perms();
	test_sentries();
	test_pcc_bounds();
	test_load_faults();
	test_restricted_loads();
	test_store_faults();
	test_jalr_faults();
	token_obj_destroy(
	  MALLOC_CAPABILITY, sealingCapability, sealedDataCapability);
	return 0;
}
