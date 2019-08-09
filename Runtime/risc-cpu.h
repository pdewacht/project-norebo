#ifndef RISC_CPU_H
#define RISC_CPU_H

struct RISC {
  uint32_t PC;
  uint32_t R[16];
  uint32_t H;
  bool     Z, N, C, V;
};

struct RISC_IO {
  uint32_t (*read_program)(struct RISC *risc, uint32_t adr);
  uint32_t (*read_word)(struct RISC *risc, uint32_t adr);
  uint32_t (*read_byte)(struct RISC *risc, uint32_t adr);
  void (*write_word)(struct RISC *risc, uint32_t adr, uint32_t val);
  void (*write_byte)(struct RISC *risc, uint32_t adr, uint32_t val);
};

void risc_run(const struct RISC_IO *io, struct RISC *risc);

#endif  // RISC_CPU_H
