#include "jit_compiler.h"

#include "asm_x64_common.h"
#include "asm_x64_jit.h"
#include "defs_6502.h"
#include "util.h"

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

struct jit_compiler {
  uint8_t* p_mem_read;
  void* (*host_address_resolver)(void*, uint16_t);
  void* p_host_address_object;
  int debug;

  struct util_buffer* p_single_opcode_buf;

  int32_t reg_a;
  int32_t reg_x;
  int32_t reg_y;
  int32_t flag_carry;
  int32_t flag_decimal;
};

struct jit_opcode {
  int32_t opcode;
  int32_t value1;
  int32_t value2;
  int32_t optype;
  int32_t opmode;
};

struct jit_opcode_details {
  uint8_t opcode_6502;
  uint8_t len;
  int branches;
  uint16_t branch_target;
  struct jit_opcode first_uop;
  struct jit_opcode second_uop;
};

static const int32_t k_value_unknown = -1;

enum {
  k_opcode_countdown = 0x100,
  k_opcode_FLAGA,
  k_opcode_FLAGX,
  k_opcode_FLAGY,
  k_opcode_ADD_IMM,
  k_opcode_SAVE_CARRY,
  k_opcode_SAVE_OVERFLOW,
  k_opcode_STOA_IMM,
};

struct jit_compiler*
jit_compiler_create(uint8_t* p_mem_read,
                    void* (*host_address_resolver)(void*, uint16_t),
                    void* p_host_address_object,
                    int debug) {
  struct jit_compiler* p_compiler = malloc(sizeof(struct jit_compiler));
  if (p_compiler == NULL) {
    errx(1, "cannot alloc jit_compiler");
  }
  (void) memset(p_compiler, '\0', sizeof(struct jit_compiler));

  p_compiler->p_mem_read = p_mem_read;
  p_compiler->host_address_resolver = host_address_resolver;
  p_compiler->p_host_address_object = p_host_address_object;
  p_compiler->debug = debug;

  p_compiler->p_single_opcode_buf = util_buffer_create();

  return p_compiler;
}

void
jit_compiler_destroy(struct jit_compiler* p_compiler) {
  util_buffer_destroy(p_compiler->p_single_opcode_buf);
  free(p_compiler);
}

static void
jit_compiler_get_opcode_details(struct jit_compiler* p_compiler,
                                struct jit_opcode_details* p_details,
                                uint16_t addr_6502) {
  uint8_t opcode_6502;
  uint8_t optype;
  uint8_t opmode;
  uint16_t could_jump_target;

  uint8_t* p_mem_read = p_compiler->p_mem_read;
  uint16_t addr_plus_1 = (addr_6502 + 1);
  uint16_t addr_plus_2 = (addr_6502 + 2);
  int could_jump = 0;

  opcode_6502 = p_mem_read[addr_6502];
  optype = g_optypes[opcode_6502];
  opmode = g_opmodes[opcode_6502];

  p_details->opcode_6502 = opcode_6502;
  p_details->len = g_opmodelens[opmode];
  p_details->branches = g_opbranch[optype];

  p_details->first_uop.opcode = opcode_6502;
  p_details->first_uop.optype = optype;
  p_details->first_uop.opmode = opmode;
  p_details->second_uop.opcode = -1;

  switch (opmode) {
  case 0:
  case k_nil:
  case k_acc:
    break;
  case k_imm:
  case k_zpg:
  case k_zpx:
  case k_zpy:
    p_details->first_uop.value1 = p_mem_read[addr_plus_1];
    break;
  case k_rel:
    could_jump_target = ((int) addr_6502 +
                         2 +
                         (int8_t) p_mem_read[addr_plus_1]);
    p_details->branch_target = could_jump_target;
    could_jump = 1;
    break;
  case k_abs:
  case k_abx:
  case k_aby:
    p_details->first_uop.value1 = ((p_mem_read[addr_plus_2] << 8) |
                                   p_mem_read[addr_plus_1]);
    break;
  default:
    assert(0);
    break;
  }

  switch (optype) {
  case k_lda:
  case k_txa:
  case k_tya:
  case k_pla:
    p_details->second_uop.opcode = k_opcode_FLAGA;
    p_details->second_uop.optype = -1;
    p_details->second_uop.opmode = -1;
    break;
  case k_ldx:
  case k_tax:
  case k_tsx:
    p_details->second_uop.opcode = k_opcode_FLAGX;
    p_details->second_uop.optype = -1;
    p_details->second_uop.opmode = -1;
    break;
  case k_ldy:
  case k_tay:
    p_details->second_uop.opcode = k_opcode_FLAGY;
    p_details->second_uop.optype = -1;
    p_details->second_uop.opmode = -1;
    break;
  case k_jmp:
  case k_jsr:
    could_jump_target = p_details->first_uop.value1;
    could_jump = 1;
    break;
  case k_cmp:
  case k_cpx:
  case k_cpy:
    p_details->second_uop.opcode = k_opcode_SAVE_CARRY;
    p_details->second_uop.optype = -1;
    p_details->second_uop.opmode = -1;
    break;
  default:
    break;
  }

  if (could_jump) {
    p_details->first_uop.value1 =
        (int32_t) (size_t) p_compiler->host_address_resolver(
            p_compiler->p_host_address_object,
            could_jump_target);
  }
}

static void
jit_compiler_emit_opcode(struct util_buffer* p_dest_buf,
                         struct jit_opcode* p_opcode) {
  int opcode = p_opcode->opcode;
  int value1 = p_opcode->value1;
  int value2 = p_opcode->value2;

  switch (opcode) {
  case k_opcode_ADD_IMM:
    asm_x64_emit_jit_ADD_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case k_opcode_FLAGA:
    asm_x64_emit_jit_FLAGA(p_dest_buf);
    break;
  case k_opcode_FLAGX:
    asm_x64_emit_jit_FLAGX(p_dest_buf);
    break;
  case k_opcode_FLAGY:
    asm_x64_emit_jit_FLAGY(p_dest_buf);
    break;
  case k_opcode_SAVE_CARRY:
    asm_x64_emit_jit_SAVE_CARRY(p_dest_buf);
    break;
  case k_opcode_STOA_IMM:
    asm_x64_emit_jit_STOA_IMM(p_dest_buf, (uint16_t) value1, (uint8_t) value2);
    break;
  case 0x02:
    asm_x64_emit_instruction_EXIT(p_dest_buf);
    break;
  case 0x08:
    asm_x64_emit_instruction_PHP(p_dest_buf);
    break;
  case 0x10:
    asm_x64_emit_jit_BPL(p_dest_buf, (void*) (size_t) value1);
    break;
  case 0x18:
    asm_x64_emit_instruction_CLC(p_dest_buf);
    break;
  case 0x28:
    asm_x64_emit_instruction_PLP(p_dest_buf);
    break;
  case 0x30:
    asm_x64_emit_jit_BMI(p_dest_buf, (void*) (size_t) value1);
    break;
  case 0x38:
    asm_x64_emit_instruction_SEC(p_dest_buf);
    break;
  case 0x4C:
    asm_x64_emit_jit_JMP(p_dest_buf, (void*) (size_t) value1);
    break;
  case 0x69:
    asm_x64_emit_jit_ADC_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case 0x88:
    asm_x64_emit_instruction_DEY(p_dest_buf);
    break;
  case 0x90:
    asm_x64_emit_jit_BCC(p_dest_buf, (void*) (size_t) value1);
    break;
  case 0x9A:
    asm_x64_emit_instruction_TXS(p_dest_buf);
    break;
  case 0x9D:
    asm_x64_emit_jit_STA_ABX(p_dest_buf, (uint16_t) value1);
    break;
  case 0xA0:
    asm_x64_emit_jit_LDY_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case 0xA2:
    asm_x64_emit_jit_LDX_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case 0xA8:
    asm_x64_emit_instruction_TAY(p_dest_buf);
    break;
  case 0xA9:
    asm_x64_emit_jit_LDA_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case 0xAA:
    asm_x64_emit_instruction_TAX(p_dest_buf);
    break;
  case 0xAD:
    asm_x64_emit_jit_LDA_ABS(p_dest_buf, (uint16_t) value1);
    break;
  case 0xB0:
    asm_x64_emit_jit_BCS(p_dest_buf, (void*) (size_t) value1);
    break;
  case 0xB8:
    asm_x64_emit_instruction_CLV(p_dest_buf);
    break;
  case 0xBA:
    asm_x64_emit_instruction_TSX(p_dest_buf);
    break;
  case 0xBD:
    asm_x64_emit_jit_LDA_ABX(p_dest_buf, (uint16_t) value1);
    break;
  case 0xC0:
    asm_x64_emit_jit_CPY_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case 0xC8:
    asm_x64_emit_instruction_INY(p_dest_buf);
    break;
  case 0xC9:
    asm_x64_emit_jit_CMP_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case 0xCA:
    asm_x64_emit_instruction_DEX(p_dest_buf);
    break;
  case 0xD0:
    asm_x64_emit_jit_BNE(p_dest_buf, (void*) (size_t) value1);
    break;
  case 0xD8:
    asm_x64_emit_instruction_CLD(p_dest_buf);
    break;
  case 0xE0:
    asm_x64_emit_jit_CPX_IMM(p_dest_buf, (uint8_t) value1);
    break;
  case 0xE6:
    asm_x64_emit_jit_INC_ZPG(p_dest_buf, (uint8_t) value1);
    break;
  case 0xE8:
    asm_x64_emit_instruction_INX(p_dest_buf);
    break;
  case 0xF0:
    asm_x64_emit_jit_BEQ(p_dest_buf, (void*) (size_t) value1);
    break;
  case 0xF2:
    asm_x64_emit_instruction_CRASH(p_dest_buf);
    break;
  case 0xF8:
    asm_x64_emit_instruction_SED(p_dest_buf);
    break;
  default:
    asm_x64_emit_instruction_ILLEGAL(p_dest_buf);
    break;
  }
}

static void
jit_compiler_process_opcode(struct jit_compiler* p_compiler,
                            struct util_buffer* p_dest_buf,
                            struct jit_opcode* p_opcode) {
  int32_t opcode = p_opcode->opcode;
  int32_t optype = p_opcode->optype;
  int32_t value1 = p_opcode->value1;
  int32_t opreg = -1;
  int32_t changes_carry = 0;

  if (optype != -1) {
    opreg = g_optype_sets_register[optype];
    changes_carry = g_optype_changes_carry[optype];
  }

  /* Re-write the opcode if we have an optimization opportunity. */
  switch (opcode) {
  case 0x69: /* ADC imm */
    if (p_compiler->flag_carry == 0) {
      p_opcode->opcode = k_opcode_ADD_IMM;
    }
    break;
  case 0x85: /* STA zpg */
  case 0x8D: /* STA abs */
    if (p_compiler->reg_a != k_value_unknown) {
      p_opcode->opcode = k_opcode_STOA_IMM;
      p_opcode->value2 = p_compiler->reg_a;
    }
    break;
  default:
    break;
  }

  jit_compiler_emit_opcode(p_dest_buf, p_opcode);

  /* Update known state of registers, flags, etc. */
  switch (opreg) {
  case k_a:
    p_compiler->reg_a = k_value_unknown;
    break;
  case k_x:
    p_compiler->reg_x = k_value_unknown;
    break;
  case k_y:
    p_compiler->reg_y = k_value_unknown;
    break;
  default:
    break;
  }

  if (changes_carry) {
    p_compiler->flag_carry = k_value_unknown;
  }

  switch (opcode) {
  case 0x18: /* CLC */
    p_compiler->flag_carry = 0;
    break;
  case 0x38: /* SEC */
    p_compiler->flag_carry = 1;
    break;
  case 0xA0: /* LDY imm */
    p_compiler->reg_y = value1;
    break;
  case 0xA2: /* LDX imm */
    p_compiler->reg_x = value1;
    break;
  case 0xA9: /* LDA imm */
    p_compiler->reg_a = value1;
    break;
  case 0xD8: /* CLD */
    p_compiler->flag_decimal = 0;
    break;
  case 0xF8: /* SED */
    p_compiler->flag_decimal = 1;
    break;
  default:
    break;
  }
}

void
jit_compiler_compile_block(struct jit_compiler* p_compiler,
                           struct util_buffer* p_buf,
                           uint16_t addr_6502) {
  struct jit_opcode_details opcode_details = {};

  struct util_buffer* p_single_opcode_buf = p_compiler->p_single_opcode_buf;

  p_compiler->reg_a = k_value_unknown;
  p_compiler->reg_x = k_value_unknown;
  p_compiler->reg_y = k_value_unknown;
  p_compiler->flag_carry = k_value_unknown;
  p_compiler->flag_decimal = k_value_unknown;

  while (1) {
    uint8_t single_opcode_buffer[64];

    int branches_always = 0;

    util_buffer_setup(p_single_opcode_buf,
                      &single_opcode_buffer[0],
                      sizeof(single_opcode_buffer));

    util_buffer_set_base_address(p_single_opcode_buf,
                                 (util_buffer_get_base_address(p_buf) +
                                     util_buffer_get_pos(p_buf)));

    if (p_compiler->debug) {
      asm_x64_emit_jit_call_debug(p_single_opcode_buf, addr_6502);
    }

    jit_compiler_get_opcode_details(p_compiler, &opcode_details, addr_6502);

    if (opcode_details.branches == k_bra_y) {
      branches_always = 1;
    }

    jit_compiler_process_opcode(p_compiler,
                                p_single_opcode_buf,
                                &opcode_details.first_uop);
    if (opcode_details.second_uop.opcode != -1) {
      jit_compiler_process_opcode(p_compiler,
                                  p_single_opcode_buf,
                                  &opcode_details.second_uop);
    }

    util_buffer_append(p_buf, p_single_opcode_buf);

    if (branches_always) {
      break;
    }

    addr_6502 += opcode_details.len;
  }
}
