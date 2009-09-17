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

#ifndef DIHDFIMPORTER_H
#define DIHDFIMPORTER_H

#include "CachedPager.h"
#include "RasterElementImporterShell.h"
#include <hdf.h>

class DiHdfImporter : public RasterElementImporterShell
{
public:
   DiHdfImporter();
   virtual ~DiHdfImporter();

   std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   unsigned char getFileAffinity(const std::string &filename);
   virtual bool validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const;
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
   virtual bool createRasterPager(RasterElement *pRaster) const;

private:
   std::vector<std::string> mErrors;
   std::vector<std::string> mWarnings;
};

class DiHdfRasterPager : public CachedPager
{
public:
   DiHdfRasterPager();
   virtual ~DiHdfRasterPager();

private:
   virtual bool openFile(const std::string &filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest *pOriginalRequest);

   int32 mHid;
   int32 mGrid;
};

#endif
