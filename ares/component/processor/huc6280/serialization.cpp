auto HuC6280::serialize(serializer& s) -> void {
  s(r.a);
  s(r.x);
  s(r.y);
  s(r.s);
  s(r.pc);
  s(r.mpr);
  s(r.mpl);
  s(r.cs);
  s(r.p.c);
  s(r.p.z);
  s(r.p.i);
  s(r.p.d);
  s(r.p.b);
  s(r.p.t);
  s(r.p.v);
  s(r.p.n);
  s(blockMove);
  s(random);
}