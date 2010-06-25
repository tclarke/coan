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
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

class AoiElement;
class CBlobResult;
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
   void setFocus(LocationType loc, int maxSize);

   struct TrackVertexProps
   {
      TrackVertexProps() : mFrameNum(-1), mCentroidA(0,0), mCentroidB(0,0), mTexture(0.0), mDispersion(0.0) {}

      int mFrameNum;                     // frame number where this objects was found
      Opticks::PixelLocation mCentroidA; // position when this is the "current" frame (pre-transform)
      Opticks::PixelLocation mCentroidB; // position when this is the "base" frame (post-transform)
      float mTexture;                    // grayscale texture
      float mDispersion;                 // grayscale dispersion
      // optional HSI sig goes here
   };
   struct TrackEdgeProps
   {
      TrackEdgeProps() : mVelocity(0,0), mVelDiff(0.0), mSpeedDiff(0.0), mDiffDispersion(0.0) {}

      Opticks::Location<int, 2> mVelocity; // previous frame's centroid B to this frame's centroid A
      float mVelDiff;                      // angular difference between this velocity vector and the previous location's  
      float mSpeedDiff;                    // magnitude difference between this velocity vector and the previous location's  
      float mDiffDispersion;               // difference in dispersion values
      float mCost;                         // total "cost" of this track
   };
   typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, TrackVertexProps, TrackEdgeProps> TrackGraph;
   typedef boost::graph_traits<TrackGraph> TrackTraits;
   typedef boost::graph_traits<TrackGraph>::vertex_descriptor TrackVertex;
   typedef boost::graph_traits<TrackGraph>::edge_descriptor TrackEdge;

protected:
   void processFrame(Subject& subject, const std::string& signal, const boost::any& val);
   void clearData(Subject& subject, const std::string& signal, const boost::any& val);
   std::vector<TrackVertex> updateTrackObjects(CBlobResult& blobs, IplImage* pFrame, bool current);
   void matchTracks(const std::vector<TrackVertex>& curObjs);

private:
   void initializeDataset();
   void initializeFrame0();

   TrackGraph mTracks;

   bool mCalcBaseObjects; // should the base objects be calculated or results from the previous iteration used?
   std::vector<TrackVertex> mBaseObjects;
   bool mPaused;
   AttachmentPtr<RasterLayer> mpLayer; // tracked layer
   AttachmentPtr<Animation> mpAnimation; // tracked animation
   const RasterDataDescriptor* mpDesc; // tracked element descriptor
   RasterElement* mpElement; // tracked element

   // base frame information, passed to the next iteration
   int mBaseFrameNum;
   DataAccessor mBaseAcc;
   IplImageResource mpBaseFrame;
   IplImageResource mpEigImage;
   IplImageResource mpTmpImage;
   std::auto_ptr<CvPoint2D32f> mpBaseCorners;
   IplImageResource mpBasePyramid;

   // pass feature state to the next iteration
   char mpFeaturesFound[500];
   float mpFeatureErrors[500];

   int mCurrentFrameNum;
   GraphicGroup* mpGroup; // draw flow vectors
   GraphicGroup* mpTracks; // draw tracks

   int mCornerCount;

   RasterElement* mpRes; // base frame object blobs
   RasterElement* mpRes2; // current frame object blobs

   // sub-frame AOI and bounding box information
   AoiElement* mpFocus;
   Opticks::PixelLocation mMinBb;
   Opticks::PixelLocation mMaxBb;
};

#endif