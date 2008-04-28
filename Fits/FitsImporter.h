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

#ifndef FITSIMPORTER_H
#define FITSIMPORTER_H

#include "RasterElementImporterShell.h"

#include <memory>
#include <string>
#include <vector>

class DynamicObject;
class LargeFileResource;
class QWidget;

class FitsImporter : public RasterElementImporterShell
{
public:
   FitsImporter();
   ~FitsImporter();

   std::vector<ImportDescriptor*> getImportDescriptors(const std::string& filename);
   unsigned char getFileAffinity(const std::string& filename);
   QWidget *getImportOptionsWidget(DataDescriptor *pDescriptor);

protected:
   void defaultAxisMappings(int numAxis, int &row, int &column, int &band) const;

private:
   std::auto_ptr<QWidget> mImportOptionsWidget;
};

#endif
