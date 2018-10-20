#include "bbc.h"
#include "state.h"
#include "test.h"
#include "util.h"
#include "video.h"
#include "x.h"

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int
main(int argc, const char* argv[]) {
  size_t read_ret;
  unsigned char os_rom[k_bbc_rom_size];
  unsigned char load_rom[k_bbc_rom_size];
  int i;
  struct x_struct* p_x;
  struct bbc_struct* p_bbc;
  int x_fd;
  int bbc_fd;
  struct pollfd poll_fds[2];
  const char* rom_names[k_bbc_num_roms] = {};

  const char* os_rom_name = "os12.rom";
  const char* load_name = NULL;
  const char* opt_flags = "";
  const char* log_flags = "";
  int debug_flag = 0;
  int run_flag = 0;
  int print_flag = 0;
  int slow_flag = 0;
  int test_flag = 0;
  int debug_stop_addr = 0;
  int pc = 0;
  int mode = k_bbc_mode_jit;

  rom_names[k_bbc_default_lang_rom_slot] = "basic.rom";

  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (i + 2 < argc) {
      int rom;
      const char* val1 = argv[i + 1];
      const char* val2 = argv[i + 2];
      if (!strcmp(arg, "-rom")) {
        (void) sscanf(val1, "%x", &rom);
        if (rom < 0 || rom >= k_bbc_num_roms) {
          errx(1, "rom number out of range");
        }
        rom_names[rom] = val2;
      }
    }
    if (i + 1 < argc) {
      const char* val = argv[i + 1];
      if (!strcmp(arg, "-os")) {
        os_rom_name = val;
        ++i;
      } else if (!strcmp(arg, "-load")) {
        load_name = val;
        ++i;
      } else if (!strcmp(arg, "-opt")) {
        opt_flags = val;
        ++i;
      } else if (!strcmp(arg, "-log")) {
        log_flags = val;
        ++i;
      } else if (!strcmp(arg, "-stopat")) {
        (void) sscanf(val, "%x", &debug_stop_addr);
        ++i;
      } else if (!strcmp(arg, "-pc")) {
        (void) sscanf(val, "%x", &pc);
        ++i;
      } else if (!strcmp(arg, "-mode")) {
        if (!strcmp(val, "jit")) {
          mode = k_bbc_mode_jit;
        } else if (!strcmp(val, "interp")) {
          mode = k_bbc_mode_interp;
        } else {
          errx(1, "unknown mode");
        }
        ++i;
      }
    }
    if (!strcmp(arg, "-d")) {
      debug_flag = 1;
    } else if (!strcmp(arg, "-r")) {
      run_flag = 1;
    } else if (!strcmp(arg, "-p")) {
      print_flag = 1;
    } else if (!strcmp(arg, "-s")) {
      slow_flag = 1;
    } else if (!strcmp(arg, "-t")) {
      test_flag = 1;
    }
  }

  (void) memset(os_rom, '\0', k_bbc_rom_size);
  (void) memset(load_rom, '\0', k_bbc_rom_size);

  read_ret = util_file_read(os_rom, k_bbc_rom_size, os_rom_name);
  if (read_ret != k_bbc_rom_size) {
    errx(1, "can't load OS rom");
  }

  p_bbc = bbc_create(os_rom,
                     debug_flag,
                     run_flag,
                     print_flag,
                     slow_flag,
                     opt_flags,
                     log_flags,
                     debug_stop_addr);
  if (p_bbc == NULL) {
    errx(1, "bbc_create failed");
  }

  if (test_flag) {
    test_do_tests(p_bbc);
    return 0;
  }

  bbc_set_mode(p_bbc, mode);

  if (load_name != NULL) {
    state_load(p_bbc, load_name);
  }
  if (pc != 0) {
    bbc_set_pc(p_bbc, pc);
  }

  for (i = 0; i < k_bbc_num_roms; ++i) {
    const char* p_rom_name = rom_names[i];
    if (p_rom_name != NULL) {
      (void) memset(load_rom, '\0', k_bbc_rom_size);
      (void) util_file_read(load_rom, k_bbc_rom_size, p_rom_name);
      bbc_load_rom(p_bbc, i, load_rom);
    }
  }

  p_x = x_create(p_bbc, k_bbc_mode7_width, k_bbc_mode7_height);
  if (p_x == NULL) {
    errx(1, "x_create failed");
  }

  bbc_run_async(p_bbc);

  x_fd = x_get_fd(p_x);
  bbc_fd = bbc_get_client_fd(p_bbc);

  poll_fds[0].fd = x_fd;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = bbc_fd;
  poll_fds[1].events = POLLIN;

  while (1) {
    int ret;
    char message;

    poll_fds[0].revents = 0;
    poll_fds[1].revents = 0;
    ret = poll(&poll_fds[0], 2, -1);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      errx(1, "poll failed");
    }

    if (poll_fds[0].revents & POLLIN) {
      assert(ret > 0);
      /* No x_event_check here -- see below. */
    }
    if (poll_fds[1].revents & POLLIN) {
      assert(ret > 0);
      message = bbc_client_receive_message(p_bbc);
      if (message == k_message_exited) {
        break;
      } else {
        assert(message == k_message_vsync);
        x_render(p_x);
        if (bbc_get_vsync_wait_for_render(p_bbc)) {
          bbc_client_send_message(p_bbc, k_message_render_done);
        }
      }
    }

    /* We need to call x_event_check unconditionally, in case a key event comes
     * in during X rendering. In that case, the data could be read from the
     * socket and placed in the event queue during standard X queue processing.
     * This would lead to a delayed event because the poll() wouldn't see it
     * in the socket queue.
     */
    x_event_check(p_x);
  }

  x_destroy(p_x);
  bbc_destroy(p_bbc);

  return 0;
}
