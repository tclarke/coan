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
#include "ConfigurationSettings.h"
#include "DataAccessor.h"
#include "ExecutableShell.h"
#include "RasterData.h"
#include "RasterLayer.h"
#include "TrackingUtils.h"
#include <boost/any.hpp>

class GraphicGroup;
class RasterDataDescriptor;
class RasterElement;

class TrackingManager : public ExecutableShell
{
public:
   SETTING(InitialSubcubeSize, TrackingManager, unsigned int, 0);

   static const char* spPlugInName;

   TrackingManager();
   virtual ~TrackingManager();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

   void setTrackedLayer(RasterLayer* pLayer);
   void setPauseState(bool state);

protected:
   void processFrame(Subject& subject, const std::string& signal, const boost::any& val);
   void clearData(Subject& subject, const std::string& signal, const boost::any& val);

private:
   void initializeFrame0();

   bool mPaused;
   AttachmentPtr<RasterLayer> mpLayer;
   AttachmentPtr<Animation> mpAnimation;
   const RasterDataDescriptor* mpDesc;
   RasterElement* mpElement;

   DataAccessor mBaseAcc;
   IplImageResource mpBaseFrame;
   IplImageResource mpEigImage;
   IplImageResource mpTmpImage;
   std::auto_ptr<CvPoint2D32f> mpBaseCorners;
   IplImageResource mpBasePyramid;

   char mpFeaturesFound[500];
   float mpFeatureErrors[500];
   GraphicGroup* mpGroup;
   int mCornerCount;
   RasterElement* mpRes;
   RasterElement* mpRes2;
};

#endif