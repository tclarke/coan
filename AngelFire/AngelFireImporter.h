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

#ifndef ANGELFIREIMPORTER_H
#define ANGELFIREIMPORTER_H

#include "CachedPager.h"
#include "FileResource.h"
#include "RasterElementImporterShell.h"


struct AngelFireFileHeader
{
   unsigned char dst;
   unsigned char numlevels;
   uint32_t buildnum;
   uint32_t timeadjust;
   uint32_t imustatus;
   uint32_t orthodimensions[2];
   uint32_t extents[4];
   uint32_t levelextents[4];
   uint32_t ncols;
   uint32_t nrows;
   uint32_t maxnumtiles;
   double time;
   double imulat;
   double imulon;
   double imualt;
   double imunvel;
   double imueval;
   double imuuvel;
   double imuroll;
   double imupitch;
   double imuyaw;
   float lllat;
   float lllon;
   float urlat;
   float urlon;
   std::auto_ptr<float> points;
   std::auto_ptr<uint32_t> leveltable;
   std::auto_ptr<uint32_t> leveltiletable;
};

class AngelFireImporter : public RasterElementImporterShell
{
public:
   AngelFireImporter();
   virtual ~AngelFireImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   virtual unsigned char getFileAffinity(const std::string &filename);
   virtual bool validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const;
   virtual bool createRasterPager(RasterElement *pRaster) const;

protected:
   bool validateDefaultOnDiskReadOnly(const DataDescriptor *pDescriptor, std::string &errorMessage) const;

private:
   std::vector<std::string> mErrors;
   std::vector<std::string> mWarnings;
};


class AngelFireRasterPager : public CachedPager
{
public:
   AngelFireRasterPager();
   virtual ~AngelFireRasterPager();

private:
   virtual bool openFile(const std::string &filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest *pOriginalRequest);

   LargeFileResource mFile;
   AngelFireFileHeader mHeader;
};

#endif
