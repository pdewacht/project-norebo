#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "risc-cpu.h"

enum {
  MOV, LSL, ASR, ROR,
  AND, ANN, IOR, XOR,
  ADD, SUB, MUL, DIV,
  FAD, FSB, FML, FDV,
};

static void risc_single_step(const struct RISC_IO *risc_io, struct RISC *risc);
static void risc_set_register(struct RISC *risc, int reg, uint32_t value);
static uint32_t fp_add(uint32_t x, uint32_t y, bool u, bool v);
static uint32_t fp_mul(uint32_t x, uint32_t y);
static uint32_t fp_div(uint32_t x, uint32_t y);
static struct idiv { uint32_t quot, rem; } idiv(uint32_t x, uint32_t y, bool signed_div);


void risc_run(const struct RISC_IO *io, struct RISC *risc) {
  for (;;) {
    risc_single_step(io, risc);
  }
}

static void risc_single_step(const struct RISC_IO *io, struct RISC *risc) {
  uint32_t ir = io->read_program(risc, risc->PC);
  risc->PC++;

  const uint32_t pbit = 0x80000000;
  const uint32_t qbit = 0x40000000;
  const uint32_t ubit = 0x20000000;
  const uint32_t vbit = 0x10000000;

  if ((ir & pbit) == 0) {
    // Register instructions
    uint32_t a  = (ir & 0x0F000000) >> 24;
    uint32_t b  = (ir & 0x00F00000) >> 20;
    uint32_t op = (ir & 0x000F0000) >> 16;
    uint32_t im =  ir & 0x0000FFFF;
    uint32_t c  =  ir & 0x0000000F;

    uint32_t a_val, b_val, c_val;
    b_val = risc->R[b];
    if ((ir & qbit) == 0) {
      c_val = risc->R[c];
    } else if ((ir & vbit) == 0) {
      c_val = im;
    } else {
      c_val = 0xFFFF0000 | im;
    }

    switch (op) {
      case MOV: {
        if ((ir & ubit) == 0) {
          a_val = c_val;
        } else if ((ir & qbit) != 0) {
          a_val = c_val << 16;
        } else if ((ir & vbit) != 0) {
          a_val = 0xD0 |   // ???
            (risc->N * 0x80000000U) |
            (risc->Z * 0x40000000U) |
            (risc->C * 0x20000000U) |
            (risc->V * 0x10000000U);
        } else {
          a_val = risc->H;
        }
        break;
      }
      case LSL: {
        a_val = b_val << (c_val & 31);
        break;
      }
      case ASR: {
        a_val = ((int32_t)b_val) >> (c_val & 31);
        break;
      }
      case ROR: {
        a_val = (b_val >> (c_val & 31)) | (b_val << (-c_val & 31));
        break;
      }
      case AND: {
        a_val = b_val & c_val;
        break;
      }
      case ANN: {
        a_val = b_val & ~c_val;
        break;
      }
      case IOR: {
        a_val = b_val | c_val;
        break;
      }
      case XOR: {
        a_val = b_val ^ c_val;
        break;
      }
      case ADD: {
        a_val = b_val + c_val;
        if ((ir & ubit) != 0) {
          a_val += risc->C;
        }
        risc->C = a_val < b_val;
        risc->V = ((a_val ^ c_val) & (a_val ^ b_val)) >> 31;
        break;
      }
      case SUB: {
        a_val = b_val - c_val;
        if ((ir & ubit) != 0) {
          a_val -= risc->C;
        }
        risc->C = a_val > b_val;
        risc->V = ((b_val ^ c_val) & (a_val ^ b_val)) >> 31;
        break;
      }
      case MUL: {
        uint64_t tmp;
        if ((ir & ubit) == 0) {
          tmp = (int64_t)(int32_t)b_val * (int64_t)(int32_t)c_val;
        } else {
          tmp = (uint64_t)b_val * (uint64_t)c_val;
        }
        a_val = (uint32_t)tmp;
        risc->H = (uint32_t)(tmp >> 32);
        break;
      }
      case DIV: {
        if ((int32_t)c_val > 0) {
          if ((ir & ubit) == 0) {
            a_val = (int32_t)b_val / (int32_t)c_val;
            risc->H = (int32_t)b_val % (int32_t)c_val;
            if ((int32_t)risc->H < 0) {
              a_val--;
              risc->H += c_val;
            }
          } else {
            a_val = b_val / c_val;
            risc->H = b_val % c_val;
          }
        } else {
          struct idiv q = idiv(b_val, c_val, ir & ubit);
          a_val = q.quot;
          risc->H = q.rem;
        }
        break;
      }
      case FAD: {
        a_val = fp_add(b_val, c_val, ir & ubit, ir & vbit);
        break;
      }
      case FSB: {
        a_val = fp_add(b_val, c_val ^ 0x80000000, ir & ubit, ir & vbit);
        break;
      }
      case FML: {
        a_val = fp_mul(b_val, c_val);
        break;
      }
      case FDV: {
        a_val = fp_div(b_val, c_val);
        break;
      }
      default: {
        abort();  // unreachable
      }
    }
    risc_set_register(risc, a, a_val);
  }
  else if ((ir & qbit) == 0) {
    // Memory instructions
    uint32_t a = (ir & 0x0F000000) >> 24;
    uint32_t b = (ir & 0x00F00000) >> 20;
    int32_t off = ir & 0x000FFFFF;
    off = (off ^ 0x00080000) - 0x00080000;  // sign-extend

    uint32_t address = risc->R[b] + off;
    if ((ir & ubit) == 0) {
      uint32_t a_val;
      if ((ir & vbit) == 0) {
        a_val = io->read_word(risc, address);
      } else {
        a_val = io->read_byte(risc, address);
      }
      risc_set_register(risc, a, a_val);
    } else {
      if ((ir & vbit) == 0) {
        io->write_word(risc, address, risc->R[a]);
      } else {
        io->write_byte(risc, address, (uint8_t)risc->R[a]);
      }
    }
  }
  else {
    // Branch instructions
    bool t = (ir >> 27) & 1;
    switch ((ir >> 24) & 7) {
      case 0: t ^= risc->N; break;
      case 1: t ^= risc->Z; break;
      case 2: t ^= risc->C; break;
      case 3: t ^= risc->V; break;
      case 4: t ^= risc->C | risc->Z; break;
      case 5: t ^= risc->N ^ risc->V; break;
      case 6: t ^= (risc->N ^ risc->V) | risc->Z; break;
      case 7: t ^= true; break;
      default: abort();  // unreachable
    }
    if (t) {
      if ((ir & vbit) != 0) {
        risc_set_register(risc, 15, risc->PC * 4);
      }
      if ((ir & ubit) == 0) {
        uint32_t c = ir & 0x0000000F;
        risc->PC = risc->R[c] / 4;
      } else {
        int32_t off = ir & 0x00FFFFFF;
        off = (off ^ 0x00800000) - 0x00800000;  // sign-extend
        risc->PC = risc->PC + off;
      }
    }
  }
}

static void risc_set_register(struct RISC *risc, int reg, uint32_t value) {
  risc->R[reg] = value;
  risc->Z = value == 0;
  risc->N = (int32_t)value < 0;
}


static uint32_t fp_add(uint32_t x, uint32_t y, bool u, bool v) {
  bool xs = (x & 0x80000000) != 0;
  uint32_t xe;
  int32_t x0;
  if (!u) {
    xe = (x >> 23) & 0xFF;
    uint32_t xm = ((x & 0x7FFFFF) << 1) | 0x1000000;
    x0 = (int32_t)(xs ? -xm : xm);
  } else {
    xe = 150;
    x0 = (int32_t)(x & 0x00FFFFFF) << 8 >> 7;
  }

  bool ys = (y & 0x80000000) != 0;
  uint32_t ye = (y >> 23) & 0xFF;
  uint32_t ym = ((y & 0x7FFFFF) << 1);
  if (!u && !v) ym |= 0x1000000;
  int32_t y0 = (int32_t)(ys ? -ym : ym);

  uint32_t e0;
  int32_t x3, y3;
  if (ye > xe) {
    uint32_t shift = ye - xe;
    e0 = ye;
    x3 = shift > 31 ? x0 >> 31 : x0 >> shift;
    y3 = y0;
  } else {
    uint32_t shift = xe - ye;
    e0 = xe;
    x3 = x0;
    y3 = shift > 31 ? y0 >> 31 : y0 >> shift;
  }

  uint32_t sum = ((xs << 26) | (xs << 25) | (x3 & 0x01FFFFFF))
    + ((ys << 26) | (ys << 25) | (y3 & 0x01FFFFFF));

  uint32_t s = (((sum & (1 << 26)) ? -sum : sum) + 1) & 0x07FFFFFF;

  uint32_t e1 = e0 + 1;
  uint32_t t3 = s >> 1;
  if ((s & 0x3FFFFFC) != 0) {
    while ((t3 & (1<<24)) == 0) {
      t3 <<= 1;
      e1--;
    }
  } else {
    t3 <<= 24;
    e1 -= 24;
  }

  if (v) {
    return (int32_t)(sum << 5) >> 6;
  } else if ((x & 0x7FFFFFFF) == 0) {
    return !u ? y : 0;
  } else if ((y & 0x7FFFFFFF) == 0) {
    return x;
  } else if ((t3 & 0x01FFFFFF) == 0 || (e1 & 0x100) != 0) {
    return 0;
  } else {
    return ((sum & 0x04000000) << 5) | (e1 << 23) | ((t3 >> 1) & 0x7FFFFF);
  }
}

static uint32_t fp_mul(uint32_t x, uint32_t y) {
  uint32_t sign = (x ^ y) & 0x80000000;
  uint32_t xe = (x >> 23) & 0xFF;
  uint32_t ye = (y >> 23) & 0xFF;

  uint32_t xm = (x & 0x7FFFFF) | 0x800000;
  uint32_t ym = (y & 0x7FFFFF) | 0x800000;
  uint64_t m = (uint64_t)xm * ym;

  uint32_t e1 = (xe + ye) - 127;
  uint32_t z0;
  if ((m & (1ULL << 47)) != 0) {
    e1++;
    z0 = ((m >> 23) + 1) & 0xFFFFFF;
  } else {
    z0 = ((m >> 22) + 1) & 0xFFFFFF;
  }

  if (xe == 0 || ye == 0) {
    return 0;
  } else if ((e1 & 0x100) == 0) {
    return sign | ((e1 & 0xFF) << 23) | (z0 >> 1);
  } else if ((e1 & 0x80) == 0) {
    return sign | (0xFF << 23) | (z0 >> 1);
  } else {
    return 0;
  }
}

static uint32_t fp_div(uint32_t x, uint32_t y) {
  uint32_t sign = (x ^ y) & 0x80000000;
  uint32_t xe = (x >> 23) & 0xFF;
  uint32_t ye = (y >> 23) & 0xFF;

  uint32_t xm = (x & 0x7FFFFF) | 0x800000;
  uint32_t ym = (y & 0x7FFFFF) | 0x800000;
  uint32_t q1 = (uint32_t)(xm * (1ULL << 25) / ym);

  uint32_t e1 = (xe - ye) + 126;
  uint32_t q2;
  if ((q1 & (1 << 25)) != 0) {
    e1++;
    q2 = (q1 >> 1) & 0xFFFFFF;
  } else {
    q2 = q1 & 0xFFFFFF;
  }
  uint32_t q3 = q2 + 1;

  if (xe == 0) {
    return 0;
  } else if (ye == 0) {
    return sign | (0xFF << 23);
  } else if ((e1 & 0x100) == 0) {
    return sign | ((e1 & 0xFF) << 23) | (q3 >> 1);
  } else if ((e1 & 0x80) == 0) {
    return sign | (0xFF << 23) | (q2 >> 1);
  } else {
    return 0;
  }
}

static struct idiv idiv(uint32_t x, uint32_t y, bool signed_div) {
  bool sign = ((int32_t)x < 0) & signed_div;
  uint32_t x0 = sign ? -x : x;

  uint64_t RQ = x0;
  for (int S = 0; S < 32; ++S) {
    uint32_t w0 = (uint32_t)(RQ >> 31);
    uint32_t w1 = w0 - y;
    if ((int32_t)w1 < 0) {
      RQ = ((uint64_t)w0 << 32) | ((RQ & 0x7FFFFFFFU) << 1);
    } else {
      RQ = ((uint64_t)w1 << 32) | ((RQ & 0x7FFFFFFFU) << 1) | 1;
    }
  }

  struct idiv d = { (uint32_t)RQ, (uint32_t)(RQ >> 32) };
  if (sign) {
    d.quot = -d.quot;
    if (d.rem) {
      d.quot -= 1;
      d.rem = y - d.rem;
    }
  }
  return d;
}
