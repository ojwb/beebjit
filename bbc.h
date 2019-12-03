#ifndef BEEBJIT_BBC_H
#define BEEBJIT_BBC_H

#include <stddef.h>
#include <stdint.h>

struct cpu_driver;
struct keyboard_struct;
struct sound_struct;
struct state_6502;
struct via_struct;
struct video_struct;

enum {
  k_bbc_rom_size = 0x4000,
  k_bbc_ram_size = 0x8000,
};
enum {
  k_bbc_num_roms = 16,
  k_bbc_default_dfs_rom_slot = 0xE,
  k_bbc_default_lang_rom_slot = 0xF,
};
enum {
  k_bbc_registers_start = 0xFC00,
  k_bbc_registers_len = 0x300,
};
enum {
  k_message_exited = 1,
  k_message_vsync = 2,
  k_message_render_done = 3,
};
enum {
  k_bbc_max_ssd_disc_size = (256 * 10 * 80),
  k_bbc_max_dsd_disc_size = (256 * 10 * 80 * 2),
};

struct bbc_struct;

struct bbc_struct* bbc_create(int mode,
                              uint8_t* p_os_rom,
                              int debug_flag,
                              int run_flag,
                              int print_flag,
                              int fast_flag,
                              int accurate_flag,
                              const char* p_opt_flags,
                              const char* p_log_flags,
                              uint16_t debug_stop_addr);
void bbc_destroy(struct bbc_struct* p_bbc);

void bbc_power_on_reset(struct bbc_struct* p_bbc);
void bbc_load_rom(struct bbc_struct* p_bbc,
                  uint8_t index,
                  uint8_t* p_rom_src);
void bbc_save_rom(struct bbc_struct* p_bbc,
                  uint8_t index,
                  uint8_t* p_dest);
void bbc_make_sideways_ram(struct bbc_struct* p_bbc, uint8_t index);
uint8_t bbc_get_romsel(struct bbc_struct* p_bbc);
void bbc_sideways_select(struct bbc_struct* p_bbc, uint8_t index);
void bbc_load_disc(struct bbc_struct* p_bbc,
                   int drive,
                   uint8_t* p_data,
                   size_t buffer_size,
                   size_t buffer_filled,
                   int is_dsd,
                   int writeable);
void bbc_set_stop_cycles(struct bbc_struct* p_bbc, uint64_t cycles);

struct cpu_driver* bbc_get_cpu_driver(struct bbc_struct* p_bbc);
void bbc_get_registers(struct bbc_struct* p_bbc,
                       uint8_t* a,
                       uint8_t* x,
                       uint8_t* y,
                       uint8_t* s,
                       uint8_t* flags,
                       uint16_t* pc);
void bbc_set_registers(struct bbc_struct* p_bbc,
                       uint8_t a,
                       uint8_t x,
                       uint8_t y,
                       uint8_t s,
                       uint8_t flags,
                       uint16_t pc);
size_t bbc_get_cycles(struct bbc_struct* p_bbc);
void bbc_set_pc(struct bbc_struct* p_bbc, uint16_t pc);

void bbc_run_async(struct bbc_struct* p_bbc);
uint32_t bbc_get_run_result(struct bbc_struct* p_bbc);
int bbc_check_do_break(struct bbc_struct* p_bbc);

struct state_6502* bbc_get_6502(struct bbc_struct* p_bbc);
struct via_struct* bbc_get_sysvia(struct bbc_struct* p_bbc);
struct via_struct* bbc_get_uservia(struct bbc_struct* p_bbc);
struct keyboard_struct* bbc_get_keyboard(struct bbc_struct* p_bbc);
struct sound_struct* bbc_get_sound(struct bbc_struct* p_bbc);
struct video_struct* bbc_get_video(struct bbc_struct* p_bbc);
struct render_struct* bbc_get_render(struct bbc_struct* p_bbc);

uint8_t* bbc_get_mem_read(struct bbc_struct* p_bbc);
uint8_t* bbc_get_mem_write(struct bbc_struct* p_bbc);
void bbc_set_memory_block(struct bbc_struct* p_bbc,
                          uint16_t addr,
                          uint16_t len,
                          uint8_t* p_src_mem);
void bbc_memory_write(struct bbc_struct* p_bbc,
                      uint16_t addr_6502,
                      uint8_t val);

int bbc_get_run_flag(struct bbc_struct* p_bbc);
int bbc_get_print_flag(struct bbc_struct* p_bbc);
int bbc_get_vsync_wait_for_render(struct bbc_struct* p_bbc);

size_t bbc_get_client_handle(struct bbc_struct* p_bbc);
void bbc_client_send_message(struct bbc_struct* p_bbc, char message);
char bbc_client_receive_message(struct bbc_struct* p_bbc);

#endif /* BEEJIT_JIT_H */
