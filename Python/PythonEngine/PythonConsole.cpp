/*
 * The information in this file is
 * subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "PythonVersion.h"
#include "PythonConsole.h"
#include "PlugInFactory.h"
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <QtGui/QKeyEvent>

PLUGINFACTORY(PythonConsole);

namespace
{
   QString prompt1("python> ");
   class auto_obj
   {
   public:
      auto_obj(PyObject* pObj, bool takeOwnership=false) : mpObj(pObj), mOwned(takeOwnership) {}
      ~auto_obj()
      {
         if (mOwned)
         {
            Py_XDECREF(mpObj);
         }
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
      PyObject* borrow()
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

   PyObject* QIODeviceWrite(PyObject* pSelf, PyObject* pArgs)
   {
      Py_ssize_t opaque = 0;
      Py_ssize_t strLen = 0;
      char* pStr = NULL;
      if (PyArg_ParseTuple(pArgs, "ns#", &opaque, &pStr, &strLen) == 0)
      {
         PyErr_SetString(PyExc_ValueError, "Invalid arguments. Opaque handle and string required.");
         return NULL;
      }
      QIODevice* pDev = reinterpret_cast<QIODevice*>(opaque);
      if (pDev == NULL)
      {
         PyErr_SetString(PyExc_ValueError, "Invalid opaque handle.");
         return NULL;
      }
      pDev->write(pStr, strLen);
      Py_RETURN_NONE;
   }


   PyMethodDef opticksMethods[] = {
      {"QIODeviceWrite", QIODeviceWrite, METH_VARARGS, "Pass an opaque handle and string."},
      {NULL, NULL, 0, NULL} // sentinel
   };
}

PyMODINIT_FUNC initOpticks(void)
{
   Py_InitModule("_opticks", opticksMethods);
}

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

   mStdOut.open(QIODevice::ReadWrite);
   mStdErr.open(QIODevice::ReadWrite);

   Py_Initialize();
   initOpticks();
   QString initStr = QString(
      "class optickswrite:\n"
      "  def __init__(self, opaque):\n"
      "    self.__opaque = opaque\n"
      "  def write(self, s):\n"
      "    _opticks.QIODeviceWrite(self.__opaque, s)\n\n"
      "import sys\n"
      "sys.stdin = None\n"
      "sys.stdout = optickswrite(%1)\n"
      "sys.stderr = optickswrite(%2)\n").arg(reinterpret_cast<Py_ssize_t>(&mStdOut)).arg(reinterpret_cast<Py_ssize_t>(&mStdErr));
   QByteArray initBytes = initStr.toAscii();
   PyRun_SimpleString(initBytes.data());

   append(mStdErr.readAll());
   append(mStdOut.readAll());
   append(prompt1);
}

ConsoleWindow::~ConsoleWindow()
{
   Py_Finalize();
}

void ConsoleWindow::keyPressEvent(QKeyEvent* pEvent)
{
   if (pEvent->key() == Qt::Key_Enter || pEvent->key() == Qt::Key_Return)
   {
      QStringList lines = toPlainText().split("\n", QString::SkipEmptyParts);
      if (!lines.empty())
      {
         QString cmd = lines.back();
         if (cmd.startsWith(prompt1))
         {
            cmd.remove(0, prompt1.size());
         }
         if (PyRun_SimpleString(cmd.toAscii()) == -1)
         {
            append("Error occured!\n");
            // err
         }
      }
      append(mStdErr.readAll());
      append(mStdOut.readAll());
      append(prompt1);
      pEvent->accept();
   }
   else
   {
      QTextEdit::keyPressEvent(pEvent);
   }
}
