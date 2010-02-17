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
#include <opencv/cv.h>
#include <opencv/highgui.h>

REGISTER_PLUGIN_BASIC(Thesis, OpticalFlow);

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

      IplImage* pEig = cvCreateImage(sz, IPL_DEPTH_32F, 1);
      IplImage* pTemp = cvCreateImage(sz, IPL_DEPTH_32F, 1);

      int numBaseCorners(10);
      int numCurrentCorners(10);
      CvPoint2D32f* pBaseCorners(new CvPoint2D32f[numBaseCorners]);
      CvPoint2D32f* pCurrentCorners(new CvPoint2D32f[numCurrentCorners]);
      progress.report("Calculating features in base frame.", 10, NORMAL);
      cvGoodFeaturesToTrack(pBaseFrame, pEig, pTemp, pBaseCorners, &numBaseCorners, 0.8, 100.0);
      progress.report("Calculating features in current frame.", 60, NORMAL);
      cvGoodFeaturesToTrack(pCurrentFrame, pEig, pTemp, pCurrentCorners, &numCurrentCorners, 0.8, 100.0);

      progress.report("Adding features.", 90, NORMAL);
      { // scope
         ModelResource<AnnotationElement> pBaseAnno("Base Corners", pElement);
         GraphicGroup* pBaseGroup = pBaseAnno->getGroup();
         ModelResource<AnnotationElement> pCurrentAnno("Current Corners", pElement);
         GraphicGroup* pCurrentGroup = pCurrentAnno->getGroup();
         GraphicObject* pBasePoints = pBaseGroup->addObject(MULTIPOINT_OBJECT);
         GraphicObject* pCurrentPoints = pCurrentGroup->addObject(MULTIPOINT_OBJECT);
         std::vector<LocationType> basePoints;
         basePoints.reserve(numBaseCorners);
         for (int idx = 0; idx < numBaseCorners; ++idx)
         {
            basePoints.push_back(LocationType(pBaseCorners[idx].x, pBaseCorners[idx].y));
         }
         pBasePoints->addVertices(basePoints);
         std::vector<LocationType> currentPoints;
         currentPoints.reserve(numCurrentCorners);
         for (int idx = 0; idx < numCurrentCorners; ++idx)
         {
            currentPoints.push_back(LocationType(pCurrentCorners[idx].x, pCurrentCorners[idx].y));
         }
         pCurrentPoints->addVertices(currentPoints);
         pView->createLayer(ANNOTATION, pBaseAnno.release());
         pView->createLayer(ANNOTATION, pCurrentAnno.release());
      }
      delete pBaseCorners;
      delete pCurrentCorners;
      cvReleaseImage(&pEig);
      cvReleaseImage(&pTemp);
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
