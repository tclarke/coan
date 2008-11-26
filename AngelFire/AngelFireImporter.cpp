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
#include "ImportDescriptor.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "ProgressResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpecialMetadata.h"
#include "StringUtilities.h"
#include "TypeConverter.h"

#include <QtCore/QByteArray>
#include <QtCore/QBuffer>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtGui/QImage>
#include <QtGui/QImageReader>

namespace
{
   const unsigned int tilesizeX = 256;
   const unsigned int tilesizeY = 256;

   bool loadIndexFile(const QString &filename, IndexFile &idx)
   {
      QFile indexFile(filename);
      if(!indexFile.open(QFile::ReadOnly))
      {
         return false;
      }
      idx.indexFile = QFileInfo(filename);
      while(!indexFile.atEnd())
      {
         QString line(indexFile.readLine());
         QStringList parts = line.split("=");
         if(parts.size() == 2)
         {
            if(parts[0] == "basename")
            {
               idx.baseName = parts[1].trimmed();
            }
            else if(parts[0] == "firstframe")
            {
               idx.firstFrameNum = parts[1].toInt();
            }
            else if(parts[0] == "numframes")
            {
               idx.numFrames = parts[1].toInt();
            }
            else if(parts[0] == "maxwidth")
            {
               idx.maxwidth = parts[1].toUInt();
            }
            else if(parts[0] == "maxheight")
            {
               idx.maxheight = parts[1].toUInt();
            }
         }
      }
      return true;
   }

   bool loadFileHeader(QDataStream &fileData, AngelFireFileHeader &header)
   {
      if(fileData.status() != QDataStream::Ok)
      {
         return false;
      }
      unsigned char ver=0, majver=0, minver=0;
      fileData >> ver >> majver >> minver;
      if(ver != 2 && majver != 1 && minver != 0)
      {
         return false;
      }
      fileData >> header.dst;
      fileData >> header.buildnum;
      fileData >> header.time;
      fileData >> header.timeadjust;
      fileData >> header.imustatus;
      fileData >> header.imulat;
      fileData >> header.imulon;
      fileData >> header.imualt;
      fileData >> header.imunvel;
      fileData >> header.imueval;
      fileData >> header.imuuvel;
      fileData >> header.imuroll;
      fileData >> header.imupitch;
      fileData >> header.imuyaw;
      fileData >> header.orthodimensions[0];
      fileData >> header.orthodimensions[1];
      fileData >> header.lllat;
      fileData >> header.urlat;
      fileData >> header.lllon;
      fileData >> header.urlon;
      fileData >> header.extents[0];
      fileData >> header.extents[1];
      fileData >> header.extents[2];
      fileData >> header.extents[3];
      if(fileData.status() != QDataStream::Ok)
      {
         return false;
      }
      uint32_t numpoints = (header.extents[1] - header.extents[0] + 2) * (header.extents[3] - header.extents[2] + 2) * 3;
      header.points.resize(numpoints);
      for(size_t pointIdx = 0; pointIdx < numpoints; pointIdx++)
      {
         fileData >> header.points[pointIdx];
         if(fileData.status() != QDataStream::Ok)
         {
            return false;
         }
      }
      fileData >> header.numlevels;
      header.leveltable.resize(header.numlevels);
      for(size_t levelIdx = 0; levelIdx < header.numlevels; levelIdx++)
      {
         fileData >> header.leveltable[levelIdx];
         if(fileData.status() != QDataStream::Ok)
         {
            return false;
         }
      }
      fileData >> header.levelextents[0] >> header.levelextents[1] >> header.levelextents[2] >> header.levelextents[3];
      header.ncols = header.levelextents[1] - header.levelextents[0] + 1;
      header.nrows = header.levelextents[3] - header.levelextents[2] + 1;
      header.maxnumtiles = header.ncols * header.nrows + 1;
      header.leveltiletable.resize(header.maxnumtiles);
      for(size_t tileTableIdx = 0; tileTableIdx < header.maxnumtiles; tileTableIdx++)
      {
         fileData >> header.leveltiletable[tileTableIdx];
         if(fileData.status() != QDataStream::Ok)
         {
            return false;
         }
      }
      return fileData.status() == QDataStream::Ok;
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

   ImportDescriptorResource pImportDescriptor(filename, TypeConverter::toString<RasterElement>());
   descriptors.push_back(pImportDescriptor.release());

   QFileInfo startFile(QString::fromStdString(filename));
   IndexFile idx;
   if(!loadIndexFile(startFile.absoluteDir().absoluteFilePath("opticks.idx"), idx))
   {
      mErrors.push_back("Index file not present.");
      return descriptors;
   }

   std::string realFileName = idx.indexFile.absoluteDir().absoluteFilePath(QString("%1%2.af")
      .arg(idx.baseName).arg(idx.firstFrameNum)).toStdString();
   pImportDescriptor->setDataDescriptor(RasterUtilities::generateRasterDataDescriptor(
      realFileName, NULL, idx.maxheight * tilesizeY, idx.maxwidth * tilesizeX, idx.numFrames, BSQ, INT1UBYTE, IN_MEMORY));
   RasterDataDescriptor *pDataDesc = static_cast<RasterDataDescriptor*>(pImportDescriptor->getDataDescriptor());
   RasterFileDescriptor *pFileDesc = 
      static_cast<RasterFileDescriptor*>(RasterUtilities::generateAndSetFileDescriptor(pDataDesc, realFileName, std::string(), LITTLE_ENDIAN));
   DynamicObject *pMetadata = pDataDesc->getMetadata();

   return descriptors;
}

unsigned char AngelFireImporter::getFileAffinity(const std::string &filename)
{
   QFile file(QString::fromStdString(filename));
   if(!file.open(QFile::ReadOnly))
   {
      return CAN_NOT_LOAD;
   }
   QDataStream fileData(&file);
   fileData.setByteOrder(QDataStream::LittleEndian);
   AngelFireFileHeader header;
   if(!loadFileHeader(fileData, header))
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
      QFileInfo info(QString::fromStdString(filename.getFullPathAndName()));
      if(!info.isFile() && !info.isReadable())
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

AngelFireRasterPager::AngelFireRasterPager() : //CachedPager(500 * 1024 * 1024) // 500MB page cache
      mFileCache(100 * 1024 * 1024) // 100MB file cache
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
   QFileInfo startFile(QString::fromStdString(filename));
   IndexFile idx;
   if(!loadIndexFile(startFile.absoluteDir().absoluteFilePath("opticks.idx"), mIndex))
   {
      return false;
   }
   return true;
}

CachedPage::UnitPtr AngelFireRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   unsigned int frameNumber = pOriginalRequest->getStartBand().getOnDiskNumber();
   const DynamicObject *pMetadata = pDesc->getMetadata();
   int baseNum = dv_cast<int>(pMetadata->getAttribute("BaseNum"), 0);

   QByteArray *pFileBytes = NULL;
   if(!mFileCache.contains(frameNumber))
   {
      QString frameFileName = mIndex.indexFile.absoluteDir().absoluteFilePath(QString("%1%2.af")
         .arg(mIndex.baseName).arg(mIndex.firstFrameNum + frameNumber));
      QFile file(frameFileName);
      if(!file.open(QFile::ReadOnly))
      {
         return CachedPage::UnitPtr();
      }
      pFileBytes = new QByteArray(file.readAll());
      mFileCache.insert(frameNumber, pFileBytes, pFileBytes->size());
   }
   else
   {
      pFileBytes = mFileCache[frameNumber];
   }
   if(pFileBytes == NULL)
   {
      return CachedPage::UnitPtr();
   }
   QDataStream fileBytes(pFileBytes, QIODevice::ReadOnly);
   fileBytes.setByteOrder(QDataStream::LittleEndian);
   AngelFireFileHeader header;
   if(!loadFileHeader(fileBytes, header))
   {
      return CachedPage::UnitPtr();
   }

   unsigned int rowCount = pDesc->getRowCount();
   unsigned int colCount = pDesc->getColumnCount();
   unsigned int tileCountY = rowCount / tilesizeY;
   unsigned int tileCountX = colCount / tilesizeX;
   size_t fullBufferSize = rowCount * colCount * pDesc->getBytesPerElement();
   std::auto_ptr<char> pFullBuffer(new char[fullBufferSize]);
   VERIFYRV(pFullBuffer.get() != NULL, CachedPage::UnitPtr());
   memset(pFullBuffer.get(), 255, fullBufferSize);

   for(unsigned int curTileX = 0; curTileX < tileCountX; curTileX++)
   {
      for(unsigned int curTileY = 0; curTileY < tileCountY; curTileY++)
      {
         if(curTileX >= header.ncols || curTileY >= header.nrows)
         {
            // this frame doesn't have data for the requested tile so we'll leave it blank
            continue;
         }
         size_t index = curTileY * header.ncols + curTileX;
         size_t tilelength = header.leveltiletable[index + 1] - header.leveltiletable[index];
         if(tilelength == 0)
         {
            continue;
         }
         size_t bytesToSkip = header.leveltable[0] + 16 + ((header.ncols * header.nrows + 1) * 4);
         size_t offsetSkip = header.leveltiletable[index];
         // jpeg decode
         QImage img = QImage::fromData(reinterpret_cast<const uchar*>(pFileBytes->constData())
            + (bytesToSkip + header.leveltiletable[index]), tilelength);
         if(img.isNull() || !img.isGrayscale())
         {
            VERIFYNR_MSG(false, "Image is null");
            return CachedPage::UnitPtr();
         }

         // position pBuffer...curTileX * tileWidth, ResultHeight - (curTileY * tileHeight)
         unsigned int xOffset = curTileX * tilesizeX;
         for(unsigned int curRow = 0; curRow < tilesizeY; curRow++)
         {
            unsigned int targetRow = tilesizeY - curRow;
            unsigned int yOffset = curTileY * tilesizeY + targetRow;
            unsigned int offset = yOffset * (mIndex.maxwidth * tilesizeX) + xOffset;
            memcpy(pFullBuffer.get() + offset, img.scanLine(curRow), img.bytesPerLine());
         }
      }
   }

   return CachedPage::UnitPtr(new CachedPage::CacheUnit(pFullBuffer.release(),
      pDesc->getOnDiskRow(0), rowCount, fullBufferSize, pOriginalRequest->getStartBand()));
}