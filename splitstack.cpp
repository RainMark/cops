#include <stdio.h>

int foo(int x1) {
    printf("%d\n", x1);
    return 0;
}

int bar(int x2) {
    return foo(x2);
}

int main() {
    return bar(0);
}
