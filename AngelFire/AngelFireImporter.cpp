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

#include "AngelFireImporter.h"
#include "AppVerify.h"
#include "DataRequest.h"
#include "DynamicObject.h"
#include "Filename.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpecialMetadata.h"
#include "StringUtilities.h"
#include "TypeConverter.h"

#include <QtCore/QByteArray>
#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtGui/QImageReader>

#define READ(var__) if(file.read(&var__, sizeof(var__)) != sizeof(var__)) return false;
#define READARRAY(var__, cnt__) for(size_t idx = 0; idx < cnt__; idx++) { \
   if(file.read(var__+idx, sizeof(*var__)) != sizeof(*var__)) return false; }

namespace
{
   const unsigned int tilesizeX = 256;
   const unsigned int tilesizeY = 256;

   bool loadFileHeader(LargeFileResource &file, AngelFireFileHeader &header)
   {
      READ(header.dst);
      READ(header.buildnum);
      READ(header.time);
      READ(header.timeadjust);
      READ(header.imustatus);
      READ(header.imulat);
      READ(header.imulon);
      READ(header.imualt);
      READ(header.imunvel);
      READ(header.imueval);
      READ(header.imuuvel);
      READ(header.imuroll);
      READ(header.imupitch);
      READ(header.imuyaw);
      READARRAY(header.orthodimensions, 2);
      READ(header.lllat);
      READ(header.urlat);
      READ(header.lllon);
      READ(header.urlon);
      READARRAY(header.extents, 4);
      uint32_t numpoints = (header.extents[1] - header.extents[0] + 2) * (header.extents[3] - header.extents[2] + 2) * 3;
      header.points.reset(new float[numpoints]);
      READARRAY(header.points.get(), numpoints);
      READ(header.numlevels);
      header.leveltable.reset(new uint32_t[header.numlevels]);
      READARRAY(header.leveltable.get(), header.numlevels);
      READARRAY(header.levelextents, 4);
      header.ncols = header.levelextents[1] - header.levelextents[0] + 1;
      header.nrows = header.levelextents[3] - header.levelextents[2] + 1;
      header.maxnumtiles = header.ncols * header.nrows + 1;
      header.leveltiletable.reset(new uint32_t[header.maxnumtiles]);
      READARRAY(header.leveltiletable.get(), header.maxnumtiles);
      return true;
   }
}

AngelFireImporter::AngelFireImporter()
{
   setDescriptorId("{51160977-f119-4dc7-ae6b-437d5524204a}");
   setName("Angel Fire Data Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright("Copyright 2008, BATC");
   setVersion("0.1");
   setExtensions("Angel Fire (*.af)");
}

AngelFireImporter::~AngelFireImporter()
{
}

std::vector<ImportDescriptor*> AngelFireImporter::getImportDescriptors(const std::string &filename)
{
   mErrors.clear();
   mWarnings.clear();
   std::vector<ImportDescriptor*> descriptors;

   LargeFileResource file;
   if(!file.open(filename.c_str(), O_RDONLY|O_BINARY, S_IREAD))
   {
      mErrors.push_back("Can't open file.");
      return descriptors;
   }

   unsigned char ver=0, majver=0, minver=0;
   file.read(&ver, 1);
   file.read(&majver, 1);
   file.read(&minver, 1);
   if(ver != 2 && majver != 1 && minver != 0)
   {
      mErrors.push_back("Not an angel fire file or version not supported.");
      return descriptors;
   }

   ImportDescriptorResource pImportDescriptor(filename, TypeConverter::toString<RasterElement>());
   descriptors.push_back(pImportDescriptor.release());

   AngelFireFileHeader header;
   if(!loadFileHeader(file, header))
   {
      mErrors.push_back("Unable to read header information.");
      return descriptors;
   }

   pImportDescriptor->setDataDescriptor(RasterUtilities::generateRasterDataDescriptor(
      filename, NULL, header.nrows*tilesizeY, header.ncols*tilesizeX, 1, BSQ, INT1UBYTE, ON_DISK_READ_ONLY));
   RasterFileDescriptor *pFileDesc = 
      dynamic_cast<RasterFileDescriptor*>(RasterUtilities::generateAndSetFileDescriptor(
      pImportDescriptor->getDataDescriptor(), filename, "", LITTLE_ENDIAN));

   return descriptors;
}

unsigned char AngelFireImporter::getFileAffinity(const std::string &filename)
{
   LargeFileResource file;
   if(!file.open(filename.c_str(), O_RDONLY|O_BINARY, S_IREAD))
   {
      return CAN_NOT_LOAD;
   }

   unsigned char ver=0, majver=0, minver=0;
   file.read(&ver, 1);
   file.read(&majver, 1);
   file.read(&minver, 1);
   if(ver != 2 && majver != 1 && minver != 0)
   {
      return CAN_NOT_LOAD;
   }
   return CAN_LOAD;
}


bool AngelFireImporter::validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const
{
   errorMessage = "";
   if(!mErrors.empty())
   {
      errorMessage = StringUtilities::join(mErrors, "\n");
      return false;
   }
   std::string baseErrorMessage;
   bool valid = RasterElementImporterShell::validate(pDescriptor, baseErrorMessage);
   if(!mWarnings.empty())
   {
      if(!baseErrorMessage.empty())
      {
         errorMessage += baseErrorMessage + "\n";
      }
      errorMessage += StringUtilities::join(mWarnings, "\n");
   }
   else
   {
      errorMessage = baseErrorMessage;
   }
   return valid;
}

bool AngelFireImporter::validateDefaultOnDiskReadOnly(const DataDescriptor *pDescriptor, std::string &errorMessage) const
{
   const RasterDataDescriptor *pRasterDescriptor = dynamic_cast<const RasterDataDescriptor*>(pDescriptor);
   if(pRasterDescriptor == NULL)
   {
      errorMessage = "The data descriptor is invalid!";
      return false;
   }

   const RasterFileDescriptor *pFileDescriptor =
      dynamic_cast<const RasterFileDescriptor*>(pRasterDescriptor->getFileDescriptor());
   if(pFileDescriptor == NULL)
   {
      errorMessage = "The file descriptor is invalid!";
      return false;
   }

   ProcessingLocation loc = pDescriptor->getProcessingLocation();
   if(loc == ON_DISK_READ_ONLY)
   {
      // Invalid filename
      const Filename &filename = pFileDescriptor->getFilename();
      if(filename.getFullPathAndName().empty())
      {
         errorMessage = "The filename is invalid!";
         return false;
      }

      // Existing file
      LargeFileResource file;
      if(!file.open(filename.getFullPathAndName(), O_RDONLY | O_BINARY, S_IREAD))
      {
         errorMessage = "The file: " + std::string(filename) + " does not exist!";
         return false;
      }

      // Interleave conversions
      InterleaveFormatType dataInterleave = pRasterDescriptor->getInterleaveFormat();
      InterleaveFormatType fileInterleave = pFileDescriptor->getInterleaveFormat();
      if(dataInterleave != fileInterleave)
      {
         errorMessage = "Interleave format conversions are not supported with on-disk read-only processing!";
         return false;
      }

      // Subset
      unsigned int loadedBands = pRasterDescriptor->getBandCount();
      unsigned int fileBands = pFileDescriptor->getBandCount();

      if(loadedBands != fileBands)
      {
         errorMessage = "Band subsets are not supported with on-disk read-only processing!";
         return false;
      }

      unsigned int skipFactor = 0;
      if(!RasterUtilities::determineSkipFactor(pRasterDescriptor->getRows(), skipFactor) || skipFactor != 0)
      {
         errorMessage = "Skip factors are not supported for rows or columns with on-disk read-only processing.";
         return false;
      }
      if(!RasterUtilities::determineSkipFactor(pRasterDescriptor->getColumns(), skipFactor) || skipFactor != 0)
      {
         errorMessage = "Skip factors are not supported for rows or columns with on-disk read-only processing.";
         return false;
      }
   }

   return true;
}

bool AngelFireImporter::createRasterPager(RasterElement *pRaster) const
{
   VERIFY(pRaster != NULL);
   DataDescriptor *pDescriptor = pRaster->getDataDescriptor();
   VERIFY(pDescriptor != NULL);
   FileDescriptor *pFileDescriptor = pDescriptor->getFileDescriptor();
   VERIFY(pFileDescriptor != NULL);

   std::string filename = pRaster->getFilename();
   Progress *pProgress = getProgress();

   FactoryResource<Filename> pFilename;
   pFilename->setFullPathAndName(filename);

   ExecutableResource pagerPlugIn("AngelFireRasterPager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager *pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if(!success || pPager == NULL)
   {
      std::string message = "Execution of AngelFireRasterPager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

AngelFireRasterPager::AngelFireRasterPager()
{
   setName("AngelFireRasterPager");
   setCopyright("Copyright 2008 BATC");
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk Angel Fire data");
   setDescriptorId("{551f1c8d-b6ac-4bdf-ada4-c0470461295f}");
   setVersion("0.1");
   setProductionStatus(false);
   setShortDescription("Angel Fire pager");
}

AngelFireRasterPager::~AngelFireRasterPager()
{
}

bool AngelFireRasterPager::openFile(const std::string &filename)
{
  return mFile.open(filename.c_str(), O_RDONLY|O_BINARY, S_IREAD) && mFile.seek(3, SEEK_SET) && loadFileHeader(mFile, mHeader);
}

CachedPage::UnitPtr AngelFireRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   unsigned int startRow = pOriginalRequest->getStartRow().getOnDiskNumber();
   unsigned int startCol = pOriginalRequest->getStartColumn().getOnDiskNumber();
   unsigned int endRow = startRow + pOriginalRequest->getConcurrentRows();
   unsigned int endCol = startCol + pOriginalRequest->getConcurrentColumns();
   unsigned int startTileY = startRow / tilesizeY;
   unsigned int startTileX = startCol / tilesizeX;
   unsigned int endTileY = endRow / tilesizeY;
   unsigned int endTileX = endCol / tilesizeX;
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   size_t fullBufferSize = ((endTileY - startTileY + 1) * tilesizeY) * ((endTileX - startTileX + 1) * tilesizeX) * pDesc->getBytesPerElement();
   std::auto_ptr<char> pFullBuffer(new char[fullBufferSize]);
   VERIFYRV(pFullBuffer.get() != NULL, CachedPage::UnitPtr());
   memset(pFullBuffer.get(), 0, fullBufferSize);

   for(unsigned int curTileX = startTileX; curTileX <= endTileX; curTileX++)
   {
      for(unsigned int curTileY = startTileY; curTileY <= endTileY; curTileY++)
      {
         size_t index = curTileY * mHeader.ncols + curTileY;
         size_t tilelength = (mHeader.leveltiletable.get())[index + 1] - (mHeader.leveltiletable.get())[index];
         if(tilelength == 0)
         {
            continue;
         }
         size_t bytesToSkip = (mHeader.leveltable.get())[0] + 16 + ((mHeader.ncols * mHeader.nrows + 1) * 4);
         QByteArray buffer(tilelength, 0);
         if(mFile.seek(bytesToSkip + (mHeader.leveltiletable.get())[index], SEEK_SET) == -1)
         {
            return CachedPage::UnitPtr();
         }
         if(mFile.read(buffer.data(), buffer.size()) != buffer.size())
         {
            return CachedPage::UnitPtr();
         }
         // jpeg decode
         QImage img = QImage::fromData(buffer);
         if(img.isNull() || !img.isGrayscale())
         {
            return CachedPage::UnitPtr();
         }

         // position pBuffer...curTileX * tileWidth, ResultHeight - (curTileY * tileHeight)
         unsigned int xOffset = (curTileX - startTileX) * tilesizeX;
         for(unsigned int curRow = 0; curRow < tilesizeY; curRow++)
         {
            unsigned int yOffset = (curTileY - startTileY) * tilesizeY + curRow;
            unsigned int offset = yOffset * (mHeader.ncols * tilesizeX) + xOffset;
            memcpy(pFullBuffer.get() + offset, img.scanLine(curRow), img.bytesPerLine());
         }
      }
   }

   return CachedPage::UnitPtr(new CachedPage::CacheUnit(pFullBuffer.release(),
      pDesc->getOnDiskRow(startRow), endRow - startRow + 1, fullBufferSize));
}

/*
readTile(header, row, col)
{
   size_t index = row *header.ncols + col;
   size_t tilelength = header.leveltiletable[index+1] - header.leveltiletable[index];
   bytestoskip = header.leveltable[0] + 16 + ((header.ncols * header.nrows + 1) * 4);
   file.seek(bytestoskip + header.leveltioletable[index];
   file.read(pBuffer, tilelength);
   jpegdecode(pBuffer);
}
*/