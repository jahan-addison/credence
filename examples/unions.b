///////////////////////////////////////////////////////////////////////////
// Example of a tagged union. A union holds one tag at a time, the type of
// each tag is the type of its initial value, and a tag with a list of
// values is a tuple of their types.
//
// `is` tests the active tag of a union variable, and binds the values of
// its tag to locals.
///////////////////////////////////////////////////////////////////////////

union value_t {
  integer 0,
  real 0.0f,
  text "",
  pair (0, 0)
};

union value_t last;

main() {
  auto width, height, *name;
  extrn last;
  union value_t v;

  v.text = "a unit square";
  if (v is value_t.text(name))
    printf("%s\n", name);

  // each value of a tuple tag is assigned at its offset
  v.pair[0] = 4;
  v.pair[1] = 3;
  if (v is value_t.pair(width, height))
    printf("area %d\n", width * height);

  last.real = 1.5f;
  if (last is value_t.real)
    printf("real %f\n", last.real);
}
