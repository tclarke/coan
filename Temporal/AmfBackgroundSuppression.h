/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef AMFBACKGROUNDSUPPRESSION_H__
#define AMFBACKGROUNDSUPPRESSION_H__

#include "AoiElement.h"
#include "BackgroundSuppressionShell.h"
#include "RasterElement.h"

class AmfBackgroundSuppression : public BackgroundSuppressionShell
{
public:
   AmfBackgroundSuppression();
   virtual ~AmfBackgroundSuppression();

protected:
   virtual BackgroundSuppressionShell::InitializeReturnType initializeModel();
   virtual bool preprocess();
   virtual bool updateBackgroundModel();
   virtual bool extractForeground();
   virtual bool validate();
   virtual bool displayResults();

private:
   ModelResource<RasterElement> mBackground;
   ModelResource<AoiElement> mForeground;
   double mThreshold;
};

#endif