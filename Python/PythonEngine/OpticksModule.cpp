/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DesktopServices.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "OpticksModule.h"
#include "PythonConsole.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "StringUtilities.h"
#include "Undo.h"
#include <numpy/arrayobject.h>
#include <QtCore/QtDebug>

namespace OpticksModule
{
auto_obj opticksErr;

PyObject* get_data_element(PyObject* pSelf, PyObject* pArgs)
{
   const char* pName = NULL;
   if (PyArg_ParseTuple(pArgs, "|z", &pName) == 0)
   {
      return NULL;
   }

   SpatialDataView* pView = NULL;
   if (pName == NULL)
   {
      // get the primary in the active view
      pView = dynamic_cast<SpatialDataView*>(Service<DesktopServices>()->getCurrentWorkspaceWindowView());
      if (pView == NULL)
      {
         PyErr_SetString(opticksErr, "There is no active spatial data view.");
         return NULL;
      }
   }
   else
   {
      SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(Service<DesktopServices>()->getWindow(pName, SPATIAL_DATA_WINDOW));
      pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
      if (pView == NULL)
      {
         PyErr_SetString(opticksErr, "Invalid view name.");
         return NULL;
      }
   }
   RasterElement* pElement = pView->getLayerList()->getPrimaryRasterElement();
   if (pElement == NULL)
   {
      PyErr_SetString(opticksErr, "Invalid raster element.");
      return NULL;
   }
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   auto_obj opaque(PyCObject_FromVoidPtr(reinterpret_cast<void*>(pElement), NULL), true);
   return Py_BuildValue("{sss(III)sssssO}", "name", pElement->getName().c_str(),
         "dims", pDesc->getRowCount(), pDesc->getColumnCount(), pDesc->getBandCount(),
         "interleave", StringUtilities::toXmlString(pDesc->getInterleaveFormat()).c_str(),
         "datatype", StringUtilities::toXmlString(pDesc->getDataType()).c_str(),
         "opaque", opaque);
}

void free_data_accessor(void* pPtr)
{
   DataAccessor* pAcc = reinterpret_cast<DataAccessor*>(pPtr);
   delete pAcc;
}

PyObject* get_raster_data(PyObject* pSelf, PyObject* pArgs)
{
   PyObject* pElementDict = NULL;
   PyObject* pSubcubeDict = NULL;
   if (PyArg_ParseTuple(pArgs, "O!|O", &PyDict_Type, &pElementDict, &pSubcubeDict) == 0)
   {
      return NULL;
   }
   RasterElement* pElement = reinterpret_cast<RasterElement*>(
      PyCObject_AsVoidPtr(PyDict_GetItemString(pElementDict, "opaque")));
   if (pElement == NULL)
   {
      PyErr_SetString(opticksErr, "Invalid raster element.");
      return NULL;
   }

   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   FactoryResource<DataRequest> pReq;
   int rows = pDesc->getRowCount();
   int cols = pDesc->getColumnCount();
   int bands = pDesc->getBandCount();
   if (pSubcubeDict != NULL && pSubcubeDict != Py_None)
   {
      if (!PyDict_Check(pSubcubeDict))
      {
         PyErr_SetString(opticksErr, "Subcube specification must be a dict.");
         return NULL;
      }
      auto_obj rowSpec(PyDict_GetItemString(pSubcubeDict, "rows"));
      auto_obj bandSpec(PyDict_GetItemString(pSubcubeDict, "bands"));
      if (rowSpec.get() != NULL && PyTuple_Check(rowSpec.get()))
      {
         DimensionDescriptor startRow(pDesc->getActiveRow(0));
         DimensionDescriptor endRow(pDesc->getActiveRow(rows-1));
         if (PyTuple_GET_SIZE(rowSpec.get()) >= 1)
         {
            startRow = pDesc->getActiveRow(PyInt_AsLong(PyTuple_GET_ITEM(rowSpec.get(), 0)) - 1);
            if (PyErr_Occurred())
            {
               return NULL;
            }
         }
         if (PyTuple_GET_SIZE(rowSpec.get()) >= 2)
         {
            endRow = pDesc->getActiveRow(PyInt_AsLong(PyTuple_GET_ITEM(rowSpec.get(), 1)) - 1);
            if (PyErr_Occurred())
            {
               return NULL;
            }
         }
         rows = endRow.getActiveNumber() - startRow.getActiveNumber() + 1;
         pReq->setRows(startRow, endRow, rows);
      }
      if (pDesc->getInterleaveFormat() != BIP && bandSpec.get() != NULL)
      {
         DimensionDescriptor band;
         if (PyTuple_Check(bandSpec.get()))
         {
            pDesc->getActiveBand(PyInt_AsLong(PyTuple_GetItem(bandSpec, 0)) - 1);
         }
         else
         {
            pDesc->getActiveBand(PyInt_AsLong(bandSpec) - 1);
         }
         if (PyErr_Occurred())
         {
            return NULL;
         }
         bands = 1;
         pReq->setBands(band, band, bands);
      }
   }
   pReq->setWritable(false);
   DataAccessor* pAcc = new DataAccessor(pElement->getDataAccessor(pReq.release()));
   if (!pAcc->isValid())
   {
      PyErr_SetString(opticksErr, "Unable to access the requested sub-cube in the requested format.");
      delete pAcc;
      return NULL;
   }

   // create numpy array
   npy_intp pDims[] = {rows,cols,bands};
   PyArray_Descr* pArrayDesc = NULL;
   switch(pDesc->getDataType())
   {
   case INT1SBYTE:
      pArrayDesc = PyArray_DescrFromType(NPY_INT8);
      break;
   case INT1UBYTE:
      pArrayDesc = PyArray_DescrFromType(NPY_UINT8);
      break;
   case INT2SBYTES:
      pArrayDesc = PyArray_DescrFromType(NPY_INT16);
      break;
   case INT2UBYTES:
      pArrayDesc = PyArray_DescrFromType(NPY_UINT16);
      break;
   case INT4SBYTES:
      pArrayDesc = PyArray_DescrFromType(NPY_INT32);
      break;
   case INT4UBYTES:
      pArrayDesc = PyArray_DescrFromType(NPY_UINT32);
      break;
   case FLT4BYTES:
      pArrayDesc = PyArray_DescrFromType(NPY_FLOAT);
      break;
   case FLT8BYTES:
      pArrayDesc = PyArray_DescrFromType(NPY_DOUBLE);
      break;
   case FLT8COMPLEX:
      pArrayDesc = PyArray_DescrFromType(NPY_COMPLEX64);
      break;
   }
   if (pArrayDesc == NULL)
   {
      PyErr_SetString(opticksErr, "Opticks data format not supported in numpy.");
      delete pAcc;
      return NULL;
   }
   auto_obj numpyArray(PyArray_NewFromDescr(&PyArray_Type, pArrayDesc, 3, pDims, NULL, (*pAcc)->getRow(), NPY_CARRAY_RO, NULL), true);
   if (numpyArray.get() == NULL || PyErr_Occurred())
   {
      PyErr_SetString(opticksErr, "Unable to create numpy array.");
      delete pAcc;
      return NULL;
   }
   auto_obj opaque(PyCObject_FromVoidPtr(reinterpret_cast<void*>(pAcc), free_data_accessor), true);
   return Py_BuildValue("OO", opaque.take(), numpyArray);
}

PyObject* new_raster_from_array(PyObject* pSelf, PyObject* pArgs)
{
   const char* pName = NULL;
   PyObject* pArr = NULL;
   if (PyArg_ParseTuple(pArgs, "sO!", &pName, &PyArray_Type, &pArr) == 0)
   {
      return NULL;
   }
   int ndim = PyArray_NDIM(pArr);
   if (ndim < 2 || ndim > 3)
   {
      PyErr_SetString(opticksErr, "Only two and three dimensional arrays are supported.");
      return NULL;
   }
   unsigned int rows = PyArray_DIM(pArr, 0);
   unsigned int cols = PyArray_DIM(pArr, 1);
   unsigned int bands = (ndim == 3) ? PyArray_DIM(pArr, 2) : 1;
   EncodingType dataType;
   switch (PyArray_TYPE(pArr))
   {
   case NPY_INT8:
      dataType = INT1SBYTE;
      break;
   case NPY_UINT8:
      dataType = INT1UBYTE;
      break;
   case NPY_INT16:
      dataType = INT2SBYTES;
      break;
   case NPY_UINT16:
      dataType = INT2UBYTES;
      break;
   case NPY_INT32:
      dataType = INT4SBYTES;
      break;
   case NPY_UINT32:
      dataType = INT4UBYTES;
      break;
   case NPY_FLOAT:
      dataType = FLT4BYTES;
      break;
   case NPY_DOUBLE:
      dataType = FLT8BYTES;
      break;
   case NPY_COMPLEX64:
      dataType = FLT8COMPLEX;
      break;
   }
   if (!dataType.isValid())
   {
      PyErr_SetString(opticksErr, "Numpy data type not supported.");
      return NULL;
   }
   ModelResource<RasterElement> pElement(RasterUtilities::createRasterElement(pName, rows, cols, bands, dataType));
   if (pElement.get() == NULL)
   {
      PyErr_SetString(opticksErr, "Unable to create data element.");
      return NULL;
   }
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (PyArray_FLAGS(pArr) & (NPY_CARRAY_RO))
   {
      FactoryResource<DataRequest> pReq;
      pReq->setWritable(true);
      pReq->setRows(pDesc->getActiveRow(0), pDesc->getActiveRow(rows - 1), rows);
      DataAccessor acc(pElement->getDataAccessor(pReq.release()));
      memcpy(acc->getRow(), PyArray_DATA(pArr), PyArray_NBYTES(pArr));
      pElement->updateData();
   }
   else
   {
      PyErr_SetString(opticksErr, "Numpy array must be C contiguous and aligned.");
      return NULL;
   }

   SpatialDataWindow* pWindow = static_cast<SpatialDataWindow*>(
      Service<DesktopServices>()->createWindow(pName, SPATIAL_DATA_WINDOW));
   SpatialDataView* pView = (pWindow == NULL) ? NULL : pWindow->getSpatialDataView();
   if (pView == NULL)
   {
      PyErr_SetString(opticksErr, "Unable to create view.");
      return NULL;
   }
   pView->setPrimaryRasterElement(pElement.get());
   UndoLock undo(pView);
   pView->createLayer(RASTER, pElement.release());
   Py_RETURN_NONE;
}

PyMethodDef opticksMethods[] = {
   {"get_data_element", get_data_element, METH_VARARGS, ""},
   {"get_raster_data", get_raster_data, METH_VARARGS, ""},
   {"new_raster_from_array", new_raster_from_array, METH_VARARGS, ""},
   {NULL, NULL, 0, NULL} // sentinel
};

} // namespace

PyMODINIT_FUNC init_opticks()
{
   auto_obj pModule(Py_InitModule("_opticks", OpticksModule::opticksMethods));
   if (pModule.get() == NULL)
   {
      return;
   }
   OpticksModule::opticksErr.reset(PyErr_NewException("opticks.error", NULL, NULL));
   OpticksModule::opticksErr.take();
   PyModule_AddObject(pModule, "error", OpticksModule::opticksErr);
   import_array();
}