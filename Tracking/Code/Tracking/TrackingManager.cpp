/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "ApiUtilities.h"
#include "PlugInRegistration.h"
#include "RasterDataDescriptor.h"
#include "RasterData.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SpatialDataView.h"
#include "TrackingManager.h"
#include "TrackingUtils.h"

#include "imgfeatures.h"
#include "kdtree.h"
#include "sift.h"
#include "xform.h"

REGISTER_PLUGIN_BASIC(Tracking, TrackingManager);

/* the maximum number of keypoint nearest neighbor candidates to check during BBF search */
#define KDTREE_BBF_MAX_NN_CHKS 200

/* threshold on squared ratio of distances between two nearest neighbors */
#define NN_SQ_DIST_RATIO_THR 0.49

const char* TrackingManager::spPlugInName("TrackingManager");

TrackingManager::TrackingManager() :
      mpDesc(NULL), mpElement(NULL)
{
   mpAnimation.addSignal(SIGNAL_NAME(Animation, FrameChanged), Slot(this, &TrackingManager::processFrame));
   setName(spPlugInName);
   setDescriptorId("{c5f096e1-1584-4d7c-aa6f-29c4e422aad1}");
   setType("Manager");
   setSubtype("Video");
   setAbortSupported(false);
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setWizardSupported(false);

   setHandle(ModuleManager::instance()->getService()); // initialize the SimpleAPI
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
#include <opencv/highgui.h>
void TrackingManager::processFrame(Subject& subject, const std::string& signal, const boost::any& val)
{
   VERIFYNRV(mpLayer.get());

   mDpargs.bandStart = mDpargs.bandEnd = mpAnimation->getCurrentFrame()->mFrameNumber;
   dataptr pDataPtr(mpElement, mDpargs);
   if (pDataPtr.get() == NULL)
   {
      return;
   }
   IplImageResource pFrame(mDpargs.columnEnd - mDpargs.columnStart + 1,
      mDpargs.rowEnd - mDpargs.rowStart + 1, mpDesc->getBytesPerElement() * 8, 1, pDataPtr);
   ca_ptr<struct feature> pFeatures;
   int numFeatures = sift_features(pFrame, &pFeatures);

   // Match features between the two frames
   int numMatches = 0;
   for (int idx = 0; idx < numFeatures; ++idx)
   {
      ca_ptr<struct feature*> pNbrs;
      // best bin first search of the KD tree to locate 2 nearest features
      int k = kdtree_bbf_knn(mpKdRoot, &(pFeatures[idx]), 2, &pNbrs, KDTREE_BBF_MAX_NN_CHKS);
      if (k == 2) // sanity check....ensure we found 2 features
      {
         // calculate Euclidean distance to the features and use as a filter for a good match
         double dist0 = descr_dist_sq(&(pFeatures[idx]), pNbrs[0]);
         double dist1 = descr_dist_sq(&(pFeatures[idx]), pNbrs[1]);
         if (dist0 < dist1 * NN_SQ_DIST_RATIO_THR)
         {
            // update data structure to indicate a matched feature
            pFeatures[idx].fwd_match = pNbrs[0];
            numMatches++;
         }
      }
   }
   if (numMatches == 0)
   {
      return;
   }
   CvMat* pXform = ransac_xform(pFeatures, numFeatures, FEATURE_FWD_MATCH,
      lsq_homog, 6, 0.1, homog_xfer_err, 10.0, NULL, NULL);
   if (pXform != NULL)
   {
      IplImageResource pXformed((*mpBaseFrame).width, (*mpBaseFrame).height, IPL_DEPTH_8U, 1);
      cvWarpPerspective(pFrame, pXformed, pXform, CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
      CvSize sz = cvGetSize(pXformed);
      cvReleaseMat(&pXform);

      cvNamedWindow("foo");
      cvShowImage("foo", pXformed);
      /*for (int i = 0; i < (*mpBaseFrame).imageSize; ++i)
      {
         (*pXformed).imageData[i] -= (*mpBaseFrame).imageData[i];
      }*/
      copyDataToRasterElement(mpTemp, NULL, (*pXformed).imageData);
      updateRasterElement(mpTemp);

      /*mpBaseFrame = pFrame;
      mpBaseFeatures.reset(pFeatures.release());
      mpKdRoot = kdtree_build(mpBaseFeatures, numFeatures);*/
   }
}

void TrackingManager::initializeFrame0()
{
   if (mpLayer.get() == NULL)
   {
      return;
   }
   VERIFYNRV(mpElement = static_cast<RasterElement*>(mpLayer->getDataElement()));
   VERIFYNRV(mpDesc = static_cast<RasterDataDescriptor*>(mpElement->getDataDescriptor()));
   LocationType center = mpLayer->getView()->getVisibleCenter();
   Opticks::PixelLocation maxBb(mpDesc->getColumnCount() - 1, mpDesc->getRowCount() - 1);
   uint8_t levels = TrackingUtils::calculateNeededLevels(TrackingManager::getSettingInitialSubcubeSize(), maxBb);
   if (levels == 0xff)
   {
      return;
   }
   TrackingUtils::subcubeid_t id = TrackingUtils::calculateSubcubeId(center, levels, maxBb);
   Opticks::PixelLocation minBb;
   TrackingUtils::calculateSubcubeBounds(id, levels, maxBb, minBb);

   mDpargs.bandStart = mDpargs.bandEnd = mpAnimation->getCurrentFrame()->mFrameNumber;
   mDpargs.rowStart = minBb.mY;
   mDpargs.rowEnd = maxBb.mY;
   mDpargs.columnStart = minBb.mX;
   mDpargs.columnEnd = maxBb.mX;
   mDpargs.interleaveFormat = 0; // BSQ
   dataptr pBaseDataPtr = dataptr(mpElement, mDpargs);
   if (pBaseDataPtr.get() == NULL)
   {
      return;
   }

   mpBaseFrame = IplImageResource(mDpargs.columnEnd - mDpargs.columnStart + 1,
      mDpargs.rowEnd - mDpargs.rowStart + 1, mpDesc->getBytesPerElement() * 8, 1);
   memcpy((*mpBaseFrame).imageData, pBaseDataPtr, (*mpBaseFrame).imageSize);
   int numFeatures = sift_features(mpBaseFrame, &mpBaseFeatures);
   mpKdRoot = kdtree_build(mpBaseFeatures, numFeatures);

   RasterElementArgs args;
   args.numRows = (mpBaseFrame.get())->height;
   args.numColumns = (mpBaseFrame.get())->width;
   args.numBands = 1;
   args.interleaveFormat = 0;
   args.encodingType = 0;
   args.location = 0;
   args.numBadValues = 0;
   args.pParent = mpElement;
   mpTemp = createRasterElement("Temp", args);
   RasterLayer* pTempLayer = static_cast<RasterLayer*>(static_cast<SpatialDataView*>(mpLayer->getView())->createLayer(RASTER, mpTemp));
   pTempLayer->setXOffset(mDpargs.columnStart);
   pTempLayer->setYOffset(mDpargs.rowStart);
}