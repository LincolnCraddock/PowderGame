/*
 * Filename: PowderGame.h
 * Authors:  Lincoln Craddock, John Hershey
 * Date:     2024-04-28
 * header file with universal data types for powder rendering
 */

#include <map>
#include <vector>
#include "microui.h"
#include <string>

enum PowderType
{
  EMPTY,
  DIRT,
  STONE,
};

static std::map<PowderType, std::string> powderNames {
  { EMPTY, "Eraser" },
  { DIRT, "Dirt" },
  { STONE, "Stone" },
};

static std::map<PowderType, mu_Color> powderColors {
  { EMPTY, { 0, 0, 0, 255 } },
  { DIRT, { 255, 128, 64, 255 } },
  { STONE, { 128, 128, 128, 255 } },
};

struct Data
{
  PowderType type = EMPTY;
  // only computed for stone
  unsigned dy = 0;
};

extern std::vector<std::vector<Data>>
process_powder (std::vector<std::vector<Data>>& world, unsigned H, unsigned W);