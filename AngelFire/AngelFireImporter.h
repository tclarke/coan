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

#include <QtCore/QCache>
#include <QtCore/QFileInfo>

struct AngelFireFileHeader
{
   quint8 dst;
   quint8 numlevels;
   quint32 buildnum;
   quint32 timeadjust;
   quint32 imustatus;
   quint32 orthodimensions[2];
   quint32 extents[4];
   quint32 levelextents[4];
   quint32 ncols;
   quint32 nrows;
   quint32 maxnumtiles;
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
   std::vector<float> points;
   std::vector<quint32> leveltable;
   std::vector<quint32> leveltiletable;
};

struct IndexFile
{
   IndexFile() : firstFrameNum(0), numFrames(0), maxwidth(0), maxheight(0) {}
   QFileInfo indexFile;
   QString baseName;
   int firstFrameNum;
   int numFrames;
   unsigned int maxwidth;
   unsigned int maxheight;
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

   QCache<unsigned int, QByteArray> mFileCache;
   IndexFile mIndex;
};

#endif
