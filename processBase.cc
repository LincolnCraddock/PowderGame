/*
 * Authors  : Lincoln Craddock, John Hershey
 * Date     : 2026-04-27
 * Professor: Dr. Gary Zoppetti
 * Class    : CMSC 476 Parallel Programming
 * Description: 
 */

#include "PowderGame.h"

std::vector<std::vector<Data>>
process_powder (std::vector<std::vector<Data>>& world, unsigned H, unsigned W)
{
  std::vector<std::vector<Data>> newWorld (
        H, std::vector<Data> (W, { EMPTY, 0 }));
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
          unsigned dy = 1;
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
}