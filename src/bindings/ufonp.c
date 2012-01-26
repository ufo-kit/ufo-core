#include <Python.h>
#include <pygobject.h>
#include <numpy/arrayobject.h>
#include "ufo-buffer.h"

static PyTypeObject *PyGObject_Type = NULL;

static PyObject *ufo_asarray(PyObject *self, PyObject *args)
{
    PyObject *py_buffer;
    guint *dim_size = NULL;
    guint num_dims = 0;

    if (!PyArg_ParseTuple(args, "O", &py_buffer))
        return NULL;

    UfoBuffer *buffer = UFO_BUFFER(pygobject_get(py_buffer));
    float *host_array = ufo_buffer_get_host_array(buffer, NULL);
    ufo_buffer_get_dimensions(buffer, &num_dims, &dim_size);

    npy_intp np_dim_size[num_dims];
    for (int i = 0; i < num_dims; i++)
        np_dim_size[i] = dim_size[i];

    PyObject *np_array = PyArray_NewFromDescr(&PyArray_Type, 
            PyArray_DescrFromType(NPY_FLOAT32),
            num_dims, np_dim_size, NULL,
            host_array, 0, NULL);

    return np_array;
}

static void resize_buffer(UfoBuffer *buffer, guint np_ndims, npy_intp *np_dim_size)
{
    guint buf_new_dim_size[np_ndims];
    for (int i = 0; i < np_ndims; i++)
        buf_new_dim_size[i] = np_dim_size[i];

    ufo_buffer_set_dimensions(buffer, np_ndims, buf_new_dim_size);
}

static PyObject *ufo_fromarray(PyObject *self, PyObject *args)
{
    PyObject *py_buffer;
    PyArrayObject *np_array;

    if (!PyArg_ParseTuple(args, "OO!", &py_buffer, &PyArray_Type, &np_array))
        return NULL;

    UfoBuffer *buffer = UFO_BUFFER(pygobject_get(py_buffer));
    guint buf_ndims = 0;
    guint *buf_dim_size = NULL;
    ufo_buffer_get_dimensions(buffer, &buf_ndims, &buf_dim_size);

    int np_ndims = PyArray_NDIM(np_array);
    npy_intp *np_dim_size = PyArray_DIMS(np_array);

    if (buf_ndims == np_ndims) {
        for (int i = 0; i < np_ndims; i++) {
            if (buf_dim_size[i] != np_dim_size[i]) {
                resize_buffer(buffer, buf_ndims, np_dim_size);
                break;
            }
        }
    }
    else {
        resize_buffer(buffer, np_ndims, np_dim_size);
    }

    float *host_array = ufo_buffer_get_host_array(buffer, NULL);
    memcpy(host_array, PyArray_DATA(np_array), ufo_buffer_get_size(buffer));
    return Py_BuildValue("");
}

static PyMethodDef exported_methods[] = {
    {"asarray", ufo_asarray, METH_VARARGS, "Convert UfoBuffer to Numpy array"},
    {"fromarray", ufo_fromarray, METH_VARARGS, "Convert Numpy array to UfoBuffer"},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initufonp(void)
{
    PyObject *module = Py_InitModule("ufonp", exported_methods);

    if (module == NULL)
        return;

    import_array();
    init_pygobject();
    module = PyImport_ImportModule("gobject");
    if (module) {
        PyGObject_Type = (PyTypeObject *) PyObject_GetAttrString(module, "GObject");
        Py_DECREF(module);
    }
}
