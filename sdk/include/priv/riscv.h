// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef _PRIV_RISCV_H_
#define _PRIV_RISCV_H_

#include <stddef.h>
#include <utils.h>

namespace priv
{
#define CSR_ZIMM(val) (__builtin_constant_p(val) && ((unsigned long)(val) < 32))

#define csr_swap(csr, val)                                                     \
	({                                                                         \
		if (CSR_ZIMM(val))                                                     \
			__asm __volatile("csrrwi %0, " #csr ", %1"                         \
				             : "=r"(val)                                       \
				             : "i"(val));                                      \
		else                                                                   \
			__asm __volatile("csrrw %0, " #csr ", %1" : "=r"(val) : "r"(val)); \
		val;                                                                   \
	})

#define csr_write(csr, val)                                                    \
	({                                                                         \
		if (CSR_ZIMM(val))                                                     \
			__asm __volatile("csrwi " #csr ", %0" ::"i"(val));                 \
		else                                                                   \
			__asm __volatile("csrw " #csr ", %0" ::"r"(val));                  \
	})

#define csr_set(csr, val)                                                      \
	({                                                                         \
		if (CSR_ZIMM(val))                                                     \
			__asm __volatile("csrsi " #csr ", %0" ::"i"(val));                 \
		else                                                                   \
			__asm __volatile("csrs " #csr ", %0" ::"r"(val));                  \
	})

#define csr_clear(csr, val)                                                    \
	({                                                                         \
		if (CSR_ZIMM(val))                                                     \
			__asm __volatile("csrci " #csr ", %0" ::"i"(val));                 \
		else                                                                   \
			__asm __volatile("csrc " #csr ", %0" ::"r"(val));                  \
	})

#define csr_read(csr)                                                          \
	({                                                                         \
		unsigned int val;                                                      \
		__asm __volatile("csrr %0, " #csr : "=r"(val));                        \
		val;                                                                   \
	})

#define wfi() __asm volatile("wfi")

	constexpr size_t MCAUSE_INTR      = (1u << 31);
	constexpr size_t MCAUSE_CODE_MASK = (~MCAUSE_INTR);
	constexpr size_t MCAUSE_MTIME     = 7;
	constexpr size_t MCAUSE_MEXTERN   = 11;

	constexpr size_t MCAUSE_INST_MISALIGNED     = 0;
	constexpr size_t MCAUSE_INST_ACCESS_FAULT   = 1;
	constexpr size_t MCAUSE_ILLEGAL_INSTRUCTION = 2;
	constexpr size_t MCAUSE_BREAKPOINT          = 3;
	constexpr size_t MCAUSE_LOAD_MISALIGNED     = 4;
	constexpr size_t MCAUSE_LOAD_ACCESS_FAULT   = 5;
	constexpr size_t MCAUSE_STORE_MISALIGNED    = 6;
	constexpr size_t MCAUSE_STORE_ACCESS_FAULT  = 7;
	constexpr size_t MCAUSE_ECALL_USER          = 8;
	constexpr size_t MCAUSE_ECALL_SUPERVISOR    = 9;
	constexpr size_t MCAUSE_ECALL_MACHINE       = 11;
	constexpr size_t MCAUSE_INST_PAGE_FAULT     = 12;
	constexpr size_t MCAUSE_LOAD_PAGE_FAULT     = 13;
	constexpr size_t MCAUSE_STORE_PAGE_FAULT    = 15;
	constexpr size_t MCAUSE_THREAD_EXIT         = 24;
	constexpr size_t MCAUSE_CHERI               = 28;

	constexpr size_t MSTATUS_UIE = (1 << 0);
	constexpr size_t MSTATUS_SIE = (1 << 1);
	constexpr size_t MSTATUS_HIE = (1 << 2);
	constexpr size_t MSTATUS_MIE = (1 << 3);
	constexpr size_t MSTATUS_AIE =
	  (MSTATUS_UIE | MSTATUS_SIE | MSTATUS_HIE | MSTATUS_MIE);
	constexpr size_t MSTATUS_UPIE       = (1 << 4);
	constexpr size_t MSTATUS_SPIE       = (1 << 5);
	constexpr size_t MSTATUS_SPIE_SHIFT = 5;
	constexpr size_t MSTATUS_HPIE       = (1 << 6);
	constexpr size_t MSTATUS_MPIE       = (1 << 7);
	constexpr size_t MSTATUS_MPIE_SHIFT = 7;
	constexpr size_t MSTATUS_SPP        = (1 << 8);
	constexpr size_t MSTATUS_SPP_SHIFT  = 8;
	constexpr size_t MSTATUS_HPP_MASK   = 0x3;
	constexpr size_t MSTATUS_HPP_SHIFT  = 9;
	constexpr size_t MSTATUS_MPP_MASK   = 0x3;
	constexpr size_t MSTATUS_MPP_SHIFT  = 11;
	constexpr size_t MSTATUS_FS_MASK    = 0x3;
	constexpr size_t MSTATUS_FS_SHIFT   = 13;
	constexpr size_t MSTATUS_XS_MASK    = 0x3;
	constexpr size_t MSTATUS_XS_SHIFT   = 15;
	constexpr size_t MSTATUS_MPRV       = (1 << 17);
	constexpr size_t MSTATUS_PUM        = (1 << 18);
	constexpr size_t MSTATUS_VM_MASK    = 0x1f;
	constexpr size_t MSTATUS_VM_SHIFT   = 24;
	constexpr size_t MSTATUS_VM_MBARE   = 0;
	constexpr size_t MSTATUS_VM_MBB     = 1;
	constexpr size_t MSTATUS_VM_MBBID   = 2;
	constexpr size_t MSTATUS_VM_SV32    = 8;
	constexpr size_t MSTATUS_VM_SV39    = 9;
	constexpr size_t MSTATUS_VM_SV48    = 10;
	constexpr size_t MSTATUS_VM_SV57    = 11;
	constexpr size_t MSTATUS_VM_SV64    = 12;
	constexpr size_t MSTATUS_SD         = (1 << 31);

	constexpr size_t MSTATUS_PRV_U = 0;
	constexpr size_t MSTATUS_PRV_S = 1;
	constexpr size_t MSTATUS_PRV_H = 2;
	constexpr size_t MSTATUS_PRV_M = 3;

	constexpr size_t MIE_USIE = (1 << 0);
	constexpr size_t MIE_SSIE = (1 << 1);
	constexpr size_t MIE_HSIE = (1 << 2);
	constexpr size_t MIE_MSIE = (1 << 3);
	constexpr size_t MIE_UTIE = (1 << 4);
	constexpr size_t MIE_STIE = (1 << 5);
	constexpr size_t MIE_HTIE = (1 << 6);
	constexpr size_t MIE_MTIE = (1 << 7);

	constexpr size_t MIP_USIP = (1 << 0);
	constexpr size_t MIP_SSIP = (1 << 1);
	constexpr size_t MIP_HSIP = (1 << 2);
	constexpr size_t MIP_MSIP = (1 << 3);
	constexpr size_t MIP_UTIP = (1 << 4);
	constexpr size_t MIP_STIP = (1 << 5);
	constexpr size_t MIP_HTIP = (1 << 6);
	constexpr size_t MIP_MTIP = (1 << 7);
	constexpr size_t MIP_SEIP = (1 << 9);
	constexpr size_t MIP_MEIP = (1 << 11);

	static inline size_t intr_disable(void)
	{
		size_t ret;

		__asm volatile("csrrci %0, mstatus, %1"
		               : "=&r"(ret)
		               : "i"(MSTATUS_AIE));

		return (ret & (MSTATUS_AIE));
	}

	static inline void mie_enable()
	{
		csr_set(mie, 0x880);
	}

	static inline void intr_restore(size_t s)
	{
		__asm volatile("csrs mstatus, %0" ::"r"(s));
	}

	static inline void intr_enable()
	{
		csr_set(mstatus, 0x08);
	}

} // namespace priv

#endif // _PRIV_RISCV_H_
