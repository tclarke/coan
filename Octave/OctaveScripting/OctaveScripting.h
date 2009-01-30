/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU General Public License Version 2
 * The license text is available from   
 * http://www.gnu.org/licenses/gpl.html
 */

#ifndef OCTAVESCRIPTING_H__
#define OCTAVESCRIPTING_H__

#include "DockWindowShell.h"
#include <QtGui/QTextEdit>
#include <sstream>

class OctaveScripting : public DockWindowShell
{
   Q_OBJECT

public:
   OctaveScripting();
   virtual ~OctaveScripting();

protected:
   virtual QAction* createAction();
   virtual QWidget* createWidget();
};

class ScriptWindow : public QTextEdit
{
   Q_OBJECT

public:
   ScriptWindow(QWidget* pParent=NULL);
   virtual ~ScriptWindow();

protected:
   virtual void keyPressEvent(QKeyEvent* pEvent);

   int octave_call(const QString& cmd);
   void recover_from_exception();

private:
   std::ostringstream mOut;
   std::ostringstream mErr;
};

#endif