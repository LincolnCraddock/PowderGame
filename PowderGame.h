/*
 * Filename: PowderGame.h
 * Authors:  Lincoln Craddock, John Hershey
 * Date:     2024-04-24
 * header file with universal data types for powder rendering
 */

#include <map>
#include "microui.h"

enum PowderType
{
  EMPTY,
  DIRT,
  STONE,
};

std::map<PowderType, std::string> powderNames {
  { EMPTY, "Eraser" },
  { DIRT, "Dirt" },
  { STONE, "Stone" },
};

std::map<PowderType, mu_Color> powderColors {
  { EMPTY, { 0, 0, 0, 255 } },
  { DIRT, { 255, 128, 64, 255 } },
  { STONE, { 128, 128, 128, 255 } },
};

struct Data
{
  PowderType type = EMPTY;
  // only computed for stone
  uint32_t dy = 0;
};