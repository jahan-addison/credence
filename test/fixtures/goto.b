main() {
  auto sign;
  if(sign)  goto syntax;
  else return(5);
syntax:
  sign = 10;
  return (sign);
}