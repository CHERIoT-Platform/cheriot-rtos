#![no_std]

#[cfg(not(target_family = "cheriot"))]
compile_error!("This crate can only compile to CHERIoT!");

extern crate alloc;

/// An allocator based on the CHERIoT RTOS allocator.
struct _RustTestGlobalAllocator;

unsafe extern "C" {
    pub fn __rust_alloc(bytes: u32) -> *mut core::ffi::c_void;
    pub fn __rust_free(ptr: *mut core::ffi::c_void);
}

unsafe impl alloc::alloc::GlobalAlloc for _RustTestGlobalAllocator {
    unsafe fn alloc(&self, layout: core::alloc::Layout) -> *mut u8 {
        unsafe { __rust_alloc(layout.size() as _) as _ }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: core::alloc::Layout) {
        unsafe { __rust_free(ptr as _) }
    }
}

#[global_allocator]
static _RUST_TEST_GLOBAL_ALLOCATOR: _RustTestGlobalAllocator = _RustTestGlobalAllocator;

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
extern "C" {
    fn print(buf: *const core::ffi::c_char);
}

#[unsafe(no_mangle)]
extern "C" fn __call_rust() {
    let str = alloc::ffi::CString::new("hello\n").unwrap();

    unsafe {
        print(str.as_ptr());
    }

    drop(str);
}
