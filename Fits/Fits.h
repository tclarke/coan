/**
* The information in this file is
* Copyright(c) 2007 Ball Aerospace & Technologies Corporation
* and is subject to the terms and conditions of the
* Opticks Limited Open Source License Agreement Version 1.0
* The license text is available from https://www.ballforge.net/
* 
* This material (software and user documentation) is subject to export
* controls imposed by the United States Export Administration Act of 1979,
* as amended and the International Traffic In Arms Regulation (ITAR),
* 22 CFR 120-130
*/

#ifndef FITS_H
#define FITS_H

class DynamicObject;
class LargeFileResource;

namespace Fits
{
   // some sizes and counts that are part of the FITS standard
   const int64_t LOGICAL_RECORD_SIZE = 2880; // in bytes
   struct CardImage
   {
      char mKeyword[8];
      char mValueIndicator[2];
      char mValueAndComment[70];
   };

   bool canParseFile(const std::string& filename);
   DynamicObject *parseHeaderUnit(LargeFileResource &data);
};

#endif
