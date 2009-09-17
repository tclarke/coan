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

class HdfContext
{
public:
   HdfContext(const std::string& filename) : mHid(FAIL), mGrid(FAIL), mRiid(FAIL)
   {
      open(filename);
   }

   HdfContext() : mHid(FAIL), mGrid(FAIL), mRiid(FAIL)
   {
   }

   ~HdfContext()
   {
      if (mRiid != FAIL)
      {
         GRendaccess(mRiid);
      }
      if (mGrid != FAIL)
      {
         GRend(mGrid);
      }
      if (mHid != FAIL)
      {
         Hclose(mHid);
      }
   }

   int32 hid() const { return mHid; }
   int32 grid() const { return mGrid; }
   int32 riid() const { return mRiid; }

   void open(const std::string& filename)
   {
      if (mRiid != FAIL)
      {
         GRendaccess(mRiid);
      }
      if (mGrid != FAIL)
      {
         GRend(mGrid);
      }
      if (mHid != FAIL)
      {
         Hclose(mHid);
      }
      mHid = Hopen(filename.c_str(), DFACC_READ, 0);
      mGrid = GRstart(mHid);
   }

   int32 toFrame(int32 frame)
   {
      if (mRiid != FAIL)
      {
         GRendaccess(mRiid);
      }
      mRiid = GRselect(mGrid, frame);
      return mRiid;
   }

private:
   int32 mHid;
   int32 mGrid;
   int32 mRiid;
};

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

   HdfContext mHdf;
};

#endif
