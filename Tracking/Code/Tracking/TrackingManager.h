/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef TRACKINGMANAGER_H__
#define TRACKINGMANAGER_H__

#include "Animation.h"
#include "AttachmentPtr.h"
#include "ExecutableShell.h"
#include "RasterLayer.h"
#include <boost/any.hpp>

class TrackingManager : public ExecutableShell
{
public:
   static const char* spPlugInName;

   TrackingManager();
   virtual ~TrackingManager();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

   void setTrackedLayer(RasterLayer* pLayer);

protected:
   void processFrame(Subject& subject, const std::string& signal, const boost::any& val);

private:
   AttachmentPtr<RasterLayer> mpLayer;
   AttachmentPtr<Animation> mpAnimation;
};

#endif