/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "PlugInFactory.h"
#include "StreamManager.h"
#include "VideoStream.h"
#include "VideoVersion.h"
#include <vector>

PLUGINFACTORY(StreamManager);

StreamManager::StreamManager()
{
   setName("Video Stream Manager");
   setDescription("Singleton plug-in to manage the video stream data type.");
   setDescriptorId("{cc73adbb-3295-4b6c-b215-6094aabcdc68}");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(VIDEO_COPYRIGHT);
   setVersion(VIDEO_VERSION_NUMBER);
   setProductionStatus(VIDEO_IS_PRODUCTION_RELEASE);
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
}

StreamManager::~StreamManager()
{
   std::vector<DataElement*> streamElements = Service<ModelServices>()->getElements("VideoStream");
   for (std::vector<DataElement*>::iterator element = streamElements.begin(); element != streamElements.end(); ++element)
   {
      Any* pAnyElement = dynamic_cast<Any*>(*element);
      if (model_cast<VideoStream*>(*element) != NULL)
      {
         pAnyElement->setData(NULL);
      }
   }
}

bool StreamManager::getInputSpecification(PlugInArgList*& pInArgList)
{
   pInArgList = NULL;
   return true;
}

bool StreamManager::getOutputSpecification(PlugInArgList*& pOutArgList)
{
   pOutArgList = NULL;
   return true;
}

bool StreamManager::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   Service<ModelServices>()->addElementType("VideoStream");
   return true;
}

bool StreamManager::initializeStream(Any* pStreamContainer)
{
   VERIFY(pStreamContainer != NULL);
   if (pStreamContainer->getData() != NULL)
   {
      // already initialized
      return false;
   }

   VideoStream* pData = new VideoStream(pStreamContainer);
   pStreamContainer->setData(pData);
   return pStreamContainer->getData() != NULL;
}
