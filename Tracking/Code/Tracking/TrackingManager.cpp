/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "TrackingManager.h"
#include "TrackingUtils.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterLayer.h"

REGISTER_PLUGIN_BASIC(Tracking, TrackingManager);

const char* TrackingManager::spPlugInName("TrackingManager");

TrackingManager::TrackingManager() :
      mpAnimation(SIGNAL_NAME(Animation, FrameChanged), Slot(this, &TrackingManager::processFrame)),
      mpDesc(NULL)
{
   setName(spPlugInName);
   setDescriptorId("{c5f096e1-1584-4d7c-aa6f-29c4e422aad1}");
   setType("Manager");
   setSubtype("Video");
   setAbortSupported(false);
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setWizardSupported(false);
}

TrackingManager::~TrackingManager()
{
}

bool TrackingManager::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool TrackingManager::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool TrackingManager::execute(PlugInArgList*, PlugInArgList*)
{
   return true;
}

void TrackingManager::setTrackedLayer(RasterLayer* pLayer)
{
   mpLayer.reset(pLayer);
   mpAnimation.reset((pLayer == NULL) ? NULL : pLayer->getAnimation());
   initializeFrame0();
}

void TrackingManager::processFrame(Subject& subject, const std::string& signal, const boost::any& val)
{
   VERIFYNRV(mpLayer.get());
   RasterElement* pElement = static_cast<RasterElement*>(mpLayer->getDataElement());
   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   Opticks::PixelLocation maxBb(pDesc->getColumnCount(), pDesc->getRowCount());
   uint8_t levels = TrackingUtils::calculateNeededLevels(TrackingManager::getSettingInitialSubcubeSize(), maxBb);
   if (levels == 0xff)
   {
      return;
   }
   TrackingUtils::subcubeid_t id = TrackingUtils::calculateSubcubeId(LocationType(100, 100), levels, maxBb);
   Opticks::PixelLocation minBb;
   TrackingUtils::calculateSubcubeBounds(id, levels, maxBb, minBb);
}

void TrackingManager::initializeFrame0()
{
   if (mpLayer.get() == NULL)
   {
      mpDesc = NULL;
      return;
   }
   mpDesc = static_cast<RasterDataDescriptor*>(static_cast<RasterElement*>(mpLayer->getDataElement())->getDataDescriptor());
}