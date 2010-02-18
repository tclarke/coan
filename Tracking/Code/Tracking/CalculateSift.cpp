/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "CalculateSift.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "PlugInArgList.h"
#include "PlugInManagerServices.h"
#include "PlugInRegistration.h"
#include "ProgressTracker.h"
#include "RasterElement.h"
#include "RasterDataDescriptor.h"
#include "RasterLayer.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "StringUtilities.h"

#include "imgfeatures.h"
#include "kdtree.h"
#include "sift.h"
#include "xform.h"

#include <opencv/cv.h>
#include <stdarg.h>

REGISTER_PLUGIN_BASIC(Tracking, CalculateSift);

Progress* spProgress(NULL);

extern "C" void fatal_error(char* format, ...)
{
   if (spProgress == NULL)
   {
      return;
   }
   va_list ap;
   char err[256];
   va_start(ap, format);
   vsnprintf(err, 255, format, ap);
   va_end(ap);
   spProgress->updateProgress(std::string(err), 0, ERRORS);
}

template<typename T>
class ca_ptr
{
public:
   ca_ptr() : mpData(NULL) {}

   ca_ptr(T* pData) : mpData(pData) {}

   ~ca_ptr()
   {
      if (mpData != NULL)
      {
         free(mpData);
      }
   }

   T* get()
   {
      return mpData;
   }

   T* release()
   {
      T* pData = mpData;
      reset(NULL);
      return pData;
   }

   void reset(T* pData)
   {
      release();
      mpData = pData;
   }

   operator T*()
   {
      return get();
   }

   T** operator&()
   {
      return &mpData;
   }

private:
   T* mpData;
};

class IplImageResource
{
public:
   IplImageResource() : mpImage(NULL), mShallow(false) {}

   IplImageResource(IplImage* pImage) : mpImage(pImage), mShallow(false) {}

   IplImageResource(IplImageResource& other)
   {
      reset(other.get());
      if (other.isShallow())
      {
         release();
      }
      else
      {
         take();
      }
      other.mpImage = NULL;
   }

   IplImageResource(int width, int height, int depth, int channels) : mShallow(false)
   {
      mpImage = cvCreateImage(cvSize(width, height), depth, channels);
   }

   IplImageResource(int width, int height, int depth, int channels, char* pData) : mShallow(true)
   {
      mpImage = cvCreateImageHeader(cvSize(width, height), depth, channels);
      mpImage->imageData = pData;
   }

   ~IplImageResource()
   {
      reset(NULL);
   }

   bool isShallow()
   {
      return mShallow;
   }

   void reset(IplImage* pImage)
   {
      if (mpImage != NULL)
      {
         if (mShallow)
         {
            mpImage->imageData = NULL;
         }
         cvReleaseImage(&mpImage);
      }
      mpImage = pImage;
      mShallow = false;
   }

   IplImage* get()
   {
      return mpImage;
   }

   IplImage* release()
   {
      mShallow = true;
      return mpImage;
   }

   IplImage* take()
   {
      mShallow = false;
      return mpImage;
   }

   operator IplImage*()
   {
      return get();
   }

   IplImage& operator*()
   {
      return *get();
   }

   IplImageResource& operator=(IplImageResource& other)
   {
      reset(other.get());
      if (other.isShallow())
      {
         release();
      }
      else
      {
         take();
      }
      other.mpImage = NULL;
      return *this;
   }

private:
   IplImage* mpImage;
   bool mShallow;
};

CalculateSift::CalculateSift()
{
   setName("CalculateSift");
   setDescription("Calculate correspondence between two frames using SIFT and a KD tree to filter the results.");
   setDescriptorId("{e0bdb3c7-73b2-46d8-895d-f590fe51a55a}");
   setMenuLocation("[Tracking]/SIFT");
   setType("Algorithm");
   setSubtype("Video");
   setAbortSupported(true);
}

CalculateSift::~CalculateSift()
{
}

bool CalculateSift::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(ProgressArg()));
   VERIFY(pArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pArgList->addArg<unsigned int>("Base Frame", 0, "The frame (active number) which is used as the basis for comparison."));
   VERIFY(pArgList->addArg<unsigned int>("Current Frame", 1, "The frame (active number) which is being compared to 'Base Frame'."));
   VERIFY(pArgList->addArg<unsigned int>("Start Row", "Optional sub-cube specification for the frames. (active numbers)"));
   VERIFY(pArgList->addArg<unsigned int>("End Row", "Optional sub-cube specification for the frames. (active numbers)"));
   VERIFY(pArgList->addArg<unsigned int>("Start Column", "Optional sub-cube specification for the frames. (active numbers)"));
   VERIFY(pArgList->addArg<unsigned int>("End Column", "Optional sub-cube specification for the frames. (active numbers)"));
   return true;
}

bool CalculateSift::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

/* the maximum number of keypoint nearest neighbor candidates to check during BBF search */
#define KDTREE_BBF_MAX_NN_CHKS 200

/* threshold on squared ratio of distances between two nearest neighbors */
#define NN_SQ_DIST_RATIO_THR 0.49

bool CalculateSift::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Calculating optical flow", "thesis", "optical_flow_1");
   spProgress = progress.getCurrentProgress();
   SpatialDataView* pView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg());
   if (!pElement)
   {
      progress.report("Invalid raster element", 0, ERRORS, true);
      return false;
   }
   unsigned int baseFrame=0, currentFrame=0;
   if (!pInArgList->getPlugInArgValue("Base Frame", baseFrame) ||
       !pInArgList->getPlugInArgValue("Current Frame", currentFrame))
   {
      progress.report("Invalid frame specification", 0, ERRORS, true);
      return false;
   }

   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (pDesc->getDataType() != INT1UBYTE && pDesc->getDataType() != INT1SBYTE)
   {
      progress.report("Invalid data type, only 1-byte data is currently supported.", 0, ERRORS, true);
      return false;
   }
   unsigned int startRow(0), endRow(pDesc->getRowCount() - 1), startCol(0), endCol(pDesc->getColumnCount() - 1);
   bool hasSubcube = pInArgList->getPlugInArgValue("Start Row", startRow);
   hasSubcube = pInArgList->getPlugInArgValue("End Row", endRow) || hasSubcube;
   hasSubcube = pInArgList->getPlugInArgValue("Start Column", startCol) || hasSubcube;
   hasSubcube = pInArgList->getPlugInArgValue("End Column", endCol) || hasSubcube;
   try
   {
      // Get acccessors for the two frames
      int width = pDesc->getColumnCount();
      int height = pDesc->getRowCount();
      if (hasSubcube)
      {
         width = endCol - startCol + 1;
         height = endRow - startRow + 1;
      }

      FactoryResource<DataRequest> baseRequest;
      baseRequest->setBands(pDesc->getActiveBand(baseFrame), pDesc->getActiveBand(baseFrame), 1);
      baseRequest->setInterleaveFormat(BSQ);
      if (hasSubcube)
      {
         baseRequest->setRows(pDesc->getActiveRow(startRow), pDesc->getActiveRow(endRow));
         baseRequest->setColumns(pDesc->getActiveColumn(startCol), pDesc->getActiveRow(endCol));
      }
      DataAccessor baseAcc(pElement->getDataAccessor(baseRequest.release()));

      FactoryResource<DataRequest> currentRequest;
      currentRequest->setBands(pDesc->getActiveBand(currentFrame), pDesc->getActiveBand(currentFrame), 1);
      currentRequest->setInterleaveFormat(BSQ);
      if (hasSubcube)
      {
         currentRequest->setRows(pDesc->getActiveRow(startRow), pDesc->getActiveRow(endRow));
         currentRequest->setColumns(pDesc->getActiveColumn(startCol), pDesc->getActiveRow(endCol));
      }
      DataAccessor currentAcc(pElement->getDataAccessor(currentRequest.release()));

      if (!baseAcc.isValid() || !currentAcc.isValid())
      {
         progress.report("Invalid accessors.", 0, ERRORS, true);
         return false;
      }
      // Wrap the accessors with OpenCV data structures.
      IplImageResource pBaseFrame;
      IplImageResource pCurrentFrame;
      if (hasSubcube)
      {
         // subcube so we make a copy
         pBaseFrame = IplImageResource(width, height, 8, 1);
         pCurrentFrame = IplImageResource(width, height, 8, 1);
         for (unsigned int row = startRow; row <= endRow; ++row)
         {
            VERIFY(baseAcc.isValid() && currentAcc.isValid());
            memcpy((*pBaseFrame).imageData + ((*pBaseFrame).widthStep * (row - startRow)),
                   baseAcc->getRow(), (*pBaseFrame).widthStep);
            memcpy((*pCurrentFrame).imageData + ((*pCurrentFrame).widthStep * (row - startRow)),
                   currentAcc->getRow(), (*pCurrentFrame).widthStep);
            baseAcc->nextRow();
            currentAcc->nextRow();
         }
      }
      else
      {
         // entire frame, we don't need to make a copy
         pBaseFrame = IplImageResource(width, height, 8, 1, reinterpret_cast<char*>(baseAcc->getColumn()));;
         pCurrentFrame = IplImageResource(width, height, 8, 1, reinterpret_cast<char*>(currentAcc->getColumn()));;
      }

      // Calculate strong features in each frame using SIFT
      if (isAborted())
      {
         progress.report("User aborted.", 100, ABORT, true);
         return false;
      }
      progress.report("Locating SIFT features in base frame.", 20, NORMAL);
      ca_ptr<struct feature> pBaseFeatures;
      int numBaseFeatures = sift_features(pBaseFrame, &pBaseFeatures);
      if (isAborted())
      {
         progress.report("User aborted.", 100, ABORT, true);
         return false;
      }
      progress.report("Locating SIFT features in current frame.", 40, NORMAL);
      ca_ptr<struct feature> pCurrentFeatures;
      int numCurrentFeatures = sift_features(pCurrentFrame, &pCurrentFeatures);

      // Generate a KD tree of the features in the base frame for quick indexing
      if (isAborted())
      {
         progress.report("User aborted.", 100, ABORT, true);
         return false;
      }
      progress.report("Building KD tree.", 60, NORMAL);
      struct kd_node* pKdRoot = kdtree_build(pBaseFeatures, numBaseFeatures);

      // Match features between the two frames
      int numMatches = 0;
      for (int idx = 0; idx < numCurrentFeatures; ++idx)
      {
         if (idx % 20 == 0)
         {
            if (isAborted())
            {
               progress.report("User aborted.", 100, ABORT, true);
               return false;
            }
            progress.report("Matching features.", 80 * idx / numCurrentFeatures, NORMAL);
         }
         ca_ptr<struct feature*> pNbrs;
         // best bin first search of the KD tree to locate 2 nearest features
         int k = kdtree_bbf_knn(pKdRoot, &(pCurrentFeatures[idx]), 2, &pNbrs, KDTREE_BBF_MAX_NN_CHKS);
         if (k == 2) // sanity check....ensure we found 2 features
         {
            // calculate Euclidean distance to the features and use as a filter for a good match
            double dist0 = descr_dist_sq(&(pCurrentFeatures[idx]), pNbrs[0]);
            double dist1 = descr_dist_sq(&(pCurrentFeatures[idx]), pNbrs[1]);
            if (dist0 < dist1 * NN_SQ_DIST_RATIO_THR)
            {
               // update data structure to indicate a matched feature
               pCurrentFeatures[idx].fwd_match = pNbrs[0];
               numMatches++;
            }
         }
      }
      if (numMatches < 10) // minumum required matches for calculating of affine transform
      {
         progress.report("Did not locate enough SIFT matches.", 0 ,ERRORS, true);
         return false;
      }
      progress.report("Found " + StringUtilities::toDisplayString(numMatches) + " matches.", 80, WARNING);

      CvMat* pXform = ransac_xform(pCurrentFeatures, numCurrentFeatures, FEATURE_FWD_MATCH,
                                   lsq_homog, 6, 0.1, homog_xfer_err, 10.0, NULL, NULL);
      if (pXform != NULL)
      {
         progress.report("Warping current frame.", 85, NORMAL);
         {
            std::string st = "Transform:\n";
            cv::Mat m(pXform);
            for (int y = 0; y < 3; y++)
            {
               for (int x = 0; x < 3; x++)
               {
                  st += "   " + StringUtilities::toDisplayString(m.at<double>(y, x));
               }
               st += "\n";
            }
            progress.report(st, 86, WARNING, false);
         }
         IplImageResource pXformed;
         if (hasSubcube)
         {
            FactoryResource<DataRequest> fullBaseRequest;
            fullBaseRequest->setBands(pDesc->getActiveBand(baseFrame), pDesc->getActiveBand(baseFrame), 1);
            fullBaseRequest->setInterleaveFormat(BSQ);
            DataAccessor fullBaseAcc(pElement->getDataAccessor(fullBaseRequest.release()));
            VERIFY(fullBaseAcc.isValid());
            IplImageResource pFullBase(pDesc->getColumnCount(), pDesc->getRowCount(), IPL_DEPTH_8U, 1, reinterpret_cast<char*>(fullBaseAcc->getRow()));

            FactoryResource<DataRequest> fullCurrentRequest;
            fullCurrentRequest->setBands(pDesc->getActiveBand(currentFrame), pDesc->getActiveBand(currentFrame), 1);
            fullCurrentRequest->setInterleaveFormat(BSQ);
            DataAccessor fullCurrentAcc(pElement->getDataAccessor(fullCurrentRequest.release()));
            VERIFY(fullCurrentAcc.isValid());
            IplImageResource pFullCurrent(pDesc->getColumnCount(), pDesc->getRowCount(), IPL_DEPTH_8U, 1, reinterpret_cast<char*>(fullCurrentAcc->getRow()));

            pXformed = IplImageResource((*pFullBase).width, (*pFullBase).height, IPL_DEPTH_8U, 1);
            cvWarpPerspective(pFullCurrent, pXformed, pXform, CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
         }
         else
         {
            pXformed = IplImageResource((*pBaseFrame).width, (*pBaseFrame).height, IPL_DEPTH_8U, 1);
            cvWarpPerspective(pCurrentFrame, pXformed, pXform, CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
         }
         CvSize sz = cvGetSize(pXformed);
         ModelResource<RasterElement> pOut(RasterUtilities::createRasterElement("Transformed", sz.height, sz.width, INT1UBYTE, true, pElement));
         memcpy(pOut->getRawData(), (*pXformed).imageData, (*pXformed).imageSize);
         static_cast<RasterDataDescriptor*>(pOut->getDataDescriptor())->setBadValues(std::vector<int>(1, 42));
         pView->createLayer(RASTER, pOut.release());
         cvReleaseMat(&pXform);
      }
      else
      {
         progress.report("Unable to transform data.", 0, WARNING, true);
      }
   }
   catch (cv::Exception& err)
   {
      // an error occured in opencv
      progress.report(err.err, 0, ERRORS, true);
      return false;
   }

   progress.report("Finished", 100, NORMAL);
   progress.upALevel();
   return true;
}
