#include "render.h"

#include "bbc_options.h"
#include "teletext.h"
#include "util.h"

#include <assert.h>
#include <string.h>

struct render_struct {
  void (*p_flyback_callback)(void*);
  void* p_flyback_callback_object;

  uint32_t width;
  uint32_t height;

  uint32_t* p_buffer;
  uint32_t* p_buffer_end;
  int is_buffer_owned;

  struct teletext_struct* p_teletext;

  uint32_t palette[16];
  int render_table_dirty[k_render_num_modes];
  struct render_table_2MHz render_table_mode0;
  struct render_table_2MHz render_table_mode1;
  struct render_table_2MHz render_table_mode2;
  struct render_table_1MHz render_table_mode4;
  struct render_table_1MHz render_table_mode5;
  struct render_table_1MHz render_table_mode8;

  struct render_character_1MHz render_character_1MHz_black;
  struct render_character_2MHz render_character_2MHz_black;
  struct render_table_1MHz render_table_1MHz_black;
  struct render_table_2MHz render_table_2MHz_black;

  struct render_table_1MHz* p_render_table_1MHz;
  struct render_table_2MHz* p_render_table_2MHz;

  int render_mode;
  int is_clock_2MHz;
  int is_rendering_black;
  uint32_t pixels_size;
  int32_t horiz_beam_pos;
  int32_t vert_beam_pos;
  int32_t horiz_beam_window_start_pos;
  int32_t horiz_beam_window_end_pos;
  int32_t vert_beam_window_start_pos;
  int32_t vert_beam_window_end_pos;
  uint32_t* p_render_pos;
  uint32_t* p_render_pos_row;
  uint32_t* p_render_pos_row_max;
  int do_interlace_wobble;
  int do_show_frame_boundaries;
  int32_t cursor_segment_index;
  int cursor_segments[4];
  int is_double_size;
};

static void
render_dirty_all_tables(struct render_struct* p_render) {
  uint32_t i;
  for (i = 0; i < k_render_num_modes; ++i) {
    p_render->render_table_dirty[i] = 1;
  }
}

struct render_struct*
render_create(struct teletext_struct* p_teletext,
              struct bbc_options* p_options) {
  uint32_t width;
  uint32_t height;
  uint32_t i;

  /* The border appears on all screen edges, and 1 character unit is the width /
   * height of a MODE 4 glyph / character.
   */
  uint32_t border_chars = 4;

  /* These numbers, 15 and 4, come from the delta between horiz/vert sync
   * position and of line/frame, in a standard 1MHz MODE.
   * Note that AUG states R7 as 34 for a lot of the screen modes whereas it
   * should be 35!
   */
  uint32_t k_horiz_standard_offset = 15;
  uint32_t k_vert_standard_offset = 4;

  const char* p_opt_flags = p_options->p_opt_flags;

  struct render_struct* p_render = util_mallocz(sizeof(struct render_struct));

  p_render->p_teletext = p_teletext;

  /* "border characters" is the number of MODE1 square 8x8 pixel characters
   * used to pad the display window beyond the standard viewport for MODE1.
   * If set to zero, standard modes will fit perfectly. If set larger than
   * zero, pixels rendered in "overscan" areas will start to be visible.
   */
  (void) util_get_u32_option(&border_chars, p_opt_flags, "video:border-chars=");
  if (border_chars > 16) {
    util_bail("border-chars must be 16 or less");
  }

  p_render->do_interlace_wobble = util_has_option(p_opt_flags,
                                                  "video:interlace-wobble");

  p_render->do_show_frame_boundaries = util_has_option(
      p_opt_flags, "video:frame-boundaries");

  width = (640 + (border_chars * 2 * 16));
  height = (512 + (border_chars * 2 * 16));

  if (util_has_option(p_opt_flags, "video:double-size")) {
    width *= 2;
    height *= 2;
    p_render->is_double_size = 1;
  } else {
    p_render->is_double_size = 0;
  }

  p_render->width = width;
  p_render->height = height;

  if (border_chars > k_horiz_standard_offset) {
    p_render->horiz_beam_window_start_pos = 0;
  } else {
    p_render->horiz_beam_window_start_pos =
        ((k_horiz_standard_offset - border_chars) * 16);
  }
  p_render->horiz_beam_window_end_pos = (p_render->width +
                                         p_render->horiz_beam_window_start_pos);

  if (border_chars > k_vert_standard_offset) {
    p_render->vert_beam_window_start_pos = 0;
  } else {
    p_render->vert_beam_window_start_pos =
        ((k_vert_standard_offset - border_chars) * 16);
  }
  p_render->vert_beam_window_end_pos = (p_render->height +
                                        p_render->vert_beam_window_start_pos);

  p_render->render_mode = k_render_mode0;
  p_render->is_clock_2MHz = 1;
  p_render->pixels_size = 8;
  p_render->is_rendering_black = 0;

  p_render->cursor_segment_index = -1;

  render_dirty_all_tables(p_render);

  for (i = 0; i < 16; ++i) {
    p_render->render_character_1MHz_black.host_pixels[i] = 0xff000000;
  }
  for (i = 0; i < 8; ++i) {
    p_render->render_character_2MHz_black.host_pixels[i] = 0xff000000;
  }
  for (i = 0; i < 256; ++i) {
    p_render->render_table_1MHz_black.values[i] =
        p_render->render_character_1MHz_black;
    p_render->render_table_2MHz_black.values[i] =
        p_render->render_character_2MHz_black;
  }

  return p_render;
}

void
render_destroy(struct render_struct* p_render) {
  if (p_render->is_buffer_owned) {
    util_free(p_render->p_buffer);
  }
  util_free(p_render);
}

void
render_set_flyback_callback(struct render_struct* p_render,
                            void (*p_flyback_callback)(void* p),
                            void* p_flyback_callback_object) {
  p_render->p_flyback_callback = p_flyback_callback;
  p_render->p_flyback_callback_object = p_flyback_callback_object;
}

uint32_t
render_get_width(struct render_struct* p_render) {
  return p_render->width;
}

uint32_t
render_get_height(struct render_struct* p_render) {
  return p_render->height;
}

uint32_t*
render_get_buffer(struct render_struct* p_render) {
  return p_render->p_buffer;
}

uint32_t
render_get_buffer_size(struct render_struct* p_render) {
  return (p_render->width * p_render->height * 4);
}

static inline void
render_reset_render_pos(struct render_struct* p_render) {
  uint32_t window_horiz_pos;
  uint32_t window_vert_pos;

  p_render->p_render_pos = p_render->p_buffer_end;
  p_render->p_render_pos_row = p_render->p_buffer_end;
  p_render->p_render_pos_row_max = p_render->p_buffer_end;

  if (p_render->p_buffer == NULL) {
    return;
  }

  if (p_render->vert_beam_pos >= p_render->vert_beam_window_end_pos) {
    return;
  }
  if (p_render->vert_beam_pos < p_render->vert_beam_window_start_pos) {
    return;
  }

  window_vert_pos = (p_render->vert_beam_pos -
                     p_render->vert_beam_window_start_pos);
  p_render->p_render_pos_row = p_render->p_buffer;
  p_render->p_render_pos_row += (window_vert_pos * p_render->width);

  if (p_render->horiz_beam_pos >= p_render->horiz_beam_window_end_pos) {
    return;
  }
  if (p_render->horiz_beam_pos < p_render->horiz_beam_window_start_pos) {
    return;
  }

  window_horiz_pos = (p_render->horiz_beam_pos -
                      p_render->horiz_beam_window_start_pos);

  p_render->p_render_pos = p_render->p_render_pos_row;
  p_render->p_render_pos += window_horiz_pos;
  p_render->p_render_pos_row_max = (p_render->p_render_pos_row +
                                    p_render->width -
                                    p_render->pixels_size);
}

static void
render_setup_new_buffer(struct render_struct* p_render) {
  uint32_t* p_buffer = p_render->p_buffer;
  p_render->p_buffer_end = p_buffer;
  p_render->p_buffer_end += (p_render->width * p_render->height);

  render_clear_buffer(p_render);

  p_render->horiz_beam_pos = 0;
  p_render->vert_beam_pos = 0;
  render_reset_render_pos(p_render);
}

void
render_set_buffer(struct render_struct* p_render, uint32_t* p_buffer) {
  assert(p_render->p_buffer == NULL);
  assert(p_buffer != NULL);

  p_render->p_buffer = p_buffer;
  p_render->is_buffer_owned = 0;
  render_setup_new_buffer(p_render);
}

void
render_create_internal_buffer(struct render_struct* p_render) {
  uint32_t size = render_get_buffer_size(p_render);
  assert(p_render->p_buffer == NULL);

  p_render->p_buffer = util_malloc(size);
  p_render->is_buffer_owned = 1;
  render_setup_new_buffer(p_render);
}

static inline void
render_check_cursor(struct render_struct* p_render,
                    uint32_t* p_render_pos,
                    uint32_t num_pixels) {
  if (p_render->cursor_segment_index == -1) {
    return;
  }

  /* NOTE: the -16 here is a dodgy hack to shift the cursor to the left
   * while we don't support 6845 skew.
   */
  if (p_render->render_mode == k_render_mode7) {
    p_render_pos -= 16;
  }

  if (p_render->cursor_segments[p_render->cursor_segment_index] &&
      (p_render_pos >= p_render->p_render_pos_row)) {
    uint32_t i;
    for (i = 0; i < num_pixels; ++i) {
      p_render_pos[i] ^= 0x00ffffff;
    }
  }
  p_render->cursor_segment_index++;
  if (p_render->cursor_segment_index == 4) {
    p_render->cursor_segment_index = -1;
  }
}

static void
render_function_teletext(struct render_struct* p_render, uint8_t data) {
  uint32_t* p_render_pos = p_render->p_render_pos;
  struct render_character_1MHz* p_character =
      (struct render_character_1MHz*) p_render_pos;

  p_render->horiz_beam_pos += 16;

  if (p_render_pos < p_render->p_render_pos_row_max) {
    teletext_render_data(p_render->p_teletext, p_character, data);
    render_check_cursor(p_render, p_render_pos, 16);
    p_render->p_render_pos += 16;
  } else {
    /* In teletext mode, we still need to tell the SAA5050 chip about data
     * bytes that are off-screen, so that it can maintain state.
     */
    teletext_render_data(p_render->p_teletext, NULL, data);
    if ((p_render->horiz_beam_pos & ~15) ==
        p_render->horiz_beam_window_start_pos) {
      render_reset_render_pos(p_render);
    }
  }
}

static void
render_function_1MHz_data(struct render_struct* p_render, uint8_t data) {
  uint32_t* p_render_pos = p_render->p_render_pos;
  struct render_character_1MHz* p_character =
      (struct render_character_1MHz*) p_render_pos;

  p_render->horiz_beam_pos += 16;

  if (p_render_pos < p_render->p_render_pos_row_max) {
    *p_character = p_render->p_render_table_1MHz->values[data];
    render_check_cursor(p_render, p_render_pos, 16);
    p_render->p_render_pos += 16;
  } else if ((p_render->horiz_beam_pos & ~15) ==
             p_render->horiz_beam_window_start_pos) {
    render_reset_render_pos(p_render);
  }
}

static void
render_function_1MHz_blank(struct render_struct* p_render, uint8_t data) {
  uint32_t* p_render_pos = p_render->p_render_pos;
  struct render_character_1MHz* p_character =
      (struct render_character_1MHz*) p_render_pos;

  (void) data;
  /* TODO: If we're maintaining the canvas background correctly as black, we
   * should skip painting anything and just advance the pointers.
   */
  p_render->horiz_beam_pos += 16;

  if (p_render_pos < p_render->p_render_pos_row_max) {
    *p_character = p_render->render_character_1MHz_black;
    render_check_cursor(p_render, p_render_pos, 16);
    p_render->p_render_pos += 16;
  } else if ((p_render->horiz_beam_pos & ~15) ==
             p_render->horiz_beam_window_start_pos) {
    render_reset_render_pos(p_render);
  }
}

static void
render_function_2MHz_data(struct render_struct* p_render, uint8_t data) {
  uint32_t* p_render_pos = p_render->p_render_pos;
  struct render_character_2MHz* p_character =
      (struct render_character_2MHz*) p_render_pos;

  p_render->horiz_beam_pos += 8;

  if (p_render_pos < p_render->p_render_pos_row_max) {
    *p_character = p_render->p_render_table_2MHz->values[data];
    render_check_cursor(p_render, p_render_pos, 8);
    p_render->p_render_pos += 8;
  } else if ((p_render->horiz_beam_pos & ~7) ==
             p_render->horiz_beam_window_start_pos) {
    render_reset_render_pos(p_render);
  }
}

static void
render_function_2MHz_blank(struct render_struct* p_render, uint8_t data) {
  uint32_t* p_render_pos = p_render->p_render_pos;
  struct render_character_2MHz* p_character =
      (struct render_character_2MHz*) p_render_pos;

  (void) data;

  p_render->horiz_beam_pos += 8;

  if (p_render_pos < p_render->p_render_pos_row_max) {
    *p_character = p_render->render_character_2MHz_black;
    render_check_cursor(p_render, p_render_pos, 8);
    p_render->p_render_pos += 8;
  } else if ((p_render->horiz_beam_pos & ~7) ==
             p_render->horiz_beam_window_start_pos) {
    render_reset_render_pos(p_render);
  }
}

void
render_set_mode(struct render_struct* p_render, int mode) {
  assert((mode >= k_render_mode0) && (mode <= k_render_mode8));

  if (mode == p_render->render_mode) {
    return;
  }

  render_dirty_all_tables(p_render);

  switch (mode) {
  case k_render_mode0:
  case k_render_mode1:
  case k_render_mode2:
    p_render->is_clock_2MHz = 1;
    p_render->pixels_size = 8;
    break;
  case k_render_mode4:
  case k_render_mode5:
  case k_render_mode7:
  case k_render_mode8:
    p_render->is_clock_2MHz = 0;
    p_render->pixels_size = 16;
    break;
  default:
    assert(0);
    break;
  }

  p_render->render_mode = mode;

  /* Changing 1MHz <-> 2MHz changes the size of the pixel blocks we write, and
   * therefore the bounds.
   */
  render_reset_render_pos(p_render);
}

void
render_set_palette(struct render_struct* p_render,
                   uint8_t index,
                   uint32_t rgba) {
  if (p_render->palette[index] == rgba) {
    return;
  }

  p_render->palette[index] = rgba;
  render_dirty_all_tables(p_render);
}

void
render_set_cursor_segments(struct render_struct* p_render,
                           int s0,
                           int s1,
                           int s2,
                           int s3) {
  p_render->cursor_segments[0] = s0;
  p_render->cursor_segments[1] = s1;
  p_render->cursor_segments[2] = s2;
  p_render->cursor_segments[3] = s3;
}

static void
render_generate_1MHz_table(struct render_struct* p_render,
                           struct render_table_1MHz* p_table,
                           uint32_t num_pixels) {
  uint32_t i;
  uint32_t j;

  uint32_t pixel_stride = (16 / num_pixels);
  uint32_t palette_index = 0;
  uint32_t pixel_value = 0;

  for (i = 0; i < 256; ++i) {
    struct render_character_1MHz* p_character = &p_table->values[i];
    uint8_t shift_register = i;
    for (j = 0; j < 16; ++j) {
      if ((j % pixel_stride) == 0) {
        palette_index = (((shift_register & 0x02) >> 1) |
                         ((shift_register & 0x08) >> 2) |
                         ((shift_register & 0x20) >> 3) |
                         ((shift_register & 0x80) >> 4));
        pixel_value = p_render->palette[palette_index];
        shift_register <<= 1;
        shift_register |= 1;
      }
      p_character->host_pixels[j] = pixel_value;
    }
  }
}

static void
render_generate_2MHz_table(struct render_struct* p_render,
                           struct render_table_2MHz* p_table,
                           uint32_t num_pixels) {
  uint32_t i;
  uint32_t j;

  uint32_t pixel_stride = (8 / num_pixels);
  uint32_t palette_index = 0;
  uint32_t pixel_value = 0;

  for (i = 0; i < 256; ++i) {
    struct render_character_2MHz* p_character = &p_table->values[i];
    uint8_t shift_register = i;
    for (j = 0; j < 8; ++j) {
      if ((j % pixel_stride) == 0) {
        palette_index = (((shift_register & 0x02) >> 1) |
                         ((shift_register & 0x08) >> 2) |
                         ((shift_register & 0x20) >> 3) |
                         ((shift_register & 0x80) >> 4));
        pixel_value = p_render->palette[palette_index];
        shift_register <<= 1;
        shift_register |= 1;
      }
      p_character->host_pixels[j] = pixel_value;
    }
  }
}

static void
render_generate_mode0_table(struct render_struct* p_render) {
  render_generate_2MHz_table(p_render, &p_render->render_table_mode0, 8);
}

static void
render_generate_mode1_table(struct render_struct* p_render) {
  render_generate_2MHz_table(p_render, &p_render->render_table_mode1, 4);
}

static void
render_generate_mode2_table(struct render_struct* p_render) {
  render_generate_2MHz_table(p_render, &p_render->render_table_mode2, 2);
}

static void
render_generate_mode4_table(struct render_struct* p_render) {
  render_generate_1MHz_table(p_render, &p_render->render_table_mode4, 8);
}

static void
render_generate_mode5_table(struct render_struct* p_render) {
  render_generate_1MHz_table(p_render, &p_render->render_table_mode5, 4);
}

static void
render_generate_mode8_table(struct render_struct* p_render) {
  render_generate_1MHz_table(p_render, &p_render->render_table_mode8, 2);
}

static void
render_check_2MHz_render_table(struct render_struct* p_render) {
  int mode;

  if (p_render->is_rendering_black) {
    p_render->p_render_table_2MHz = &p_render->render_table_2MHz_black;
    return;
  }

  mode = p_render->render_mode;

  if (p_render->render_table_dirty[mode]) {
    switch (mode) {
    case k_render_mode0:
      render_generate_mode0_table(p_render);
      break;
    case k_render_mode1:
      render_generate_mode1_table(p_render);
      break;
    case k_render_mode2:
      render_generate_mode2_table(p_render);
      break;
    default:
      assert(0);
      break;
    }
    p_render->render_table_dirty[mode] = 0;
  }

  switch (mode) {
  case k_render_mode0:
    p_render->p_render_table_2MHz = &p_render->render_table_mode0;
    break;
  case k_render_mode1:
    p_render->p_render_table_2MHz = &p_render->render_table_mode1;
    break;
  case k_render_mode2:
    p_render->p_render_table_2MHz = &p_render->render_table_mode2;
    break;
  default:
    assert(0);
    break;
  }
}

static void
render_check_1MHz_render_table(struct render_struct* p_render) {
  int mode;

  if (p_render->is_rendering_black) {
    p_render->p_render_table_1MHz = &p_render->render_table_1MHz_black;
    return;
  }

  mode = p_render->render_mode;

  if (p_render->render_table_dirty[mode]) {
    switch (mode) {
    case k_render_mode4:
      render_generate_mode4_table(p_render);
      break;
    case k_render_mode5:
      render_generate_mode5_table(p_render);
      break;
    case k_render_mode8:
      render_generate_mode8_table(p_render);
      break;
    default:
      assert(0);
      break;
    }
    p_render->render_table_dirty[mode] = 0;
  }

  switch (mode) {
  case k_render_mode4:
    p_render->p_render_table_1MHz = &p_render->render_table_mode4;
    break;
  case k_render_mode5:
    p_render->p_render_table_1MHz = &p_render->render_table_mode5;
    break;
  case k_render_mode8:
    p_render->p_render_table_1MHz = &p_render->render_table_mode8;
    break;
  default:
    assert(0);
    break;
  }
}

void (*render_get_render_data_function(struct render_struct* p_render))
    (struct render_struct*, uint8_t) {
  if (p_render->render_mode == k_render_mode7) {
    return render_function_teletext;
  } else if (p_render->is_clock_2MHz) {
    render_check_2MHz_render_table(p_render);
    return render_function_2MHz_data;
  } else {
    render_check_1MHz_render_table(p_render);
    return render_function_1MHz_data;
  }
}

void (*render_get_render_blank_function(struct render_struct* p_render))
    (struct render_struct*, uint8_t) {
  if (p_render->is_clock_2MHz) {
    return render_function_2MHz_blank;
  } else {
    return render_function_1MHz_blank;
  }
}

void
render_set_RA(struct render_struct* p_render, uint32_t row_address) {
  int is_rendering_black = 0;
  int old_is_rendering_black = p_render->is_rendering_black;

  if (row_address & 0x08) {
    is_rendering_black = 1;
  }

  if (is_rendering_black == old_is_rendering_black) {
    return;
  }

  p_render->is_rendering_black = is_rendering_black;
  if (p_render->render_mode == k_render_mode7) {
    /* Nothing to do. */
  } else if (p_render->is_clock_2MHz) {
    render_check_2MHz_render_table(p_render);
  } else {
    render_check_1MHz_render_table(p_render);
  }
}

void
render_clear_buffer(struct render_struct* p_render) {
  uint32_t size = render_get_buffer_size(p_render);
  (void) memset(p_render->p_buffer, '\0', size);
}

void
render_double_up_lines(struct render_struct* p_render) {
  /* TODO: only need to double up partial lines within the render border. */
  uint32_t width = p_render->width;
  uint32_t line_size = (width * sizeof(uint32_t));
  uint32_t double_width = (width * 2);
  uint32_t* p_buffer = p_render->p_buffer;
  uint32_t* p_buffer_next_line = (p_buffer + width);
  
  if (p_render->is_double_size) {
    int32_t line;   /* Must be signed. */
    int32_t column; /* Must be signed. */
    int32_t lines = (p_render->height / 4);
    uint32_t half_width = (width / 2);
    for (line = 0; line < lines; ++line) {
      for (column = (half_width - 1); column >= 0; --column) {
        p_buffer[column * 2] = p_buffer[column];
        p_buffer[(column * 2) + 1] = p_buffer[column];
      }
      (void) memcpy(p_buffer_next_line, p_buffer, line_size);
      p_buffer += double_width;
      p_buffer_next_line += double_width;
    }
    p_buffer = p_render->p_buffer;
    lines = (p_render->height / 2);
    for (line = (lines - 1); line >= 0; --line) {
      uint32_t* p_buffer_src = (p_buffer + (line * width));
      uint32_t* p_buffer_dest = (p_buffer + (2 * (line * width)));
      if (line == 0) {
        /* Don't copy line 0 to line 2*0=0 (memcpy overlap = undefined) */
      } else {
        (void) memcpy(p_buffer_dest, p_buffer_src, line_size);
      }
      (void) memcpy((p_buffer_dest + width), p_buffer_src, line_size);
    }
  } else {
    /* Not double size. */
    uint32_t line;
    uint32_t lines = (p_render->height / 2);
    for (line = 0; line < lines; ++line) {
      (void) memcpy(p_buffer_next_line, p_buffer, line_size);
      p_buffer += double_width;
      p_buffer_next_line += double_width;
    }
  }
}

void
render_hsync(struct render_struct* p_render, uint32_t hsync_pulse_ticks) {
  /* A real CRT appears to sync to the middle of the hsync pulse?!! This
   * permits half-character horizontal scrolling.
   * Used by tricky's RallyX demo.
   * For now, handle just the special case.
   */
  p_render->horiz_beam_pos = 0;
  if (hsync_pulse_ticks & 1) {
    p_render->horiz_beam_pos = -4;
  }
  p_render->vert_beam_pos += 2;

  /* If the CRT beam gets too low with no vsync signal in sight, it will do
   * flyback anyway.
   */
  if (p_render->vert_beam_pos >= 768) {
    render_vsync(p_render, 1);
  }

  render_reset_render_pos(p_render);

  /* NOTE: dodgy hack to get MODE7 shifted to the right by two characters
   * because we do not yet support 6845 skew.
   */
  if (p_render->render_mode == k_render_mode7) {
    render_function_1MHz_blank(p_render, 0);
    render_function_1MHz_blank(p_render, 0);
  }
}

void
render_vsync(struct render_struct* p_render, int do_interlace_compensate) {
  if (p_render->p_flyback_callback) {
    p_render->p_flyback_callback(p_render->p_flyback_callback_object);
  }

  p_render->vert_beam_pos = 0;
  if (do_interlace_compensate &&
      !p_render->do_interlace_wobble &&
      (p_render->horiz_beam_pos < 512)) {
    /* TODO: the interlace wobble, if enabled, is wobbling too much. It wobbles
     * 1 full vertical scanline (2 host pixels) instead of a half scanline.
     */
    p_render->vert_beam_pos = 2;
  }
  render_reset_render_pos(p_render);
}

void
render_frame_boundary(struct render_struct* p_render) {
  uint32_t i;

  if (!p_render->do_show_frame_boundaries) {
    return;
  }
  if (p_render->p_render_pos_row == p_render->p_buffer_end) {
    return;
  }

  /* Paint a red line to edge of canvas denote CRTC frame boundary. */
  for (i = 0; i < p_render->width; ++i) {
    p_render->p_render_pos_row[i] = 0xffff0000;
  }
}

void
render_cursor(struct render_struct* p_render) {
  p_render->cursor_segment_index = 0;
}
