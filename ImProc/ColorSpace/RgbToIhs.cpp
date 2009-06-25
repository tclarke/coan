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
#include "RgbToIhs.h"

REGISTER_PLUGIN_BASIC(ColorSpace, RgbToIhs);

#define EPSILON 0.000001

RgbToIhs::RgbToIhs() : mNormalizeHue(true)
{
   setName("RgbToIhs");
   setDescription("Convert between RGB and IHS.");
   setDescriptorId("{56F659E1-3388-4a4c-A1DF-D60225C415C6}");
   setMenuLocation("[General Algorithms]/Colorspace Conversion/RGB->IHS");
   setScaleData(true);
}

RgbToIhs::~RgbToIhs()
{
}

bool RgbToIhs::getInputSpecification(PlugInArgList*& pInArgList)
{
   if (!ColorSpaceConversionShell::getInputSpecification(pInArgList))
   {
      return false;
   }
   VERIFY(pInArgList->addArg<bool>("Normalize Hue", mNormalizeHue, std::string("Should the hue be normalized to (0,360]?")));
   return true;
}

bool RgbToIhs::extractInputArgs(PlugInArgList* pInArgList)
{
   if (!ColorSpaceConversionShell::extractInputArgs(pInArgList))
   {
      return false;
   }
   pInArgList->getPlugInArgValue("Normalize Hue", mNormalizeHue);
   return true;
}

void RgbToIhs::colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent)
{
   /***
    |I |   |     1/3        1/3   1/3|   |R|
    |v1| = |    -1/2       -1/2   1  | * |G|
    |v2|   |sqrt(3)/2 -sqrt(3)/2  0  |   |B|

        0                          if v1=0 and v2=0
    H = atan(v2/v1) + 2*pi         if v1>=0 and v2<0
        atan(v2/v1)                if v1>=0 and v2>=0
        atan(v2/v1) + pi           if v1<0
    
    S=sqrt(v1^2 + v2^2)
   ***/

   double red = pInput[0];
   double green = pInput[1];
   double blue = pInput[2];
   double i = 0.0;
   double h = 0.0;
   double s = 0.0;

   // 0.866025 = sqrt(3)/2
   double v1 = -0.5 * red + -0.5 * green + blue;
   double v2 = 0.866025 * red + -0.866025 * green;

   // intensity
   i = (red + green + blue) / 3.0;

   // hue
   // pi = 3.141593
   double atanComp = atan(v2 / v1);
   if (fabs(v1) < EPSILON && fabs(v2) < EPSILON)
   {
      h = 0;
   }
   else if (v1 >= 0.0 && v2 < 0.0)
   {
      h = atanComp + 6.283186;
   }
   else if (v1 >= 0.0 && v2 >= 0.0)
   {
      h = atanComp;
   }
   else
   {
      h = atanComp + 3.141593;
   }
   h *= 57.295773; // convert to degrees

   // saturation
   s = sqrt(v1 * v1 + v2 * v2);

   if (mNormalizeHue && h < 0.0)
   {
      h += 360.0;
   }
   pOutput[0] = i;
   pOutput[1] = h;
   pOutput[2] = s;
}
