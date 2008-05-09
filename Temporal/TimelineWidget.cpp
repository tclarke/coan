/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Animation.h"
#include "AnimationController.h"
#include "AppConfig.h"
#include "AppVerify.h"
#include "DesktopServices.h"
#include "TimelineWidget.h"
#include <QtGui/QVBoxLayout>
#include <qwt_slider.h>
#include <boost/any.hpp>

TimelineWidget::TimelineWidget(QWidget *pParent) : QWidget(pParent),
      mpToolbar(SIGNAL_NAME(AnimationToolBar, ControllerChanged), Slot(this, &TimelineWidget::controllerChanged)),
      mpController(NULL)
{
   QVBoxLayout *pTopLevel = new QVBoxLayout(this);
   mpScale = new QwtSlider(this);
   pTopLevel->addWidget(mpScale);
   mpScale->setOrientation(Qt::Horizontal);
   mpScale->setReadOnly(true);

   AnimationToolBar *pToolBar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
   mpToolbar.reset(pToolBar);
   setAnimationController(pToolBar->getAnimationController());
}

TimelineWidget::~TimelineWidget()
{
}

void TimelineWidget::controllerChanged(Subject &subject, const std::string &signal, const boost::any &v)
{
   AnimationController *pController = boost::any_cast<AnimationController*>(v);
   setAnimationController(pController);
}

void TimelineWidget::setAnimationController(AnimationController *pController)
{
   mpController = pController;
   setEnabled(mpController != NULL);
   if(mpController == NULL)
   {
      return;
   }
   mpScale->setRange(mpController->getStartFrame(), mpController->getStopFrame());
}