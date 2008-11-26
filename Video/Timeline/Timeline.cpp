/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "Timeline.h"
#include "TimelineWidget.h"
#include "DesktopServices.h"
#include "DockWindow.h"
#include "MenuBar.h"
#include "PlugInFactory.h"
#include "RasterElement.h"
#include "RasterLayer.h"
#include "SessionManager.h"
#include "Slot.h"
#include "ThresholdLayer.h"
#include "VideoVersion.h"

#include <QtGui/QAction>
#include <QtGui/QMenu>

PLUGINFACTORY(Timeline);

namespace
{
   class DropFilter : public Window::SessionItemDropFilter
   {
   public:
      DropFilter() {}
      virtual ~DropFilter() {}

      virtual bool accept(SessionItem *pItem) const
      {
         return dynamic_cast<RasterElement*>(pItem) != NULL ||
                dynamic_cast<RasterLayer*>(pItem) != NULL ||
                dynamic_cast<ThresholdLayer*>(pItem) != NULL;
      }
   };
}

Timeline::Timeline() : mpWindowAction(NULL)
{
   Q_INIT_RESOURCE(video);
   AlgorithmShell::setName("Animation Timeline");
   setCreator("Ball Aerospace & Technologies Corp.");
   setCopyright(VIDEO_COPYRIGHT);
   setVersion(VIDEO_VERSION_NUMBER);
   setDescription("Advanced animation timeline display.");
   executeOnStartup(true);
   setDescriptorId("{ffea5038-9903-4a46-b2ff-50b7ef3f9ded}");
   allowMultipleInstances(false);
   destroyAfterExecute(false);
   setProductionStatus(VIDEO_IS_PRODUCTION_RELEASE);
   addDependencyCopyright("Timeline", "Some Icons by http://dryicons.com");
}

Timeline::~Timeline()
{
   if(mpWindowAction != NULL)
   {
      MenuBar *pMenuBar = Service<DesktopServices>()->getMainMenuBar();
      if(pMenuBar != NULL)
      {
         pMenuBar->removeMenuItem(mpWindowAction);
      }

      if(Service<DesktopServices>()->getMainWidget() != NULL)
      {
         delete mpWindowAction;
      }
   }
   Window *pWindow = Service<DesktopServices>()->getWindow(getWindowName(), DOCK_WINDOW);
   if(pWindow != NULL)
   {
      pWindow->detach(SIGNAL_NAME(DockWindow, Shown), Slot(this, &Timeline::windowShown));
      pWindow->detach(SIGNAL_NAME(DockWindow, Hidden), Slot(this, &Timeline::windowHidden));
      Service<DesktopServices>()->deleteWindow(pWindow);
   }
   Q_CLEANUP_RESOURCE(video);
}

void Timeline::windowHidden(Subject &subject, const std::string &signal, const boost::any &v)
{
   DockWindow *pWindow = static_cast<DockWindow*>(Service<DesktopServices>()->getWindow(getWindowName(), DOCK_WINDOW));
   if(pWindow != NULL)
   {
      if((dynamic_cast<DockWindow*>(&subject) == pWindow) && (mpWindowAction != NULL))
      {
         mpWindowAction->setChecked(false);
      }
   }
}

void Timeline::windowShown(Subject &subject, const std::string &signal, const boost::any &v)
{
   DockWindow *pWindow = static_cast<DockWindow*>(Service<DesktopServices>()->getWindow(getWindowName(), DOCK_WINDOW));
   if(pWindow != NULL)
   {
      if((dynamic_cast<DockWindow*>(&subject) == pWindow) && (mpWindowAction != NULL))
      {
         mpWindowAction->setChecked(true);
      }
   }
}

bool Timeline::execute(PlugInArgList *pInputArgList, PlugInArgList *pOutputArgList)
{
   createMenuItem();
   return createEditorWindow() && mpWindowAction != NULL;
}

void Timeline::createMenuItem()
{
   // Add a menu command to invoke the window
   MenuBar *pMenuBar = Service<DesktopServices>()->getMainMenuBar();
   if(pMenuBar != NULL)
   {
      QAction *pBeforeAction = NULL;
      QAction *pToolsAction = pMenuBar->getMenuItem("&View");
      if(pToolsAction != NULL)
      {
         QMenu *pMenu = pToolsAction->menu();
         if(pMenu != NULL)
         {
            QList<QAction*> actions = pMenu->actions();
            for(int i = 0; i < actions.count(); ++i)
            {
               QAction *pAction = actions[i];
               if(pAction != NULL)
               {
                  if((pAction->text() == "Create &Animation") && (pAction != actions.back()))
                  {
                     pBeforeAction = actions[i + 1];
                     break;
                  }
               }
            }
         }
      }

      mpWindowAction = pMenuBar->addCommand("&View/&" + getWindowName(), getName(), pBeforeAction);
      if(mpWindowAction != NULL)
      {
         mpWindowAction->setIcon(getIcon());
         mpWindowAction->setAutoRepeat(false);
         mpWindowAction->setCheckable(true);
         mpWindowAction->setToolTip(QString::fromStdString(getWindowName()));
         mpWindowAction->setStatusTip("Toggles the display of the " + QString::fromStdString(getWindowName()));
         connect(mpWindowAction, SIGNAL(triggered(bool)), this, SLOT(displayEditorWindow(bool)));

         ToolBar *pToolBar = static_cast<ToolBar*>(Service<DesktopServices>()->getWindow("Temporal", TOOLBAR));
         if(pToolBar != NULL)
         {
            pToolBar->addButton(mpWindowAction);
         }
      }
   }
}

const QIcon &Timeline::getIcon() const
{
   static QIcon sIcon(":/coan/video/timeline/timeline.png");
   return sIcon;
}

bool Timeline::createEditorWindow()
{
   // Create the editor window
   if(mpWindowAction != NULL)
   {
      DockWindow *pEditorWindow = static_cast<DockWindow*>(Service<DesktopServices>()->getWindow(getWindowName(), DOCK_WINDOW));
      if(pEditorWindow == NULL)
      {
         pEditorWindow = static_cast<DockWindow*>(Service<DesktopServices>()->createWindow(getWindowName(), DOCK_WINDOW));
         if(pEditorWindow != NULL)
         {
            pEditorWindow->setIcon(getIcon());
            attachToEditorWindow(pEditorWindow);
            pEditorWindow->hide();
         }
         else
         {
            return false;
         }
      }
   }
   return true;
}

void Timeline::attachToEditorWindow(DockWindow *pDockWindow)
{
   if(pDockWindow)
   {
      pDockWindow->attach(SIGNAL_NAME(DockWindow, Shown), Slot(this, &Timeline::windowShown));
      pDockWindow->attach(SIGNAL_NAME(DockWindow, Hidden), Slot(this, &Timeline::windowHidden));

      TimelineWidget *pWidget = new TimelineWidget(Service<DesktopServices>()->getMainWidget());
      if(pWidget != NULL)
      {
         pDockWindow->setWidget(pWidget);
         pDockWindow->enableSessionItemDrops(new DropFilter);
         pDockWindow->attach(SIGNAL_NAME(Window, SessionItemDropped), Slot(pWidget, &TimelineWidget::sessionItemDropped));
      }
   }
}

void Timeline::displayEditorWindow(bool bDisplay)
{
   DockWindow *pEditorWindow = static_cast<DockWindow*>(Service<DesktopServices>()->getWindow(getWindowName(), DOCK_WINDOW));
   if(pEditorWindow != NULL)
   {
      if (bDisplay == true)
      {
         pEditorWindow->show();
      }
      else
      {
         pEditorWindow->hide();
      }
   }
}

bool Timeline::serialize(SessionItemSerializer &serializer) const
{
   return false;
}

bool Timeline::deserialize(SessionItemDeserializer &deserializer)
{
   return false;
}
