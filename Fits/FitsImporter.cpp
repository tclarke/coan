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
#include "DynamicObject.h"
#include "Filename.h"
#include "FileResource.h"
#include "Fits.h"
#include "FitsImporter.h"
#include "FitsImportOptionsWidget.h"
#include "ImportDescriptor.h"
#include "ObjectResource.h"
#include "RasterDataDescriptor.h"
#include "RasterFileDescriptor.h"
#include "RasterUtilities.h"
#include "TypeConverter.h"

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

using namespace std;

FitsImporter::FitsImporter() : mImportOptionsWidget(NULL)
{
   setDescriptorId("{299E1A6D-F80B-45D2-910D-0E318303CF88}");
   setName("FITS Importer");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright("Copyright 2007, BATC");
   setVersion("0.1");
   setExtensions("FITS Files (*.fit *.fts *.fiz *.fits)");
}

FitsImporter::~FitsImporter()
{
}

vector<ImportDescriptor*> FitsImporter::getImportDescriptors(const string& filename)
{
   vector<ImportDescriptor*> descriptors;
   FactoryResource<DynamicObject> pHeader;
   unsigned int rows(0), columns(0), bands(0);
   EncodingType encoding(INT1SBYTE);
   InterleaveFormatType interleave(BSQ);
   string datasetName = filename;
   { // scope the resource
      FactoryResource<Filename> pFilename;
      pFilename->setFullPathAndName(filename);
      datasetName = pFilename->getTitle();
   }
   try
   {
      // open the file
      LargeFileResource fitsFile;
      if(!fitsFile.open(filename, O_RDONLY | O_BINARY, S_IREAD))
      {
         return descriptors;
      }

      // parse the first header unit
      pHeader = FactoryResource<DynamicObject>(Fits::parseHeaderUnit(fitsFile));
      if(pHeader.get() == NULL)
      {
         return descriptors;
      }

      // set up the data descriptor
      switch(dv_cast<int>(pHeader->getAttribute("BITPIX")))
      {
      case 8:
         encoding = INT1UBYTE;
         break;
      case 16:
         encoding = INT2SBYTES;
         break;
      case 32:
         encoding = INT4SBYTES;
         break;
      case -32:
         encoding = FLT4BYTES;
         break;
      case -64:
         encoding = FLT8BYTES;
         break;
      }
      int numAxis = dv_cast<int>(pHeader->getAttribute("NAXIS"));
      int row, column, band;
      if(mImportOptionsWidget.get() == NULL)
      {
         defaultAxisMappings(numAxis, row, column, band);
      }
      else
      {
         row = mImportOptionsWidget->property("rowAxis").toInt();
         column = mImportOptionsWidget->property("columnAxis").toInt();
         band = mImportOptionsWidget->property("bandAxis").toInt();
      }
     
      if(numAxis > 0)
      {
         rows = (row == 0) ? 1 : dv_cast<int>(pHeader->getAttribute(QString("NAXIS%1").arg(row).toStdString()));
         columns = (column == 0) ? 1 : dv_cast<int>(pHeader->getAttribute(QString("NAXIS%1").arg(column).toStdString()));
         bands = (band == 0) ? 1 : dv_cast<int>(pHeader->getAttribute(QString("NAXIS%1").arg(band).toStdString()));
      }
   }
   catch(bad_cast&)
   {
      // there is a required header keyword missing or incorrectly formatted
      return descriptors;
   }
   ImportDescriptorResource pImportDescriptor(datasetName, "RasterElement");
   VERIFYRV(pImportDescriptor.get() != NULL, descriptors);
   RasterDataDescriptor* pDescriptor = RasterUtilities::generateRasterDataDescriptor(
      datasetName, NULL, rows, columns, bands, interleave, encoding, IN_MEMORY);
   if(pDescriptor != NULL)
   {
      pImportDescriptor->setDataDescriptor(pDescriptor);
      pDescriptor->setMetadata(pHeader.get());
      FileDescriptor *pFileDescriptor =
         RasterUtilities::generateFileDescriptor(pDescriptor, filename, string(), BIG_ENDIAN);
      RasterFileDescriptor *pRasterFileDescriptor = dynamic_cast<RasterFileDescriptor*>(pFileDescriptor);
      if(pRasterFileDescriptor != NULL)
      {
         pRasterFileDescriptor->setHeaderBytes(Fits::LOGICAL_RECORD_SIZE);
      }
      pDescriptor->setFileDescriptor(pFileDescriptor);
      vector<int> badValues;
      badValues.push_back(0);
      DataVariant blank = pHeader->getAttribute("BLANK");
      if(blank.isValid())
      {
         try
         {
            badValues.push_back(dv_cast<int>(blank));
         }
         catch(bad_cast&) {}
      }
      pDescriptor->setBadValues(badValues);
      descriptors.push_back(pImportDescriptor.release());
   }
   return descriptors;
}

unsigned char FitsImporter::getFileAffinity(const string& filename)
{
   if(Fits::canParseFile(filename))
   {
      return Importer::CAN_LOAD;
   }
   return Importer::CAN_NOT_LOAD;
}

QWidget *FitsImporter::getImportOptionsWidget(DataDescriptor *pDescriptor)
{
   if(mImportOptionsWidget.get() == NULL)
   {
      int numAxis = 0;
      if(pDescriptor != NULL)
      {
         DynamicObject *pMeta = pDescriptor->getMetadata();
         if(pMeta != NULL)
         {
            try
            {
               numAxis = dv_cast<int>(pMeta->getAttribute("NAXIS"));
            }
            catch(bad_cast&) {}
         }
      }
      mImportOptionsWidget.reset(new FitsImportOptionsWidget(numAxis));
      int row, column, band;
      defaultAxisMappings(numAxis, row, column, band);
      mImportOptionsWidget->setProperty("rowAxis", row);
      mImportOptionsWidget->setProperty("columnAxis", column);
      mImportOptionsWidget->setProperty("bandAxis", band);
   }
   return mImportOptionsWidget.get();
}

void FitsImporter::defaultAxisMappings(int numAxis, int &row, int &column, int &band) const
{
   row = column = band = 0;
   switch(numAxis)
   {
   case 1:
      row = 1;
      break;
   case 3:
      band = 3; // fall through
   case 2:
      row = 2;
      column = 1;
   }
}
