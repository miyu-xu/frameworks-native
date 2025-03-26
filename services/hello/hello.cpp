#include <stdio.h>
#include "hello.hpp"

void fizz(int i, foo* my_foo){
    printf("hello from c! i = %i, my_foo->x = %i\n", i, my_foo->x);
}

void hello(android::MotionEvent e) {
    printf("dev: %d", e.getDeviceId());
}