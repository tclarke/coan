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

#ifndef DICOMIMPORTER_H
#define DICOMIMPORTER_H

#include "CachedPager.h"
#include "RasterElementImporterShell.h"

class DicomImage;

class DicomImporter : public RasterElementImporterShell
{
public:
   DicomImporter();
   virtual ~DicomImporter();

   std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   unsigned char getFileAffinity(const std::string &filename);
   virtual bool validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const;
   virtual bool validateDefaultOnDiskReadOnly(const DataDescriptor *pDescriptor, std::string &errorMessage) const;
   virtual bool createRasterPager(RasterElement *pRaster) const;

private:
   std::vector<std::string> mErrors;
   std::vector<std::string> mWarnings;
};

class DicomRasterPager : public CachedPager
{
public:
   DicomRasterPager();
   virtual ~DicomRasterPager();

private:
   virtual bool openFile(const std::string &filename);
   virtual CachedPage::UnitPtr fetchUnit(DataRequest *pOriginalRequest);

   std::auto_ptr<DicomImage> mpImage;
   bool mConvertRgb;
};

#endif
