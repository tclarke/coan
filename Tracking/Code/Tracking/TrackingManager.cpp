/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnnotationElement.h"
#include "AoiElement.h"
#include "AoiLayer.h"
#include "ApiUtilities.h"
#include "ColorMap.h"
#include "ColorType.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "GraphicGroup.h"
#include "GraphicObject.h"
#include "LayerList.h"
#include "MessageLogResource.h"
#include "PlugInRegistration.h"
#include "PseudocolorLayer.h"
#include "RasterDataDescriptor.h"
#include "RasterData.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SpatialDataView.h"
#include "Statistics.h"
#include "StringUtilities.h"
#include "ThresholdLayer.h"
#include "TrackingManager.h"
#include "TrackingUtils.h"
#include <time.h>
#include <opencv/cv.h>
#include <BlobResult.h>
#include <vector>
#include "HighResolutionTimer.h"

REGISTER_PLUGIN_BASIC(Tracking, TrackingManager);

// Define this to generate an annotation layer showing the optical flow vectors
// This will slow down processing quite a bit.
//#define SHOW_FLOW_VECTORS

// Define this to calculate connected components on the object results
#define CONNECTED

// Define the threshold for locating objects
#define OBJECT_THRESHOLD 15

// Maximum number of corners to use when calculating optical flow.
// Higher numbers may result in more accurate calculations but may also slow down calculations.
// There's a point where increasing this number does nothing as there are only so many strong corners in a frame.
#define MAX_CORNERS 500

const char* TrackingManager::spPlugInName("TrackingManager");

TrackingManager::TrackingManager() :
      mPaused(false),
      mpDesc(NULL),
      mpElement(NULL),
      mBaseAcc(NULL, NULL),
      mpBaseCorners(new CvPoint2D32f[MAX_CORNERS]),
      mpGroup(NULL),
      mCornerCount(MAX_CORNERS),
      mpRes(NULL),
      mpRes2(NULL)
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
   initializeDataset();
}

void TrackingManager::setPauseState(bool state)
{
   mPaused = state;
}

void TrackingManager::setFocus(LocationType loc, int maxSize)
{
   if (mpDesc == NULL)
   {
      return;
   }
   LocationType dloc;
   mpLayer->translateScreenToData(loc.mX, loc.mY, dloc.mX, dloc.mY);
   Opticks::PixelLocation minBb(0,0);
   Opticks::PixelLocation maxBb(mpDesc->getColumnCount(), mpDesc->getRowCount());
   uint8_t level = TrackingUtils::calculateNeededLevels(maxSize, maxBb, minBb);
   TrackingUtils::subcubeid_t id = TrackingUtils::calculateSubcubeId(dloc, level, maxBb, minBb);
   TrackingUtils::calculateSubcubeBounds(id, level, maxBb, minBb);
   mpFocus->clearPoints();
   GraphicObject* pRect = mpFocus->getGroup()->addObject(RECTANGLE_OBJECT);
   pRect->setBoundingBox(LocationType(minBb.mX, minBb.mY), LocationType(maxBb.mX, maxBb.mY));
   pRect->setFillState(false);
   mMinBb = minBb;
   mMaxBb = maxBb;
   mMaxBb.mX--;
   mMaxBb.mY--;
   initializeFrame0();
}

void TrackingManager::processFrame(Subject& subject, const std::string& signal, const boost::any& val)
{
   if (mPaused)
   {
      return;
   }
   VERIFYNRV(mpLayer.get());
   try
   {
      unsigned int curFrame = mpAnimation->getCurrentFrame()->mFrameNumber;
      int width = (*mpBaseFrame).width;
      int height = (*mpBaseFrame).height;
      bool fullScene = (width == mpDesc->getColumnCount() && height == mpDesc->getRowCount());

      FactoryResource<DataRequest> req;
      if (!fullScene)
      {
         req->setRows(mpDesc->getActiveRow(mMinBb.mY), mpDesc->getActiveRow(mMaxBb.mY), height);
         req->setColumns(mpDesc->getActiveColumn(mMinBb.mX), mpDesc->getActiveColumn(mMaxBb.mX), width);
      }
      req->setBands(mpDesc->getActiveBand(curFrame), mpDesc->getActiveBand(curFrame), 1);
      req->setInterleaveFormat(BSQ);
      DataAccessor curAcc = mpElement->getDataAccessor(req.release());

      if (!curAcc.isValid())
      {
         return;
      }
      IplImageResource pCurFrame(NULL);
      if (fullScene)
      {
         pCurFrame = IplImageResource(width, height, 8, 1, reinterpret_cast<char*>(curAcc->getColumn()));
      }
      else
      {
         pCurFrame = IplImageResource(width, height, 8, 1);
         for (int row = 0; row < height; ++row)
         {
            if (row > 0)
            {
               curAcc->nextRow();
               VERIFYNRV(curAcc.isValid());
            }
            memcpy((*pCurFrame).imageData + (row * width), curAcc->getRow(), (*pCurFrame).width);
         }
      }
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
         corr.push_back(std::make_pair(mpBaseCorners.get()[i], pCurCorners.get()[i]));
      }
      srand((unsigned int)time(NULL));
      CvMat* pMapMatrix = TrackingUtils::ransac_affine(corr, 15, 5.0f, 5);
      if (pMapMatrix != NULL)
      {
         IplImageResource pXform(width, height, 8, 1);
         IplImageResource pTemp(width, height, 8, 1);
         IplImageResource pRes(width, height, 8, 1, reinterpret_cast<char*>(mpRes->getRawData()));
         IplImageResource pRes2(width, height, 8, 1, reinterpret_cast<char*>(mpRes2->getRawData()));
         cvCopy(pCurFrame, pXform); // initialize to the current frame so any "offsets" have a consistent background
         cvWarpAffine(mpBaseFrame, pXform, pMapMatrix, CV_INTER_LINEAR); // warp the base frame to the new position
         cvCopy(pXform, mpBaseFrame);
         /*double t1v(0.0),t2v(0.0);
         {HrTimer::Resource t1(&t1v);
         {HrTimer::Resource t2(&t2v);*/
         cvSmooth(pXform, pRes, CV_MEDIAN, 5, 5);
         cvSmooth(pCurFrame, pTemp, CV_MEDIAN, 5, 5);

         // threhold the results
         cvThreshold(pCurFrame, pTemp, OBJECT_THRESHOLD, 255, CV_THRESH_BINARY);
         cvThreshold(pXform, pRes, OBJECT_THRESHOLD, 255, CV_THRESH_BINARY);
         // erode to remove some noise
         cvErode(pTemp, pTemp);
         cvErode(pRes,pRes);
         // difference the frames
         cvSub(pTemp, pRes, pRes2);
         cvSub(pRes, pTemp, pRes);
         // remove final small differences with an open
         cvErode(pRes,pRes, NULL, 3);
         cvDilate(pRes,pRes,NULL,3);
         cvErode(pRes2,pRes2, NULL, 3);
         cvDilate(pRes2,pRes2,NULL,3);
         /*}*/
#ifdef CONNECTED
         {
         CBlobResult blobs(pRes, NULL, 0);
         for (int bidx = 0; bidx < blobs.GetNumBlobs(); ++bidx)
         {
            CBlob blob(blobs.GetBlob(bidx));
            blob.FillBlob(pRes, CV_RGB(bidx+1,bidx+1,bidx+1));
         }
         }
         {
         CBlobResult blobs(pRes2, NULL, 0);
         for (int bidx = 0; bidx < blobs.GetNumBlobs(); ++bidx)
         {
            CBlob blob(blobs.GetBlob(bidx));
            blob.FillBlob(pRes2, CV_RGB(bidx+1,bidx+1,bidx+1));
         }
         }
         /*}
         MessageResource msg("Timer Results", "timer", "timer");
         msg->addProperty("Total", t1v);
         msg->addProperty("Diff", t2v);*/
#endif
         if (!fullScene)
         {
            mBaseAcc->toPixel(mMinBb.mY, mMinBb.mX);
            VERIFYNRV(mBaseAcc.isValid());
            for (int row = 0; row < height; ++row)
            {
               if (row > 0)
               {
                  mBaseAcc->nextRow();
                  VERIFYNRV(mBaseAcc.isValid());
               }
               memcpy(mBaseAcc->getRow(), (*mpBaseFrame).imageData + (row * width), (*mpBaseFrame).width);
            }
         }
         mpElement->updateData();
         if (mpRes != NULL)
         {
            mpRes->updateData();
         }
         if (mpRes2 != NULL)
         {
            mpRes2->updateData();
         }

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
}

void TrackingManager::clearData(Subject& subject, const std::string& signal, const boost::any& val)
{
   setTrackedLayer(NULL);
}

void TrackingManager::initializeDataset()
{
   if (mpLayer.get() == NULL)
   {
      mpElement = NULL;
      mpDesc = NULL;
      return;
   }
   VERIFYNRV(mpElement = static_cast<RasterElement*>(mpLayer->getDataElement()));
   VERIFYNRV(mpDesc = static_cast<RasterDataDescriptor*>(mpElement->getDataDescriptor()));
   if (mpDesc->getBytesPerElement() != 1)
   {
      // only 8-bit supported right now
      mpElement = NULL;
      mpDesc = NULL;
      return;
   }

   mpGroup = NULL;
#ifdef SHOW_FLOW_VECTORS
   // Create elements and views to show the optical flow vectors
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

   // Create an AOI to define a sub-area for processing.
   mpFocus = static_cast<AoiElement*>(
      Service<ModelServices>()->getElement("Focus", TypeConverter::toString<AoiElement>(), mpElement));
   if (mpFocus == NULL)
   {
      mpFocus = static_cast<AoiElement*>(
         Service<ModelServices>()->createElement("Focus", TypeConverter::toString<AoiElement>(), mpElement));
      AoiLayer* pLayer = static_cast<AoiLayer*>(
         static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(AOI_LAYER, mpFocus));
      pLayer->setSymbol(BOX);
      pLayer->setColor(ColorType(0, 0, 128));
   }
   mMinBb = Opticks::PixelLocation(0, 0);
   mMaxBb = Opticks::PixelLocation(mpDesc->getColumnCount()-1, mpDesc->getRowCount()-1);

   initializeFrame0();
}
#include <QtCore/QtDebug>
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
      unsigned int baseFrame = mpLayer->getDisplayedBand(GRAY).getActiveNumber();

      int width = mMaxBb.mX - mMinBb.mX + 1;
      int height = mMaxBb.mY - mMinBb.mY + 1;
      bool fullScene = (width == mpDesc->getColumnCount() && height == mpDesc->getRowCount());

      FactoryResource<DataRequest> baseRequest;
      if (!fullScene)
      {
         baseRequest->setRows(mpDesc->getActiveRow(mMinBb.mY), mpDesc->getActiveRow(mMaxBb.mY), height);
         baseRequest->setColumns(mpDesc->getActiveColumn(mMinBb.mX), mpDesc->getActiveColumn(mMaxBb.mX), width);
      }
      baseRequest->setBands(mpDesc->getActiveBand(baseFrame), mpDesc->getActiveBand(baseFrame), 1);
      baseRequest->setInterleaveFormat(BSQ);
      mBaseAcc = mpElement->getDataAccessor(baseRequest.release());

      if (!mBaseAcc.isValid())
      {
         return;
      }
      // Wrap the accessors with OpenCV data structures and create some temporary arrays.
      if (fullScene)
      {
         mpBaseFrame = IplImageResource(width, height, 8, 1, reinterpret_cast<char*>(mBaseAcc->getColumn()));
      }
      else
      {
         mpBaseFrame = IplImageResource(width, height, 8, 1);
         for (int row = 0; row < height; ++row)
         {
            if (row > 0)
            {
               mBaseAcc->nextRow();
               VERIFYNRV(mBaseAcc.isValid());
            }
            memcpy((*mpBaseFrame).imageData + (row * width), mBaseAcc->getRow(), (*mpBaseFrame).width);
         }
      }

      mpEigImage = IplImageResource(width, height, IPL_DEPTH_32F, 1);
      mpTmpImage = IplImageResource(width, height, IPL_DEPTH_32F, 1);
      mCornerCount = MAX_CORNERS;
      mpBaseCorners.reset(new CvPoint2D32f[MAX_CORNERS]);
      cvGoodFeaturesToTrack(mpBaseFrame, mpEigImage, mpTmpImage, mpBaseCorners.get(), &mCornerCount, 0.01, 5.0);
      cvFindCornerSubPix(mpBaseFrame, mpBaseCorners.get(), mCornerCount, cvSize(10, 10), cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03));

      CvSize pyr_sz = cvSize((*mpBaseFrame).width + 8, (*mpBaseFrame).height / 3);
      mpBasePyramid = IplImageResource(pyr_sz.width, pyr_sz.height, IPL_DEPTH_32F, 1);

      // Create elements and views to show identified objects
      mpRes = NULL;
      mpRes2 = NULL;

      RasterElementArgs args={height, width, 1, 0, 1, 1, mpElement, 0, NULL}; // BSQ, uchar (ushort)
      DataElement* pTmp = Service<ModelServices>()->getElement("Current Objects", TypeConverter::toString<RasterElement>(), mpElement);
      if (pTmp != NULL)
      {
         Service<ModelServices>()->destroyElement(pTmp);
      }
      mpRes = static_cast<RasterElement*>(createRasterElement("Current Objects", args));
#pragma message(__FILE__ "(" STRING(__LINE__) ") : warning : Work around for OPTICKS-932 (tclarke)")
      std::vector<int> badValues(1, 0);
      mpRes->getStatistics()->setBadValues(badValues);
#ifdef CONNECTED
      RasterLayer* pPseudo = static_cast<RasterLayer*>(
         static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(RASTER, mpRes));
      pPseudo->setXOffset(mMinBb.mX);
      pPseudo->setYOffset(mMinBb.mY);
      pTmp = Service<ModelServices>()->getElement("Base Objects", TypeConverter::toString<RasterElement>(), mpElement);
      if (pTmp != NULL)
      {
         Service<ModelServices>()->destroyElement(pTmp);
      }
      mpRes2 = static_cast<RasterElement*>(createRasterElement("Base Objects", args));
      mpRes2->getStatistics()->setBadValues(badValues);
      RasterLayer* pPseudo2 = static_cast<RasterLayer*>(
         static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(RASTER, mpRes2));
      pPseudo2->setXOffset(mMinBb.mX);
      pPseudo2->setYOffset(mMinBb.mY);
      pPseudo->setStretchUnits(GRAYSCALE_MODE, RAW_VALUE);
      pPseudo->setStretchValues(GRAY, 0, 47);
      pPseudo2->setStretchUnits(GRAYSCALE_MODE, RAW_VALUE);
      pPseudo2->setStretchValues(GRAY, 0, 47);
      ColorMap cmap("C:/Opticks/COAN/Tracking/Release/SupportFiles/ColorTables/pseudocolor.clu");
      pPseudo->setColorMap(cmap.getName(), cmap.getTable());
      pPseudo2->setColorMap(cmap.getName(), cmap.getTable());
#else
      ThresholdLayer* pThresh = static_cast<ThresholdLayer*>(
         static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(THRESHOLD, mpRes));
      pThresh->setXOffset(mMinBb.mX);
      pThresh->setYOffset(mMinBb.mY);
      pThresh->setColor(ColorType(128, 0, 0));
      pThresh->setSymbol(BOX);
      pThresh->setPassArea(UPPER);
      pThresh->setFirstThreshold(128);
      mpRes2 = static_cast<RasterElement*>(createRasterElement("Base Objects", args));
      ThresholdLayer* pThresh2 = static_cast<ThresholdLayer*>(
         static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(THRESHOLD, mpRes2));
      pThresh2->setXOffset(mMinBb.mX);
      pThresh2->setYOffset(mMinBb.mY);
      pThresh2->setColor(ColorType(0, 128, 0));
      pThresh2->setSymbol(BOX);
      pThresh2->setPassArea(UPPER);
      pThresh2->setFirstThreshold(128);
#endif
   }
   catch (const cv::Exception& exc)
   {
      MessageResource msg("OpenCV error occurred.", "tracking", "{b16ae5e9-ef7e-474f-9c45-8a061aa3cda2}");
      msg->addProperty("Code", exc.code);
      msg->addProperty("Error", exc.err);
      msg->addProperty("Function", exc.func);
      msg->addProperty("File", exc.file);
      msg->addProperty("Line", exc.line);
      msg->finalize(Message::Failure);
   }
}