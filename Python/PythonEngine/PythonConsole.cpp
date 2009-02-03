/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "OpticksModule.h"
#include "PythonVersion.h"
#include "PythonConsole.h"
#include "PlugInFactory.h"
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <QtGui/QKeyEvent>
#include <QtGui/QTextBlock>

PLUGINFACTORY(PythonConsole);

PythonConsole::PythonConsole()
{
   setName("Python Console");
   setDescription("Python execution engine and console");
   setDescriptorId("{92D4A81F-7106-4506-8DAC-9004C369AA5A}");
   setCopyright(PYTHON_COPYRIGHT);
   setVersion(PYTHON_VERSION_NUMBER);
   setProductionStatus(PYTHON_IS_PRODUCTION_RELEASE);
}

PythonConsole::~PythonConsole()
{
}

QAction* PythonConsole::createAction()
{
   return new QAction("Python Console", NULL);
}

QWidget* PythonConsole::createWidget()
{
   return new ConsoleWindow();
}

ConsoleWindow::ConsoleWindow(QWidget* pParent) : QTextEdit(pParent)
{
   setTextInteractionFlags(Qt::TextEditorInteraction);
   setReadOnly(false);
   setWordWrapMode(QTextOption::WordWrap);

   try
   {
      Py_Initialize();
      init_opticks();
      checkErr();
      auto_obj sysPath(PySys_GetObject("path"));
      auto_obj newPathItem(PyString_FromString("c:/Opticks/COAN/Python/SupportFiles/site-packages"), true);
      VERIFYNR(PyList_Append(sysPath, newPathItem) == 0);
      VERIFYNR(PySys_SetObject("path", sysPath) == 0);
      mOpticksModule.reset(PyImport_ImportModule("opticks"), true);
      checkErr();

      auto_obj opticksDict(PyModule_GetDict(mOpticksModule));
      auto_obj strBufStream(PyDict_GetItemString(opticksDict, "StrBufStream"));
      checkErr();
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mStdin.reset(PyObject_CallObject(strBufStream, NULL), true);
      checkErr();
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mStdout.reset(PyObject_CallObject(strBufStream, NULL), true);
      checkErr();
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mStderr.reset(PyObject_CallObject(strBufStream, NULL), true);
      checkErr();

      auto_obj pythonInteractiveInterpreter(PyDict_GetItemString(opticksDict, "PythonInteractiveInterpreter"));
      Py_INCREF(Py_None);
      Py_INCREF(Py_None);
      mInterpreter.reset(PyObject_CallObject(pythonInteractiveInterpreter, NULL), true);
      checkErr();

      PyObject_SetAttrString(mInterpreter, "stdin", mStdin);
      PyObject_SetAttrString(mInterpreter, "stdout", mStdout);
      PyObject_SetAttrString(mInterpreter, "stderr", mStderr);
      checkErr();

      auto_obj useps1(PyObject_CallMethod(mInterpreter, "processEvent", NULL), true);
      checkErr();
      flushout();
   }
   catch(const PythonError&)
   {
      // nothing
   }
   mPos = textCursor().position();
}

ConsoleWindow::~ConsoleWindow()
{
   mOpticksModule.reset(NULL);
   mStdin.reset(NULL);
   mStdout.reset(NULL);
   mStderr.reset(NULL);
   mInterpreter.reset(NULL);
   Py_Finalize();
}

void ConsoleWindow::keyPressEvent(QKeyEvent* pEvent)
{
   if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
   {
      if (mInterpreter.get() == NULL)
      {
         return;
      }
      try
      {
         QTextBlock block = textCursor().block();
         int sz = mPos - block.position();
         QString cmd = block.text().remove(0, sz) + "\n";
         QByteArray cmdArray = cmd.toUtf8();
         auto_obj cnt(PyObject_CallMethod(mStdin, "write", "sl", cmdArray.data(), cmdArray.size()), true);
         checkErr();
         auto_obj useps1(PyObject_CallMethod(mInterpreter, "processEvent", NULL), true);
         checkErr();
         flushout();
      }
      catch(const PythonError&)
      {
         // nothing
      }
      mPos = textCursor().position();
      pEvent->accept();
   }
   else
   {
      QTextEdit::keyPressEvent(pEvent);
   }
}

void ConsoleWindow::flushout()
{
   auto_obj stderrAvailable(PyObject_CallMethod(mStderr, "available", NULL), true);
   checkErr();
   long stderrCount = PyInt_AsLong(stderrAvailable);
   if (stderrCount > 0)
   {
      auto_obj stderrStr(PyObject_CallMethod(mStderr, "read", "O", stderrAvailable), true);
      checkErr();
      QColor cur = textColor();
      setTextColor(Qt::red);
      append(QString(PyString_AsString(stderrStr)));
      setTextColor(cur);
   }

   auto_obj stdoutAvailable(PyObject_CallMethod(mStdout, "available", NULL), true);
   checkErr();
   long stdoutCount = PyInt_AsLong(stdoutAvailable);
   if (stdoutCount > 0)
   {
      auto_obj stdoutStr(PyObject_CallMethod(mStdout, "read", "O", stdoutAvailable), true);
      checkErr();
      append(QString(PyString_AsString(stdoutStr)));
   }
}

void ConsoleWindow::checkErr()
{
   if (PyErr_Occurred() != NULL)
   {
      PyObject* pException=NULL;
      PyObject* pVal=NULL;
      PyObject* pTb=NULL;
      PyErr_Fetch(&pException, &pVal, &pTb);
      PyErr_NormalizeException(&pException, &pVal, &pTb);
      PyErr_Clear();
      QString errStr;
      auto_obj traceback(PyImport_ImportModule("traceback"), true);
      if (PyErr_Occurred() != NULL)
      {
         PyErr_Clear();
         auto_obj msg(PyObject_GetAttrString(pVal, "message"), true);
         errStr = QString("Unable to import traceback module.\n%1: %2\npath=%3\n")
            .arg(PyObject_REPR(pException)).arg(PyObject_REPR(msg)).arg(PyObject_REPR(PySys_GetObject("path")));
      }
      else
      {
         auto_obj tracebackDict(PyModule_GetDict(traceback));
         auto_obj format_exception_only(PyDict_GetItemString(tracebackDict, "format_exception_only"));
         auto_obj exc(PyObject_CallFunction(format_exception_only, "OO", pException, pVal), true);
         errStr = QString("%1\n").arg(PyObject_REPR(exc));
         if (pTb != NULL)
         {
            auto_obj format_tb(PyDict_GetItemString(tracebackDict, "format_tb"));
            auto_obj tb(PyObject_CallFunction(format_tb, "O", pTb), true);
            errStr += QString("%1\n").arg(PyObject_REPR(tb));
         }
      }
      errStr.replace("\\n", "\n");
      QColor cur = textColor();
      setTextColor(Qt::red);
      append(errStr);
      setTextColor(cur);
      throw PythonError(errStr);
   }
}