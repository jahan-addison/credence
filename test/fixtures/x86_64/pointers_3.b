main() {
    auto *a,b,*c;
    b = 100;
    c = &b;
    a = c;
    *a = 10;
    *c = *a;
}