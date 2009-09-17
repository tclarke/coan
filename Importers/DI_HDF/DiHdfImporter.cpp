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

#include "AnimationController.h"
#include "AnimationServices.h"
#include "AnimationToolBar.h"
#include "AppVerify.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "DiHdfImporter.h"
#include "DynamicObject.h"
#include "FileResource.h"
#include "ImportDescriptor.h"
#include "ImportersVersion.h"
#include "PlugInArgList.h"
#include "PlugInRegistration.h"
#include "PlugInResource.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "StringUtilities.h"
#include "TypeConverter.h"
#include <hdf.h>

REGISTER_PLUGIN_BASIC(DiHdf, DiHdfImporter);
REGISTER_PLUGIN_BASIC(DiHdf, DiHdfRasterPager);

DiHdfImporter::DiHdfImporter()
{
   setDescriptorId("{0da1caba-ee6f-440d-b53f-e0c317a8fba1}");
   setName("DI HDF Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(IMPORTERS_COPYRIGHT);
   setVersion(IMPORTERS_VERSION_NUMBER);
   setProductionStatus(IMPORTERS_IS_PRODUCTION_RELEASE);
   setExtensions("DI HDF Files (*.hdf)");
}

DiHdfImporter::~DiHdfImporter()
{
}

std::vector<ImportDescriptor*> DiHdfImporter::getImportDescriptors(const std::string &filename)
{
   mErrors.clear();
   mWarnings.clear();
   std::vector<ImportDescriptor*> descriptors;

   ImportDescriptorResource pImportDescriptor(filename, TypeConverter::toString<RasterElement>());
   VERIFYRV(pImportDescriptor.get(), descriptors);
   descriptors.push_back(pImportDescriptor.release());

   HdfContext hdf(filename);
   if (hdf.grid() == FAIL)
   {
      mErrors.push_back("Invalid DI HDF file.");
      return descriptors;
   }

   int32 frames = 0;
   int32 attrs = 0;
   GRfileinfo(hdf.grid(), &frames, &attrs);
   if (frames <= 0)
   {
      mErrors.push_back("Dataset does not contain and frames.");
      return descriptors;
   }
   if (hdf.toFrame(0) == FAIL)
   {
      mErrors.push_back("Unable to access image data.");
      return descriptors;
   }
   char pName[256];
   int32 comps = 0;
   int32 data_type = 0;
   int32 interlace_mode = 0;
   int32 pDims[2] = {0,0};
   EncodingType encoding;
   GRgetiminfo(hdf.riid(), pName, &comps, &data_type, &interlace_mode, pDims, &attrs);
   switch(data_type)
   {
   case DFNT_FLOAT32:
      encoding = FLT4BYTES;
      break;
   case DFNT_FLOAT64:
      encoding = FLT8BYTES;
      break;
   case DFNT_CHAR8:
   case DFNT_INT8:
      encoding = INT1SBYTE;
      break;
   case DFNT_UCHAR8:
   case DFNT_UINT8:
      encoding = INT1UBYTE;
      break;
   case DFNT_INT16:
      encoding = INT2SBYTES;
      break;
   case DFNT_UINT16:
      encoding = INT2UBYTES;
      break;
   case DFNT_INT32:
      encoding = INT4SBYTES;
      break;
   case DFNT_UINT32:
      encoding = INT4UBYTES;
      break;
   case DFNT_INT64:
   case DFNT_UINT64:
   default:
      mErrors.push_back("Unknown data encoding.");
      break;
   }
   pImportDescriptor->setDataDescriptor(RasterUtilities::generateRasterDataDescriptor(
            filename, NULL, pDims[1], pDims[0], frames, BSQ, encoding, IN_MEMORY));
   RasterUtilities::generateAndSetFileDescriptor(pImportDescriptor->getDataDescriptor(), filename,
      std::string(), LITTLE_ENDIAN_ORDER);

   return descriptors;
}

unsigned char DiHdfImporter::getFileAffinity(const std::string &filename)
{
   HdfContext hdf(filename);
   if (hdf.grid() == FAIL)
   {
      return CAN_NOT_LOAD;
   }
   return CAN_LOAD;
}

bool DiHdfImporter::validate(const DataDescriptor *pDescriptor, std::string &errorMessage) const
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

bool DiHdfImporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (!RasterElementImporterShell::execute(pInArgList, pOutArgList))
   {
      return false;
   }
   SpatialDataView* pView = (pOutArgList == NULL) ? NULL : pOutArgList->getPlugInArgValue<SpatialDataView>("View");
   if (pView != NULL)
   {
      if (pView->createDefaultAnimation() == NULL)
      {
         return false;
      }
      AnimationController* pController = Service<AnimationServices>()->getAnimationController(pView->getName());
      if (pController == NULL)
      {
         return false;
      }
      pController->setAnimationCycle(REPEAT);
      pController->setCanDropFrames(false);
      pController->setIntervalMultiplier(2);
      AnimationToolBar* pToolbar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
      VERIFY(pToolbar);
      pToolbar->setAnimationController(pController);
   }
   return true;
}

bool DiHdfImporter::createRasterPager(RasterElement *pRaster) const
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

   ExecutableResource pagerPlugIn("DiHdfRasterPager", std::string(), pProgress);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedElementArg(), pRaster);
   pagerPlugIn->getInArgList().setPlugInArgValue(CachedPager::PagedFilenameArg(), pFilename.get());

   bool success = pagerPlugIn->execute();

   RasterPager *pPager = dynamic_cast<RasterPager*>(pagerPlugIn->getPlugIn());
   if(!success || pPager == NULL)
   {
      std::string message = "Execution of DiHdfRasterPager failed!";
      if (pProgress != NULL) pProgress->updateProgress(message, 0, ERRORS);
      return false;
   }

   pRaster->setPager(pPager);
   pagerPlugIn->releasePlugIn();

   return true;
}

DiHdfRasterPager::DiHdfRasterPager()
{
   setName("DiHdfRasterPager");
   setCopyright(IMPORTERS_COPYRIGHT);
   setCreator("Ball Aerospace & Technologies Corp.");
   setDescription("Provides access to on-disk DI HDF data");
   setDescriptorId("{b6ebb6b8-c37d-4d2c-9472-80b6474ce6f6}");
   setVersion(IMPORTERS_VERSION_NUMBER);
   setProductionStatus(IMPORTERS_IS_PRODUCTION_RELEASE);
   setShortDescription("DI HDF pager");
}

DiHdfRasterPager::~DiHdfRasterPager()
{
}

bool DiHdfRasterPager::openFile(const std::string &filename)
{
   mHdf.open(filename);
   return mHdf.grid() != FAIL;
}

CachedPage::UnitPtr DiHdfRasterPager::fetchUnit(DataRequest *pOriginalRequest)
{
   VERIFYRV(pOriginalRequest != NULL, CachedPage::UnitPtr());
   unsigned int frame = pOriginalRequest->getStartBand().getOnDiskNumber();
   if (mHdf.toFrame(frame) == FAIL)
   {
      return CachedPage::UnitPtr();
   }
   const RasterDataDescriptor *pDesc = static_cast<const RasterDataDescriptor*>(getRasterElement()->getDataDescriptor());
   unsigned int bpp = pDesc->getBytesPerElement();
   size_t bufsize = pOriginalRequest->getConcurrentRows() * pDesc->getColumnCount() * bpp;
   char* pBuffer = new char[bufsize];
   int32 pStart[] = {pDesc->getActiveColumn(0).getOnDiskNumber(), pOriginalRequest->getStartRow().getOnDiskNumber()};
   int32 pCount[] = {pDesc->getColumnCount(), pOriginalRequest->getConcurrentRows()};
   int32 pStride[] = {1, 1};
   if (GRreadimage(mHdf.riid(), pStart, pStride, pCount, pBuffer) == FAIL)
   {
      delete pBuffer;
      return CachedPage::UnitPtr();
   }
   return CachedPage::UnitPtr(new CachedPage::CacheUnit(
      pBuffer, pOriginalRequest->getStartRow(), pOriginalRequest->getConcurrentRows(),
      bufsize, pOriginalRequest->getStartBand()));
}
