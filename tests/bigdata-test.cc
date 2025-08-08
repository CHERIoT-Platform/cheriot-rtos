#include "cheri.hh"
#define TEST_NAME "Big Data"
#include "tests.hh"

extern "C" enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Detected error in instruction {}", frame->pcc);
	debug_log("Error cause: {}, mtval: {}", mcause, mtval);
	auto [reg, cause] = CHERI::extract_cheri_mtval(mtval);
	debug_log("Error {} in register {}", reg, cause);
	return ErrorRecoveryBehaviour::ForceUnwind;
}

int test_bigdata()
{
	register char *cgpRegister asm("cgp");
	asm("" : "=C"(cgpRegister));
	static uint32_t data[16384];
	debug_log("Data: {}", data);
	debug_log("CGP: {}", cgpRegister);
	debug_log("Fill with for loop");
	for (int i = 0; i < 16000; ++i)
	{
		data[i] = 0x01020304;
	}
	debug_log("Fill with memset");
	memset(data, 0x5a, sizeof(data));

	return 0;
}
