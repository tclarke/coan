/*
 * The information in this file is
 * Copyright(c) 2010 Trevor R.H. Clarke
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "DesktopServices.h"
#include "MenuBar.h"
#include "OpticalFlow.h"
#include "OpticalFlowWidget.h"
#include "PlugInRegistration.h"
#include "ToolBar.h"

REGISTER_PLUGIN_BASIC(Tracking, OpticalFlow);

OpticalFlow::OpticalFlow()
{
   setName("OpticalFlow");
   setDescription("Calculate optical flow field.");
   setDescriptorId("{38c907e0-1a57-11df-8a39-0800200c9a66}");
   setSubtype("Video");
}

OpticalFlow::~OpticalFlow()
{
}

QAction* OpticalFlow::createAction()
{
   // Add a menu command to invoke the window
   MenuBar* pMenuBar = Service<DesktopServices>()->getMainMenuBar();
   VERIFYRV(pMenuBar, NULL);
   QAction* pAction = pMenuBar->addCommand("&View/&Optical Flow");

   ToolBar* pToolBar = static_cast<ToolBar*>(Service<DesktopServices>()->getWindow("Tracking", TOOLBAR));
   if (pToolBar == NULL)
   {
      pToolBar = static_cast<ToolBar*>(Service<DesktopServices>()->createWindow("Tracking", TOOLBAR));
   }
   VERIFYRV(pToolBar, NULL);
   pToolBar->addButton(pAction);

   return pAction;
}

QWidget* OpticalFlow::createWidget()
{
   return new OpticalFlowWidget();
}