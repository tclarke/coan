/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "IhsToRgb.h"
#include "ImProcVersion.h"
#include "PlugInArgList.h"
#include "PlugInFactory.h"

PLUGINFACTORY(IhsToRgb);

#define EPSILON 0.000001

IhsToRgb::IhsToRgb()
{
   setName("IhsToRgb");
   setDescription("Convert between IHS and RGB.");
   setDescriptorId("{1B2FBB11-B46C-4912-8A96-8B0160C1D259}");
   setMenuLocation("[General Algorithms]/Colorspace Conversion/IHS->RGB");
   setScaleData(false);
}

IhsToRgb::~IhsToRgb()
{
}

void IhsToRgb::colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent)
{
   /***
    v1 = S * cos(H)
    v2 = S * sin(H)

    |R|   |1 -1/3  1/sqrt(3)|   |I |
    |G| = |1 -1/3 -1/sqrt(3)| * |v1|
    |B|   |1  2/3  0        |   |v2|
   ***/
   double i = pInput[0];
   double h = pInput[1];
   double s = pInput[2];
   double r = 0.0;
   double g = 0.0;
   double b = 0.0;

   h *= 0.017453; // convert to radians

   double v1 = s * cos(h);
   double v2 = s * sin(h);

   // 0.577350 = 1/sqrt(3)
   r = i + (-0.333333 * v1) + (0.577350 * v2);
   g = i + (-0.333333 * v1) + (-0.577350 * v2);
   b = i + (0.666667 * v1);

   pOutput[0] = r;
   pOutput[1] = g;
   pOutput[2] = b;
}
