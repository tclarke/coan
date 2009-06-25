/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AppVerify.h"
#include "ConfigurationSettings.h"
#include "DesktopServices.h"
#include "FileFinder.h"
#include "MenuBar.h"
#include "PlugInFactory.h"
#include "SkinsPlugIn.h"
#include "SkinsVersion.h"
#include <QtCore/QFile>
#include <QtCore/QMapIterator>
#include <QtCore/QResource>
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtGui/QApplication>
#include <QtGui/QErrorMessage>
#include <QtGui/QMenu>

PLUGINFACTORY(SkinsPlugIn);

SkinsPlugIn::SkinsPlugIn() : mpDefaultAction(NULL), mpSkinsMenu(NULL)
{
   setDescriptorId("{3a01c75e-29c2-4ad6-a3d4-5794ad9b6ee0}");
   setName("Skins");
   setDescription("Plug-in to manage skins.");
   setVersion(SKINS_VERSION_NUMBER);
   setProductionStatus(SKINS_IS_PRODUCTION_RELEASE);
   setCreator("Trevor R.H. Clarke <tclarke@ball.com>");
   setCopyright(SKINS_COPYRIGHT);
   setType("Manager");
   setSubtype("Skins");
   allowMultipleInstances(false);
   executeOnStartup(true);
   destroyAfterExecute(false);
   setAbortSupported(false);
   setWizardSupported(false);
}

SkinsPlugIn::~SkinsPlugIn()
{
   if (mpSkinsMenu != NULL)
   {
      qApp->setStyleSheet(mDefaultStyleSheet);
      QMapIterator<QString, QString> resource(mRegisteredResources);
      while (resource.hasNext())
      {
         resource.next();
         QResource::unregisterResource(resource.value(), resource.key());
      }
      Service<DesktopServices>()->getMainMenuBar()->removeMenu(mpSkinsMenu);
   }
   delete mpSkinsMenu;
}

bool SkinsPlugIn::getInputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool SkinsPlugIn::getOutputSpecification(PlugInArgList*& pArgList)
{
   pArgList = NULL;
   return true;
}

bool SkinsPlugIn::execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList)
{
   MenuBar* pMenu = Service<DesktopServices>()->getMainMenuBar();
   QAction* pBefore = pMenu->getMenuItem("&View/&Status Bar");
   mpSkinsMenu = pMenu->addMenu("&View/S&kins", pBefore);
   mpSkinsMenu->setStatusTip("Change the current application skin.");
   VERIFY(mpSkinsMenu);
   VERIFY(connect(mpSkinsMenu, SIGNAL(triggered(QAction*)), this, SLOT(changeSkin(QAction*))));
   
   QActionGroup* pActionGroup = new QActionGroup(mpSkinsMenu);
   pActionGroup->setExclusive(true);
   
   // Add default menu entries
   mpDefaultAction = mpSkinsMenu->addAction("Default");
   mpDefaultAction->setCheckable(true);
   pActionGroup->addAction(mpDefaultAction);

   // Scan the skins directory
   const Filename* pPath = ConfigurationSettings::getSettingSupportFilesPath();
   FactoryResource<FileFinder> pFinder;
   VERIFY(pPath && pFinder.get());
   pFinder->findFile(pPath->getFullPathAndName() + "/Skins", "*.css", true);
   while (pFinder->findNextFile())
   {
      std::string title;
      pFinder->getFileTitle(title);
      QAction* pAction = mpSkinsMenu->addAction(QString::fromStdString(title));
      pAction->setCheckable(true);
      pActionGroup->addAction(pAction);
   }
   pFinder->findFile(pPath->getFullPathAndName() + "/Skins", "*.rcc", true);
   while (pFinder->findNextFile())
   {
      std::string fullPath;
      std::string fileTitle;
      pFinder->getFullPath(fullPath);
      pFinder->getFileTitle(fileTitle);
      QString path = QString::fromStdString(fullPath);
      QString root = "/" + QString::fromStdString(fileTitle);
      if (QResource::registerResource(path, root))
      {
         mRegisteredResources[root] = path;
      }
   }
   mDefaultStyleSheet = qApp->styleSheet();
   mpDefaultAction->setChecked(true);
   return true;
}

void SkinsPlugIn::changeSkin(QAction* pSkinAction)
{
   if (pSkinAction != NULL)
   {
      changeSkin(pSkinAction->text());
   }
}

void SkinsPlugIn::changeSkin(const QString& skinName)
{
   if (skinName != "Default")
   {
      QErrorMessage::qtHandler();
      const Filename* pPath = ConfigurationSettings::getSettingSupportFilesPath();
      VERIFYNRV(pPath);
      QFile inFile(QString("%1/Skins/%2.css").arg(QString::fromStdString(pPath->getFullPathAndName())).arg(skinName));
      if (inFile.open(QFile::ReadOnly | QFile::Text))
      {
         QString styleSheet(inFile.readAll());
         qApp->setStyleSheet(styleSheet);
         qInstallMsgHandler(0);
         return;
      }
      qInstallMsgHandler(0);
   }

   // Set the default skin
   mpDefaultAction->setChecked(true);
   qApp->setStyleSheet(mDefaultStyleSheet);
}