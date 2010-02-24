/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef TRACKINGUTILS_H__
#define TRACKINGUTILS_H__

#include "AppConfig.h"
#include "LocationType.h"

typedef uint32_t subcubeid_t;

/**
 * Calculate a sub-cube ID for a given point.
 *
 * A sub-cube ID uses a quadtree to encode a sub-cube of an image.
 * It is a 32-bit unsigned value. The least significant 4 bits encode the number of
 * the remaining bit pairs which are significat. The remaining bits encode the child
 * ID at each level of a quad tree, least significat to most. The quadrants are 0-3 where
 * 0 is the upper right quadrant (smallest y, largest x) and the remaining follow anti-clockwise.
 * The id 0x00000000 indicates the entire image. The id 0x00000902 (0b10010010) indicates
 * there are 2 significant bit pairs. Those 4 bits point to the 1 quadrant (upper-left) and the
 * 2 sub-quadrant (lower-left). The id 0xffffffff is an invalid ID and can be used for initialization
 * of variables, default error values, etc.
 *
 * @param location
 *        The point to encode.
 * @param levels
 *        The number of levels deep to calculate.
 * @param maxBb
 *        The largest x, y values (inclusive) of the bounding box used for calculations.
 * @param minBb
 *        The smallest x, y values (inclusive) of the bounding box used for calculations.
 * @return The sub-cube ID.
 */
subcubeid_t calculateSubcubeId(LocationType location, uint8_t levels, Opticks::PixelLocation maxBb, Opticks::PixelLocation minBb = Opticks::PixelLocation(0, 0))
{
   subcubeid_t id(levels);
   for (uint8_t level = 0; level < levels; ++level)
   {
      Opticks::PixelLocation center((maxBb.mX - minBb.mX + 1) / 2.0 + minBb.mX, (maxBb.mY - minBb.mY + 1) / 2.0 + minBb.mY);
      if (location.mX >= center.mX && location.mY <= center.mY) // quadrant 0
      {
         minBb.mX = center.mX;
         maxBb.mY = center.mY;
      }
      else if (location.mX < center.mX && location.mY <= center.mY) // quadrant 1
      {
         id |= 1 << (level * 2 + 4);
         maxBb = center;
      }
      else if (location.mX < center.mX && location.mY > center.mY) // quadrant 2
      {
         id |= 2 << (level * 2 + 4);
         maxBb.mX = center.mX;
         minBb.mY = center.mY;
      }
      else // quadrant 3
      {
         id |= 3 << (level * 2 + 4);
         minBb = center;
      }
   }
   return id;
}

/**
 * Calculate the extents in the sub-cube for a sub-cube ID.
 *
 * @param subcubeId
 *        The ID.
 * @param levels
 *        Output param containing the number of levels deep.
 * @param maxBb
 *        Input/output parameter.
 *        Initially The largest x, y values (inclusive) of the bounding box used for calculations.
 *        On return, contains the largest x, y values of the specified sub-cube.
 * @param minBb
 *        Input/output parameter.
 *        Initially The smallest x, y values (inclusive) of the bounding box used for calculations.
 *        On return, contains the smallest x, y values of the specified sub-cube.
 * @return True on success, false otherwise. If false, the output parameters are undefined. False usually
 *         indicates an invalid subcubeId
 */
bool calculateSubcubeBounds(subcubeid_t subcubeId, uint8_t& levels, Opticks::PixelLocation& maxBb, Opticks::PixelLocation& minBb)
{
   levels = subcubeId & 0x0f;
   subcubeId >>= 4;
   for (uint8_t level = 0; level < levels; ++level)
   {
      Opticks::PixelLocation center((maxBb.mX - minBb.mX + 1) / 2.0 + minBb.mX, (maxBb.mY - minBb.mY + 1) / 2.0 + minBb.mY);
      switch (subcubeId & 0x03) // quadrant number
      {
      case 0:
         minBb.mX = center.mX;
         maxBb.mY = center.mY;
         break;
      case 1:
         maxBb = center;
         break;
      case 2:
         maxBb.mX = center.mX;
         minBb.mY = center.mY;
         break;
      case 3:
         minBb = center;
         break;
      default:
         break; // unreachable
      }
      subcubeId >>= 2;
   }
   return subcubeId == 0;
}

/**
 * Calculate the quadtree level needed to get a sub-cube of at most the specified size.
 *
 * @param maxSubcubeSize
 *        Maximum size of the subcube in pixels^2.
 * @param maxBb
 *        The largest x, y values (inclusive) of the bounding box used for calculations.
 * @param minBb
 *        The smallest x, y values (inclusive) of the bounding box used for calculations.
 * @return The level needed. If it can't be represented, 0xff will be returned.
 */
uint8_t calculateNeededLevels(uint32_t maxSubcubeSize, Opticks::PixelLocation maxBb, Opticks::PixelLocation minBb = Opticks::PixelLocation(0, 0))
{
   maxBb = (maxBb - minBb) + 1;
   for (uint8_t level = 0; level <= 12; ++level) // maximum number of levels which can be represented
   {
      uint32_t curSize = maxBb.mX * maxBb.mY;
      if (curSize <= maxSubcubeSize)
      {
         return level;
      }
      maxBb.mX /= 2;
      maxBb.mY /= 2;
   }
   return 0xff; // Invalid return
}

#endif
