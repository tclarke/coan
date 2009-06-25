/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "HlsToRgb.h"
#include "ImProcVersion.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"

REGISTER_PLUGIN_BASIC(ColorSpace, HlsToRgb);

#define EPSILON 0.000001

HlsToRgb::HlsToRgb()
{
   setName("HlsToRgb");
   setDescription("Convert between HLS and RGB.");
   setDescriptorId("{694B7FAB-E31B-4689-A0A2-7B91FCE49AA4}");
   setMenuLocation("[General Algorithms]/Colorspace Conversion/HLS->RGB");
   setScaleData(false);
}

HlsToRgb::~HlsToRgb()
{
}

#define CALC_COMPONENT(t_C, C) \
   if (t_C < 0.0) \
   { \
      t_C += 1.0; \
   } \
   if (t_C > 1.0) \
   { \
      t_C -= 1.0; \
   } \
   if (t_C < 0.166667) \
   { \
      C = p + ((q - p) * 6 * t_C); \
   } \
   else if(t_C >= 0.166667 && t_C < 0.5) \
   { \
      C = q; \
   } \
   else if(t_C >= 0.5 && t_C < 0.666667) \
   { \
      C = p + ((q - p) * 6 * (0.666667 - t_C)); \
   } \
   else \
   { \
      C = p; \
   }

void HlsToRgb::colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent)
{
   double h = pInput[0];
   double l = pInput[1];
   double s = pInput[2];
   double r = 0.0;
   double g = 0.0;
   double b = 0.0;

   if (s == 0)
   {
      pOutput[0] = pOutput[1] = pOutput[2] = l;
      return;
   }

   double q = (l < 0.5) ? (l * (1 + s)) : (l + s - (l * s));
   double p = 2 * l - q;
   double h_k = h / 360.0;
   double t_R = h_k + 0.33333333;
   double t_G = h_k;
   double t_B = h_k - 0.33333333;
   
   CALC_COMPONENT(t_R, r);
   CALC_COMPONENT(t_G, g);
   CALC_COMPONENT(t_B, b);

   pOutput[0] = r;
   pOutput[1] = g;
   pOutput[2] = b;
}
