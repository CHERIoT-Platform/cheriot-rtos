package caesar

# Check if an import is sealed with the Caesar capability type
is_sealed_caesar_capability(capability) {
	capability.kind = "SealedObject"
	capability.sealing_type.compartment = "caesar"
	capability.sealing_type.key = "CaesarCapabilityType"
}

# Helpers for converting C integers to booleans.
# These fail if the input is not either 1 or 0
value_as_boolean(value) = output {
	value = 1
	output = true
}

value_as_boolean(value) = output {
	value = 0
	output = false
}

decode_caesar_capability(capability) = decoded {
	# Fail if this is not sealed with the Caesar capability type
	is_sealed_caesar_capability(capability)
	some permitEncrypt, permitDecrypt, shift

	# Extract the values.  Each of these will fail if the value is not as expected.
	# permitEncrypt is a (single-byte) boolean value at offset 0.
	permitEncrypt = value_as_boolean(integer_from_hex_string(capability.contents, 0, 1))

	# permitDecrypt is a (single-byte) boolean value at offset 1.
	permitDecrypt = value_as_boolean(integer_from_hex_string(capability.contents, 1, 1))

	# shift is a single-byte integer value at offset 2.
	shift = integer_from_hex_string(capability.contents, 2, 1)
	decoded = {
		"permitEncrypt": permitEncrypt,
		"permitDecrypt": permitDecrypt,
		"shift": shift,
	}
}

# Helper to extract all valid Caesar capabilities in the firmware image
all_valid_caesar_capabilities = [{"owner": owner, "capability": decode_caesar_capability(c)} | c = input.compartments[owner].imports[_]; is_sealed_caesar_capability(c)]

# Helper predicate to check that this is valid.
valid {
	# Make sure that the RTOS configuration is valid.
	data.rtos.valid

	# There are two things sealed with the Caesar capability type
	count([c | c = input.compartments[owner].imports[_]; is_sealed_caesar_capability(c)]) = 2

	# Both of them are valid Caesar capabilities
	count(all_valid_caesar_capabilities) = 2

	# Extract the producer and consumer's capabilities
	some producerCapability, consumerCapability
	producerCapability = [c | c = all_valid_caesar_capabilities[_]; c.owner = "producer"][0].capability
	consumerCapability = [c | c = all_valid_caesar_capabilities[_]; c.owner = "consumer"][0].capability

	# Check permissions
	producerCapability.permitEncrypt = true
	producerCapability.permitDecrypt = false
	consumerCapability.permitEncrypt = false
	consumerCapability.permitDecrypt = true

	# Make sure that the shift (encryption keys) are the same.
	producerCapability.shift = consumerCapability.shift

	# Make sure that only the producer and consumer compartments call the encrypt and decrypt functions
	data.compartment.compartment_call_allow_list("caesar", "caesar_encrypt.*", {"producer"})
	data.compartment.compartment_call_allow_list("caesar", "caesar_decrypt.*", {"consumer"})
	# Make sure that only the entry compartment calls the produce and consume functions
	data.compartment.compartment_call_allow_list("producer", "produce_message.*", {"entry"})
	data.compartment.compartment_call_allow_list("consumer", "consume_message.*", {"entry"})
}
