/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef VIDEOMETADATA_H
#define VIDEOMETADATA_H

#include "SpecialMetadata.h"
#include <QtCore/QDateTime>

class Animation;
class AnimationController;
class ThresholdLayer;
class RasterLayer;

#define FRAME_TIMES_METADATA_NAME (std::string("FrameTimes"))
#define FRAME_TIMES_METADATA_PATH (SPECIAL_METADATA_NAME + "/" + BAND_METADATA_NAME + "/" + FRAME_TIMES_METADATA_NAME)

namespace TimelineUtils
{
   QDateTime timetToQDateTime(double val);
   double QDateTimeToTimet(const QDateTime &dt);
   bool createAnimationForRasterLayer(RasterLayer *pRasterLayer, AnimationController *pController);
   bool createAnimationForThresholdLayer(ThresholdLayer *pThresholdLayer, AnimationController *pController);
   void rescaleAnimation(Animation *pAnim, double newVal, bool scaleEnd);
   void moveAnimation(Animation *pAnim, double newStart);
}

#endif