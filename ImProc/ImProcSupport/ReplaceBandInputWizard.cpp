/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DataVariant.h"
#include "DimensionDescriptor.h"
#include "DynamicObject.h"
#include "ModelServices.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "ReplaceBandInputWizard.h"
#include "SpecialMetadata.h"
#include "StringUtilities.h"
#include "TypeConverter.h"
#include <QtCore/QVariant>
#include <QtGui/QComboBox>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWizardPage>

ReplaceBandInputWizard::ReplaceBandInputWizard(const RasterElement* pDest, QWidget* pParent) :
      QWizard(pParent), mpDest(pDest)
{
   addSourcePage();
   addSourceBandPage();
   addDestBandPage();

   setWindowTitle("Replace band");
}

ReplaceBandInputWizard::~ReplaceBandInputWizard()
{
}

void ReplaceBandInputWizard::initializePage(int id)
{
   if (id == mSourceBandId)
   {
      mpSourceBand->clear();
      RasterElement* pSource = getSourceElement();
      if (pSource != NULL)
      {
         mpSourceBand->addItems(getBandNames(static_cast<const RasterDataDescriptor*>(pSource->getDataDescriptor())));
      }
   }
}

void ReplaceBandInputWizard::addSourcePage()
{
   QWizardPage* pPage = new QWizardPage(this);
   pPage->setTitle("Select source data element");
   QLabel* pPageLabel = new QLabel("Select the source data element.", pPage);
   pPageLabel->setWordWrap(true);
   mpSource = new QComboBox(pPage);
   mpSource->setEditable(false);

   const RasterDataDescriptor* pDestDesc = static_cast<const RasterDataDescriptor*>(mpDest->getDataDescriptor());
   std::vector<DataElement*> rasters = Service<ModelServices>()->getElements(TypeConverter::toString<RasterElement>());
   for(std::vector<DataElement*>::iterator raster = rasters.begin(); raster != rasters.end(); ++raster)
   {
      RasterElement* pRaster = static_cast<RasterElement*>(*raster);
      if (pRaster == mpDest)
      {
         continue;
      }
      const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pRaster->getDataDescriptor());
      if (pDesc->getRowCount() == pDestDesc->getRowCount() &&
          pDesc->getColumnCount() == pDestDesc->getColumnCount() &&
          pDesc->getDataType() == pDestDesc->getDataType())
      {
         mpSource->addItem(QString::fromStdString(pRaster->getName()), reinterpret_cast<qulonglong>(pRaster));
      }
   }

   QVBoxLayout* pLayout = new QVBoxLayout();
   pLayout->addWidget(pPageLabel);
   pLayout->addWidget(mpSource);
   pPage->setLayout(pLayout);

   addPage(pPage);
}

void ReplaceBandInputWizard::addSourceBandPage()
{
   QWizardPage* pPage = new QWizardPage(this);
   pPage->setTitle("Select source band");
   QLabel* pPageLabel = new QLabel("Select the source band.", pPage);
   pPageLabel->setWordWrap(true);
   mpSourceBand = new QComboBox(pPage);
   mpSourceBand->setEditable(false);

   QVBoxLayout* pLayout = new QVBoxLayout();
   pLayout->addWidget(pPageLabel);
   pLayout->addWidget(mpSourceBand);
   pPage->setLayout(pLayout);

   mSourceBandId = addPage(pPage);
}

void ReplaceBandInputWizard::addDestBandPage()
{
   QWizardPage* pPage = new QWizardPage(this);
   pPage->setTitle("Select destination band");
   QLabel* pPageLabel = new QLabel("Select the destination band.", pPage);
   pPageLabel->setWordWrap(true);
   mpDestBand = new QComboBox(pPage);
   mpDestBand->setEditable(false);
   mpDestBand->addItems(getBandNames(static_cast<const RasterDataDescriptor*>(mpDest->getDataDescriptor())));

   QVBoxLayout* pLayout = new QVBoxLayout();
   pLayout->addWidget(pPageLabel);
   pLayout->addWidget(mpDestBand);
   pPage->setLayout(pLayout);

   addPage(pPage);
}

RasterElement* ReplaceBandInputWizard::getSourceElement() const
{
   if (mpSource->currentIndex() < 0)
   {
      return NULL;
   }
   return reinterpret_cast<RasterElement*>(mpSource->itemData(mpSource->currentIndex()).toULongLong());
}

unsigned int ReplaceBandInputWizard::getDestBand() const
{
   return mpDestBand->currentIndex();
}

unsigned int ReplaceBandInputWizard::getSourceBand() const
{
   return mpSourceBand->currentIndex();
}

QStringList ReplaceBandInputWizard::getBandNames(const RasterDataDescriptor* pDesc) const
{
   const DynamicObject* pMetadata = pDesc->getMetadata();
   const std::vector<DimensionDescriptor>& bands = pDesc->getBands();

   std::vector<std::string> bandNames;
   std::string pNamesPath[] = { SPECIAL_METADATA_NAME, BAND_METADATA_NAME, NAMES_METADATA_NAME, END_METADATA_NAME };
   const std::vector<std::string>* pBandNames = dv_cast<std::vector<std::string> >(&pMetadata->getAttributeByPath(pNamesPath));
   if ((pBandNames != NULL) && (pBandNames->size() == bands.size()))
   {
      bandNames = *pBandNames;
   }
   else
   {
      std::string pPrefixPath[] = { SPECIAL_METADATA_NAME, BAND_NAME_PREFIX_METADATA_NAME, END_METADATA_NAME };
      const std::string* pBandPrefix = dv_cast<std::string>(&pMetadata->getAttributeByPath(pPrefixPath));
      if (pBandPrefix != NULL)
      {
         std::string bandNamePrefix = *pBandPrefix;
         for (std::vector<DimensionDescriptor>::const_iterator band = bands.begin(); band != bands.end(); ++band)
         {
            bandNames.push_back(bandNamePrefix + " " + StringUtilities::toDisplayString(band->getOriginalNumber() + 1));
         }
      }
   }
   QStringList qBandNames;
   for (unsigned int i = 0; i < bands.size(); i++)
   {
      DimensionDescriptor bandDim = bands[i];
      QString name;
      if ((i < bandNames.size()) && (bands.size() == bandNames.size()))
      {
         name = QString::fromStdString(bandNames[i]);
      }
      else
      {
         name = "Band ";
         if (bandDim.isOriginalNumberValid())
         {
            unsigned int originalNumber = bandDim.getOriginalNumber() + 1;
            name.append(QString::number(originalNumber));
         }
      }
      qBandNames.append(name);
   }
   return qBandNames;
}