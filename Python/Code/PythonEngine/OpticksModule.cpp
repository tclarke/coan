/*
 * The information in this file is
 * Copyright(c) 2007 Ball Aerospace & Technologies Corporation
 * and is subject to the terms and conditions of the
 * GNU Lesser General Public License Version 2.1
 * The license text is available from   
 * http://www.gnu.org/licenses/lgpl.html
 */

#include "AnimationController.h"
#include "AnimationServices.h"
#include "AnimationToolBar.h"
#include "AttachmentPtr.h"
#include "DataAccessor.h"
#include "DataAccessorImpl.h"
#include "DataRequest.h"
#include "DataVariant.h"
#include "DesktopServices.h"
#include "LayerList.h"
#include "ObjectResource.h"
#include "OpticksModule.h"
#include "PythonEngine.h"
#include "RasterDataDescriptor.h"
#include "RasterElement.h"
#include "RasterUtilities.h"
#include "Signature.h"
#include "SpatialDataView.h"
#include "SpatialDataWindow.h"
#include "StringUtilities.h"
#include "Undo.h"
#include <boost/any.hpp>
#include <numpy/arrayobject.h>

class AnimationCallbackManager
{
public:
   static void free(void* pPtr)
   {
      delete reinterpret_cast<AnimationCallbackManager*>(pPtr);
   }

   AnimationCallbackManager(AnimationController* pController, PyObject* pCoroutine) :
            mpController(pController), mpCoroutine(pCoroutine)
   {
      mpController.addSignal(SIGNAL_NAME(AnimationController, FrameChanged), Slot(this, &AnimationCallbackManager::frameChanged));
      Py_INCREF(mpCoroutine);
   }
   virtual ~AnimationCallbackManager()
   {
      Py_DECREF(mpCoroutine);
   }

private:
   void frameChanged(Subject& subject, const std::string& signal, const boost::any& data)
   {
      if (&subject == mpController.get())
      {
         double frame = boost::any_cast<double>(data);
         Py_XDECREF(PyObject_CallMethod(mpCoroutine, "send", "d", frame));
      }
   }

   AttachmentPtr<AnimationController> mpController;
   PyObject* mpCoroutine;
};

namespace OpticksModule
{
auto_obj opticksErr;

PyObject* get_data_element(PyObject* pSelf, PyObject* pArgs)
{
   const char* pName = NULL;
   const char* pType = NULL;
   const char* pParent = NULL;
   if (PyArg_ParseTuple(pArgs, "s|zz", &pType, &pName, &pParent) == 0)
   {
      return NULL;
   }
   std::string type(pType);

   if (type == "signature")
   {
      if (pName == NULL)
      {
         PyErr_SetString(opticksErr, "When getting a signature, an element name must be specified.");
         return NULL;
      }
      DataElement* pParentElement = NULL;
      if (pParent != NULL)
      {
         pParentElement = Service<ModelServices>()->getElement(pParent, std::string(), NULL);
      }
      Signature* pSig = static_cast<Signature*>(Service<ModelServices>()->getElement(
                  pName, TypeConverter::toString<Signature>(), pParentElement));
      if (pSig == NULL)
      {
         PyErr_SetString(opticksErr, "Invalid signature name.");
         return NULL;
      }
      auto_obj names(PySet_New(NULL), true);
      std::set<std::string> dataNames = pSig->getDataNames();
      for (std::set<std::string>::const_iterator dataName = dataNames.begin(); dataName != dataNames.end(); ++dataName)
      {
         auto_obj name(PyString_FromString(dataName->c_str()));
         PySet_Add(names, name);
      }
      auto_obj opaque(PyCObject_FromVoidPtr(reinterpret_cast<void*>(pSig), NULL), true);
      
      return Py_BuildValue("{sssssOsO}", "name", pSig->getName().c_str(),
            "type", "signature",
            "dataNames", names.take(),
            "opaque", opaque.take());
   }
   else if (type == "raster")
   {
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
      auto_obj opaqueview(PyCObject_FromVoidPtr(reinterpret_cast<void*>(pView), NULL), true);
      return Py_BuildValue("{sssss(III)sssssOsO}", "name", pElement->getName().c_str(),
            "type", "raster",
            "dims", pDesc->getRowCount(), pDesc->getColumnCount(), pDesc->getBandCount(),
            "interleave", StringUtilities::toXmlString(pDesc->getInterleaveFormat()).c_str(),
            "datatype", StringUtilities::toXmlString(pDesc->getDataType()).c_str(),
            "opaque", opaque.take(),
            "opaqueview", opaqueview.take());
   }
   PyErr_Format(opticksErr, "Unknown or unsupported data element type '%s'.", pType);
   return NULL;
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
   if (std::string(PyString_AsString(PyDict_GetItemString(pElementDict, "type"))) != "raster")
   {
      PyErr_SetString(PyExc_TypeError, "A raster element is required.");
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
   void* pRawData = NULL;
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
   else
   {
      // no sub-cube...try and get raw data pointer for efficiency..if this fails continue with the more generic method
      pRawData = pElement->getRawData();
   }
   pReq->setWritable(false);
   DataAccessor* pAcc = (pRawData != NULL) ? NULL : new DataAccessor(pElement->getDataAccessor(pReq.release()));
   if (pAcc != NULL && !pAcc->isValid())
   {
      PyErr_SetString(opticksErr, "Unable to access the requested sub-cube in the requested format.");
      delete pAcc;
      return NULL;
   }

   // create numpy array
   npy_intp pDims[3];
   switch (pDesc->getInterleaveFormat())
   {
   case BIP:
      pDims[0] = rows;
      pDims[1] = cols;
      pDims[2] = bands;
      break;
   case BIL:
      pDims[0] = rows;
      pDims[1] = bands;
      pDims[2] = cols;
      break;
   case BSQ:
      pDims[0] = bands;
      pDims[1] = rows;
      pDims[2] = cols;
      break;
   }
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
   auto_obj numpyArray(PyArray_NewFromDescr(&PyArray_Type, pArrayDesc, 3, pDims, NULL,
      (pRawData != NULL) ? pRawData : (*pAcc)->getRow(), NPY_CARRAY_RO, NULL), true);
   if (numpyArray.get() == NULL || PyErr_Occurred())
   {
      PyErr_SetString(opticksErr, "Unable to create numpy array.");
      delete pAcc;
      return NULL;
   }
   auto_obj opaque(PyCObject_FromVoidPtr(reinterpret_cast<void*>(pAcc), free_data_accessor), true);
   return Py_BuildValue("OO", opaque.take(), numpyArray.take());
}

PyObject* get_signature_data(PyObject* pSelf, PyObject* pArgs)
{
   PyObject* pSignatureDict = NULL;
   const char* pName = NULL;
   if (PyArg_ParseTuple(pArgs, "O!|z", &PyDict_Type, &pSignatureDict, &pName) == 0)
   {
      return NULL;
   }
   if (std::string(PyString_AsString(PyDict_GetItemString(pSignatureDict, "type"))) != "signature")
   {
      PyErr_SetString(PyExc_TypeError, "A signature is required.");
      return NULL;
   }
   Signature* pSig = reinterpret_cast<Signature*>(
      PyCObject_AsVoidPtr(PyDict_GetItemString(pSignatureDict, "opaque")));
   if (pSig == NULL)
   {
      PyErr_SetString(opticksErr, "Invalid signature.");
      return NULL;
   }

   std::string name((pName == NULL) ? "" : pName);
   if (name.empty())
   {
      if (pSig->getDataNames().size() != 1)
      {
         PyErr_SetString(opticksErr, "A data name must be specified.");
         return NULL;
      }
      name = *(pSig->getDataNames().begin());
   }
   std::vector<double> data;
   try
   {
      data = dv_cast<std::vector<double> >(pSig->getData(name));
   }
   catch (const std::bad_cast&)
   {
      PyErr_Format(PyExc_ValueError, "Data item '%s' is not a vector of doubles.", name.c_str());
      return NULL;
   }

   // create numpy array
   npy_intp pDims[] = {data.size()};
   PyArray_Descr* pArrayDesc = PyArray_DescrFromType(NPY_DOUBLE);

   auto_obj numpyArray(PyArray_NewFromDescr(&PyArray_Type, pArrayDesc, 1, pDims, NULL, NULL, 0, NULL), true);
   if (numpyArray.get() == NULL || PyErr_Occurred())
   {
      PyErr_SetString(opticksErr, "Unable to create numpy array.");
      return NULL;
   }
   memcpy(PyArray_DATA(numpyArray.get()), &data.front(), data.size() * sizeof(double));
   return Py_BuildValue("O", numpyArray.take());
}

PyObject* new_raster_from_array(PyObject* pSelf, PyObject* pArgs)
{
   const char* pName = NULL;
   PyObject* pArr = NULL;
   const char* pType = NULL;
   const char* pInterleave = NULL;
   PyObject* pReplace = NULL;
   PyObject* pElementDict = NULL;
   if (PyArg_ParseTuple(pArgs, "sO!|ssOO!", &pName, &PyArray_Type, &pArr, &pType, &pInterleave, &pReplace, &PyDict_Type, &pElementDict) == 0)
   {
      return NULL;
   }
   int ndim = PyArray_NDIM(pArr);
   if (ndim < 2 || ndim > 3)
   {
      PyErr_SetString(opticksErr, "Only two and three dimensional arrays are supported.");
      return NULL;
   }
   InterleaveFormatType interleave(BIP);
   std::string interleaveStr(pInterleave);
   if (interleaveStr == "BSQ")
   {
      interleave = BSQ;
   }
   else if (interleaveStr == "BIL")
   {
      interleave = BIL;
   }
   unsigned int rows = (ndim == 2) ? static_cast<unsigned int>(PyArray_DIM(pArr, 0)) : 0;
   unsigned int cols = (ndim == 2) ? static_cast<unsigned int>(PyArray_DIM(pArr, 1)) : 0;
   unsigned int bands = 1;
   if (ndim == 3)
   {
      switch (interleave)
      {
      case BIP:
         rows = static_cast<unsigned int>(PyArray_DIM(pArr, 0));
         cols = static_cast<unsigned int>(PyArray_DIM(pArr, 1));
         bands = static_cast<unsigned int>(PyArray_DIM(pArr, 2));
         break;
      case BIL:
         rows = static_cast<unsigned int>(PyArray_DIM(pArr, 0));
         bands = static_cast<unsigned int>(PyArray_DIM(pArr, 1));
         cols = static_cast<unsigned int>(PyArray_DIM(pArr, 2));
         break;
      case BSQ:
         bands = static_cast<unsigned int>(PyArray_DIM(pArr, 0));
         rows = static_cast<unsigned int>(PyArray_DIM(pArr, 1));
         cols = static_cast<unsigned int>(PyArray_DIM(pArr, 2));
         break;
      }
   }
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

   bool existingReplaced = false;
   ModelResource<RasterElement> pElement(RasterUtilities::createRasterElement(pName, rows, cols, bands, dataType, interleave));
   if (pElement.get() == NULL && (pReplace == NULL || PyObject_IsTrue(pReplace)))
   {
      // see if there's an existing data element we can overwrite
      pElement = ModelResource<RasterElement>(static_cast<RasterElement*>(
         Service<ModelServices>()->getElement(pName, TypeConverter::toString<RasterElement>(), NULL)));
      existingReplaced = true;
   }
   if (pElement.get() == NULL)
   {
      PyErr_SetString(opticksErr, "Unable to create data element.");
      return NULL;
   }
   const RasterDataDescriptor* pDesc = static_cast<const RasterDataDescriptor*>(pElement->getDataDescriptor());
   if (pDesc->getRowCount() != rows || pDesc->getColumnCount() != cols || pDesc->getBandCount() != bands
      || pDesc->getDataType() != dataType || pDesc->getInterleaveFormat() != interleave)
   {
      // The element must have existed but has different params
      PyErr_SetString(opticksErr, "Data element exists and can not be replaced.");
      if (existingReplaced)
      {
         pElement.release(); // don't want to delete the existing element
      }
      return NULL;
   }

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

   std::string type(pType);
   if (type == "view")
   {
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
   }
   else if (type == "raster" || type == "pseudocolor" || type == "threshold")
   {
      SpatialDataView* pView = (pElementDict == NULL) ? NULL : reinterpret_cast<SpatialDataView*>(
               PyCObject_AsVoidPtr(PyDict_GetItemString(pElementDict, "opaqueview")));
      if (pView == NULL)
      {
         PyErr_SetString(opticksErr, "Can't locate view.");
         return NULL;
      }
      LayerType layerType;
      if (type == "raster")
      {
         layerType = RASTER;
      }
      else if (type == "pseudocolor")
      {
         layerType = PSEUDOCOLOR;
      }
      else if (type == "threshold")
      {
         layerType = THRESHOLD;
      }
      // see if we need to create a new layer
      if (existingReplaced && (pView->getLayerList()->getLayer(layerType, pElement.get()) != NULL))
      {
         pElement.release();
      }
      else
      {
         UndoLock undo(pView);
         pView->createLayer(layerType, pElement.release());
      }
   }
   else
   {
      // don't create any layers
      pElement.release();
   }
   Py_RETURN_NONE;
}

PyObject* connect_animation(PyObject* pSelf, PyObject* pArgs)
{
   const char* pControllerName = NULL;
   PyObject* pCoroutine = NULL;
   if (PyArg_ParseTuple(pArgs, "zO!", &pControllerName, &PyGen_Type, &pCoroutine) == 0)
   {
      return NULL;
   }
   if (pCoroutine == NULL)
   {
      PyErr_SetString(opticksErr, "Invalid coroutine.");
      return NULL;
   }
   AnimationController* pController = NULL;
   if (pControllerName == NULL)
   {
      AnimationToolBar* pToolBar = static_cast<AnimationToolBar*>(Service<DesktopServices>()->getWindow("Animation", TOOLBAR));
      pController = (pToolBar == NULL) ? NULL : pToolBar->getAnimationController();
   }
   else
   {
      std::string controllerName(pControllerName);
      pController = Service<AnimationServices>()->getAnimationController(controllerName);
   }
   if (pController == NULL)
   {
      PyErr_SetString(opticksErr, "Animation controller not found.");
      return NULL;
   }
   AnimationCallbackManager* pCallback = new AnimationCallbackManager(pController, pCoroutine);

   auto_obj opaque(PyCObject_FromVoidPtr(reinterpret_cast<void*>(pCallback), AnimationCallbackManager::free), true);
   return Py_BuildValue("O", opaque.take());
}

PyMethodDef opticksMethods[] = {
   {"get_data_element", get_data_element, METH_VARARGS, ""},
   {"get_raster_data", get_raster_data, METH_VARARGS, ""},
   {"new_raster_from_array", new_raster_from_array, METH_VARARGS, ""},
   {"get_signature_data", get_signature_data, METH_VARARGS, ""},
   {"connect_animation", connect_animation, METH_VARARGS, ""},
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