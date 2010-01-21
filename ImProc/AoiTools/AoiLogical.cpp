/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AoiElement.h"
#include "AoiLayer.h"
#include "AoiLogical.h"
#include "AoiToolsFactory.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "DesktopServices.h"
#include "GeointVersion.h"
#include "LayerList.h"
#include "ModelServices.h"
#include "ObjectResource.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "ProgressTracker.h"
#include "SpatialDataView.h"
#include "Undo.h"

#include <QtGui/QInputDialog>
#include <QtCore/QMap>
#include <QtCore/QString>

AOITOOLSFACTORY(AoiLogical);

namespace
{
   QString getFullName(const DataElement* pElmnt)
   {
      if (pElmnt == NULL)
      {
         return QString();
      }
      QString fullName = QString::fromStdString(pElmnt->getName());
      for (DataElement* pParent = pElmnt->getParent(); pParent != NULL; pParent = pParent->getParent())
      {
         fullName = QString("%1 :: %2").arg(QString::fromStdString(pParent->getName())).arg(fullName);
      }
      return fullName;
   }
}

AoiLogical::AoiLogical() : mpSet1(NULL), mpSet2(NULL), mpResult(NULL), mpView(NULL)
{
   setName("AoiLogical");
   setDescription("Logical operations on AOIs");
   setDescriptorId("{7D29F890-26E8-48F2-9D06-FC065A451435}");
   setCopyright(GEOINT_COPYRIGHT);
   setVersion(GEOINT_VERSION_NUMBER);
   setProductionStatus(GEOINT_IS_PRODUCTION_RELEASE);
   setSubtype("AOI");
   addMenuLocation("[General Algorithms]/AOI Set Operations/Union");
   addMenuLocation("[General Algorithms]/AOI Set Operations/Intersection");
}

AoiLogical::~AoiLogical()
{
}

bool AoiLogical::getInputSpecification(PlugInArgList*& pInArgList)
{
   VERIFY(pInArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pInArgList->addArg<Progress>(ProgressArg(), NULL));
   VERIFY(pInArgList->addArg<std::string>(MenuCommandArg()));
   VERIFY(pInArgList->addArg<AoiElement>(DataElementArg(), std::string("Primary AOI")));
   VERIFY(pInArgList->addArg<AoiElement>("Secondary " + DataElementArg(), NULL, std::string("Secondary AOI")));
   VERIFY(pInArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   VERIFY(pInArgList->addArg<std::string>("Result Name"));
   return true;
}

bool AoiLogical::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   VERIFY(pOutArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pOutArgList->addArg<AoiElement>("Data Element"));
   return true;
}

bool AoiLogical::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   if (pInArgList == NULL || pOutArgList == NULL)
   {
      return false;
   }
   if (!extractInputArgs(pInArgList))
   {
      return false;
   }

   mProgress.report("Begin AOI set operation.", 1, NORMAL);

   if (isBatch())
   {
      mProgress.report("Batch mode is not supported.", 0, ERRORS, true);
      return false;
   }

   FactoryResource<BitMask> pResultMask;
   if (pResultMask.get() == NULL)
   {
      mProgress.report("Unable to create result AOI.", 0, ERRORS, true);
      return false;
   }
   mpResult = pResultMask.get();

   if (mOperation == "Union")
   {
      mpResult->merge(*mpSet1);
      mpResult->merge(*mpSet2);
   }
   else if (mOperation == "Intersection")
   {
      mpResult->merge(*mpSet1);
      mpResult->intersect(*mpSet2);
   }
   else
   {
      mProgress.report("Invalid operation: " + mOperation, 0, ERRORS, true);
      return false;
   }
   if (!displayResult())
   {
      return false;
   }
   mProgress.upALevel();
   return true;
}

bool AoiLogical::extractInputArgs(PlugInArgList* pInArgList)
{
   VERIFY(pInArgList);
   mProgress = ProgressTracker(pInArgList->getPlugInArgValue<Progress>(ProgressArg()),
      "Executing " + getName(), "app", "{3088b766-391a-4fab-806f-5ddc189b0708}");
   pInArgList->getPlugInArgValue<std::string>(MenuCommandArg(), mOperation);
   if (mOperation.empty())
   {
      mProgress.report("No operation specified.", 0, ERRORS, true);
      return false;
   }

   AoiElement* pAoi1 = pInArgList->getPlugInArgValue<AoiElement>(DataElementArg());
   mpSet1 = (pAoi1 == NULL) ? NULL : pAoi1->getSelectedPoints();
   if (mpSet1 == NULL)
   {
      mProgress.report("No primary AOI.", 0, ERRORS, true);
      return false;
   }
   AoiElement* pAoi2 = pInArgList->getPlugInArgValue<AoiElement>("Secondary " + DataElementArg());
   if (pAoi2 == NULL)
   {
      std::vector<DataElement*> aois = Service<ModelServices>()->getElements(TypeConverter::toString<AoiElement>());
      QMap<QString, AoiElement*> candidates;
      for (std::vector<DataElement*>::iterator aoi = aois.begin(); aoi != aois.end(); ++aoi)
      {
         if (*aoi && *aoi != pAoi1)
         {
            candidates[getFullName(*aoi)] = static_cast<AoiElement*>(*aoi);
         }
      }
      bool ok = false;
      QString selected = QInputDialog::getItem(Service<DesktopServices>()->getMainWidget(),
         "Select a secondary AOI", QString("Select a secondary AOI to %1 with %2")
               .arg(QString::fromStdString(mOperation)).arg(getFullName(pAoi1)),
         candidates.keys(), 0, false, &ok);
      if (ok && !selected.isEmpty())
      {
         pAoi2 = candidates[selected];
      }
   }
   mpSet2 = (pAoi2 == NULL) ? NULL : pAoi2->getSelectedPoints();
   if (mpSet2 == NULL)
   {
      mProgress.report("No secondary AOI.", 0, ERRORS, true);
      return false;
   }
   
   mpView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
   if (mpView == NULL)
   {
      mProgress.report("No view specified.", 0, ERRORS, true);
      return false;
   }

   pInArgList->getPlugInArgValue("Result Name", mResultName);
   if (mResultName.empty())
   {
      mResultName = mOperation;
   }
   return true;
}

bool AoiLogical::displayResult()
{
   if (mpResult == NULL || mpView == NULL)
   {
      return false;
   }
   
   ModelResource<AoiElement> pResult(mResultName);
   if (pResult.get() == NULL || pResult->addPoints(mpResult) == NULL)
   {
      mProgress.report("Unable to create result AOI.", 0, ERRORS, true);
      return false;
   }
   UndoLock lock(mpView);
   AoiLayer* pLayer = static_cast<AoiLayer*>(mpView->createLayer(AOI_LAYER, pResult.get()));
   if (pLayer == NULL)
   {
      mProgress.report("Unable to display result.", 0, ERRORS, true);
      return false;
   }
   pResult.release();
   return true;
}
