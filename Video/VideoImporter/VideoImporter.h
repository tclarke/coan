/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef VIDEOIMPORTER_H__
#define VIDEOIMPORTER_H__

#include "AppAssert.h"
#include "RasterElementImporterShell.h"
#include "RasterPage.h"
#include "RasterPagerShell.h"
#pragma warning(push, 1)
#pragma warning(disable: 4244)
extern "C" {
#include <avformat.h>
#include <avutil.h>
};
#pragma warning(pop)
#include <QtCore/QCache>
#include <QtCore/QMap>
#include <QtCore/QMutex>

class RasterDataDescriptor;

class FfmpegRasterPage : public RasterPage
{
public:
   /**
    * This class holds onto a reference counted block of data
    */
   class Unit
   {
   public:
      uint8_t *mpData;
      int64_t mSize;
      int mRefCount;

      Unit() : mpData(NULL), mSize(0), mRefCount(0) {}
      Unit(uint8_t *pData, int64_t size) : mpData(pData), mSize(size), mRefCount(0) {}
      ~Unit() { delete mpData; }

      void inc() { mRefCount++; }
      int dec() { return --mRefCount; }

   private:
      Unit(const Unit &other) : mpData(NULL), mSize(0), mRefCount(0) {}
   };

   FfmpegRasterPage(unsigned int key, Unit &data, int64_t offset, unsigned int rows, unsigned int columns, unsigned int bands);
   virtual ~FfmpegRasterPage();

   virtual void *getRawData();
   virtual unsigned int getNumRows();
   virtual unsigned int getNumColumns();
   virtual unsigned int getNumBands();
   virtual unsigned int getInterlineBytes();

   /**
    * The size of the page in bytes.
    *
    * @return The page size
    */
   int64_t size() const;

private:
   friend class FfmpegRasterPager;
   Unit &mData;
   unsigned int mKey;
   int64_t mOffset;
   unsigned int mRows;
   unsigned int mColumns;
   unsigned int mBands;
};

class FfmpegRasterPager : public RasterPagerShell
{
public:
   static std::string PagedElementArg() { return "Paged Element"; }
   static std::string PagedFilenameArg() { return "Paged Filename"; }

   FfmpegRasterPager();
   virtual ~FfmpegRasterPager();

   virtual bool getInputSpecification(PlugInArgList *&pArgList);
   virtual bool execute(PlugInArgList *pInputArgList, PlugInArgList *pOutArgList);

   virtual RasterPage *getPage(DataRequest *pOriginalRequest,
                               DimensionDescriptor startRow,
                               DimensionDescriptor startColumn,
                               DimensionDescriptor startBand);
   virtual void releasePage(RasterPage *pPage);
   virtual int getSupportedRequestVersion() const;

private:
   int64_t calculateSeekPosition(unsigned int frame) const;
   int64_t calculateSeekPosition(double timeInSeconds) const;
   bool getNextFrame();

   RasterElement *mpRaster;
   RasterDataDescriptor *mpDescriptor;

   QMap<unsigned int, FfmpegRasterPage::Unit*> mActivePages;
   QCache<unsigned int, FfmpegRasterPage::Unit> mInactivePages;

   int mPrevFrameNum;
   AVFrame mNextFrame;
   AVPacket mPacket;
   int mBytesRemaining;
   uint8_t *mpRawPacketData;

   AVFormatContext *mpFormatCtx;
   AVCodecContext *mpCodecCtx;
   unsigned int mStreamId;

   QMutex mLock;
};

class VideoImporter : public RasterElementImporterShell
{
public:
   VideoImporter();
   virtual ~VideoImporter();

   virtual std::vector<ImportDescriptor*> getImportDescriptors(const std::string &filename);
   virtual unsigned char getFileAffinity(const std::string &filename);

   virtual bool execute(PlugInArgList*pInArgList, PlugInArgList *pOutArgList);

protected:
   virtual bool validateDefaultOnDiskReadOnly(const DataDescriptor *pDescriptor, std::string &errorMessage) const;
   virtual bool createRasterPager(RasterElement *pRaster) const;

private:
   mutable AVFormatContext *mpFormatCtx;
   mutable AVCodecContext *mpCodecCtx;
};

#endif