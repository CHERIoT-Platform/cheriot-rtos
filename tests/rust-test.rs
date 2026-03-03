#![no_std]

#[cfg(not(target_family = "cheriot"))]
compile_error!("This crate can only compile to CHERIoT!");

extern crate alloc;

/// An allocator based on the CHERIoT RTOS allocator.
struct _RustTestGlobalAllocator;

unsafe impl alloc::alloc::GlobalAlloc for _RustTestGlobalAllocator {
    unsafe fn alloc(&self, layout: core::alloc::Layout) -> *mut u8 {
        panic!()
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: core::alloc::Layout) {
        panic!()
    }
}

#[global_allocator]
static _RUST_TEST_GLOBAL_ALLOCATOR: _RustTestGlobalAllocator = _RustTestGlobalAllocator;

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

#[unsafe(no_mangle)]
extern "C" fn call_rust() -> core::ffi::c_int {
    core::ffi::c_int::from(1)
}
