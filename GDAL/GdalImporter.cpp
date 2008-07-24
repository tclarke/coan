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
#include "GdalImporter.h"
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
#include "switchOnEncoding.h"
#include "TypeConverter.h"

#include <gdal_priv.h>

namespace
{
   EncodingType gdalDataTypeToEncodingType(GDALDataType type)
   {
      switch(type)
      {
      case GDT_Byte:
         return INT1UBYTE;
      case GDT_UInt16:
         return INT2UBYTES;
      case GDT_Int16:
         return INT2SBYTES;
      case GDT_UInt32:
         return INT4UBYTES;
      case GDT_Int32:
         return INT4SBYTES;
      case GDT_Float32:
         return FLT4BYTES;
      case GDT_Float64:
         return FLT8BYTES;
      case GDT_CInt16:
         return INT4SCOMPLEX;
      case GDT_CInt32:
      case GDT_CFloat32:
      case GDT_CFloat64:
         return FLT8COMPLEX;
      default:
         break;
      }
      return EncodingType();
   }

   GDALDataType encodingTypeToGdalDataType(EncodingType type)
   {
      switch(type)
      {
      case INT1UBYTE:
         return GDT_Byte;
      case INT2UBYTES:
         return GDT_UInt16;
      case INT2SBYTES:
         return GDT_Int16;
      case INT4UBYTES:
         return GDT_UInt32;
      case INT4SBYTES:
         return GDT_Int32;
      case FLT4BYTES:
         return GDT_Float32;
      case FLT8BYTES:
         return GDT_Float64;
      case INT4SCOMPLEX:
         return GDT_CInt16;
      case FLT8COMPLEX:
         return GDT_CFloat32;
      }
      return GDT_Byte;
   }

   size_t gdalDataTypeSize(GDALDataType type)
   {
      switch(type)
      {
      case GDT_Byte:
         return 1;
      case GDT_UInt16:
      case GDT_Int16:
         return 2;
      case GDT_UInt32:
      case GDT_Int32:
      case GDT_Float32:
      case GDT_CInt16:
         return 4;
      case GDT_Float64:
      case GDT_CInt32:
      case GDT_CFloat32:
         return 8;
      case GDT_CFloat64:
         return 16;
      }
      return 0;
   }

   template<typename T>
   void convertDataTypes(T *pBuffer, void *pRawBuffer, size_t offset, GDALDataType rawType)
   {
      switch(rawType)
      {
      case GDT_Byte:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<unsigned char*>(pRawBuffer)[offset]);
         break;
      case GDT_UInt16:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<unsigned short*>(pRawBuffer)[offset]);
         break;
      case GDT_Int16:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<short*>(pRawBuffer)[offset]);
         break;
      case GDT_UInt32:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<unsigned int*>(pRawBuffer)[offset]);
         break;
      case GDT_Int32:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<int*>(pRawBuffer)[offset]);
         break;
      case GDT_Float32:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<float*>(pRawBuffer)[offset]);
         break;
      case GDT_Float64:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<double*>(pRawBuffer)[offset]);
         break;
      case GDT_CInt16:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<short*>(pRawBuffer)[offset*2]);
         break;
      case GDT_CInt32:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<int*>(pRawBuffer)[offset*2]);
         break;
      case GDT_CFloat32:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<float*>(pRawBuffer)[offset*2]);
         break;
      case GDT_CFloat64:
         pBuffer[offset] = static_cast<T>(reinterpret_cast<double*>(pRawBuffer)[offset*2]);
         break;
      }
   }

   template<>
   void convertDataTypes<IntegerComplex>(IntegerComplex *pBuffer, void *pRawBuffer, size_t offset, GDALDataType rawType)
   {
      pBuffer[offset].mImaginary = 0;
      switch(rawType)
      {
      case GDT_Byte:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<unsigned char*>(pRawBuffer)[offset]);
         break;
      case GDT_UInt16:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<unsigned short*>(pRawBuffer)[offset]);
         break;
      case GDT_Int16:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<short*>(pRawBuffer)[offset]);
         break;
      case GDT_UInt32:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<unsigned int*>(pRawBuffer)[offset]);
         break;
      case GDT_Int32:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<int*>(pRawBuffer)[offset]);
         break;
      case GDT_Float32:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<float*>(pRawBuffer)[offset]);
         break;
      case GDT_Float64:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<double*>(pRawBuffer)[offset]);
         break;
      case GDT_CInt16:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<short*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<short>(reinterpret_cast<short*>(pRawBuffer)[offset*2+1]);
         break;
      case GDT_CInt32:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<int*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<short>(reinterpret_cast<int*>(pRawBuffer)[offset*2+1]);
         break;
      case GDT_CFloat32:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<float*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<short>(reinterpret_cast<float*>(pRawBuffer)[offset*2+1]);
         break;
      case GDT_CFloat64:
         pBuffer[offset].mReal = static_cast<short>(reinterpret_cast<double*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<short>(reinterpret_cast<double*>(pRawBuffer)[offset*2+1]);
         break;
      }
   }

   template<>
   void convertDataTypes<FloatComplex>(FloatComplex *pBuffer, void *pRawBuffer, size_t offset, GDALDataType rawType)
   {
      pBuffer[offset].mImaginary = 0.0;
      switch(rawType)
      {
      case GDT_Byte:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<unsigned char*>(pRawBuffer)[offset]);
         break;
      case GDT_UInt16:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<unsigned short*>(pRawBuffer)[offset]);
         break;
      case GDT_Int16:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<short*>(pRawBuffer)[offset]);
         break;
      case GDT_UInt32:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<unsigned int*>(pRawBuffer)[offset]);
         break;
      case GDT_Int32:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<int*>(pRawBuffer)[offset]);
         break;
      case GDT_Float32:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<float*>(pRawBuffer)[offset]);
         break;
      case GDT_Float64:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<double*>(pRawBuffer)[offset]);
         break;
      case GDT_CInt16:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<short*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<float>(reinterpret_cast<short*>(pRawBuffer)[offset*2+1]);
         break;
      case GDT_CInt32:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<int*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<float>(reinterpret_cast<int*>(pRawBuffer)[offset*2+1]);
         break;
      case GDT_CFloat32:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<float*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<float>(reinterpret_cast<float*>(pRawBuffer)[offset*2+1]);
         break;
      case GDT_CFloat64:
         pBuffer[offset].mReal = static_cast<float>(reinterpret_cast<double*>(pRawBuffer)[offset*2]);
         pBuffer[offset].mImaginary = static_cast<float>(reinterpret_cast<double*>(pRawBuffer)[offset*2+1]);
         break;
      }
   }
}

GdalImporter::GdalImporter()
{
   setDescriptorId("{842c4da3-9d83-4301-8f56-b71210d1afd4}");
   setName("Generic GDAL Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright("Copyright 2008, BATC");
   setVersion("0.1");

   GDALAllRegister();
}

GdalImporter::~GdalImporter()
{
}

std::vector<ImportDescriptor*> GdalImporter::getImportDescriptors(const std::string &filename)
{
   mErrors.clear();
   mWarnings.clear();
   std::vector<ImportDescriptor*> descriptors;

   std::auto_ptr<GDALDataset> pDataset(reinterpret_cast<GDALDataset*>(GDALOpen(filename.c_str(), GA_ReadOnly)));
   if(pDataset.get() == NULL)
   {
      mErrors.push_back("GDAL does not recognize the dataset");
      return descriptors;
   }
   ImportDescriptorResource pImportDescriptor(filename, TypeConverter::toString<RasterElement>());
   descriptors.push_back(pImportDescriptor.release());

   GDALDataType rawType = pDataset->GetRasterBand(1)->GetRasterDataType();
   EncodingType encoding = gdalDataTypeToEncodingType(rawType);
   if(!encoding.isValid())
   {
      mErrors.push_back(std::string("Unsupported data type ") + GDALGetDataTypeName(rawType));
   }
   pImportDescriptor->setDataDescriptor(RasterUtilities::generateRasterDataDescriptor(
      filename, NULL, pDataset->GetRasterYSize(), pDataset->GetRasterXSize(), pDataset->GetRasterCount(),
      BSQ, gdalDataTypeToEncodingType(pDataset->GetRasterBand(1)->GetRasterDataType()), IN_MEMORY));
   RasterFileDescriptor *pFileDesc = 
      dynamic_cast<RasterFileDescriptor*>(RasterUtilities::generateAndSetFileDescriptor(
      pImportDescriptor->getDataDescriptor(), filename, "", LITTLE_ENDIAN));

   if(pFileDesc != NULL && pDataset->GetGCPCount() > 0)
   {
      std::list<GcpPoint> gcps;
      const GDAL_GCP *pGcps = pDataset->GetGCPs();
      for(int gcpnum = 0; gcpnum < pDataset->GetGCPCount(); gcpnum++)
      {
         GcpPoint gcp;
         gcp.mPixel.mX = pGcps[gcpnum].dfGCPPixel;
         gcp.mPixel.mY = pGcps[gcpnum].dfGCPLine;
         gcp.mCoordinate.mX = pGcps[gcpnum].dfGCPX;
         gcp.mCoordinate.mY = pGcps[gcpnum].dfGCPY;
         gcps.push_back(gcp);
      }
      pFileDesc->setGcps(gcps);
   }

   DynamicObject *pMetadata = pImportDescriptor->getDataDescriptor()->getMetadata();
   char **pDatasetMetadata = pDataset->GetMetadata();
   if(pMetadata != NULL && pDatasetMetadata != NULL)
   {
      for(size_t idx = 0; pDatasetMetadata[idx] != NULL; idx++)
      {
         std::string entry(pDatasetMetadata[idx]);
         std::vector<std::string> kvpair = StringUtilities::split(entry, '=');
         if(kvpair.size() == 1)
         {
            kvpair.push_back("");
         }
         pMetadata->setAttribute(kvpair.front(), kvpair.back());
      }
   }

   return descriptors;
}

unsigned char GdalImporter::getFileAffinity(const std::string &filename)
{
   std::auto_ptr<GDALDataset> pDataset(reinterpret_cast<GDALDataset*>(GDALOpen(filename.c_str(), GA_ReadOnly)));
   if(pDataset.get() != NULL)
   {
      return CAN_LOAD_FILE_TYPE;
   }
   return CAN_NOT_LOAD;
}


bool GdalImporter::validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const
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

bool GdalImporter::createRasterPager(RasterElement *pRaster) const
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

   ExecutableResource pagerPlugIn("GdalRasterPager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager *pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if(!success || pPager == NULL)
   {
      std::string message = "Execution of GdalRasterPager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

GdalRasterPager::GdalRasterPager() : mpDataset(NULL)
{
   setName("GdalRasterPager");
   setCopyright("Copyright 2008 BATC");
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk GDAL data");
   setDescriptorId("{ca3a1d92-abf5-4077-b24e-5209a6d1d2af}");
   setVersion("0.1");
   setProductionStatus(false);
   setShortDescription("GDAL pager");
}

GdalRasterPager::~GdalRasterPager()
{
}

bool GdalRasterPager::openFile(const std::string &filename)
{
   mpDataset.reset(reinterpret_cast<GDALDataset*>(GDALOpen(filename.c_str(), GA_ReadOnly)));
   return mpDataset.get() != NULL;
}

CachedPage::UnitPtr GdalRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   unsigned int startrow = pOriginalRequest->getStartRow().getOnDiskNumber();
   unsigned int numrows = std::min(pOriginalRequest->getConcurrentRows(), pDesc->getRowCount() - startrow);
   size_t bufsize = pDesc->getColumnCount() * numrows * pDesc->getBytesPerElement();
   std::auto_ptr<char> pBuffer(new char[bufsize]);

   GDALRasterBand *pBand = mpDataset->GetRasterBand(pOriginalRequest->getStartBand().getOnDiskNumber() + 1);
   if(pBand == NULL)
   {
      return CachedPage::UnitPtr();
   }
   GDALDataType rawType = pBand->GetRasterDataType();
   if(rawType == encodingTypeToGdalDataType(pDesc->getDataType()))
   {
      if(pBand->RasterIO(GF_Read, 0, startrow, pDesc->getColumnCount(), numrows,
         pBuffer.get(), pDesc->getColumnCount(), numrows, rawType, 0, 0) == CE_Failure)
      {
         return CachedPage::UnitPtr();
      }
   }
   else
   {
      size_t rawbufsize = pDesc->getColumnCount() * numrows * gdalDataTypeSize(rawType);
      std::auto_ptr<char> pRawBuffer(new char[rawbufsize]);
      if(pBand->RasterIO(GF_Read, 0, startrow, pDesc->getColumnCount(), numrows,
         pRawBuffer.get(), pDesc->getColumnCount(), numrows, rawType, 0, 0) == CE_Failure)
      {
         return CachedPage::UnitPtr();
      }
      for(size_t offset = 0; offset < numrows * pDesc->getColumnCount(); offset++)
      {
         switchOnComplexEncoding(pDesc->getDataType(), convertDataTypes, pBuffer.get(), pRawBuffer.get(), offset, rawType);
      }
   }

   return CachedPage::UnitPtr(new CachedPage::CacheUnit(
      pBuffer.release(), pOriginalRequest->getStartRow(), numrows, bufsize));
}
