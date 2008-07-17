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

#include "ComplexData.h"
#include "DataRequest.h"
#include "DynamicObject.h"
#include "Filename.h"
#include "FitsImporter.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "StringUtilities.h"
#include "TypeConverter.h"

#include <ast.h>
#include <fitsio.h>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

namespace
{
   int encodingToDtype(EncodingType encoding)
   {
      switch(encoding)
      {
      case INT1SBYTE:
         return TSBYTE;
      case INT1UBYTE:
         return TBYTE;
      case INT2SBYTES:
         return TSHORT;
      case INT2UBYTES:
         return TUSHORT;
      case INT4SBYTES:
         return TINT;
      case INT4UBYTES:
         return TUINT;
      case FLT4BYTES:
         return TFLOAT;
      case FLT8BYTES:
         return TDOUBLE;
      default:
         return TBYTE;
      }
   }
}

FitsFileResource::FitsFileResource() : mpFile(NULL), mStatus(0)
{
}

FitsFileResource::FitsFileResource(const std::string &fname) : mStatus(0)
{
   if(fits_open_file(&mpFile, fname.c_str(), READONLY, &mStatus))
   {
      mpFile = NULL;
   }
}

FitsFileResource::~FitsFileResource()
{
   if(mpFile != NULL)
   {
      fits_close_file(mpFile, &mStatus);
      mpFile = NULL;
   }
}

bool FitsFileResource::isValid() const
{
   return mpFile != NULL;
}

std::string FitsFileResource::getStatus() const
{
   char pBuf[31];
   fits_get_errstatus(mStatus, pBuf);
   return std::string(pBuf);
}

FitsFileResource::operator fitsfile*()
{
   return mpFile;
}

void FitsFileResource::reset(const std::string &fname)
{
   if(mpFile != NULL)
   {
      fits_close_file(mpFile, &mStatus);
      mpFile = NULL;
   }
   if(fits_open_file(&mpFile, fname.c_str(), READONLY, &mStatus))
   {
      mpFile = NULL;
   }
}

FitsImporter::FitsImporter()// : mImportOptionsWidget(NULL)
{
   setDescriptorId("{299E1A6D-F80B-45D2-910D-0E318303CF88}");
   setName("FITS Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright("Copyright 2007, BATC");
   setVersion("0.2");
   setExtensions("FITS Files (*.fit *.fts *.fiz *.fits)");
}

FitsImporter::~FitsImporter()
{
}

#define CHECK_FITS(call__, warn__, onerr__) if(call__) \
   { \
      char pBuf[31]; \
      fits_get_errstatus(status, pBuf); \
      if(warn__) \
      { \
         mWarnings.push_back(std::string(pBuf)); \
         status = 0; \
         onerr__; \
      } \
      else \
      { \
         mErrors.push_back(std::string(pBuf)); \
         onerr__; \
      } \
   }

std::vector<ImportDescriptor*> FitsImporter::getImportDescriptors(const std::string &filename)
{
   mErrors.clear();
   mWarnings.clear();
   int status=0;
   std::vector<ImportDescriptor*> descriptors;

   FitsFileResource pFile(filename);
   if(!pFile.isValid())
   {
      mErrors.push_back(pFile.getStatus());
      return descriptors;
   }

   int hduCnt = 0;
   CHECK_FITS(fits_get_num_hdus(pFile, &hduCnt, &status), false, return descriptors);
   for(int hdu = 1; hdu <= hduCnt; hdu++)
   {
      std::list<GcpPoint> gcps;
      std::string datasetName = filename + "[" + StringUtilities::toDisplayString(hdu) + "]";
      int hduType;
      CHECK_FITS(fits_movabs_hdu(pFile, hdu, &hduType, &status), false, continue);
      ImportDescriptorResource pImportDescriptor(static_cast<ImportDescriptor*>(NULL));
      FactoryResource<DynamicObject> pMetadata;
      VERIFYRV(pMetadata.get() != NULL, descriptors);
      { // scope
         char pCard[81], pValue[81];
         int nkeys = 0;
         CHECK_FITS(fits_get_hdrspace(pFile, &nkeys, NULL, &status), true, ;);
         for(int keyidx = 1; keyidx <= nkeys; keyidx++)
         {
            CHECK_FITS(fits_read_record(pFile, keyidx, pCard, &status), true, continue);
            std::string name = StringUtilities::toDisplayString(keyidx);
            std::vector<std::string> splitval = StringUtilities::split(std::string(pCard), '=');
            if(splitval.size() > 1)
            {
               name = StringUtilities::stripWhitespace(splitval.front());
            }
            CHECK_FITS(fits_parse_value(pCard, pValue, NULL, &status), true, pValue[0]=0);
            std::string val = StringUtilities::stripWhitespace(std::string(pValue));
            if(!val.empty())
            {
               pMetadata->setAttributeByPath("FITS/" + name, val);
            }
         }
      }
      switch(hduType)
      {
      case IMAGE_HDU:
         {
            pImportDescriptor = ImportDescriptorResource(datasetName, TypeConverter::toString<RasterElement>());
            VERIFYRV(pImportDescriptor.get() != NULL, descriptors);

            EncodingType encoding;
            InterleaveFormatType interleave(BSQ);
            unsigned int rows=0, cols=0, bands=1;

            int bitpix, naxis;
            long axes[3];
            CHECK_FITS(fits_get_img_param(pFile, 3, &bitpix, &naxis, axes, &status), false, continue);
            switch(bitpix)
            {
            case BYTE_IMG:
               encoding = INT1UBYTE;
               break;
            case SHORT_IMG:
               encoding = INT2SBYTES;
               break;
            case LONG_IMG:
               encoding = INT4SBYTES;
               break;
            case FLOAT_IMG:
               encoding = FLT4BYTES;
               break;
            case DOUBLE_IMG:
               encoding = FLT8BYTES;
               break;
            default:
               mWarnings.push_back("Unsupported BITPIX value " + StringUtilities::toDisplayString(bitpix) + ".");
               continue;
            }
            encoding = checkForOverflow(encoding, pMetadata.get());
            if(naxis == 2)
            {
               cols = axes[0];
               rows = axes[1];
            }
            else if(naxis == 3)
            {
               cols = axes[0];
               rows = axes[1];
               bands = axes[2];
               interleave = BSQ;
            }
            else
            {
               mWarnings.push_back(StringUtilities::toDisplayString(naxis) + " axis data not supported.");
               continue;
            }

            RasterDataDescriptor *pDataDesc = RasterUtilities::generateRasterDataDescriptor(
               datasetName, NULL, rows, cols, bands, interleave, encoding, IN_MEMORY);
            // parse any WCS headers
            {
               astBegin;
               char *pHeader = NULL;
               int numkeys = 0;
               fits_hdr2str(pFile, 0, NULL, 0, &pHeader, &numkeys, &status);
               AstFitsChan *pFitschan = astFitsChan(NULL, NULL, "");
               astPutCards(pFitschan, pHeader);
               free(pHeader);
               pHeader = NULL;
               AstFrameSet *pWcsinfo = reinterpret_cast<AstFrameSet*>(astRead(pFitschan));
               if(pWcsinfo != NULL)
               {
                  double pixcol[5], pixrow[5], lons[5], lats[5];
                  pixcol[0] = 0;      pixrow[0] = 0;
                  pixcol[1] = cols-1; pixrow[1] = 0;
                  pixcol[2] = cols-1; pixrow[2] = rows-1;
                  pixcol[3] = 0;      pixrow[3] = rows-2;
                  pixcol[4] = cols/2; pixrow[4] = rows/2;
                  astTran2(pWcsinfo, 5, pixcol, pixrow, 1, lons, lats);
                  for(int pnt = 0; pnt < 5; pnt++)
                  {
                     GcpPoint gcp;
                     gcp.mPixel = LocationType(pixcol[pnt], pixrow[pnt]);
                     gcp.mCoordinate = LocationType(lons[pnt], lats[pnt]);
                     gcps.push_back(gcp);
                  }
               }
               astEnd;
            }
            pImportDescriptor->setDataDescriptor(pDataDesc);

            break;
         }
      case ASCII_TBL:
      case BINARY_TBL:
         mWarnings.push_back("Tables not supported. [HDU " + StringUtilities::toDisplayString(hdu) + "]");
         continue;
      default:
         mWarnings.push_back("HDU " + StringUtilities::toDisplayString(hdu) + " is an unknown type.");
         continue;
      }
      RasterFileDescriptor *pFileDesc = 
         dynamic_cast<RasterFileDescriptor*>(RasterUtilities::generateAndSetFileDescriptor(pImportDescriptor->getDataDescriptor(),
         filename, StringUtilities::toDisplayString(hdu), BIG_ENDIAN));
      if(pFileDesc != NULL && !gcps.empty())
      {
         pFileDesc->setGcps(gcps);
      }

      pImportDescriptor->getDataDescriptor()->setMetadata(pMetadata.release());
      descriptors.push_back(pImportDescriptor.release());
   }
   return descriptors;
}

EncodingType FitsImporter::checkForOverflow(EncodingType encoding, DynamicObject *pMetadata)
{
   double bzero = 0.0, bscale = 1.0, datamax = 0.0;
   try
   {
      bzero = StringUtilities::fromDisplayString<double>(dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/BZERO")));
   }
   catch(const std::bad_cast&) {} // empty
   try
   {
      bscale = StringUtilities::fromDisplayString<double>(dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/BSCALE")));
   }
   catch(const std::bad_cast&) {} // empty
   try
   {
      datamax = StringUtilities::fromDisplayString<double>(dv_cast<std::string>(pMetadata->getAttributeByPath("FITS/DATAMAX")));
   }
   catch(const std::bad_cast&) {} // empty

   double intpart;
   if(modf(bscale, &intpart) != 0.0)
   {
      if(encoding == INT4SCOMPLEX)
      {
         mWarnings.push_back("Data scaling will be applied, converting to float complex.");
         return FLT8COMPLEX;
      }
      else if(encoding == FLT8COMPLEX)
      {
         return encoding;
      }
      else if(encoding != FLT8BYTES)
      {
         mWarnings.push_back("Data scaling will be applied, converting to 8-byte float.");
         return FLT8BYTES;
      }
   }
   if(bzero != 0.0 || bscale != 1.0)
   {
      if(datamax == 0.0)
      {
         mWarnings.push_back(
            "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         return encoding;
      }
      double newmax = fabs(datamax * bscale + bzero);
      switch(encoding)
      {
      case INT1SBYTE:
         if(newmax <= std::numeric_limits<char>::max())
         {
            return encoding;
         }
         encoding = INT2SBYTES; // fallthru
      case INT2SBYTES:
         if(newmax <= std::numeric_limits<short>::max())
         {
            return encoding;
         }
         encoding = INT4SBYTES; // fallthru
      case INT4SBYTES:
         if(newmax > std::numeric_limits<int>::max())
         {
            mWarnings.push_back(
               "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         }
         return encoding;
      case INT1UBYTE:
         if(newmax <= std::numeric_limits<unsigned char>::max())
         {
            return encoding;
         }
         encoding = INT2UBYTES; // fallthru
      case INT2UBYTES:
         if(newmax <= std::numeric_limits<unsigned short>::max())
         {
            return encoding;
         }
         encoding = INT4UBYTES; // fallthru
      case INT4UBYTES:
         if(newmax > std::numeric_limits<unsigned int>::max())
         {
            mWarnings.push_back(
               "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         }
         return encoding;
      case FLT4BYTES:
         if(newmax <= std::numeric_limits<float>::max())
         {
            return encoding;
         }
         encoding = FLT8BYTES;
      case FLT8BYTES:
         if(newmax == std::numeric_limits<double>::infinity())
         {
            mWarnings.push_back(
               "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         }
         return encoding;
      case INT4SCOMPLEX:
      case FLT8COMPLEX:
         mWarnings.push_back(
            "Data scaling will be applied, you may need to change the Data Type in the import Data tab or overflow may occur.");
         return encoding;
      }
   }
   return encoding;
}

unsigned char FitsImporter::getFileAffinity(const std::string &filename)
{
   FitsFileResource pFile(filename);
   return pFile.isValid() ? CAN_LOAD : CAN_NOT_LOAD;
}


bool FitsImporter::validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const
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

bool FitsImporter::createRasterPager(RasterElement *pRaster) const
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

   ExecutableResource pagerPlugIn("FitsRasterPager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager *pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if(!success || pPager == NULL)
   {
      std::string message = "Execution of FitsRasterPager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

FitsRasterPager::FitsRasterPager()
{
   setName("FitsRasterPager");
   setCopyright("Copyright 2008 BATC");
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk FITS data");
   setDescriptorId("{9CFA503D-4570-4612-9996-5FAE649791B8}");
   setVersion("0.2");
   setProductionStatus(false);
   setShortDescription("FITS pager");
}

FitsRasterPager::~FitsRasterPager()
{
}

bool FitsRasterPager::openFile(const std::string &filename)
{
   mpFile.reset(filename);
   if(!mpFile.isValid())
   {
      return false;
   }
   int hdu = StringUtilities::fromDisplayString<int>(
      getRasterElement()->getDataDescriptor()->getFileDescriptor()->getDatasetLocation());
   if(hdu < 1)
   {
      return false;
   }
   int status = 0;
   if(fits_movabs_hdu(mpFile, hdu, NULL, &status))
   {
      return false;
   }
   return true;
}

CachedPage::UnitPtr FitsRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   long maxInRow = std::min(pOriginalRequest->getConcurrentRows(), pDesc->getRowCount() - pOriginalRequest->getStartRow().getOnDiskNumber());
   long pixcnt = maxInRow * pDesc->getColumnCount();
   long pStartPix[3] = {1,
                        pOriginalRequest->getStartRow().getOnDiskNumber() + 1,
                        pOriginalRequest->getStartBand().getOnDiskNumber() + 1};
   int status = 0;

   size_t bufsize = pixcnt * pDesc->getBytesPerElement();
   std::auto_ptr<char> pBuffer(new char[bufsize]);
   if(fits_read_pix(mpFile, encodingToDtype(pDesc->getDataType()), pStartPix, pixcnt, NULL, pBuffer.get(), NULL, &status))
   {
      if(status != NUM_OVERFLOW)
      {
         char pBuf[31];
         fits_get_errstatus(status, pBuf);
         VERIFYRV_MSG(false, CachedPage::UnitPtr(), pBuf);
      }
   }
   return CachedPage::UnitPtr(new CachedPage::CacheUnit(
      pBuffer.release(), pOriginalRequest->getStartRow(), pOriginalRequest->getConcurrentRows(), bufsize));
}
