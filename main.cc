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
#include <map>
#include <cmath>
#include <iostream> //debug


enum PowderType {
  EMPTY, DIRT, STONE
};

std::map<PowderType, std::string> powderNames {
  {EMPTY, "Eraser"},
  {DIRT, "Dirt"},
  {STONE, "Stone"},
};

struct Data {
  // only computed for stone
  uint32_t dy = 0;
};


#define W 1000
#define H 1000


static float bg[3] = { 0, 0, 0 };
static std::vector<std::vector<PowderType>> world (H, std::vector<PowderType>(W, EMPTY));
static std::vector<std::vector<Data>> worldData (H, std::vector<Data>(W, Data{}));


static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id(ctx);
  return res;
}

static int float_slider(mu_Context *ctx, float *value) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, -10.0f, 10.0f, 0, "%.2f", MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id(ctx);
  return res;
}

static void tool_window(mu_Context *ctx, uint8_t *brush_size, PowderType *powder, float *time_scale)
{
  if (mu_begin_window(ctx, "Tools", mu_rect(0, 1, W, 200), 0)) {
    int sw = mu_get_current_container(ctx)->body.w - 80 - 20;
    int widths1[] { 80, sw, -1 };
    mu_layout_row(ctx, 2, widths1, 0);
    mu_label(ctx, "Brush Size");
    uint8_slider(ctx, brush_size, 1, 100);
    int bw = (sw + 4) / powderNames.size() - 4;
    std::vector<int> widths2(powderNames.size() + 2, bw);
    widths2[0] = 80;
    widths2[1] += (sw + 4) % powderNames.size();
    widths2.back() = -1;
    mu_layout_row(ctx, powderNames.size() + 1, widths2.data(), 0);
    mu_label(ctx, "Powder:");
    for (const auto& pair : powderNames) {
      if (mu_button(ctx, pair.second.c_str())) {
        *powder = pair.first;
      }
    }
    mu_layout_row(ctx, 2, widths1, 0);
    mu_label(ctx, "Time Scale");
    float_slider(ctx, time_scale);
    mu_end_window(ctx);
  }
}


static void process_frame(mu_Context *ctx, uint8_t *brush_size, PowderType *powder, float *time_scale) {
  mu_begin(ctx);
  tool_window(ctx, brush_size, powder, time_scale);
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
  std::vector<std::vector<PowderType>> newWorld (H, std::vector<PowderType>(W, EMPTY));
  for (size_t y = 0; y < H; ++y)
  {
    for (size_t x = 0; x < W; ++x)
    {
      switch (world[y][x])
      {
        case DIRT:
        {
          if (y > 0 && world[y - 1][x] == EMPTY)
          {
            newWorld[y - 1][x] = DIRT;
          }
          else
          {
            newWorld[y][x] = DIRT;
          }
          break;
        }
        case STONE:
        {
          size_t dy = 1;
          while (dy <= worldData[y][x].dy + 1 && y >= dy && world[y - dy][x] == EMPTY)
            ++dy;
          --dy;
          if (dy > 0)
          {
            newWorld[y - dy][x] = STONE;
            worldData[y - dy][x] = worldData[y][x];
            worldData[y - dy][x].dy = dy;
          }
          else
          {
            newWorld[y][x] = STONE;
            worldData[y][x].dy = 0;
          }
          break;
        }
        default:
        {
          break;
        }
      }
    }
  }
  world = newWorld;
}

void render_powder()
{
  struct fenster* wnd = r_get_underlying();
  for (size_t y = 0; y < H; ++y)
  {
    for (size_t x = 0; x < W; ++x)
    {
      if (world[y][x] == DIRT)
        fenster_pixel(wnd, x, H - y) = r_color({255, 128, 64, 255});
      else if (world[y][x] == STONE)
        fenster_pixel(wnd, x, H - y) = r_color({128, 128, 128, 255});
    }
  }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  r_init(W, H);

  /* init microui */
  mu_Context *ctx = (mu_Context*)malloc(sizeof(mu_Context));
  mu_init(ctx);
  ctx->text_width = text_width;
  ctx->text_height = text_height;

  int fps = 60;
  int mousex = 0, mousey = 0;
  int oldmousex = 0, oldmousey = 0;
  uint8_t brush_size = 1;
  PowderType powder = DIRT;
  // float time_scale = std::numeric_limits<float>::infinity();
  float time_scale = 1.0f;
  bool isDrawing = false;

  /* main loop */
  for (;;) {
    int64_t before = r_get_time();
    oldmousex = mousex;
    oldmousey = mousey;
    if (r_mouse_moved(&mousex, &mousey)) {
      mu_input_mousemove(ctx, mousex, mousey);
    }
    if (r_mouse_down()) {
      mu_input_mousedown(ctx, mousex, mousey, MU_MOUSE_LEFT);
      if (!ctx->hover_root)
        isDrawing = true;
    } else if (r_mouse_up()) {
      mu_input_mouseup(ctx, mousex, mousey, MU_MOUSE_LEFT);
      isDrawing = false;
    }
    // from claude
    if (isDrawing) {
        int cy = H - mousey - 1, cx = mousex;
        int oy = H - oldmousey - 1, ox = oldmousex;
        int dx = std::abs(cx - ox), dy = std::abs(cy - oy);
        int sx = ox < cx ? 1 : -1, sy = oy < cy ? 1 : -1;
        int err = dx - dy;
        auto drawCircle = [&](int px, int py) {
            if (brush_size == 1) {
                if (py >= 0 && py < H && px >= 0 && px < W)
                    world[py][px] = powder;
                return;
            }
            int x = 0, y = brush_size, d = 1 - brush_size;
            auto fillRow = [&](int row, int x0, int x1) {
                if (row < 0 || row >= H) return;
                for (int i = std::max(0, x0); i <= std::min(W - 1, x1); i++)
                    world[row][i] = powder;
            };
            fillRow(py, px - brush_size, px + brush_size);
            while (x < y) {
                x++;
                d += d < 0 ? 2 * x + 1 : 2 * (x - y--) + 1;
                fillRow(py + y, px - x, px + x);
                fillRow(py - y, px - x, px + x);
                fillRow(py + x, px - y, px + y);
                fillRow(py - x, px - y, px + y);
            }
        };
        while (true) {
            drawCircle(ox, oy);
            if (ox == cx && oy == cy) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; ox += sx; }
            if (e2 <  dx) { err += dx; oy += sy; }
        }
    }
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
          mu_input_text(ctx, text);
        }
        continue;
      }
    }

    /* process frame */
    process_frame(ctx, &brush_size, &powder, &time_scale);

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
