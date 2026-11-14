pick(n) {
  extrn mess;
  printf("%s\n", mess[n]);
}

main() {
  pick(2);
}

mess [3] "too bad", "tough luck", "sorry";
