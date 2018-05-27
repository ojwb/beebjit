#include "jit.h"

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

static const size_t k_addr_space_size = 0x10000;
static const size_t k_guard_size = 4096;
static const size_t k_os_rom_offset = 0xc000;
static const size_t k_os_rom_len = 0x4000;
static const size_t k_lang_rom_offset = 0x8000;
static const size_t k_lang_rom_len = 0x4000;
static const size_t k_registers_offset = 0xfc00;
static const size_t k_registers_len = 0x300;
static const size_t k_vector_reset = 0xfffc;
// TODO: move into jit.h
static const int k_jit_bytes_per_byte = 256;

int
main(int argc, const char* argv[]) {
  char* p_map;
  char* p_mem;
  int fd;
  ssize_t read_ret;
  int ret;
  const char* os_rom_name = "os12.rom";
  const char* lang_rom_name = "basic.rom";
  unsigned int debug_flags = 0;
  int i;

  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (i + 1 < argc) {
      const char* val = argv[i + 1];
      if (strcmp(arg, "-o") == 0) {
        os_rom_name = val;
        ++i;
      } else if (strcmp(arg, "-l") == 0) {
        lang_rom_name = val;
        ++i;
      }
    }
    if (strcmp(arg, "-d") == 0) {
      debug_flags = 1;
    }
  }

  p_map = mmap(NULL,
               (k_addr_space_size * (k_jit_bytes_per_byte + 1)) +
                   (k_guard_size * 3),
               PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS,
               -1,
               0);
  if (p_map == MAP_FAILED) {
    errx(1, "mmap() failed");
  }

  p_mem = p_map + k_guard_size;

  ret = mprotect(p_map,
                 k_guard_size,
                 PROT_NONE);
  if (ret != 0) {
    errx(1, "mprotect() failed");
  }
  ret = mprotect(p_mem + k_addr_space_size,
                 k_guard_size,
                 PROT_NONE);
  if (ret != 0) {
    errx(1, "mprotect() failed");
  }
  ret = mprotect(p_mem + (k_addr_space_size * (k_jit_bytes_per_byte + 1)) +
                     k_guard_size,
                 k_guard_size,
                 PROT_NONE);
  if (ret != 0) {
    errx(1, "mprotect() failed");
  }

  ret = mprotect(p_mem + k_addr_space_size + k_guard_size,
                 k_addr_space_size * k_jit_bytes_per_byte,
                 PROT_READ | PROT_WRITE | PROT_EXEC);
  if (ret != 0) {
    errx(1, "mprotect() failed");
  }

  p_mem = p_map + k_guard_size;

  memset(p_mem, '\0', k_addr_space_size);

  fd = open(os_rom_name, O_RDONLY);
  if (fd < 0) {
    errx(1, "can't load OS rom");
  }
  read_ret = read(fd, p_mem + k_os_rom_offset, k_os_rom_len);
  if (read_ret != k_os_rom_len) {
    errx(1, "can't read OS rom");
  }
  close(fd);

  if (strlen(lang_rom_name) > 0) {
    fd = open(lang_rom_name, O_RDONLY);
    if (fd < 0) {
      errx(1, "can't load language rom");
    }
    read_ret = read(fd, p_mem + k_lang_rom_offset, k_lang_rom_len);
    if (read_ret != k_lang_rom_len) {
      errx(1, "can't read language rom");
    }
    close(fd);
  }

  memset(p_mem + k_registers_offset, '\0', k_registers_len);

  jit_init(p_mem);
  jit_jit(p_mem, k_os_rom_offset, k_os_rom_len, debug_flags);
  jit_jit(p_mem, k_lang_rom_offset, k_lang_rom_len, debug_flags);
  jit_enter(p_mem, k_vector_reset);

  return 0;
}
