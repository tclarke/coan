/*
 * The information in this file is
 * Copyright(c) 2010 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef OPTICALFLOWWIDGET_H__
#define OPTICALFLOWWIDGET_H__

#include "LabeledSectionGroup.h"
#include <boost/any.hpp>

class QComboBox;

class OpticalFlowWidget : public LabeledSectionGroup
{
   Q_OBJECT

public:
   OpticalFlowWidget(QWidget* pParent = NULL);
   virtual ~OpticalFlowWidget();

protected slots:
   void updateDataSelect();
   void trackIndex(int idx);

private:
   QComboBox* mpDataSelect;
};

#endif