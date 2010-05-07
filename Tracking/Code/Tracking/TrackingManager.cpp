/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnnotationElement.h"
#include "ApiUtilities.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "GraphicGroup.h"
#include "GraphicObject.h"
#include "LayerList.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterData.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SpatialDataView.h"
#include "TrackingManager.h"
#include "TrackingUtils.h"
#include <time.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <vector>

REGISTER_PLUGIN_BASIC(Tracking, TrackingManager);

#define FORWARD_XFORM
#define SHOW_FLOW_VECTORS

#define MAX_CORNERS 500

const char* TrackingManager::spPlugInName("TrackingManager");

TrackingManager::TrackingManager() :
      mpDesc(NULL), mpElement(NULL), mBaseAcc(NULL, NULL), mpBaseCorners(new CvPoint2D32f[MAX_CORNERS]), mpGroup(NULL), mCornerCount(MAX_CORNERS), mpRes(NULL)
{
   mpAnimation.addSignal(SIGNAL_NAME(Animation, FrameChanged), Slot(this, &TrackingManager::processFrame));
   mpLayer.addSignal(SIGNAL_NAME(Subject, Deleted), Slot(this, &TrackingManager::clearData));
   setName(spPlugInName);
   setDescriptorId("{c5f096e1-1584-4d7c-aa6f-29c4e422aad1}");
   setType("Manager");
   setSubtype("Video");
   setAbortSupported(false);
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setWizardSupported(false);
   setHandle(ModuleManager::instance()->getService());
}

TrackingManager::~TrackingManager()
{
   mpBaseFrame.reset(NULL);
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
   mpBasePyramid.reset(NULL);
   mpBaseCorners.reset(NULL);
   mpBaseFrame.reset(NULL);
   mBaseAcc = DataAccessor(NULL, NULL);
   initializeFrame0();
}
#include "HighResolutionTimer.h"
#include "MessageLogResource.h"
void TrackingManager::processFrame(Subject& subject, const std::string& signal, const boost::any& val)
{
   VERIFYNRV(mpLayer.get());
   double tat = 0.0;
   try
   {
      HrTimer::Resource ta(&tat);
      unsigned int curFrame = mpAnimation->getCurrentFrame()->mFrameNumber;
      int width = mpDesc->getColumnCount();
      int height = mpDesc->getRowCount();

      FactoryResource<DataRequest> req;
      req->setBands(mpDesc->getActiveBand(curFrame), mpDesc->getActiveBand(curFrame), 1);
      req->setInterleaveFormat(BSQ);
      DataAccessor curAcc = mpElement->getDataAccessor(req.release());

      if (!curAcc.isValid())
      {
         return;
      }
      IplImageResource pCurFrame(width, height, 8, 1, reinterpret_cast<char*>(curAcc->getColumn()));
      CvSize pyr_sz = cvSize((*mpBaseFrame).width + 8, (*mpBaseFrame).height / 3);
      IplImageResource pCurPyramid(pyr_sz.width, pyr_sz.height, IPL_DEPTH_32F, 1);
      std::auto_ptr<CvPoint2D32f> pCurCorners(new CvPoint2D32f[MAX_CORNERS]);
      cvCalcOpticalFlowPyrLK(mpBaseFrame, pCurFrame, mpBasePyramid, pCurPyramid, mpBaseCorners.get(), pCurCorners.get(), mCornerCount,
         cvSize(10,10), 5, mpFeaturesFound, mpFeatureErrors, cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.3), 0);

      // display flow vectors in an annotation layer
      std::vector<std::pair<CvPoint2D32f, CvPoint2D32f> > corr;

      if (mpGroup != NULL)
      {
         mpGroup->removeAllObjects(true);
      }
      for (int i = 0; i < mCornerCount; ++i)
      {
         if (mpFeaturesFound[i] == 0 || mpFeatureErrors[i] > 550)
         {
            continue;
         }
         if (mpGroup != NULL)
         {
            GraphicObject* pObj = mpGroup->addObject(ARROW_OBJECT);
            pObj->setBoundingBox(LocationType(mpBaseCorners.get()[i].x, mpBaseCorners.get()[i].y),
               LocationType(pCurCorners.get()[i].x, pCurCorners.get()[i].y));
         }
#ifdef FORWARD_XFORM
         corr.push_back(std::make_pair(mpBaseCorners.get()[i], pCurCorners.get()[i]));
#else
         corr.push_back(std::make_pair(pCurCorners.get()[i], mpBaseCorners.get()[i]));
#endif
      }
      srand((unsigned int)time(NULL));
      CvMat* pMapMatrix = TrackingUtils::ransac_affine(corr, 15, 5.0f, 5);
      if (pMapMatrix != NULL)
      {
#ifdef FORWARD_XFORM
         IplImageResource pXform(width, height, 8, 1);
         IplImageResource pRes(width, height, 8, 1, reinterpret_cast<char*>(mpRes->getRawData()));
         cvWarpAffine(mpBaseFrame, pXform, pMapMatrix);
         cvSmooth(pXform, pRes, CV_MEDIAN, 5, 5);
         //IplImageResource pBlurred(width, height, 8, 1);
         cvSmooth(pCurFrame, pCurFrame, CV_MEDIAN, 5, 5);
         /*cvAbsDiff(pXform, pCurFrame, pXform);
         cvAdaptiveThreshold(pXform, pRes, 100, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY_INV);
         cvDilate(pRes, pXform, NULL, 2);
         cvErode(pXform, pBlurred, NULL, 4);
         cvDilate(pBlurred, pRes, NULL, 2);*/
         if (mpRes != NULL)
         {
            mpRes->updateData();
         }
#else
         IplImageResource pXform(width, height, 8, 1);
         cvWarpAffine(pCurFrame, pXform, pMapMatrix);
         cvSmooth(pXform, pCurFrame, CV_MEDIAN, 5, 5);
#endif
         cvReleaseMat(&pMapMatrix);
      }

      // prep for next frame
      mpBasePyramid = pCurPyramid;
      mpBaseCorners.reset(pCurCorners.release());
      mpBaseFrame = pCurFrame;
      mBaseAcc = curAcc;

      mCornerCount = MAX_CORNERS;
      cvGoodFeaturesToTrack(mpBaseFrame, mpEigImage, mpTmpImage, mpBaseCorners.get(), &mCornerCount, 0.01, 5.0);
      cvFindCornerSubPix(mpBaseFrame, mpBaseCorners.get(), mCornerCount, cvSize(10, 10), cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03));

   }
   catch (cv::Exception& err)
   {
      int tmp = err.code;
   }
   MessageResource msg("Time to process frame", "coan", "tracking");
   msg->addProperty("Time (ms)", tat);
}

void TrackingManager::clearData(Subject& subject, const std::string& signal, const boost::any& val)
{
   RasterLayer* pLayer = dynamic_cast<RasterLayer*>(&subject);
   if (pLayer == mpLayer.get())
   {
      setTrackedLayer(NULL);
   }
}

void TrackingManager::initializeFrame0()
{
   if (mpLayer.get() == NULL)
   {
      mpElement = NULL;
      mpDesc = NULL;
      return;
   }
   try
   {
      VERIFYNRV(mpElement = static_cast<RasterElement*>(mpLayer->getDataElement()));
      VERIFYNRV(mpDesc = static_cast<RasterDataDescriptor*>(mpElement->getDataDescriptor()));
      unsigned int baseFrame = mpLayer->getDisplayedBand(GRAY).getActiveNumber();

      int width = mpDesc->getColumnCount();
      int height = mpDesc->getRowCount();

      FactoryResource<DataRequest> baseRequest;
      baseRequest->setBands(mpDesc->getActiveBand(baseFrame), mpDesc->getActiveBand(baseFrame), 1);
      baseRequest->setInterleaveFormat(BSQ);
      mBaseAcc = mpElement->getDataAccessor(baseRequest.release());

      if (!mBaseAcc.isValid())
      {
         return;
      }
      // Wrap the accessors with OpenCV data structures.
      mpBaseFrame = IplImageResource(width, height, 8, 1, reinterpret_cast<char*>(mBaseAcc->getColumn()));

      mpEigImage = IplImageResource(width, height, IPL_DEPTH_32F, 1);
      mpTmpImage = IplImageResource(width, height, IPL_DEPTH_32F, 1);
      mCornerCount = MAX_CORNERS;
      mpBaseCorners.reset(new CvPoint2D32f[MAX_CORNERS]);
      cvGoodFeaturesToTrack(mpBaseFrame, mpEigImage, mpTmpImage, mpBaseCorners.get(), &mCornerCount, 0.01, 5.0);
      cvFindCornerSubPix(mpBaseFrame, mpBaseCorners.get(), mCornerCount, cvSize(10, 10), cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03));

      CvSize pyr_sz = cvSize((*mpBaseFrame).width + 8, (*mpBaseFrame).height / 3);
      mpBasePyramid = IplImageResource(pyr_sz.width, pyr_sz.height, IPL_DEPTH_32F, 1);

      mpGroup = NULL;
#ifdef SHOW_FLOW_VECTORS
      ModelResource<AnnotationElement> pAnno(static_cast<AnnotationElement*>(
         Service<ModelServices>()->getElement("Flow Vectors", TypeConverter::toString<AnnotationElement>(), mpElement)));
      bool created = pAnno.get() == NULL;
      if (created)
      {
         pAnno = ModelResource<AnnotationElement>("Flow Vectors", mpElement);
         static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(ANNOTATION, pAnno.get());
      }
      mpGroup = pAnno->getGroup();
      pAnno.release();
#endif

      mpRes = NULL;
#ifdef FORWARD_XFORM
      RasterElementArgs args={mpDesc->getRowCount(), mpDesc->getColumnCount(), 1, 0, 1, 1, mpElement, 0, NULL};
      mpRes = static_cast<RasterElement*>(createRasterElement("Temp", args));
      static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(RASTER, mpRes);
#endif
   }
   catch (cv::Exception&) {}
}