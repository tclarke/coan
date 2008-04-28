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

#include "AppVerify.h"
#include "DataDescriptor.h"
#include "DynamicObject.h"
#include "FileDescriptor.h"
#include "Filename.h"
#include "FileResource.h"
#include "Fits.h"
#include "FitsSignatureImporter.h"
#include "ImportDescriptor.h"
#include "MessageLogResource.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInResource.h"
#include "Progress.h"
#include "RasterUtilities.h"
#include "Signature.h"

using namespace std;

FitsSignatureImporter::FitsSignatureImporter()
{
   setName("FITS Signature Importer");
   setDescriptorId("{9CD2DAE1-17BA-4520-AD1A-4F9B4FC97C52}");
   setSubtype("Signature");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright("Copyright 2007, BATC");
   setVersion("0.1");
   setExtensions("FITS Files (*.fit *.fts *.fiz *.fits)");
}

FitsSignatureImporter::~FitsSignatureImporter()
{
}

vector<ImportDescriptor*> FitsSignatureImporter::getImportDescriptors(const string& filename)
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
   ImportDescriptorResource pImportDescriptor(datasetName, "Signature");
   VERIFYRV(pImportDescriptor.get() != NULL, descriptors);
   DataDescriptor* pDescriptor = static_cast<DataDescriptor*>(pImportDescriptor->getDataDescriptor());
   if(pDescriptor != NULL)
   {
      pDescriptor->setMetadata(pHeader.get());
      FileDescriptor *pFileDescriptor =
         RasterUtilities::generateFileDescriptor(pDescriptor, filename, string(), BIG_ENDIAN);
      pDescriptor->setFileDescriptor(pFileDescriptor);
      descriptors.push_back(pImportDescriptor.release());
   }
   return descriptors;
}

unsigned char FitsSignatureImporter::getFileAffinity(const string& filename)
{
   if(Fits::canParseFile(filename))
   {
      return Importer::CAN_LOAD;
   }
   return Importer::CAN_NOT_LOAD;
}

bool FitsSignatureImporter::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY((pArgList = Service<PlugInManagerServices>()->getPlugInArgList()));
   VERIFY(pArgList->addArg<Progress>(Executable::ProgressArg(), NULL));
   VERIFY(pArgList->addArg<Signature>(Importer::ImportElementArg()));
   return true;
}

bool FitsSignatureImporter::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool FitsSignatureImporter::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if(pInArgList == NULL) return false;
   StepResource pStep("Import FITS Signature", "app", "5A6AD72C-F6BD-469B-AA34-747851A095A1");

   Progress *pProgress = pInArgList->getPlugInArgValue<Progress>(Executable::ProgressArg());
   Signature *pSignature = pInArgList->getPlugInArgValue<Signature>(Importer::ImportElementArg());
   if(pSignature == NULL)
   {
      string msg = "No signature found.";
      if(pProgress != NULL) pProgress->updateProgress(msg, 0, ERRORS);
      pStep->finalize(Message::Failure, msg);
      return false;
   }
   string filename = pSignature->getDataDescriptor()->getFileDescriptor()->getFilename().getFullPathAndName();
   const DynamicObject *pHeader = pSignature->getDataDescriptor()->getMetadata();
   VERIFY(pHeader);

   // Open the file and seek to the start of data.
   LargeFileResource fitsFile;
   if(!fitsFile.open(filename, O_RDONLY | O_BINARY, S_IREAD))
   {
      string msg = "Unable to open FITS file.";
      if(pProgress != NULL) pProgress->updateProgress(msg, 0, ERRORS);
      pStep->finalize(Message::Failure, msg);
      return false;
   }

   if(pProgress != NULL) pProgress->updateProgress("Successfully imported FITS signature.", 100, NORMAL);
   pStep->finalize(Message::Success);
   return true;
}