/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef RGBTOHSV_H__
#define RGBTOHSV_H__

#include "ColorSpaceConversionShell.h"

class RgbToHsv : public ColorSpaceConversionShell
{
public:
   RgbToHsv();
   virtual ~RgbToHsv();

   virtual bool getInputSpecification(PlugInArgList*& pInArgList);
   virtual void colorspaceConvert(double pOutput[3], double pInput[3], double maxComponent, double minComponent);

protected:
   virtual bool extractInputArgs(PlugInArgList* pInArgList);

private:
   bool mNormalizeHue;
};

#endif
