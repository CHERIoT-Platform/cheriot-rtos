This checks for a specific bug fixed in 2024/11, where it was possible to
invoke the scheduler's exception entrypoint, which must run with IRQs deferred,
with IRQs enabled if a thread exited from its initial activation via a slightly
unusual path.

See https://github.com/CHERIoT-Platform/cheriot-rtos/pull/346
