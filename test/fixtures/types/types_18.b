main() {
    // should pass
    // "a" and "c" address the same word, so "*c = *a" is a
    // dereference on both sides of the assignment
    auto *a,b,*c,*d;
    b = 100;
    c = &b;
    a = c;
    *a = 10;
    *c = *a;
}