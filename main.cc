extern "C" {
  #include "renderer.h"
  #include "microui.h"
  #include "fenster.h"
}

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <ranges>
#include <vector>


enum PowderType {
  EMPTY, DIRT, STONE, WATER, PORTAL
};


#define W 1000
#define H 1000


static float bg[3] = { 0, 0, 0 };
static std::vector<std::vector<PowderType>> world (H, std::vector<PowderType>(W));


static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id(ctx);
  return res;
}

static void tool_window(mu_Context *ctx, uint8_t *brush_size, PowderType *powder)
{
  if (mu_begin_window(ctx, "Tools", mu_rect(0, 1, W, 200), 0)) {
    int sw = mu_get_current_container(ctx)->body.w * 0.14;
    int widths[] { 80, sw, sw, sw, sw, sw, sw, -1 };
    mu_layout_row(ctx, 7, widths, 0);
    mu_label(ctx, "Brush Size");
    uint8_slider(ctx, brush_size, 0, 255);
    mu_label(ctx, "Powder:");
    if (mu_button(ctx, "Dirt")) { *powder = DIRT; }
    if (mu_button(ctx, "Stone")) { *powder = STONE; }
    if (mu_button(ctx, "Water")) { *powder = WATER; }
    if (mu_button(ctx, "Portal")) { *powder = PORTAL; }
    mu_end_window(ctx);
  }
}


static void process_frame(mu_Context *ctx, uint8_t *brush_size, PowderType *powder) {
  mu_begin(ctx);
  tool_window(ctx, brush_size, powder);
  mu_end(ctx);
}


static int text_width(mu_Font font, const char *text, int len) {
  (void)font;
  if (len == -1) { len = strlen(text); }
  return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
  (void)font;
  return r_get_text_height();
}

void process_powder()
{
  // std::vector<std::vector<PowderType>> newWorld (EMPTY);
  // for (size_t y = 0; y < H; ++y)
  // {
  //   for (size_t x = 0; x < W; ++x)
  //   {
  //     switch (world[y][x])
  //     {
  //       case DIRT:
  //       case STONE:
  //       case WATER:
  //         if (y > 0 && world[y - 1][x] == EMPTY)
  //         {
  //           newWorld[y - 1][x] = DIRT;
  //         }
  //         else
  //         {
  //           newWorld[y][x] = DIRT;
  //         }
  //         break;
  //       default:
  //         break;
  //     }
  //   }
  // }
  // world = newWorld;
}

void render_powder()
{
  struct fenster* wnd = r_get_underlying();
  for (size_t y = 0; y < H; ++y)
  {
    for (size_t x = 0; x < W; ++x)
    {
      if (world[y][x] != EMPTY)
        fenster_pixel(wnd, x, y) = r_color({255, 255, 255, 255});
    }
  }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  r_init(W, H);
  // debug
  for (int i = 0; i < W; ++i)
    for (int j = 0; j < 100; ++j)
      world[j][i] = DIRT;

  /* init microui */
  mu_Context *ctx = (mu_Context*)malloc(sizeof(mu_Context));
  mu_init(ctx);
  ctx->text_width = text_width;
  ctx->text_height = text_height;

  int fps = 60;
  int mousex = 0, mousey = 0;
  uint8_t brush_size = 1;
  PowderType powder = DIRT;

  /* main loop */
  for (;;) {
    int64_t before = r_get_time();
    if (r_mouse_moved(&mousex, &mousey)) {
      mu_input_mousemove(ctx, mousex, mousey);
    }
    if (r_mouse_down()) {
      mu_input_mousedown(ctx, mousex, mousey, MU_MOUSE_LEFT);
    } else if (r_mouse_up()) {
      mu_input_mouseup(ctx, mousex, mousey, MU_MOUSE_LEFT);
    }
    // TODO(max): scroll
    if (r_key_down(0x1b)) { break; }  // esc
    if (r_key_down('\n')) { mu_input_keydown(ctx, MU_KEY_RETURN); }
    else if (r_key_up('\n')) { mu_input_keyup(ctx, MU_KEY_RETURN); }
    if (r_key_down('\b')) { mu_input_keydown(ctx, MU_KEY_BACKSPACE); }
    else if (r_key_up('\b')) { mu_input_keyup(ctx, MU_KEY_BACKSPACE); }
    for (char i : std::views::iota(0, 256)) {
      if (r_key_down(i)) {
        if (' ' <= i  &&  i <= '~') {
          char text[2] = {i, 0};
          if (isalpha(i)) {
            if (!r_shift_pressed()) {
              text[0] = tolower(i);
            }
          }
          else {
            // TODO(kartik): depends on keyboard layout
          }
          mu_input_text(ctx, text);
        }
        continue;
      }
      // TODO(max): mod
    }

    /* process frame */
    process_frame(ctx, &brush_size, &powder);

    /* render */
    r_clear(mu_color(bg[0], bg[1], bg[2], 255));
    process_powder();
    render_powder();
    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
      switch (cmd->type) {
        case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
        case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
        case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
        case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
      }
    }
    r_present();
    int64_t after = r_get_time();
    int64_t paint_time_ms = after - before;
    int64_t frame_budget_ms = 1000 / fps;
    int64_t sleep_time_ms = frame_budget_ms - paint_time_ms;
    if (sleep_time_ms > 0) {
      r_sleep(sleep_time_ms);
    }
  }

  return 0;
}
