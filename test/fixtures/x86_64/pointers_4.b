main() {
    auto *a,b,*c;
    b = 100;
    a = &b;
    c = a;
    //*c = 10 + 10 + 10 + 10;
    *a = *c;
}