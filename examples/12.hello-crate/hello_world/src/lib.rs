//! A very bare-bones example of using Rust on CHERIoT.

// Of course, we can't use `std`.
#![no_std]

#[cfg(not(target_family = "cheriot"))]
compiler_error!("We only want to produce code for CHERIoT now!");

// We can use `core` and `alloc`, though!
extern crate alloc;
extern crate core;

// Not having `std` means that, for now, we can't interact with the filesystem or print to the
// screen in pure Rust: for the things that the CHERIoT RTOS allows us to do but don't have a way to
// reproduce in pure Rust now, we must use FFI.
//
// The runner.c file in this example exports useful functions:
unsafe extern "C" {
    pub fn cheriot_print_str(v: *const core::ffi::c_char);
    pub fn cheriot_alloc(bytes: u32) -> *mut core::ffi::c_void;
    pub fn cheriot_free(ptr: *mut core::ffi::c_void);
    pub fn cheriot_panic();
    pub fn cheriot_random_byte() -> u8;
}

// 1. Let's implement the panic handler.
#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    let str = alloc::string::ToString::to_string(&info.message());
    let str = <alloc::ffi::CString as core::str::FromStr>::from_str(&str).unwrap();
    unsafe {
        cheriot_print_str(str.as_ptr());
        drop(str);
        cheriot_panic()
    };
    loop {}
}

// 2. Let's implement the allocator.

/// An allocator based on the CHERIoT RTOS allocator.
struct CHERIoTRTOSAllocator;

unsafe impl alloc::alloc::GlobalAlloc for CHERIoTRTOSAllocator {
    unsafe fn alloc(&self, layout: core::alloc::Layout) -> *mut u8 {
        unsafe { cheriot_alloc(layout.size() as _) as _ }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: core::alloc::Layout) {
        unsafe { cheriot_free(ptr as _) }
    }
}

#[global_allocator]
static CHERIOT_RTOS_ALLOCATOR: CHERIoTRTOSAllocator = CHERIoTRTOSAllocator;

// 3. Let's reproduce the `print!` macros.
#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => (_print(format_args!($($arg)*)));
}

#[macro_export]
macro_rules! println {
    () => (print!("\n"));
    ($($arg:tt)*) => (print!("{}\n", format_args!($($arg)*)));
}

#[doc(hidden)]
pub fn _print(args: core::fmt::Arguments) {
    let str = alloc::string::ToString::to_string(&args);
    let str = alloc::ffi::CString::new(str).unwrap();

    unsafe {
        cheriot_print_str(str.as_ptr());
    }

    drop(str);
}

// 4. Let's say hi!
#[unsafe(no_mangle)]
pub extern "C" fn do_it(input: i32) -> u32 {
    let n = if input < 0 {
        let mut seed: [u8; 32] = [0; 32];
        for i in 0..32 {
            unsafe {
                seed[i] = cheriot_random_byte();
            }
        }

        let mut rng = <rand::rngs::SmallRng as rand::SeedableRng>::from_seed(seed);

        rand::Rng::random::<u32>(&mut rng) % 1000
    } else {
        input as u32
    };

    // We have ranges and vectors!
    let mut v: alloc::vec::Vec<u32> = (1..=n).collect();

    for x in &mut v {
        *x = *x * *x;
    }

    let sum: u32 = v.iter().sum();

    println!(
        "Rust: received {}, built vector of length {}, sum of squares = {}",
        input,
        v.len(),
        sum
    );

    sum % 1000
}
