//! Example crate for testing bindgen bindings

fn main() {
    let mut x: hello_bindgen::android_MotionEvent;
    x._base.mDeviceId = 10;
    println!("Hello world");
    // let mut x = hello_bindgen::foo { x: 2 };

    // SAFETY:
    // This is a valid safety comment
    // unsafe { hello_bindgen::hello() }
    // unsafe { hello_bindgen::fizz(1, &mut x as *mut hello_bindgen::foo) }
}
