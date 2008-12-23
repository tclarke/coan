/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "HsvToRgb.h"
#include "ImProcVersion.h"
#include "PlugInArgList.h"
#include "PlugInFactory.h"

PLUGINFACTORY(HsvToRgb);

#define EPSILON 0.000001

HsvToRgb::HsvToRgb()
{
   setName("HsvToRgb");
   setDescription("Convert between HSV and RGB.");
   setDescriptorId("{3C081B48-AD4C-4e0b-85AA-AE5B2443CC55}");
   setMenuLocation("[General Algorithms]/Colorspace Conversion/HSV->RGB");
   setScaleData(false);
}

HsvToRgb::~HsvToRgb()
{
}

void HsvToRgb::colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent)
{
   double h = pInput[0];
   double s = pInput[1];
   double v = pInput[2];

   double h_60 = h / 60.0;
   double h_60_floor = std::floor(h_60);
   int h_i = static_cast<int>(fmod(h_60_floor, 6.0));
   double f = h_60 - h_60_floor;
   double p = v * (1 - s);
   double q = v * (1 - f * s);
   double t = v * (1 - (1 - f) * s);
   double r = 0.0;
   double g = 0.0;
   double b = 0.0;

   switch(h_i)
   {
   case 0:
      r = v;
      g = t;
      b = p;
      break;
   case 1:
      r = q;
      g = v;
      b = p;
      break;
   case 2:
      r = p;
      g = v;
      b = t;
      break;
   case 3:
      r = p;
      g = q;
      b = v;
      break;
   case 4:
      r = t;
      g = p;
      b = v;
      break;
   case 5:
      r = v;
      g = p;
      b = q;
      break;
   }
   pOutput[0] = r;
   pOutput[1] = g;
   pOutput[2] = b;
}
