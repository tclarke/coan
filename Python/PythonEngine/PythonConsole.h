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
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <QtCore/QList>
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

class auto_obj
{
public:
   auto_obj() : mpObj(NULL), mOwned(false) {}
   auto_obj(PyObject* pObj, bool takeOwnership=false) : mpObj(pObj), mOwned(takeOwnership) {}
   ~auto_obj()
   {
      if (mOwned)
      {
         Py_XDECREF(mpObj);
      }
   }

   /**
   * Reset the internal pointer.
   */
   void reset(PyObject* pObj, bool takeOwnership=false)
   {
      release();
      mpObj = pObj;
      mOwned = takeOwnership;
   }

   /**
   * Release ownership doing a decref if needed.
   */
   PyObject* release()
   {
      if (mpObj == NULL)
      {
         return NULL;
      }
      if (mOwned)
      {
         Py_DECREF(mpObj);
         mOwned = false;
      }
      return mpObj;
   }

   /**
   * Returns a borrowed reference. This object may not own the reference.
   */
   PyObject* get()
   {
      return mpObj;
   }

   /**
   * Returns a borrowed reference. Ensures this object owns the reference.
   */
   PyObject* take()
   {
      if (mpObj == NULL)
      {
         return NULL;
      }
      if (!mOwned)
      {
         Py_INCREF(mpObj);
         mOwned = true;
      }
      return mpObj;
   }

   operator PyObject*()
   {
      return get();
   }

private:
   PyObject* mpObj;
   bool mOwned;
};

class PythonError : public std::exception
{
public:
   PythonError(const QString& what) : std::exception(what.toAscii()) {}
};

class ConsoleWindow : public QTextEdit
{
   Q_OBJECT

public:
   ConsoleWindow(QWidget* pParent=NULL);
   virtual ~ConsoleWindow();

protected:
   virtual void keyPressEvent(QKeyEvent* pEvent);
   void flushout();
   void checkErr();

private:
   auto_obj mInterpModule;
   auto_obj mStdin;
   auto_obj mStdout;
   auto_obj mStderr;
   auto_obj mInterpreter;
   int mPos;
   QList<QString> mHistory;
   QList<QString>::iterator mHistoryPos;
};

#endif
