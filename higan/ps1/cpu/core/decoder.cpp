#define OP pipeline.instruction
#define RD core.r[RDn]
#define RT core.r[RTn]
#define RS core.r[RSn]

#define jp(id, name, ...) case id: return decoder##name(__VA_ARGS__)
#define op(id, name, ...) case id: return instruction##name(__VA_ARGS__)
#define br(id, name, ...) case id: return instruction##name(__VA_ARGS__)

auto CPU::decoderEXECUTE() -> void {
  #define DECODER_EXECUTE
  #include "decoder.hpp"
}

auto CPU::decoderSPECIAL() -> void {
  #define DECODER_SPECIAL
  #include "decoder.hpp"
}

auto CPU::decoderREGIMM() -> void {
  #define DECODER_REGIMM
  #include "decoder.hpp"
}

auto CPU::decoderSCC() -> void {
  #define DECODER_SCC
  #include "decoder.hpp"
}

auto CPU::decoderGTE() -> void {
  #define DECODER_GTE
  #include "decoder.hpp"
}

auto CPU::instructionCOP1() -> void {
  if(!scc.status.enable.coprocessor1) exception.coprocessor1();
}

auto CPU::instructionCOP3() -> void {
  if(!scc.status.enable.coprocessor3) exception.coprocessor3();
}

auto CPU::instructionINVALID() -> void {
  exception.reservedInstruction();
}

#undef jp
#undef op
#undef br

#undef OP
#undef RD
#undef RT
#undef RS
