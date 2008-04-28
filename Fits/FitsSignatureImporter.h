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

#ifndef FITSSIGNATUREIMPORTER_H
#define FITSSIGNATUREIMPORTER_H

#include "ImporterShell.h"

#include <memory>
#include <string>
#include <vector>

class DynamicObject;
class LargeFileResource;
class QWidget;

/**
 * This inherits FitsImporter since it's essentially the same
 * importer except this one copies the data from one axis into a Signature.
 */
class FitsSignatureImporter : public ImporterShell
{
public:
   FitsSignatureImporter();
   ~FitsSignatureImporter();

   std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);
   unsigned char getFileAffinity(const std::string& filename);
   bool getInputSpecification(PlugInArgList*& pArgList);
   bool getOutputSpecification(PlugInArgList*& pArgList);
   bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);
};

#endif
