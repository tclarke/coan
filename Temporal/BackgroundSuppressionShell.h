/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef BACKGROUNDSUPPRESSIONSHELL_H__
#define BACKGROUNDSUPPRESSIONSHELL_H__

#include "AlgorithmShell.h"
#include "DataAccessor.h"
#include "ProgressTracker.h"

#include <vector>

class RasterElement;
class SpatialDataView;

class BackgroundSuppressionShell : public AlgorithmShell
{
public:
   BackgroundSuppressionShell();
   virtual ~BackgroundSuppressionShell();

   virtual bool getInputSpecification(PlugInArgList *&pArgList);
   virtual bool getOutputSpecification(PlugInArgList *&pArgList);
   virtual bool execute(PlugInArgList *pInArgList, PlugInArgList *pOutArgList);

protected:
   /**
    * Initialization step.
    *
    * Perform initialization. This might include multiple frames of initialization.
    *
    * @return INIT_ERROR if initialization failed, this will cause processing to terminate.
    *         INIT_COMPLETE if initialization is complete, recursive processing will start with the current frame.
    *         INIT_CONTINUE if iniitialization is partially complete, the next frame will advance and initializeModel()
    *         will be called again.
    */
   virtual enum InitializeReturnType { INIT_ERROR, INIT_COMPLETE, INIT_CONTINUE } initializeModel() = 0;

   /**
    * Recursive step 1.
    *
    * Perform any preprocessing required before updating the background model.
    * Convert the data into a different color space, perform blurring, etc.
    *
    * @return True if successful, false if processing should end.
    */
   virtual bool preprocess() = 0;

   /**
    * Recursive step 2.
    *
    * Update the background model.
    *
    * @return True if successful, false if processing should end.
    */
   virtual bool updateBackgroundModel() = 0;

   /**
    * Recursive step 3.
    *
    * Extract the foreground into the foreground mask.
    *
    * @return True if successful, false if processing should end.
    */
   virtual bool extractForeground() = 0;

   /**
    * Recursive step 4.
    *
    * Validate the foreground mask and update as necessary.
    * This may involve geomtry modeling, morphological operations, etc.
    *
    * @return True if successful, false if processing should end.
    */
   virtual bool validate() = 0;

   /**
    * Cleanup after execution.
    *
    * If overriding this method, always call the base class implementation to ensure masks, etc. are cleaned up.
    *
    * @param success
    *        If true, extraction was successful and only temporary data should be cleaned up.
    *        If false, extraction failed or was aborted and all data should be cleaned up.
    */
   virtual void cleanup(bool success);

   /**
    * Create displays for the results.
    *
    * @return False if there was an error.
    */
   virtual bool displayResults();

   /**
    * Sets how many foreground masks are maintained.
    *
    * This should be called from the constructor. If this is called after
    * execute() has begun, it will have no effect.
    *
    * @param single
    *        If true, one foreground mask is maintained for all frames.
    *        If false, a foreground mask is maintained for each frame.
    *        The default value is false.
    */
   void setSingleForegroundMask(bool single);

   /**
    * Get the number of the currently processing frame.
    *
    * @return The currently processing frame, zero based.
    */
   unsigned int getCurrentFrame() const;

   /**
    * Get the RasterElement being processed.
    *
    * @return The RasterElement. This will not be NULL if called from one of the recursive steps.
    */
   RasterElement *getRasterElement() const;

   /**
    * Get a DataAccessor for the current frame.
    *
    * @return A BSQ data accessor for the current frame. All other DataRequest values are the defaults.
    */
   DataAccessor getCurrentFrameAccessor() const;

   /**
    * Get the active view.
    *
    * @return The view, if one is available.
    */
   SpatialDataView *getView() const;

   ProgressTracker mProgress;

private:
   unsigned int mCurrentFrame;
   RasterElement *mpRaster;
   bool mSingleForegroundMask;
   SpatialDataView *mpView;
};

#endif