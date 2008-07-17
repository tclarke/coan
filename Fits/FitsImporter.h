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

#ifndef FITSIMPORTER_H
#define FITSIMPORTER_H

#include "CachedPager.h"
#include "RasterElementImporterShell.h"

#include <fitsio.h>
#define _TCHAR_DEFINED

class FitsFileResource
{
   fitsfile *mpFile;
   int mStatus;

public:
   FitsFileResource();
   explicit FitsFileResource(const std::string &fname);
   ~FitsFileResource();
   bool isValid() const;
   std::string getStatus() const;
   operator fitsfile*();
   void reset(const std::string &fname);
};

class FitsImporter : public RasterElementImporterShell
{
public:
   FitsImporter();
   virtual ~FitsImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   virtual unsigned char getFileAffinity(const std::string &filename);
   virtual bool validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const;
   virtual bool createRasterPager(RasterElement *pRaster) const;

private:
   std::vector<std::string> mErrors;
   std::vector<std::string> mWarnings;
};


class FitsRasterPager : public CachedPager
{
public:
   FitsRasterPager();
   virtual ~FitsRasterPager();

private:
   virtual bool openFile(const std::string &filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest *pOriginalRequest);

   FitsFileResource mpFile;
};

#endif
