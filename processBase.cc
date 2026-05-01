/*
 * Authors  : Lincoln Craddock, John Hershey
 * Date     : 2026-04-27
 * Professor: Dr. Gary Zoppetti
 * Class    : CMSC 476 Parallel Programming
 * Description: 
 */

#include "PowderGame.h"

static std::vector<Data> myWorld;

std::span<Data>
process_powder ()
{
  std::vector<Data> newWorld (
        H * W, { EMPTY, 0 });
  for (size_t x = 0; x < W; ++x)
  {
    for (size_t y = 0; y < H; ++y)
    {
      switch (myWorld[y + x * H].type)
      {
        case DIRT:
        {
          if (y > 0 && myWorld[y - 1 + x * H].type == EMPTY)
          {
            newWorld[y - 1 + x * H] = { DIRT, 0 };
          }
          else
          {
            newWorld[y + x * H] = { DIRT, 0 };
          }
          break;
        }
        case STONE:
        {
          unsigned dy = 1;
          while (dy <= myWorld[y + x * H].dy + 1 && y >= dy &&
                 myWorld[y - dy + x * H].type == EMPTY)
            ++dy;
          --dy;
          if (dy > 0)
          {
            newWorld[y - dy + x * H] = { STONE, dy };
          }
          else
          {
            newWorld[y + x * H] = { STONE, 0 };
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
  myWorld = newWorld;
  return myWorld;
}

std::span<Data>
set_up_processing ()
{
  myWorld = std::vector<Data> (H * W, { EMPTY, 0 });
  return myWorld;
}