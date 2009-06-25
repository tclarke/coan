/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "ImProcVersion.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "RgbToHls.h"

REGISTER_PLUGIN_BASIC(ColorSpace, RgbToHls);

#define EPSILON 0.000001

RgbToHls::RgbToHls() : mNormalizeHue(true)
{
   setName("RgbToHls");
   setDescription("Convert between RGB and HLS.");
   setDescriptorId("{6AAEA42F-73A4-45ec-A70B-A36E3B38D9A6}");
   setMenuLocation("[General Algorithms]/Colorspace Conversion/RGB->HLS");
   setScaleData(true);
}

RgbToHls::~RgbToHls()
{
}

bool RgbToHls::getInputSpecification(PlugInArgList*& pInArgList)
{
   if (!ColorSpaceConversionShell::getInputSpecification(pInArgList))
   {
      return false;
   }
   VERIFY(pInArgList->addArg<bool>("Normalize Hue", mNormalizeHue, std::string("Should the hue be normalized to (0,360]?")));
   return true;
}

bool RgbToHls::extractInputArgs(PlugInArgList* pInArgList)
{
   if (!ColorSpaceConversionShell::extractInputArgs(pInArgList))
   {
      return false;
   }
   pInArgList->getPlugInArgValue("Normalize Hue", mNormalizeHue);
   return true;
}

void RgbToHls::colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent)
{
   double red = pInput[0];
   double green = pInput[1];
   double blue = pInput[2];
   double h = 0.0;
   double l = 0.0;
   double s = 0.0;
   if (fabs(maxComponent) < EPSILON)
   {
      pOutput[0] = pOutput[1] = pOutput[2] = 0.0;
      return;
   }

   // hue
   if (fabs(maxComponent - minComponent) < EPSILON)
   {
      h = 0;
   }
   else if (fabs(maxComponent - red) < EPSILON)
   {
      h = fmod(60 * (green - blue) / (maxComponent - minComponent), 360);
   }
   else if (fabs(maxComponent - green) < EPSILON)
   {
      h = 60 * (blue - red) / (maxComponent - minComponent) + 120;
   }
   else if (fabs(maxComponent - blue) < EPSILON)
   {
      h = 60 * (red - green) / (maxComponent - minComponent) + 240;
   }

   // lightness
   l = (maxComponent + minComponent) / 2.0;

   // saturation
   if (l < 0.5)
   {
      s = (maxComponent - minComponent) / (maxComponent + minComponent);
   }
   else
   {
      s = (maxComponent - minComponent) / (2 - maxComponent - minComponent);
   }

   if (mNormalizeHue && h < 0.0)
   {
      h += 360.0;
   }
   pOutput[0] = h;
   pOutput[1] = l;
   pOutput[2] = s;
}
