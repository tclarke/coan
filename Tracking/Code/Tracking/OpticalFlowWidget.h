/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef OPTICALFLOWWIDGET_H__
#define OPTICALFLOWWIDGET_H__

#include "AnimationToolBar.h"
#include "AttachmentPtr.h"
#include "LabeledSectionGroup.h"
#include <boost/any.hpp>

class MouseMode;
class QComboBox;
class QSpinBox;

class OpticalFlowWidget : public LabeledSectionGroup
{
   Q_OBJECT

public:
   OpticalFlowWidget(QWidget* pParent = NULL);
   virtual ~OpticalFlowWidget();

protected slots:
   void updatePause(bool state);
   void activateSelectMode();

protected:
   bool eventFilter(QObject* pObj, QEvent* pEvent);

private:
   void updateAnimation(Subject& subject, const std::string& signal, const boost::any& val);

   AttachmentPtr<AnimationToolBar> mpToolbar;
   MouseMode* mpSelectMode;
   MouseMode* mpActiveMode;
   QSpinBox* mpMaxSize;
};

#endif