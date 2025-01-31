#include <token.h>

struct CaesarCapability
{
	bool    permitEncrypt;
	bool    permitDecrypt;
	uint8_t shift;
};

#define DECLARE_AND_DEFINE_CAESAR_CAPABILITY(                                  \
  name, permitEncrypt, permitDecrypt, shift)                                   \
	DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(struct CaesarCapability,            \
	                                       caesar,                             \
	                                       CaesarCapabilityType,               \
	                                       name,                               \
	                                       permitEncrypt,                      \
	                                       permitDecrypt,                      \
	                                       shift)

int __cheri_compartment("caesar")
  caesar_encrypt(CHERI_SEALED(struct CaesarCapability *) capability,
                 const char *input,
                 char       *output,
                 size_t      len);

int __cheri_compartment("caesar")
  caesar_decrypt(CHERI_SEALED(struct CaesarCapability *) capability,
                 const char *input,
                 char       *output,
                 size_t      len);

ssize_t __cheri_compartment("producer")
  produce_message(char *buffer, size_t length);
void __cheri_compartment("consumer")
  consume_message(const char *buffer, size_t length);
