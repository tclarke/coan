/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef HLSTORGB_H__
#define HLSTORGB_H__

#include "ColorSpaceConversionShell.h"

class HlsToRgb : public ColorSpaceConversionShell
{
public:
   HlsToRgb();
   virtual ~HlsToRgb();

   virtual void colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent);
};

#endif
