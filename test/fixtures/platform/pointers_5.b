main() {
    auto *a,b,*c,e;
    b = 100;
    e = 50;
    a = &b;
    c = &e;
    *c = 10;
    *a = *c;
}