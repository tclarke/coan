/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnnotationElement.h"
#include "AoiElement.h"
#include "AppVerify.h"
#include "BitMask.h"
#include "CalculateOpticalFlow.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "GraphicGroup.h"
#include "GraphicObject.h"
#include "LayerList.h"
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
#include "TrackingUtils.h"

#include <opencv/cv.h>
#include <stdarg.h>
#include <opencv/highgui.h>

REGISTER_PLUGIN_BASIC(Tracking, CalculateOpticalFlow);

CalculateOpticalFlow::CalculateOpticalFlow()
{
   setName("CalculateOpticalFlow");
   setDescription("Calculate correspondence between two frames using SIFT and a KD tree to filter the results.");
   setDescriptorId("{b1436248-cf81-40b2-bd5c-803e2ac75c82}");
   setMenuLocation("[Tracking]/Optical Flow");
   setType("Algorithm");
   setSubtype("Video");
   setAbortSupported(true);
}

CalculateOpticalFlow::~CalculateOpticalFlow()
{
}

bool CalculateOpticalFlow::getInputSpecification(PlugInArgList*& pArgList)
{
   VERIFY(pArgList = Service<PlugInManagerServices>()->getPlugInArgList());
   VERIFY(pArgList->addArg<Progress>(ProgressArg()));
   VERIFY(pArgList->addArg<SpatialDataView>(ViewArg(), NULL));
   VERIFY(pArgList->addArg<RasterElement>(DataElementArg()));
   VERIFY(pArgList->addArg<unsigned int>("Base Frame", "The frame (active number) which is used as the basis for comparison."));
   VERIFY(pArgList->addArg<unsigned int>("Current Frame", "The frame (active number) which is being compared to 'Base Frame'."));
   VERIFY(pArgList->addArg<AoiElement>("AOI", "Optional sub-cube specification for the frames. Only the bounding box will be used and inverted AOIs will be ignored."));
   return true;
}

bool CalculateOpticalFlow::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool CalculateOpticalFlow::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
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
   RasterLayer* pLayer = (pView == NULL) ? NULL : static_cast<RasterLayer*>(pView->getLayerList()->getLayer(RASTER, pElement));
   unsigned int baseFrame=0, currentFrame=0;
   if (!pInArgList->getPlugInArgValue("Base Frame", baseFrame))
   {
      if (pLayer == NULL)
      {
         progress.report("Invalid frame specification", 0, ERRORS, true);
         return false;
      }
      baseFrame = pLayer->getDisplayedBand(GRAY).getActiveNumber();
      if (baseFrame == 0)
      {
         progress.report("If the current frame is frame 0, you must specify a base frame.", 0, ERRORS, true);
         return false;
      }
      baseFrame--;
   }
   if (!pInArgList->getPlugInArgValue("Current Frame", currentFrame))
   {
      if (pLayer == NULL)
      {
         progress.report("Invalid frame specification", 0, ERRORS, true);
         return false;
      }
      currentFrame = pLayer->getDisplayedBand(GRAY).getActiveNumber();
   }

   progress.report("Using base frame " + StringUtilities::toDisplayString(baseFrame) + " and current frame " + StringUtilities::toDisplayString(currentFrame), 0, NORMAL, true);
   RasterDataDescriptor* pDesc = static_cast<RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (pDesc->getDataType() != INT1UBYTE && pDesc->getDataType() != INT1SBYTE)
   {
      progress.report("Invalid data type, only 1-byte data is currently supported.", 0, ERRORS, true);
      return false;
   }
   int startRow(0), endRow(pDesc->getRowCount() - 1), startCol(0), endCol(pDesc->getColumnCount() - 1);
   AoiElement* pAoi = pInArgList->getPlugInArgValue<AoiElement>("AOI");
   bool hasSubcube = (pAoi != NULL);
   if (hasSubcube)
   {
      pAoi->getSelectedPoints()->getMinimalBoundingBox(startCol, startRow, endCol, endRow);
   }
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
         for (int row = startRow; row <= endRow; ++row)
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

      // Calculate optical flow vectors using pyramidal Lucas-Kanade
      progress.report("Calculating tracking features for base frame...", 10, NORMAL);
      int win_size = 10;
      IplImageResource eig_image(width, height, IPL_DEPTH_32F, 1);
      IplImageResource tmp_image(width, height, IPL_DEPTH_32F, 1);
      int corner_count = 500;
      std::auto_ptr<CvPoint2D32f> cornersA(new CvPoint2D32f[500]);
      cvGoodFeaturesToTrack(pBaseFrame, eig_image, tmp_image, cornersA.get(), &corner_count, 0.01, 5.0);
      progress.report("Locating corner sub-pixels for base frame...", 15, NORMAL);
      cvFindCornerSubPix(pBaseFrame, cornersA.get(), corner_count, cvSize(win_size, win_size), cvSize(-1, -1), cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03));
      char features_found[500];
      float feature_errors[500];
      CvSize pyr_sz = cvSize((*pBaseFrame).width + 8, (*pBaseFrame).height / 3);
      IplImageResource pyrA(pyr_sz.width, pyr_sz.height, IPL_DEPTH_32F, 1);
      IplImageResource pyrB(pyr_sz.width, pyr_sz.height, IPL_DEPTH_32F, 1);
      std::auto_ptr<CvPoint2D32f> cornersB(new CvPoint2D32f[500]);
      progress.report("Calculating pyramidal Lucas-Kanade...", 25, NORMAL);
      cvCalcOpticalFlowPyrLK(pBaseFrame, pCurrentFrame, pyrA, pyrB, cornersA.get(), cornersB.get(), corner_count, cvSize(win_size,win_size), 5, features_found, feature_errors, cvTermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.3), 0);
      progress.report("Displaying flow vectors...", 95, NORMAL);

      // display flow vectors in an annotation layer
      ModelResource<AnnotationElement> pAnno(static_cast<AnnotationElement*>(
            Service<ModelServices>()->getElement("Flow Vectors", TypeConverter::toString<AnnotationElement>(), pElement)));
      bool created = pAnno.get() == NULL;
      if (created)
      {
          pAnno = ModelResource<AnnotationElement>("Flow Vectors", pElement);
      }
      GraphicGroup* pGroup = pAnno->getGroup();
      if (!created)
      {
         pGroup->removeAllObjects(true);
      }
      for (int i = 0; i < corner_count; ++i)
      {
         if (features_found[i] == 0 || feature_errors[i] > 550)
         {
            continue;
         }
         GraphicObject* pObj = pGroup->addObject(ARROW_OBJECT);
         pObj->setBoundingBox(LocationType(cornersA.get()[i].x, cornersA.get()[i].y),
                              LocationType(cornersB.get()[i].x, cornersB.get()[i].y));

      }
      if (created)
      {
         Layer* pLayer = pView->createLayer(ANNOTATION, pAnno.get());
         pLayer->setXOffset(startCol);
         pLayer->setYOffset(startRow);
      }
      pAnno.release();
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
