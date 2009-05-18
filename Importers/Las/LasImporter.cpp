/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DataVariant.h"
#include "DesktopServices.h"
#include "DynamicObject.h"
#include "ImportDescriptor.h"
#include "ImportersVersion.h"
#include "LasImporter.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "ProgressTracker.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "Undo.h"
#include <liblas/lasfile.hpp>
#include <liblas/lasheader.hpp>
#include <QtCore/QString>

REGISTER_PLUGIN_BASIC(Las, LasImporter);

LasImporter::LasImporter()
{
   setName("LasImporter");
   setDescription("LAS importer");
   setDescriptorId("{43DD7AED-0B29-4106-B524-B55ED86FAEB8}");
   setCopyright(IMPORTERS_COPYRIGHT);
   setVersion(IMPORTERS_VERSION_NUMBER);
   setProductionStatus(IMPORTERS_IS_PRODUCTION_RELEASE);
   setExtensions("LAS Files (*.las)");
   setAbortSupported(true);
}

LasImporter::~LasImporter()
{
}

std::vector<ImportDescriptor*> LasImporter::getImportDescriptors(const std::string& filename)
{
   std::vector<ImportDescriptor*> descriptors;

   liblas::LASFile las(filename);
   if (las.IsNull())
   {
      return descriptors;
   }
   const liblas::LASHeader& header = las.GetHeader();
   double width = header.GetMaxX() - header.GetMinX();
   double height = header.GetMaxY() - header.GetMinY();
   ImportDescriptorResource descZ(las.GetName(), TypeConverter::toString<RasterElement>());
   ImportDescriptorResource descI(las.GetName(), TypeConverter::toString<RasterElement>(), NULL, false);
   VERIFYRV(descZ.get() && descI.get(), descriptors);
   std::vector<liblas::uint32_t> pointsByReturn = header.GetPointRecordsByReturnCount();
   RasterDataDescriptor* pDataDescZ = RasterUtilities::generateRasterDataDescriptor(las.GetName()+":height", NULL,
      static_cast<unsigned int>(std::ceil(width)), static_cast<unsigned int>(std::ceil(height)),
      std::max<unsigned int>(1, pointsByReturn.size()), BSQ, FLT8BYTES, IN_MEMORY);
   RasterDataDescriptor* pDataDescI = RasterUtilities::generateRasterDataDescriptor(las.GetName()+":intensity", NULL,
      static_cast<unsigned int>(std::ceil(width)), static_cast<unsigned int>(std::ceil(height)),
      std::max<unsigned int>(1, pointsByReturn.size()), BSQ, INT2UBYTES, IN_MEMORY);

   std::vector<int> badValues;
   badValues.push_back(0);
   pDataDescZ->setBadValues(badValues);
   pDataDescI->setBadValues(badValues);
   VERIFYRV(pDataDescZ && pDataDescI, descriptors);
   descZ->setDataDescriptor(pDataDescZ);
   descI->setDataDescriptor(pDataDescI);

   VERIFYRV(RasterUtilities::generateAndSetFileDescriptor(pDataDescZ, filename, "height", LITTLE_ENDIAN_ORDER), descriptors);
   VERIFYRV(RasterUtilities::generateAndSetFileDescriptor(pDataDescI, filename, "intensity", LITTLE_ENDIAN_ORDER), descriptors);

   DynamicObject* pMetadataZ = pDataDescZ->getMetadata();
   DynamicObject* pMetadataI = pDataDescI->getMetadata();
   VERIFYRV(pMetadataZ && pMetadataI, descriptors);
   pMetadataZ->setAttributeByPath("LAS/Version", QString("%1.%2").arg(header.GetVersionMajor()).arg(header.GetVersionMinor()).toStdString());
   pMetadataZ->setAttributeByPath("LAS/Creation DOY", header.GetCreationDOY());
   pMetadataZ->setAttributeByPath("LAS/Creation Year", header.GetCreationYear());
   pMetadataZ->setAttributeByPath("LAS/Point Format", header.GetDataFormatId() == liblas::LASHeader::ePointFormat0 ? 0 : 1);
   pMetadataZ->setAttributeByPath("LAS/Point Count", header.GetPointRecordsCount());
   pMetadataZ->setAttributeByPath("LAS/Points Per Return", pointsByReturn);
   pMetadataZ->setAttributeByPath("LAS/Scale/X", header.GetScaleX());
   pMetadataZ->setAttributeByPath("LAS/Scale/Y", header.GetScaleY());
   pMetadataZ->setAttributeByPath("LAS/Scale/Z", header.GetScaleZ());
   pMetadataZ->setAttributeByPath("LAS/Offset/X", header.GetOffsetX());
   pMetadataZ->setAttributeByPath("LAS/Offset/Y", header.GetOffsetY());
   pMetadataZ->setAttributeByPath("LAS/Offset/Z", header.GetOffsetZ());
   //pMetadataZ->setAttributeByPath("LAS/Projection", header.GetProj4());
   pMetadataI->merge(pMetadataZ);

   RasterFileDescriptor* pFileDescZ = static_cast<RasterFileDescriptor*>(pDataDescZ->getFileDescriptor());
   RasterFileDescriptor* pFileDescI = static_cast<RasterFileDescriptor*>(pDataDescI->getFileDescriptor());
   std::vector<DimensionDescriptor> bandsToLoadZ;
   std::vector<DimensionDescriptor> bandsToLoadI;
   for (unsigned int bandNum = 0; bandNum < pointsByReturn.size(); bandNum++)
   {
      if (pointsByReturn[bandNum] > 0)
      {
         bandsToLoadZ.push_back(pFileDescZ->getOnDiskBand(bandNum));
         bandsToLoadI.push_back(pFileDescI->getOnDiskBand(bandNum));
      }
   }
   if (bandsToLoadZ.empty())
   {
      bandsToLoadZ.push_back(pFileDescZ->getOnDiskBand(0));
   }
   if (bandsToLoadI.empty())
   {
      bandsToLoadI.push_back(pFileDescI->getOnDiskBand(0));
   }
   pDataDescZ->setBands(bandsToLoadZ);
   pDataDescI->setBands(bandsToLoadI);

   descriptors.push_back(descZ.release());
   descriptors.push_back(descI.release());
   return descriptors;
}

unsigned char LasImporter::getFileAffinity(const std::string& filename)
{
   if (liblas::LASFile(filename).IsNull())
   {
      return CAN_NOT_LOAD;
   }
   return CAN_LOAD;
}

bool LasImporter::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   pInArgList->addArg<Progress>(ProgressArg(), NULL);
   pInArgList->addArg<RasterElement>(ImportElementArg());
   return true;
}

bool LasImporter::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   if (!isBatch())
   {
      pOutArgList->addArg<SpatialDataView>("View");
   }
   return true;
}

bool LasImporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Loading LAS data",
      "LIDAR", "{b781a8a7-94fb-4172-b3ca-8666f45d0bb3}");
   progress.report("Loading LAS data...", 1, NORMAL);
   RasterElement* pData = pInArgList->getPlugInArgValue<RasterElement>(ImportElementArg());
   if (pData == NULL)
   {
      progress.report("No data element specified.", 0, ERRORS, true);
      return false;
   }
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pData->getDataDescriptor());
   VERIFY(pDesc);
   bool intensity = pDesc->getFileDescriptor()->getDatasetLocation() == "intensity";
   if (pDesc->getProcessingLocation() == ON_DISK_READ_ONLY)
   {
      progress.report("On-disk read only not supported.", 0, ERRORS, true);
      return false;
   }
   liblas::LASFile las(pDesc->getFileDescriptor()->getFilename().getFullPathAndName());
   VERIFY(!las.IsNull());
   bool has0 = false, has1 = false;
   std::vector<DataAccessor> accessors;
   for (unsigned int bandNum = 0; bandNum < pDesc->getBandCount(); bandNum++)
   {
      FactoryResource<DataRequest> pReq;
      pReq->setBands(pDesc->getActiveBand(bandNum), pDesc->getActiveBand(bandNum), 1);
      pReq->setRows(pDesc->getActiveRow(0), pDesc->getActiveRow(pDesc->getRowCount()-1), 1);
      pReq->setColumns(pDesc->getActiveColumn(0), pDesc->getActiveColumn(pDesc->getColumnCount()-1), 1);
      pReq->setWritable(true);
      accessors.push_back(pData->getDataAccessor(pReq.release()));
   }

   liblas::LASReader& reader(las.GetReader());
   liblas::LASHeader const& header(las.GetHeader());
   unsigned int cur = 0;
   while(reader.ReadNextPoint())
   {
      if (isAborted())
      {
         progress.report("Aborted load...", 0, ABORT, true);
         return false;
      }
      progress.report("Loading LAS data...", cur++ * 100 / header.GetPointRecordsCount(), NORMAL);
      liblas::LASPoint const& p = reader.GetPoint();
      unsigned int bandNum = pDesc->getOnDiskBand(p.GetReturnNumber()).getActiveNumber();
      DimensionDescriptor row = pDesc->getOnDiskRow(static_cast<unsigned int>(p.GetX() - header.GetMinX()));
      DimensionDescriptor col = pDesc->getOnDiskColumn(static_cast<unsigned int>(p.GetY() - header.GetMinY()));
      if (!row.isValid() || !col.isValid())
      {
         continue;
      }
      accessors[bandNum]->toPixel(row.getActiveNumber(), col.getActiveNumber());
      if (!accessors[bandNum].isValid())
      {
         progress.report("Error reading LAS data...", 0, ERRORS, true);
         return false;
      }
      if (intensity)
      {
         *reinterpret_cast<unsigned short*>(accessors[bandNum]->getColumn()) = p.GetIntensity();
      }
      else
      {
         *reinterpret_cast<double*>(accessors[bandNum]->getColumn()) = p.GetZ();
      }
   }

   // Create the view
   if (!isBatch())
   {
      progress.report("Creating view...", 99, NORMAL);
      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(
         Service<DesktopServices>()->createWindow(pData->getName(), SPATIAL_DATA_WINDOW));
      SpatialDataView* pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
      if (pView == NULL)
      {
         progress.report("Error creating view...", 0, ERRORS, true);
         return false;
      }
      pView->setPrimaryRasterElement(pData);

      // Block undo actions when creating the layers
      UndoLock lock(pView);

      // Add the cube layer
      RasterLayer* pLayer = static_cast<RasterLayer*>(pView->createLayer(RASTER, pData));
      if (pLayer == NULL)
      {
         progress.report("Erorr creating layer...", 0, ERRORS, true);
         return false;
      }
      pOutArgList->setPlugInArgValue("View", pView);
   }

   progress.report("Finished loading LAS data...", 100, NORMAL);
   progress.upALevel();
   return true;
}
