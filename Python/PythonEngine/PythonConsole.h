/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#ifndef PYTHONCONSOLE_H__
#define PYTHONCONSOLE_H__

#include "DockWindowShell.h"
#include <QtCore/QBuffer>
#include <QtGui/QTextEdit>

class PythonConsole : public DockWindowShell
{
   Q_OBJECT

public:
   PythonConsole();
   virtual ~PythonConsole();

protected:
   virtual QAction* createAction();
   virtual QWidget* createWidget();
};

class ConsoleWindow : public QTextEdit
{
   Q_OBJECT

public:
   ConsoleWindow(QWidget* pParent=NULL);
   virtual ~ConsoleWindow();

protected:
   virtual void keyPressEvent(QKeyEvent* pEvent);

private:
   QBuffer mStdOut;
   QBuffer mStdErr;
};

#endif
