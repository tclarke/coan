/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnimationController.h"
#include "AnimationServices.h"
#include "AnimationToolBar.h"
#include "AttachmentPtr.h"
#include "DesktopServices.h"
#include "ModelServices.h"
#include "LabeledSection.h"
#include "LayerList.h"
#include "MouseMode.h"
#include "OpticalFlowWidget.h"
#include "PlugInManagerServices.h"
#include "RasterLayer.h"
#include "SessionManager.h"
#include "Slot.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "TrackingManager.h"
#include <QtGui/QAction>
#include <QtGui/QCheckBox>
#include <QtGui/QCursor>
#include <QtGui/QHBoxLayout>
#include <QtGui/QMouseEvent>
#include <QtGui/QPushButton>
#include <QtGui/QSpinBox>
#include <vector>

OpticalFlowWidget::OpticalFlowWidget(QWidget* pParent) :
      LabeledSectionGroup(pParent),
      mpToolbar(SIGNAL_NAME(AnimationToolBar, ControllerChanged), Slot(this, &OpticalFlowWidget::updateAnimation)),
      mpActiveMode(NULL)
{
   mpToolbar.reset(static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR)));
   QWidget* pSelectWidget = new QWidget(this);
   QHBoxLayout* pSelectLayout = new QHBoxLayout(pSelectWidget);

   mpMaxSize = new QSpinBox(pSelectWidget);
   mpMaxSize->setRange(16, 8192);
   mpMaxSize->setSingleStep(16);
   mpMaxSize->setValue(1024);
   mpMaxSize->setSuffix(" pel^2");
   pSelectLayout->addWidget(mpMaxSize);
   QPushButton* pSelectButton = new QPushButton("Select Area", pSelectWidget);
   pSelectLayout->addWidget(pSelectButton);
   VERIFYNR(connect(pSelectButton, SIGNAL(clicked()), this, SLOT(activateSelectMode())));

   QCheckBox* pPause = new QCheckBox("Pause", pSelectWidget);
   pSelectLayout->addWidget(pPause);
   VERIFYNR(connect(pPause, SIGNAL(toggled(bool)), this, SLOT(updatePause(bool))));

   pSelectLayout->addStretch(10);
   LabeledSection* pDataSelectSection = new LabeledSection("Data Set", this);
   pDataSelectSection->setSectionWidget(pSelectWidget);
   addSection(pDataSelectSection);

   addStretch(100);

   mpSelectMode = Service<DesktopServices>()->createMouseMode("OpticalFlowCenterSelect", Qt::CrossCursor);
}

OpticalFlowWidget::~OpticalFlowWidget()
{
}

void OpticalFlowWidget::updateAnimation(Subject& subject, const std::string& signal, const boost::any& val)
{
   AnimationController* pController = boost::any_cast<AnimationController*>(val);
   if (pController == NULL || pController->getAnimations().empty())
   {
      return;
   }
   Animation* pAnimation = pController->getAnimations().front();
   std::vector<Window*> windows;
   Service<DesktopServices>()->getWindows(windows);
   for (std::vector<Window*>::iterator window = windows.begin(); window != windows.end(); ++window)
   {
      SpatialDataWindow* pWindow = dynamic_cast<SpatialDataWindow*>(*window);
      SpatialDataView* pView = pWindow == NULL ? NULL : pWindow->getSpatialDataView();
      if (pView != NULL)
      {
         std::vector<Layer*> layers;
         pView->getLayerList()->getLayers(RASTER, layers);
         for (std::vector<Layer*>::iterator layer = layers.begin(); layer != layers.end(); ++layer)
         {
            RasterLayer* pLayer = static_cast<RasterLayer*>(*layer);
            if (pLayer != NULL && pLayer->getAnimation() == pAnimation)
            {
               std::vector<PlugIn*> manager =
                  Service<PlugInManagerServices>()->getPlugInInstances(TrackingManager::spPlugInName);
               if (!manager.empty())
               {
                  static_cast<TrackingManager*>(manager.front())->setTrackedLayer(pLayer);
               }
               return;
            }
         }
      }
   }
}

void OpticalFlowWidget::updatePause(bool state)
{
   std::vector<PlugIn*> manager = Service<PlugInManagerServices>()->getPlugInInstances(TrackingManager::spPlugInName);
   if (!manager.empty())
   {
      static_cast<TrackingManager*>(manager.front())->setPauseState(state);
   }
}

void OpticalFlowWidget::activateSelectMode()
{
   View* pView = Service<DesktopServices>()->getCurrentWorkspaceWindowView();
   if (pView != NULL)
   {
      mpActiveMode = pView->getCurrentMouseMode();
      if (mpActiveMode == mpSelectMode)
      {
         mpActiveMode = NULL;
      }
      pView->addMouseMode(mpSelectMode);
      pView->setMouseMode(mpSelectMode);
      pView->getWidget()->installEventFilter(this);
   }
}

bool OpticalFlowWidget::eventFilter(QObject* pObj, QEvent* pEvent)
{
   if (pEvent != NULL && pEvent->type() == QEvent::MouseButtonRelease)
   {
      View* pView = Service<DesktopServices>()->getCurrentWorkspaceWindowView();
      if (pView != NULL)
      {
         QPointF loc = static_cast<QMouseEvent*>(pEvent)->posF();
         loc.setY(pView->getWidget()->height() - loc.y());
         std::vector<PlugIn*> manager = Service<PlugInManagerServices>()->getPlugInInstances(TrackingManager::spPlugInName);
         if (!manager.empty())
         {
            int maxSz = mpMaxSize->value();
            static_cast<TrackingManager*>(manager.front())->setFocus(LocationType(loc.x(), loc.y()), maxSz*maxSz);
         }

         pView->setMouseMode(mpActiveMode);
         mpActiveMode = NULL;
         pView->getWidget()->removeEventFilter(this);
         return true;
      }
   }
   return false;
}