package hello_compartment

# For non-simulation platforms, we only allow the debug library to access the
# UART
uart_access_valid {
	data.compartment.mmio_allow_list("uart", {"debug"})
}

# For simulation platforms, we allow the scheduler to access the UART as well
uart_access_valid {
	data.compartment.mmio_allow_list("uart", {"debug", "scheduler"})
	data.board.simulation
}


# Check that the UART is accessible only to the authorised libraries and
# compartments and that only the `uart` compartment can call the library that
# has direct access.
valid {
	uart_access_valid
	data.compartment.compartment_allow_list("debug", {"uart"})
}

