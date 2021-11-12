#ifndef BEEBJIT_ASM_DEFS_HOST_H
#define BEEBJIT_ASM_DEFS_HOST_H

#define K_BBC_MEM_RAW_ADDR                      0x30f008000ull
#define K_BBC_MEM_READ_IND_ADDR                 0x310008000ull
#define K_BBC_MEM_WRITE_IND_ADDR                0x311008000ull
#define K_BBC_MEM_READ_FULL_ADDR                0x312008000ull
#define K_BBC_MEM_WRITE_FULL_ADDR               0x313008000ull
#define K_BBC_MEM_OFFSET_TO_WRITE_IND           0x01000000
#define K_BBC_MEM_OFFSET_TO_READ_FULL           0x02000000
#define K_BBC_MEM_OFFSET_TO_WRITE_FULL          0x03000000
#define K_BBC_MEM_OFFSET_READ_TO_WRITE          0x01000000
#define K_BBC_MEM_OS_ROM_OFFSET                 0xC000
#define K_BBC_MEM_INACCESSIBLE_OFFSET           0xF000
#define K_BBC_MEM_INACCESSIBLE_LEN              0x1000
#define K_6502_ADDR_SPACE_SIZE                  0x10000
#define K_6502_VECTOR_IRQ                       0xFFFE
#define K_ASM_TABLE_ADDR                        0x50000000
#define K_ASM_TABLE_6502_FLAGS_TO_X64           0x50000000
#define K_ASM_TABLE_6502_FLAGS_TO_MASK          0x50000100
#define K_ASM_TABLE_X64_FLAGS_TO_6502           0x50000200
#define K_ASM_TABLE_PAGE_WRAP_CYCLE_INV         0x50000300
#define K_ASM_TABLE_OF_TO_6502                  0x50000500
#define K_ASM_TABLE_6502_FLAGS_TO_X64_OFFSET    0
#define K_ASM_TABLE_6502_FLAGS_TO_MASK_OFFSET   0x100
#define K_ASM_TABLE_X64_FLAGS_TO_6502_OFFSET    0x200
#define K_ASM_TABLE_PAGE_WRAP_CYCLE_INV_OFFSET  0x300
#define K_ASM_TABLE_OF_TO_6502_OFFSET           0x500

#define K_CONTEXT_OFFSET_STATE_6502             8
#define K_CONTEXT_OFFSET_DEBUG_CALLBACK         16
#define K_CONTEXT_OFFSET_DEBUG_OBJECT           24
#define K_CONTEXT_OFFSET_INTERP_CALLBACK        32
#define K_CONTEXT_OFFSET_INTERP_OBJECT          40
#define K_CONTEXT_OFFSET_ABI_END                48
#define K_CONTEXT_OFFSET_DRIVER_END             (K_CONTEXT_OFFSET_ABI_END + 56)

#define K_STATE_6502_OFFSET_REG_A               0
#define K_STATE_6502_OFFSET_REG_X               4
#define K_STATE_6502_OFFSET_REG_Y               8
#define K_STATE_6502_OFFSET_REG_S               12
#define K_STATE_6502_OFFSET_REG_PC              16
#define K_STATE_6502_OFFSET_REG_FLAGS           20
#define K_STATE_6502_OFFSET_REG_IRQ_FIRE        24
#define K_STATE_6502_OFFSET_REG_HOST_PC         28
#define K_STATE_6502_OFFSET_REG_HOST_FLAGS      32
#define K_STATE_6502_OFFSET_REG_HOST_VALUE      36

#endif /* BEEBJIT_ASM_DEFS_HOST_H */
