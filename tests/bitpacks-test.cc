#define TEST_NAME "Test bitpacks"
#include "tests.hh"

#include <__macro_map.h>
#include <bitpacks.hh>

/// An example "device" structure type, composed of two Bitpack "registers".
struct Foo
{
	/*
	 * Many bit-packed structures are just products of distinct types, so we
	 * can tell C++ essentially exactly that: what types are and where they are
	 * placed within the packed structure.
	 */
	struct Control : Bitpack<uint32_t>
	{
		BITPACK_USUAL_PREFIX;

		enum class Drive : uint8_t
		{
			No   = 0,
			Up   = 1,
			Down = 2,
			Both = 3,
		};

		template<>
		constexpr FieldInfo field_info_for_type<Drive>()
		{
			return {0, 1};
		}

		enum class Flag0 : uint8_t
		{
			No  = 0,
			Yes = 1,
		};

		template<>
		constexpr FieldInfo field_info_for_type<Flag0>()
		{
			return {4, 4};
		}

		BITPACK_DEFINE_TYPE_ENUM_CLASS(Flag1, uint8_t, 5, 5) {
			No  = 0,
			Yes = 1,
		};

		struct Scalar : Numeric<uint8_t>
		{
			using Numeric::Numeric;
		};

		template<>
		constexpr FieldInfo field_info_for_type<Scalar>()
		{
			return {8, 14};
		}

		BITPACK_DEFINE_TYPE_NUMERIC(Scalar2, uint8_t, 15, 20);
	} control;

	/*
	 * Explicit field accessors, without type-level computation, also work, and
	 * are useful when types are reused within the product.
	 */
	struct More : Bitpack<uint32_t>
	{
		using Bitpack::Bitpack;
		using Bitpack::operator=;

		enum class ReusedE : uint8_t
		{
			A = 0,
			B = 1,
			C = 2,
			D = 3,
		};

		// Nested bitpacks are also supported
		struct ReusedT : Bitpack<uint8_t>
		{
			using Bitpack::Bitpack;
			using Bitpack::operator=;

			template<typename Self>
			constexpr auto f1(this Self &&self)
			{
				return self.template view<ReusedE, {0, 1}>();
			}

			BITPACK_NAMED_VIEW(f1c, ReusedE, 0, 1, true);
			BITPACK_NAMED_VIEW(f2, ReusedE, 4, 5);
		};

		BITPACK_NAMED_VIEW(r1, ReusedT, 0, 7);

		using R2 = Field<ReusedT, {8, 15}>;

		template<typename Self>
		constexpr auto r2(this Self &&self)
		{
			return R2::View(self);
		}
	} more;
};
static_assert(offsetof(Foo, control) == 0);
static_assert(offsetof(Foo, more) == 4);
static_assert(sizeof(Foo) == 8);

template<typename F>
static constexpr bool CanAssignFieldV =
  std::is_assignable_v<F &, typename F::Field::Type &>;

// We can assign into Foo::Control's Drive field...
static_assert(CanAssignFieldV<decltype(std::declval<Foo::Control>()
                                         .view<Foo::Control::Drive>())>);

// We can assign into Foo::More::ReusedT's f1() field...
static_assert(
  CanAssignFieldV<decltype(std::declval<Foo::More::ReusedT>().f1())>);

// ... but not an alias of it that's marked constant.
static_assert(
  !CanAssignFieldV<decltype(std::declval<Foo::More::ReusedT>().f1c())>);

constexpr static auto C0 = Foo::Control();
static_assert(C0.raw() == 0);

// Construct a constant using type-directed interfaces, verbosely
constexpr static auto C1 =
  C0.with<Foo::Control::Scalar>({7}).with<Foo::Control::Drive>(
    Foo::Control::Drive::Up);
static_assert(C1.raw() == 0x701);

static_assert(C1.with<Foo::Control::Scalar2>(0xA) == 0x50701);

/*
 * OK, that's too chatty.  With some macro terror, we can get something much
 * better, with the only moderately vexing need for an immediately-evaluated
 * lambda, since statement expressions aren't supported at top-level scope.
 */
constexpr static auto C1a = []() {
	return BITPACK_QWITHS(C0, Scalar{7}, Drive::Up);
}();
static_assert(C1 == C1a);

// Chain with()s for explicit (not-type-directed) fields
constexpr static auto M1 =
  Foo::More()
    .r1()
    .with([](auto r) { return r.f2().with(Foo::More::ReusedE::B); })
    .r2()
    .with([](auto r) { return r.f1().with(Foo::More::ReusedE::C); });
static_assert(M1.raw() == 0x0210);

// We can also build up values with chains of mutation
constexpr static Foo F1 = []() {
	auto f = Foo();

	/*
	 * Rawest form, with no deduction or type-level computation other than the
	 * .view<>() resolution.
	 */
	f.control.view<Foo::Control::Drive>() = Foo::Control::Drive::Down;

	/*
	 * Slightly more baked: .set()'s template argument is deducable from its
	 * argument's type; field information is computed therefrom.
	 */
	f.control.set(Foo::Control::Flag0::Yes);

	/*
	 * Convenience macros get rid of lots of those ::s through some abuse while
	 * the compilation unit is still just a sequence of tokens.
	 */
	BITPACK_ENUM_QTYPE_ASSIGN(f.control, Flag1, Yes);

	/*
	 * `.view<>` with a little syntactic sugar behind a macro.  Note that
	 * `Numeric<T>` fields are implicitly convertible to their backing type T,
	 * so we don't need anyhing fancier than their value.
	 *
	 * All bitpack fields are clamped to their field width, so even though the
	 * Scalar field is a `Numeric<uint8_t>`, and we can set bits arbitrarily,
	 * only seven of these eight bits make it into the backing storage (checked
	 * implicitly below with our `static_assert`-s).
	 */
	BITPACK_BY_QTYPE(f.control, Scalar) = 0xFF;

	/*
	 * Nested bitpack assignment, with explicit fields for both levels.
	 *
	 * Constant fields don't support assignment or .alter(), but do support
	 * .with(), since that produces a new Bitpack value.
	 */
	f.more.r1() = Foo::More::ReusedT{}
	                .f1c()
	                .with(Foo::More::ReusedE::C)
	                .f2()
	                .with(Foo::More::ReusedE::B);

	/*
	 * Perhaps slightly more convenient?
	 *
	 * The argument type to the lambda must be specified and not `auto` or a
	 * template parameter, as `using enum` (as inside BITPACK_ENUM_*) works only
	 * with non-dependent types.
	 */
	f.more.r2().alter([](Foo::More::ReusedT r2) {
		BITPACK_ENUM_ASSIGN(r2.f1(), D);
		return r2;
	});
	f.more.r2().alter([](Foo::More::ReusedT r2) {
		BITPACK_ENUM_ASSIGN(r2.f2(), B);
		return r2;
	});

	return f;
}();
static_assert([]() {
	return BITPACK_NUMERIC_QTYPE_VALUE(F1.control, Scalar);
}() == 0x7F);
static_assert(F1.control.raw() == 0x7F32);
static_assert(F1.more.raw() == 0x1312);

__noinline static void test_volatile(volatile Foo *vfp)
{
	static_assert(std::is_same_v<volatile Foo::More &, decltype((vfp->more))>);

	BITPACK_QVAL_ASSIGN(vfp->control, Flag0::Yes);

	TEST_EQUAL(vfp->control.read().raw(), 0x10U, "bad volatile handling?");

	vfp->control.alter([](auto v) {
		BITPACK_BY_TQTYPE(v, Scalar).alter([](auto v) { return v + 7; });
		return BITPACK_TQWITHS(v, Scalar2{7});
	});

	TEST_EQUAL(vfp->control.read().raw(), 0x38710U, "bad volatile handling?");

	volatile Foo::More &m = vfp->more;

	// Fields of volatile bitpacks have volatile internal reference types
	static_assert(
	  std::is_same_v<volatile uint32_t &, decltype(m.r1())::RefType>);

	// First write to vfp->more by populating its .r2() field from all zeros..a.
	m.assign_from([](auto z) {
		z.r2().assign_from(
		  [](auto zr2) { return zr2.f2().with(Foo::More::ReusedE::C); });
		return z;
	});

	// Read-modify-write vfp->more to update its .r1() field.
	m.r1().alter([](auto r1) {
		/*
		 * m.r1() was volatile, but .alter() has performed a read, and so the
		 * value shown to us is not volatile.
		 *
		 * We can, however, verify that our "this field is constant in an
		 * otherwise non-constant Bitpack" logic works, so .f1c() qualifies its
		 * reference to the bitpack's underlying storage as constant.
		 */
		static_assert(
		  std::is_same_v<uint8_t &, typename decltype(r1.f1())::RefType>);
		static_assert(std::is_same_v<const uint8_t &,
		                             typename decltype(r1.f1c())::RefType>);

		return r1.f1()
		  .with(Foo::More::ReusedE::A)
		  .f2()
		  .with(Foo::More::ReusedE::D);
	});

	TEST_EQUAL(vfp->more.read().raw(), 0x2030U, "bad volatile handling?");

	vfp->more = F1.more;

	TEST_EQUAL(vfp->more.read().raw(), 0x1312U, "bad volatile handling?");

	/*
	 * Bitpacks' assignment operator rejects volatile RHS, so we can't just
	 * write `vfp->more = vfp->more;` for example.  And, in fact, we can have
	 * the compiler check that we can't do that...
	 */
	static_assert(
	  !std::is_assignable_v<volatile Foo::More &, volatile Foo::More &>);
	// Instead, we have to perform an explicit read.
	static_assert(std::is_assignable_v<volatile Foo::More &, Foo::More &>);
	vfp->more = vfp->more.read();
}

static void test_static_asserts()
{
	static_assert(BITPACK_ENUM_OP(C1.view<Foo::Control::Drive>(), ==, Up));
	static_assert(BITPACK_ENUM_QTYPE_OP(C1, Control::Flag0, ==, No));
	static_assert((BITPACK_ENUM_QTYPE_WITH(C1, Drive, Both) <=> C1) ==
	              std::strong_ordering::greater);
	static_assert(
	  BITPACK_NUMERIC_OP(F1.control.view<Foo::Control::Scalar>(), ==, 0x7F));
	static_assert(BITPACK_NUMERIC_QTYPE_OP(F1.control, Scalar, <=>, 0x7E) ==
	              std::strong_ordering::greater);

	static_assert(
	  static_cast<uint32_t>(F1.control.get<Foo::Control::Flag1>()) == 1);

	static_assert(BITPACK_MASKED_OP_Q(C1, ==, Flag0::No, Scalar{7}));
}

namespace MaybeBetterErgonomicsTypes
{
	/*
	 * As much as it may be morally correct for field types to belong to the
	 * `Bitpack`-based `struct`-ure to which they belong, C++ really doesn't
	 * make that easy to use, thus all the C preprocessor macros and a lot of
	 * the ::s in the above.  Let's try that experiment again with the types
	 * floated out into a namespace to which we can apply `using namespace`
	 * statements.
	 */

	enum class Drive : uint8_t
	{
		No   = 0,
		Up   = 1,
		Down = 2,
		Both = 3,
	};

	enum class Flag0 : uint8_t
	{
		No  = 0,
		Yes = 1,
	};

} // namespace MaybeBetterErgonomicsTypes

struct MBE : Bitpack<uint32_t>
{
	using Bitpack::Bitpack;
	using Bitpack::operator=;

	template<typename FieldType>
	static constexpr FieldInfo field_info_for_type() = delete;

	template<>
	constexpr FieldInfo field_info_for_type<MaybeBetterErgonomicsTypes::Drive>()
	{
		return {0, 1};
	}

	template<>
	constexpr FieldInfo field_info_for_type<MaybeBetterErgonomicsTypes::Flag0>()
	{
		return {2, 2};
	}
};

// If we assume the usual use case here is that the definitions (above) are in a
// header and the users are in separate compliation units, we can bring the
// namespace(s) involved into scope with using.  For the purpose of this test,
// do so in a lambda and pass out anything we need for later consideration.
static constexpr auto MBETops = []() {
	using namespace MaybeBetterErgonomicsTypes;

	constexpr auto MBE0 = MBE();
	static_assert(MBE0.raw() == 0);

	constexpr auto MBE1 = MBE0.with(Drive::Up).with(Flag0::Yes);
	static_assert(MBE1.raw() == 0b1'01);

	auto mbe2 = MBE1;
	mbe2.view<Drive>().alter([](auto) { return Drive::Down; });
	BITPACK_UVAL_ASSIGN(mbe2, Flag0::No);

	return std::make_tuple(MBE1, mbe2);
}();

static void test_maybe_better_ergonomics()
{
	using namespace MaybeBetterErgonomicsTypes;

	constexpr static auto MBE1 = std::get<0>(MBETops);

	/*
	 * This is slightly better, but still, I think having some macros help,
	 * trading repetition of the type for slightly longer spelling.
	 */
	static_assert(MBE1.view<Drive>() == Drive::Up);
	static_assert(BITPACK_ENUM_OP(MBE1.view<Drive>(), ==, Up));
	static_assert(BITPACK_ENUM_UTYPE_OP(MBE1, Drive, ==, Up));

	static_assert((BITPACK_ENUM_UTYPE_WITH(MBE1, Drive, Both) <=> MBE1) ==
	              std::strong_ordering::greater);

	constexpr static auto MBE2 = std::get<1>(MBETops);
	static_assert(MBE2.raw() == 0b0'10);

	static_assert(BITPACK_MASKED_OP(MBE2, ==, Drive::Down));
}

int test_bitpacks()
{
	test_static_asserts();
	test_maybe_better_ergonomics();

	volatile Foo vf = Foo{};
	test_volatile(&vf);

	return 0;
}
