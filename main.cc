/*
 * Authors: Lincoln Craddock, John Hershey
 * Last Edit Date: 2026-04-24
 * Professor: Dr. Gary Zoppetti
 * Class: CMSC 476 Parallel Programming
 * Description: Uses Fenster to render a gravity-based powder simulation
 */
extern "C"
{
#include "fenster.h"
#include "microui.h"
#include "renderer.h"
}

#include <charconv>
#include <cmath>
#include <ctype.h>
#include <getopt.h>
#include <iostream> //debug
#include <limits>
#include <numbers>
#include <ranges>
#include <stdio.h>
#include <stdlib.h>

#if defined METAL
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "metal-cpp/Metal/Metal.hpp"
#endif

#include "PowderGame.h"

size_t W = 1000;
size_t H = 1000;
static float bg[3] = { 0, 0, 0 };
static std::vector<std::vector<Data>> world;
static float tan_time_scale = 60.0f;

// Sets the W and H global vars, which represent the width and height of the
// window.
void
setWH (int argc, char** argv);

// Floors a float and returns a string representing that value.
static char*
uint8_representation (mu_Real value);

// Updates a UI slider over uint8 values. Call this every frame.
static int
uint8_slider (mu_Context* ctx, unsigned char* value, int low, int high);

// Takes the tan() of a float and returns a string representing that value.
//
// Also updates the public variable tan_time_scale
static char*
tan_representation (mu_Real value);

// Updates a UI slider over float values. Call this every frame.
static int
float_slider (mu_Context* ctx, float* value);

// Displays the main toolbar window with the brush size, time scale, and other
// buttons and sliders. Call this every frame.
static void
tool_window (mu_Context* ctx,
             uint8_t* brush_size,
             PowderType* powder,
             float* time_scale,
             int64_t compute_time_ms);

// Processes and updates the UI. Call this every frame.
static void
process_frame (mu_Context* ctx,
               uint8_t* brush_size,
               PowderType* powder,
               float* time_scale,
               int64_t compute_time_ms);

// Gets the pixel length a string of text will be when diplayed in the UI.
static int
text_width (mu_Font font, const char* text, int len);

// Gets the pixel height a string of text will be when diplayed in the UI.
static int
text_height (mu_Font font);

// Computes a single step in the powder simulation. Call this at a rate
// determined by the user.

// Displays the powder in the simulation. Call this every frame.
void
render_powder ();

int
main (int argc, char** argv)
{
  setWH (argc, argv);
  world =
    std::vector<std::vector<Data>> (H, std::vector<Data> (W, { EMPTY, 0 }));
  r_init (W, H);

  /* init microui */
  mu_Context* ctx = (mu_Context*) malloc (sizeof (mu_Context));
  mu_init (ctx);
  ctx->text_width = text_width;
  ctx->text_height = text_height;

  const int fps = 60;
  const int64_t frame_budget_ms = 1'000 / fps;
  int mousex = 0, mousey = 0;
  int oldmousex = 0, oldmousey = 0;

  uint8_t brush_size = 1;
  PowderType powder = DIRT;
  float time_scale = std::atan (tan_time_scale / 10.0f);
  bool isDrawing = false;

#if defined METAL
  /* Find a GPU */
  MTL::Device* device = MTL::CreateSystemDefaultDevice ();
  if (!device)
  {
    std::cerr << "Metal not supported" << std::endl;
    return 1;
  }
  std::cout << "Using GPU: " << device->name ()->utf8String () << "\n";

  if (device->supportsFamily (MTL::GPUFamilyMetal4))
    std::cout << "Supports Metal 4!" << std::endl;
  else if (device->supportsFamily (MTL::GPUFamilyMetal3))
    std::cout << "Only supports Metal 3..." << std::endl;

  /* Metal function */
  const char* shaderSrc = R"(
#include <metal_stdlib>
using namespace metal;

struct Data
{
PowderType type = EMPTY;
uint32_t dy = 0;
};

kernel void process_powder (
device const Data* world [[ buffer (0) ]],
device Data* newWorld [[ buffer (1) ]],
uint index [[ thread_position_in_grid ]]
) {
// TODO
newWorld[index] = world[index];
}
)";

  /* Get a ref to the metal function */
  NS::Error* error = nullptr;
  NS::String* src = NS::String::string (shaderSrc, NS::UTF8StringEncoding);
  MTL::CompileOptions* opts = MTL::CompileOptions::alloc ()->init ();
  MTL::Library* library = device->newLibrary (src, opts, &error);
  if (!library)
  {
    std::cerr << "Shader error: "
              << error->localizedDescription ()->utf8String () << std::endl;
    return 1;
  }

  /* Prepare a metal pipeline */
  NS::String* fnName =
    NS::String::string ("process_powder", NS::UTF8StringEncoding);
  MTL::Function* function = library->newFunction (fnName);
  // A 'compute' pipeline runs a single 'compute' function
  MTL::ComputePipelineState* pipeline =
    device->newComputePipelineState (function, &error);

  /* Create data buffers and load data into them */
  const uint count = 16;
  const size_t bufferSize = count * sizeof (Data);

  MTL::Buffer* worldBuf = device->newBuffer (
    world.data (), bufferSize, MTL::ResourceStorageModeShared);
  // TODO: call a method to fill the data, not this
  std::vector<std::vector<Data>> newWorld (H,
                                           std::vector<Data> (W, { EMPTY, 0 }));
  MTL::Buffer* newWorldBuf = device->newBuffer (
    newWorld.data (), bufferSize, MTL::ResourceStorageModeShared);

  /* Create descriptors */
  MTL4::CommandQueueDescriptor* queueDesc =
    MTL4::CommandQueueDescriptor::alloc ()->init ();
  MTL4::CommandAllocatorDescriptor* allocDesc =
    MTL4::CommandAllocatorDescriptor::alloc ()->init ();
  MTL4::ArgumentTableDescriptor* tableDesc =
    MTL4::ArgumentTableDescriptor::alloc ()->init ();
  tableDesc->setMaxBufferBindCount (2);

  /* Create a command queue, allocator, buffer, and argument table from
   * descriptors */
  MTL4::CommandQueue* queue = device->newMTL4CommandQueue (queueDesc, &error);
  MTL4::CommandAllocator* allocator =
    device->newCommandAllocator (allocDesc, &error);
  MTL4::CommandBuffer* cmdBuf = device->newCommandBuffer ();
  MTL4::ArgumentTable* argTable = device->newArgumentTable (tableDesc, &error);

  /* Give the argument table access to data */
  // the indices (0 and 1) represent the paramaters the buffers supply
  argTable->setAddress (worldBuf->gpuAddress (), 0);
  argTable->setAddress (newWorldBuf->gpuAddress (), 1);

  /* Create a shared event */
  MTL::SharedEvent* frameCompletionEvent = device->newSharedEvent ();
  uint64_t frameIdx = 0;
  frameCompletionEvent->setSignaledValue (frameIdx);
#endif

  int64_t compute_time_ms = 0;
  int64_t ui_time_ms = 0;

  /* main loop */
  int64_t time_of_last_compute = r_get_time ();
  for (;;)
  {
    int64_t ui_before = r_get_time ();

    /* process user input */
    oldmousex = mousex;
    oldmousey = mousey;
    if (r_mouse_moved (&mousex, &mousey))
    {
      mu_input_mousemove (ctx, mousex, mousey);
    }
    if (r_mouse_down ())
    {
      mu_input_mousedown (ctx, mousex, mousey, MU_MOUSE_LEFT);
      if (!ctx->hover_root)
        isDrawing = true;
    }
    else if (r_mouse_up ())
    {
      mu_input_mouseup (ctx, mousex, mousey, MU_MOUSE_LEFT);
      isDrawing = false;
    }
    // from claude
    if (isDrawing)
    {
      int cy = H - mousey - 1, cx = mousex;
      int oy = H - oldmousey - 1, ox = oldmousex;
      int dx = std::abs (cx - ox), dy = std::abs (cy - oy);
      int sx = ox < cx ? 1 : -1, sy = oy < cy ? 1 : -1;
      int err = dx - dy;
      auto drawCircle = [&] (int px, int py)
      {
        if (brush_size == 1)
        {
          if (py >= 0 && (size_t) py < H && px >= 0 && (size_t) px < W)
            world[py][px] = { powder, 0 };
          return;
        }
        int x = 0, y = brush_size - 1, d = -brush_size;
        auto fillRow = [&] (int row, int x0, int x1)
        {
          if (row < 0 || (size_t) row >= H)
            return;
          for (int i = std::max (0, x0); i <= std::min ((int) W - 1, x1); i++)
            world[row][i] = { powder, 0 };
        };
        fillRow (py, px - brush_size - 1, px + brush_size - 1);
        while (x < y)
        {
          x++;
          d += d < 0 ? 2 * x + 1 : 2 * (x - y--) + 1;
          fillRow (py + y, px - x, px + x);
          fillRow (py - y, px - x, px + x);
          fillRow (py + x, px - y, px + y);
          fillRow (py - x, px - y, px + y);
        }
      };
      while (true)
      {
        drawCircle (ox, oy);
        if (ox == cx && oy == cy)
          break;
        int e2 = 2 * err;
        if (e2 > -dy)
        {
          err -= dy;
          ox += sx;
        }
        if (e2 < dx)
        {
          err += dx;
          oy += sy;
        }
      }
    }

    if (r_key_down (0x1b))
    {
      break;
    } // esc
    if (r_key_down ('\n'))
    {
      mu_input_keydown (ctx, MU_KEY_RETURN);
    }
    else if (r_key_up ('\n'))
    {
      mu_input_keyup (ctx, MU_KEY_RETURN);
    }
    if (r_key_down ('\b'))
    {
      mu_input_keydown (ctx, MU_KEY_BACKSPACE);
    }
    else if (r_key_up ('\b'))
    {
      mu_input_keyup (ctx, MU_KEY_BACKSPACE);
    }
    for (char i : std::views::iota (0, 256))
    {
      if (r_key_down (i))
      {
        if (' ' <= i && i <= '~')
        {
          char text[2] = { i, 0 };
          if (isalpha (i))
          {
            if (!r_shift_pressed ())
            {
              text[0] = tolower (i);
            }
          }
          mu_input_text (ctx, text);
        }
        continue;
      }
    }

    /* process next ui frame */
    process_frame (ctx, &brush_size, &powder, &time_scale, compute_time_ms);

    /* render */
    r_clear (mu_color (bg[0], bg[1], bg[2], 255));
    render_powder ();
    mu_Command* cmd = NULL;
    while (mu_next_command (ctx, &cmd))
    {
      switch (cmd->type)
      {
        case MU_COMMAND_TEXT:
          r_draw_text (cmd->text.str, cmd->text.pos, cmd->text.color);
          break;
        case MU_COMMAND_RECT:
          r_draw_rect (cmd->rect.rect, cmd->rect.color);
          break;
        case MU_COMMAND_ICON:
          r_draw_icon (cmd->icon.id, cmd->icon.rect, cmd->icon.color);
          break;
        case MU_COMMAND_CLIP:
          r_set_clip_rect (cmd->clip.rect);
          break;
      }
    }
    r_present ();

    int64_t ui_after = r_get_time ();
    ui_time_ms = ui_after - ui_before;
    int64_t compute_before = r_get_time ();

    int64_t sleep_time_ms = frame_budget_ms - ui_time_ms;

    // compute frames at a rate set by time_scale
    if (compute_before - time_of_last_compute > 1000 / tan_time_scale)
    {
      time_of_last_compute = compute_before;
/* compute a timestep in the powder simulation */
#if defined METAL
      /* Wait for GPU to finish to begin next frame*/
      frameCompletionEvent->waitUntilSignaledValue (
        frameIdx - 1, /* timeout milliseconds = */ 1000);
      ++frameIdx;

      /* Create a command encoder that reuses the allocator */
      allocator->reset ();
      cmdBuf->beginCommandBuffer (allocator);
      MTL4::ComputeCommandEncoder* encoder = cmdBuf->computeCommandEncoder ();

      /* Write data in the argument table to the buffer via the encoder */
      encoder->setArgumentTable (argTable);

      /* Write commands to the buffer via the encoder */
      encoder->setComputePipelineState (pipeline);

      // count × 1 × 1 grid of threads
      // TODO: 2D
      MTL::Size threadsPerGrid = MTL::Size (count, 1, 1);
      NS::UInteger maxThreads = pipeline->maxTotalThreadsPerThreadgroup ();
      if (maxThreads > count)
        maxThreads = count;
      MTL::Size threadsPerGroup = MTL::Size (maxThreads, 1, 1);
      encoder->dispatchThreads (threadsPerGrid, threadsPerGroup);
      encoder->endEncoding ();
      cmdBuf->endCommandBuffer ();

      // /* Finally, run the commands encoded in the command buffer on the GPU
      // */ cmdBuf->commit ();

      /* Finally, run the commands encoded in the command buffer on the GPU */
      MTL4::CommitOptions* commitOpts = MTL4::CommitOptions::alloc ()->init ();
      queue->commit (&cmdBuf, 1, commitOpts);
      commitOpts->release ();

      // /* Wait until the commands have finished */
      // cmdBuf->waitUntilCompleted ();

      /* Wait until the commands have finished */
      queue->signalEvent (frameCompletionEvent, frameIdx);
#else
      std::vector<std::vector<Data>> newWorld (
        H, std::vector<Data> (W, { EMPTY, 0 }));
      process_powder (world, newWorld, H, W);
#endif
      world = newWorld;
      int64_t compute_after = r_get_time ();
      compute_time_ms = compute_after - compute_before;
      sleep_time_ms -= compute_time_ms;
    }

    if (sleep_time_ms > 0)
    {
      r_sleep (sleep_time_ms);
    }
  }

  return 0;
}

void
setWH (int argc, char** argv)
{
  /* State */
  int opt = '\0';
  char prevOpt = '\0';
  int numArgsPrevOpt = 0;

  /* Parse using getopt_long */
  const option longOptions[] = { option { "help", 0, NULL, 'h' },
                                 option { "size", 1, NULL, 's' },
                                 option { 0, 0, 0, 0 } };
  const int numLongOptions = 2;
  while (true)
  {
    switch (opt = getopt_long (argc, argv, "-:s:C:h", longOptions, NULL))
    {
      case 'h':
      {
        std::cout << "./main [-h] [-s LENGTH[, HEIGHT]]" << std::endl;
        exit (EXIT_SUCCESS);
      }
      case 's':
      {
        prevOpt = 's';
        numArgsPrevOpt = 0;

        char* optArgCpy = strdup (optarg);
        char* tok = strtok (optArgCpy, ", \t\n");
        if (!tok)
        {
          std::cerr << "" << std::endl;
          exit (EXIT_FAILURE);
        }

        /* 1st arg */
        {
          ++numArgsPrevOpt;
          char* begin = tok;
          char* end = tok + strlen (tok);

          auto [ptr, ec] = std::from_chars (begin, end, W);
          if (ec == std::errc::result_out_of_range)
          {
            std::cerr << "-s has a numeric argument that is too large."
                      << std::endl;
            exit (EXIT_FAILURE);
          }
          else if (ptr != end || ec != std::errc () || W == 0)
          {
            std::cerr << "-s should have positive, non-zero numeric arguments."
                      << std::endl;
            exit (EXIT_FAILURE);
          }
          H = W;

          tok = strtok (NULL, ", \t\n");
          if (!tok)
            break;
        }

        /* 2nd arg */
        {
          ++numArgsPrevOpt;
          char* begin = tok;
          char* end = tok + strlen (tok);

          auto [ptr, ec] = std::from_chars (begin, end, H);
          if (ec == std::errc::result_out_of_range)
          {
            std::cerr << "-s has a numeric argument that is too large."
                      << std::endl;
            exit (EXIT_FAILURE);
          }
          else if (ptr != end || ec != std::errc () || H == 0)
          {
            std::cerr << "-s should have positive, non-zero numeric arguments."
                      << std::endl;
            exit (EXIT_FAILURE);
          }

          tok = strtok (NULL, " \t\n");
          if (tok)
          {
            std::cerr << "-s has too many arguments." << std::endl;
            exit (EXIT_FAILURE);
          }

          free (optArgCpy);
          break;
        }
      }
      case 1:
      {
        /* 2nd arg for an opt, or arg without an opt */
        switch (prevOpt)
        {
          case 's':
          {
            if (numArgsPrevOpt > 1)
            {
              std::cerr << "-s has too many arguments." << std::endl;
              exit (EXIT_FAILURE);
            }

            /* 2nd arg */
            ++numArgsPrevOpt;
            const char* begin = optarg;
            const char* end = optarg + strlen (optarg);

            auto [ptr, ec] = std::from_chars (begin, end, H);
            if (ec == std::errc::result_out_of_range)
            {
              std::cerr << "-s has a numeric argument that is too large."
                        << std::endl;
              exit (EXIT_FAILURE);
            }
            else if (ptr != end || ec != std::errc () || H == 0)
            {
              std::cerr
                << "-s should have positive, non-zero numeric arguments."
                << std::endl;
              exit (EXIT_FAILURE);
            }
          }

          break;
          case '\0':
          {
            /* Arg without an opt */
            // treat it like an option that was misspelled
            std::cerr << argv[optind - 1]
                      << " isn't a recognized command-line option.";
            break;
          }
        }
        break;
      }
      case -1:
      {
        /* No more opts or args */
        return;
      }
      case ':':
      {
        /* Missing arg */
        std::string unknownOpt = argv[optind - 1];
        if (unknownOpt[0] == '-' && unknownOpt[1] == '-')
        {
          // long opt
          std::string name = unknownOpt.substr (2);
          for (int i = 0; i < numLongOptions; ++i)
          {
            option o = longOptions[i];
            if (name == o.name && o.flag == NULL)
            {
              std::cerr << "-" << (char) o.val << " has too few arguments."
                        << std::endl;
              exit (EXIT_FAILURE);
            }
          }

          // should not reach this point, all long options must have a
          // single-char variant
        }
        else
        {
          // single-char opt
          std::cerr << "-" << (char) optopt << " has too few arguments."
                    << std::endl;
          exit (EXIT_FAILURE);
        }
        break;
      }
      case '?':
      {
        /* Unknown opt, or opt with too many args */
        std::string unknownOpt = argv[optind - 1];
        if (unknownOpt[0] == '-' && unknownOpt[1] == '-')
        {
          // long opt
          size_t i = unknownOpt.find ('=');
          std::string name =
            unknownOpt.substr (2, std::min (i, unknownOpt.size ()) - 2);
          for (int i = 0; i < numLongOptions; ++i)
          {
            option o = longOptions[i];
            if (name == o.name)
            {
              if (o.flag == NULL)
              {
                if (optopt != 'h')
                {
                  std::cerr << "-" << (char) o.val << " has too many arguments."
                            << std::endl;
                  exit (EXIT_FAILURE);
                }
                else
                {
                  std::cout << "./main [-h] [-s LENGTH[, HEIGHT]]" << std::endl;
                  exit (EXIT_SUCCESS);
                }
              }
              else
              {
                // should not reach this point, all long options must have a
                // single-char variant
              }
            }
          }

          std::cerr << "--" << name << " is not a valid command-line argument."
                    << std::endl;
          exit (EXIT_FAILURE);
        }
        else
        {
          // single-char opt
        }
        std::cerr << "-" << (char) optopt
                  << " is not a valid command-line argument." << std::endl;
        exit (EXIT_FAILURE);
      }
    }
  }
}

static char*
uint8_representation (mu_Real value)
{
  char* buf = (char*) malloc (sizeof (char) * (MU_MAX_FMT + 1));
  uint8_t disp_value = (uint8_t) value;
  snprintf (buf, MU_MAX_FMT + 1, "%i", disp_value);
  return buf;
}

static int
uint8_slider (mu_Context* ctx, unsigned char* value, int low, int high)
{
  static float tmp;
  mu_push_id (ctx, &value, sizeof (value));
  tmp = *value;
  int res = mu_slider_ex (
    ctx, &tmp, low, high, 0, uint8_representation, MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id (ctx);
  return res;
}

static char*
tan_representation (mu_Real value)
{
  char* buf = (char*) malloc (sizeof (char) * (MU_MAX_FMT + 1));
  tan_time_scale = 10.0f * std::tan (value);
  if (tan_time_scale > 9'000.0f)
    snprintf (buf, MU_MAX_FMT + 1, "infinity");
  else
    snprintf (buf, MU_MAX_FMT + 1, "%.2f", tan_time_scale);
  return buf;
}

static int
float_slider (mu_Context* ctx, float* value)
{
  static float tmp;
  mu_push_id (ctx, &value, sizeof (value));
  tmp = *value;
  int res = mu_slider_ex (ctx,
                          &tmp,
                          0.0f,
                          (std::numbers::pi_v<float>) / 2 - 0.001f,
                          0,
                          tan_representation,
                          MU_OPT_ALIGNCENTER);
  if (tmp > 9'000.0f)
    *value = std::numeric_limits<float>::infinity ();
  else
    *value = tmp;
  mu_pop_id (ctx);
  return res;
}

static void
tool_window (mu_Context* ctx,
             uint8_t* brush_size,
             PowderType* powder,
             float* time_scale,
             int64_t compute_time_ms)
{
  if (mu_begin_window (ctx, "Tools", mu_rect (0, 1, W, 110), 0))
  {
    // draw 1st row -- brush size
    int sw = mu_get_current_container (ctx)->body.w - 80 - 20;
    int widths1[] { 80, sw, -1 };
    mu_layout_row (ctx, 2, widths1, 0);
    mu_label (ctx, "Brush Size");
    uint8_slider (ctx, brush_size, 1, 100);

    // draw 2nd row -- powder type
    int bw = (sw + 4 - 20 - 4) / powderNames.size () - 4;
    std::vector<int> widths2 (powderNames.size () + 3, bw);
    widths2[0] = 80;
    widths2[1] += (sw + 4 - 20 - 4) % powderNames.size ();
    *--(--widths2.end ()) = 20;
    widths2.back () = -1;
    mu_layout_row (ctx, powderNames.size () + 2, widths2.data (), 0);
    mu_label (ctx, "Powder:");
    for (const auto& pair : powderNames)
    {
      if (mu_button (ctx, pair.second.c_str ()))
      {
        *powder = pair.first;
      }
    }
    mu_draw_rect (ctx, mu_layout_next (ctx), powderColors[*powder]);

    // draw 3rd row -- time scale
    int widths3[] { 80, sw - 160, 160, -1 };
    mu_layout_row (ctx, 3, widths3, 0);
    mu_label (ctx, "Time Scale:");
    float_slider (ctx, time_scale);
    static char buf[64];
    snprintf (buf, 64, "Compute Time: %lld ms", (long long) compute_time_ms);
    mu_label (ctx, buf);
    mu_end_window (ctx);
  }
}

static void
process_frame (mu_Context* ctx,
               uint8_t* brush_size,
               PowderType* powder,
               float* time_scale,
               int64_t compute_time_ms)
{
  mu_begin (ctx);
  tool_window (ctx, brush_size, powder, time_scale, compute_time_ms);
  mu_end (ctx);
}

static int
text_width (mu_Font font, const char* text, int len)
{
  (void) font;
  if (len == -1)
  {
    len = strlen (text);
  }
  return r_get_text_width (text, len);
}

static int
text_height (mu_Font font)
{
  (void) font;
  return r_get_text_height ();
}

#if defined METAL
void
process_powder ()
{
  std::vector<std::vector<Data>> newWorld (H,
                                           std::vector<Data> (W, { EMPTY, 0 }));

  for (size_t y = 0; y < H; ++y)
  {
    for (size_t x = 0; x < W; ++x)
    {
      switch (world[y][x].type)
      {
        case DIRT:
        {
          if (y > 0 && world[y - 1][x].type == EMPTY)
          {
            newWorld[y - 1][x] = { DIRT, 0 };
          }
          else
          {
            newWorld[y][x] = { DIRT, 0 };
          }
          break;
        }
        case STONE:
        {
          uint32_t dy = 1;
          while (dy <= world[y][x].dy + 1 && y >= dy &&
                 world[y - dy][x].type == EMPTY)
            ++dy;
          --dy;
          if (dy > 0)
          {
            newWorld[y - dy][x] = { STONE, dy };
          }
          else
          {
            newWorld[y][x] = { STONE, 0 };
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
#endif

void
render_powder ()
{
  struct fenster* wnd = r_get_underlying ();
  for (size_t y = 0; y < H; ++y)
  {
    for (size_t x = 0; x < W; ++x)
    {
      fenster_pixel (wnd, x, H - y - 1) =
        r_color (powderColors[world[y][x].type]);
    }
  }
}
