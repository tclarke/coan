/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef TIMELINE_H__
#define TIMELINE_H__

#include "AlgorithmShell.h"
#include <boost/any.hpp>
#include <QtCore/QObject>

class DockWindow;

class Timeline : public QObject, public AlgorithmShell
{
   Q_OBJECT

public:
   static std::string getWindowName() { return "Animation Timeline"; }
   Timeline();
   virtual ~Timeline();

   void windowHidden(Subject &subject, const std::string &signal, const boost::any &v);
   void windowShown(Subject &subject, const std::string &signal, const boost::any &v);

   bool execute(PlugInArgList *pInputArgList, PlugInArgList *pOutputArgList);
   bool getInputSpecification(PlugInArgList*&) { return true; }
   bool getOutputSpecification(PlugInArgList*&) { return true; }

   bool setBatch() { return false; }
   const QIcon &getIcon() const;

   bool serialize(SessionItemSerializer &serializer) const;
   bool deserialize(SessionItemDeserializer &deserializer);

protected slots:
   void displayEditorWindow(bool bDisplay);
   bool createEditorWindow();
   void createMenuItem();
   void attachToEditorWindow(DockWindow *pDockWindow);

private:
   QAction *mpWindowAction;
};

#endif