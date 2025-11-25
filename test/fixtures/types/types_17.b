main() {
    auto *a,b,*c,*d;
    b = 100;
    c = &b;
    a = c;
    *a = 10;
    *c = *d;
}