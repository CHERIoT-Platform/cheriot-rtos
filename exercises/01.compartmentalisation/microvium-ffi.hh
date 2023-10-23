#pragma once

#include <magic_enum/magic_enum.hpp>
#include <microvium/microvium.h>
/**
 * Code related to the JavaScript interpreter.
 */
namespace
{
	using CHERI::Capability;

	/**
	 * Constants for functions exposed to JavaScript->C++ FFI
	 *
	 * The values here must match the ones used in cheri.js.
	 */
	enum Exports : mvm_HostFunctionID
	{
		Print = 1,
		Move,
		LoadCapability,
		LoadInt,
		Store,
		GetAddress,
		SetAddress,
		GetBase,
		GetLength,
		GetPermissions,
		CheckSecret,
	};

	/// Constant for the say_hello function exposed to C++->JavaScript FFI
	static constexpr mvm_VMExportID ExportRun = 1234;

	/**
	 * Type used for the set of capabilities that JavaScript has complete
	 * control over.
	 */
	using AttackerRegisterState = std::array<void *, 8>;

	/**
	 * Address of the `AttackerRegisterState` on the stack.  This structure is
	 * on the stack so that it can hold local capabilities.
	 */
	ptraddr_t attackerRegisterStateAddress;

	/**
	 * Re-derive the pointer to the on-stack register state.
	 */
	AttackerRegisterState &state()
	{
		register AttackerRegisterState *cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		Capability<AttackerRegisterState> rs{cspRegister};
		rs.address() = attackerRegisterStateAddress;
		rs.bounds()  = sizeof(AttackerRegisterState);
		return *rs;
	}

	/**
	 * Write a capability into one of the VM's register set.
	 */
	void register_write(int regno, void *value)
	{
		if ((regno >= 0) && (regno < 8))
		{
			state()[regno] = value;
		}
	}

	/**
	 * Read a register from the VM's register set.  This reads one of the 8
	 * that can be written or CSP (8), CGP (9), or PCC (10).
	 */
	void *register_read(int regno)
	{
		switch (regno)
		{
			default:
				return nullptr;
			case 0 ... 7:
				return state()[regno];
			case 8:
			{
				register void *cspRegister asm("csp");
				asm("" : "=C"(cspRegister));
				return cspRegister;
			}
			case 9:
			{
				register void *cgpRegister asm("cgp");
				asm("" : "=C"(cgpRegister));
				return cgpRegister;
			}
			case 10:
			{
				void *pcc;
				asm("auipcc %0, 0\n" : "=C"(pcc));
				return pcc;
			}
		}
	}

	/**
	 * Template that returns JavaScript argument specified in `arg` as a C++
	 * type T.
	 */
	template<typename T>
	T extract_argument(mvm_VM *vm, mvm_Value arg);

	/**
	 * Specialisation to return integers.
	 */
	template<>
	__always_inline int32_t extract_argument<int32_t>(mvm_VM *vm, mvm_Value arg)
	{
		return mvm_toInt32(vm, arg);
	}

	/**
	 * Specialisation to return booleans.
	 */
	template<>
	__always_inline bool extract_argument<bool>(mvm_VM *vm, mvm_Value arg)
	{
		return mvm_toBool(vm, arg);
	}

	/**
	 * Populate a tuple with arguments from an array of JavaScript values.
	 * This uses `extract_argument` to coerce each JavaScript value to the
	 * expected type.
	 */
	template<typename Tuple, int Idx = 0>
	__always_inline void
	args_to_tuple(Tuple &tuple, mvm_VM *vm, mvm_Value *args)
	{
		if constexpr (Idx < std::tuple_size_v<Tuple>)
		{
			std::get<Idx>(tuple) = extract_argument<
			  std::remove_reference_t<decltype(std::get<Idx>(tuple))>>(
			  vm, args[Idx]);
			args_to_tuple<Tuple, Idx + 1>(tuple, vm, args);
		}
	}

	/**
	 * Helper template to extract the arguments from a function type.
	 */
	template<typename T>
	struct FunctionSignature;

	/**
	 * The concrete specialisation that decomposes the function type.
	 */
	template<typename R, typename... Args>
	struct FunctionSignature<R(Args...)>
	{
		/**
		 * A tuple type containing all of the argument types of the function
		 * whose type is being extracted.
		 */
		using ArgumentType = std::tuple<Args...>;
	};

	/**
	 * Call `Fn` with arguments from the Microvium arguments array.
	 *
	 * This is a wrapper that allows automatic forwarding from a function
	 * exported to JavaScript
	 */
	template<auto Fn>
	__always_inline mvm_TeError call_export(mvm_VM    *vm,
	                                        mvm_Value *result,
	                                        mvm_Value *args,
	                                        uint8_t    argsCount)
	{
		using TupleType = typename FunctionSignature<
		  std::remove_pointer_t<decltype(Fn)>>::ArgumentType;
		// Return an error if we have the wrong number of arguments.
		if (argsCount < std::tuple_size_v<TupleType>)
		{
			return MVM_E_UNEXPECTED;
		}
		// Get the arguments in a tuple.
		TupleType arguments;
		args_to_tuple(arguments, vm, args);
		// If this returns void, we don't need to do anything with the return.
		if constexpr (std::is_same_v<void, decltype(std::apply(Fn, arguments))>)
		{
			std::apply(Fn, arguments);
		}
		else
		{
			// Coerce the return type to a JavaScript object of the correct
			// type and return it.
			auto primitiveResult = std::apply(Fn, arguments);
			if constexpr (std::is_same_v<decltype(primitiveResult), bool>)
			{
				*result = mvm_newBoolean(primitiveResult);
			}
			if constexpr (std::is_same_v<decltype(primitiveResult), int32_t>)
			{
				*result = mvm_newInt32(vm, primitiveResult);
			}
			if constexpr (std::is_same_v<decltype(primitiveResult),
			                             std::string>)
			{
				*result = mvm_newString(
				  vm, primitiveResult.data(), primitiveResult.size());
			}
		}
		return MVM_E_SUCCESS;
	}

	/**
	 * Helper that maps from Exports
	 */
	template<Exports>
	constexpr static std::nullptr_t ExportedFn = nullptr;

	/**
	 * Move a value from the source register to the destination.
	 */
	void export_move(int32_t destination, int32_t source)
	{
		register_write(destination, register_read(source));
	}

	template<>
	constexpr static auto ExportedFn<Move> = export_move;

	/**
	 * Load a capability into the destination register.
	 */
	void export_load(int32_t destination, int32_t source, int32_t offset)
	{
		Capability<void *> s{static_cast<void **>(register_read(source))};
		s.address() += offset;
		register_write(destination, *s);
	}

	template<>
	constexpr static auto ExportedFn<LoadCapability> = export_load;

	/**
	 * Load and return an integer.
	 */
	int32_t export_load_int(int32_t source, int32_t offset)
	{
		Capability<int32_t> s{static_cast<int32_t *>(register_read(source))};
		s.address() += offset;
		return *s;
	}

	template<>
	constexpr static auto ExportedFn<LoadInt> = export_load_int;

	/**
	 * Store a capability from a register at a specified location.
	 */
	void export_store(int32_t value, int32_t source, int32_t offset)
	{
		Capability<void *> s{static_cast<void **>(register_read(source))};
		s.address() += offset;
		*s = register_read(value);
	}

	template<>
	constexpr static auto ExportedFn<Store> = export_store;

	/**
	 * Returns the address of the capability in the specified register.
	 */
	int32_t export_get_address(int32_t regno)
	{
		Capability<void> value{register_read(regno)};
		return value.address();
	}

	template<>
	constexpr static auto ExportedFn<GetAddress> = export_get_address;

	/**
	 * Set the address of the capability in the specified register.
	 */
	void export_set_address(int32_t regno, int32_t address)
	{
		Capability<void> value{register_read(regno)};
		value.address() = address;
		register_write(regno, value);
	}

	template<>
	constexpr static auto ExportedFn<SetAddress> = export_set_address;

	/**
	 * Return the base address of the capability in the specified register.
	 */
	int32_t export_get_base(int32_t regno)
	{
		Capability<void> value{register_read(regno)};
		return value.base();
	}

	template<>
	constexpr static auto ExportedFn<GetBase> = export_get_base;

	/**
	 * Return the length of the capability in the specified register.
	 */
	int32_t export_get_length(int32_t regno)
	{
		Capability<void> value{register_read(regno)};
		return value.length();
	}

	template<>
	constexpr static auto ExportedFn<GetLength> = export_get_length;

	/**
	 * Return the permissions of the capability in the specified register.
	 */
	int32_t export_get_permissions(int32_t regno)
	{
		Capability<void> value{register_read(regno)};
		return value.permissions().as_raw();
	}

	template<>
	constexpr static auto ExportedFn<GetPermissions> = export_get_permissions;

	template<>
	constexpr static auto ExportedFn<CheckSecret> = check_secret;

	/**
	 * Base template for exported functions.  Forwards to the function defined
	 * with `ExportedFn<E>`.
	 */
	template<Exports E>
	mvm_TeError exported_function(mvm_VM *vm,
	                              mvm_HostFunctionID,
	                              mvm_Value *result,
	                              mvm_Value *args,
	                              uint8_t    argCount)
	{
		return call_export<ExportedFn<E>>(vm, result, args, argCount);
	}

	/**
	 * Print a string passed from JavaScript.
	 */
	template<>
	mvm_TeError exported_function<Print>(mvm_VM            *vm,
	                                     mvm_HostFunctionID funcID,
	                                     mvm_Value         *result,
	                                     mvm_Value         *args,
	                                     uint8_t            argCount)
	{
		// Iterate over the arguments.
		for (unsigned i = 0; i < argCount; i++)
		{
			// Coerce the argument to a string and get it as a C string
			const char *str = mvm_toStringUtf8(vm, args[i], nullptr);
			// Write each character to the UART
			for (; *str != '\0'; str++)
			{
				ImplicitUARTOutput::write(*str);
			}
		}
		// Write a trailing newline
		ImplicitUARTOutput::write('\n');
		// Unconditionally return success
		return MVM_E_SUCCESS;
	}

	/**
	 * Callback from microvium that resolves imports.
	 *
	 * This resolves each function to the template instantiation of
	 * `exported_function` with `funcID` as the template parameter.
	 */
	mvm_TeError
	resolve_import(mvm_HostFunctionID funcID, void *, mvm_TfHostFunction *out)
	{
		return magic_enum::enum_switch(
		  [&](auto val) {
			  constexpr Exports Export = val;
			  *out                     = exported_function<Export>;
			  return MVM_E_SUCCESS;
		  },
		  Exports(funcID),
		  MVM_E_UNRESOLVED_IMPORT);
	}

	/**
	 * Helper that deletes a Microvium VM when used with a C++ unique pointer.
	 */
	struct MVMDeleter
	{
		void operator()(mvm_VM *mvm) const
		{
			mvm_free(mvm);
		}
	};
} // namespace
