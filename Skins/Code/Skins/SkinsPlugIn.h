/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef SKINSPLUGIN_H
#define SKINSPLUGIN_H

#include "ExecutableShell.h"
#include <QtCore/QObject>
#include <QtCore/QMap>

class QAction;
class QMenu;

class SkinsPlugIn : public QObject, public ExecutableShell
{
   Q_OBJECT

public:
   SkinsPlugIn();
   virtual ~SkinsPlugIn();

   virtual bool getInputSpecification(PlugInArgList*& pArgList);
   virtual bool getOutputSpecification(PlugInArgList*& pArgList);
   virtual bool execute(PlugInArgList* pInArgList, PlugInArgList* pOutArgList);

protected slots:
   void changeSkin(QAction* pSkinAction);
   void changeSkin(const QString& skinName);

private:
   QString mDefaultStyleSheet;
   QMap<QString, QString> mRegisteredResources;
   QAction* mpDefaultAction;
   QMenu* mpSkinsMenu;
};

#endif