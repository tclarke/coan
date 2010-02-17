#include "AnnotationElement.h"
#include "AnnotationLayer.h"
#include "AppVerify.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "GraphicGroup.h"
#include "GraphicObject.h"
#include "OpticalFlow.h"
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
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "sift.h"
#include "imgfeatures.h"
#include "kdtree.h"
#include "xform.h"
#include <stdarg.h>

REGISTER_PLUGIN_BASIC(Thesis, OpticalFlow);

Progress* pProgress(NULL);

extern "C" void fatal_error(char* format, ...)
{
   if (pProgress == NULL)
   {
      return;
   }
   va_list ap;
   char err[256];
   va_start(ap, format);
   vsnprintf(err, 255, format, ap);
   va_end(ap);
   pProgress->updateProgress(std::string(err), 0, ERRORS);
}

OpticalFlow::OpticalFlow()
{
   setName("OpticalFlow");
   setDescription("Calculate optical flow field.");
   setDescriptorId("{38c907e0-1a57-11df-8a39-0800200c9a66}");
   setMenuLocation("[Thesis]/Optical Flow");
   setType("Algorithm");
   setSubtype("Video");
}

OpticalFlow::~OpticalFlow()
{
}

bool OpticalFlow::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(ProgressArg()));
   VERIFY(pArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pArgList->addArg<unsigned int>("Base Frame", 0));
   VERIFY(pArgList->addArg<unsigned int>("Current Frame", 1));
   return true;
}

bool OpticalFlow::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool OpticalFlow::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   VERIFY(pInArgList);
   ProgressTracker progress(pInArgList->getPlugInArgValue<Progress>(ProgressArg()), "Calculating optical flow", "thesis", "optical_flow_1");
   SpatialDataView* pView = pInArgList->getPlugInArgValue<SpatialDataView>(ViewArg());
   RasterElement* pElement = pInArgList->getPlugInArgValue<RasterElement>(DataElementArg());
   if (!pElement)
   {
      progress.report("Invalid raster element", 0, ERRORS, true);
      return false;
   }
   unsigned int baseFrame=0, currentFrame=0;
   if (!pInArgList->getPlugInArgValue<unsigned int>("Base Frame", baseFrame) ||
       !pInArgList->getPlugInArgValue<unsigned int>("Current Frame", currentFrame))
   {
      progress.report("Invalid frame specification", 0, ERRORS, true);
      return false;
   }
   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (pDesc->getDataType() != INT1UBYTE)
   {
      progress.report("Invalid data type.", 0, ERRORS, true);
      return false;
   }
   int width = pDesc->getColumnCount();
   int height = pDesc->getRowCount();

   FactoryResource<DataRequest> baseRequest;
   baseRequest->setBands(pDesc->getActiveBand(baseFrame), pDesc->getActiveBand(baseFrame), 1);
   baseRequest->setInterleaveFormat(BSQ);
   DataAccessor baseAcc(pElement->getDataAccessor(baseRequest.release()));
   FactoryResource<DataRequest> currentRequest;
   currentRequest->setBands(pDesc->getActiveBand(currentFrame), pDesc->getActiveBand(currentFrame), 1);
   currentRequest->setInterleaveFormat(BSQ);
   DataAccessor currentAcc(pElement->getDataAccessor(currentRequest.release()));

   if (!baseAcc.isValid() || !currentAcc.isValid())
   {
      progress.report("Invalid accessors.", 0, ERRORS, true);
      return false;
   }
   try
   {
      CvSize sz = cvSize(width, height);
      IplImage* pBaseFrame = cvCreateImageHeader(sz, 8, 1);
      IplImage* pCurrentFrame = cvCreateImageHeader(sz, 8, 1);
      pBaseFrame->imageData = reinterpret_cast<char*>(baseAcc->getColumn());
      pCurrentFrame->imageData = reinterpret_cast<char*>(currentAcc->getColumn());

      progress.report("Locating SIFT features in base frame.", 20, NORMAL);
      struct feature* pBaseFeatures(NULL);
      int numBaseFeatures = sift_features(pBaseFrame, &pBaseFeatures);
      progress.report("Locating SIFT features in current frame.", 40, NORMAL);
      struct feature* pCurrentFeatures(NULL);
      int numCurrentFeatures = sift_features(pCurrentFrame, &pCurrentFeatures);

      progress.report("Building kD tree.", 60, NORMAL);
      struct kd_node* pKdRoot = kdtree_build(pCurrentFeatures, numCurrentFeatures);

      /*ModelResource<AnnotationElement> pAnno("Matches", pElement);
      GraphicGroup* pGrp = pAnno->getGroup();*/
      int m = 0;
      for (int idx = 0; idx < numBaseFeatures; ++idx)
      {
         if (idx % 20 == 0) progress.report("Matching features.", 80 * idx / numBaseFeatures, NORMAL);
         struct feature** pNbrs = NULL;
         int k = kdtree_bbf_knn(pKdRoot, &(pBaseFeatures[idx]), 2, &pNbrs, 200);
         if (k == 2)
         {
            double dist0 = descr_dist_sq(&(pBaseFeatures[idx]), pNbrs[0]);
            double dist1 = descr_dist_sq(&(pBaseFeatures[idx]), pNbrs[1]);
            if (dist0 < dist1 * 0.49)
            {
               /*LocationType pt1(pBaseFeatures[idx].x, pBaseFeatures[idx].y);
               LocationType pt2(pNbrs[0]->x, pNbrs[0]->y);
               GraphicObject* pLine = pGrp->addObject(LINE_OBJECT);
               pLine->setBoundingBox(pt1, pt2);*/
               pBaseFeatures[idx].fwd_match = pNbrs[0];
               m++;
            }
         }
         free(pNbrs);
      }
      progress.report("Found " + StringUtilities::toDisplayString(m) + " matches.", 80, WARNING);
      //pView->createLayer(ANNOTATION, pAnno.release());

      CvMat* pXform = ransac_xform(pBaseFeatures, numBaseFeatures, FEATURE_FWD_MATCH,
                                   lsq_homog, 6, 0.1, homog_xfer_err, 10.0, NULL, NULL);
      if (pXform != NULL)
      {
         IplImage* pXformed = cvCreateImage(cvGetSize(pCurrentFrame), IPL_DEPTH_8U, 1);
         cvWarpPerspective(pBaseFrame, pXformed, pXform, CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
         CvSize sz = cvGetSize(pXformed);
         ModelResource<RasterElement> pOut(RasterUtilities::createRasterElement("Transformed", sz.height, sz.width, INT1UBYTE, true, pElement));
         memcpy(pOut->getRawData(), pXformed->imageData, pXformed->imageSize);
         pView->createLayer(RASTER, pOut.release());
         cvReleaseImage(&pXformed);
         cvReleaseMat(&pXform);
      }
      else
      {
         progress.report("Unable to transform data.", 0, WARNING, true);
      }
      free(pCurrentFeatures);
      free(pBaseFeatures);
      pBaseFrame->imageData = pCurrentFrame->imageData = NULL;
      cvReleaseImage(&pBaseFrame);
      cvReleaseImage(&pCurrentFrame);
   }
   catch (cv::Exception& err)
   {
      progress.report(err.err, 0, ERRORS, true);
      return false;
   }

   progress.report("Finished", 100, NORMAL);
   progress.upALevel();
   return true;
}
