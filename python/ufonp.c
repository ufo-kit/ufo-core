#include <Python.h>
#include <pygobject.h>
#include <numpy/arrayobject.h>
#include <ufo/ufo.h>

static PyTypeObject *PyGObject_Type = NULL;

static PyObject *
ufo_asarray(PyObject *self, PyObject *args)
{
    PyObject *py_buffer;
    PyObject *np_array;
    UfoRequisition req;
    UfoBuffer *buffer;
    gfloat *host_array;

    if (!PyArg_ParseTuple(args, "O", &py_buffer))
        return NULL;

    buffer = UFO_BUFFER (pygobject_get (py_buffer));
    host_array = ufo_buffer_get_host_array (buffer, NULL);
    ufo_buffer_get_requisition (buffer, &req);

    npy_intp np_dim_size[req.n_dims];

    for (int i = 0; i < req.n_dims; i++)
        np_dim_size[i] = req.dims[i];

    np_array = PyArray_NewFromDescr (&PyArray_Type, PyArray_DescrFromType(NPY_FLOAT32),
                                     req.n_dims, np_dim_size, NULL, host_array, 0, NULL);

    return np_array;
}

static void
resize_buffer (UfoBuffer *buffer, guint np_ndims, npy_intp *np_dim_size)
{
    UfoRequisition req;

    req.n_dims = np_ndims;

    for (int i = 0; i < np_ndims; i++)
        req.dims[i] = np_dim_size[i];

    ufo_buffer_resize (buffer, &req);
}

static PyObject *
ufo_fromarray (PyObject *self, PyObject *args)
{
    PyArrayObject *np_array;
    guint np_ndims;
    npy_intp *np_dims;
    UfoBuffer *buffer;
    UfoRequisition req;
    float *host_array;

    if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &np_array))
        return NULL;

    np_ndims = PyArray_NDIM (np_array);
    np_dims = PyArray_DIMS (np_array);

    req.n_dims = np_ndims;

    for (guint i = 0; i < np_ndims; i++)
        req.dims[i] = np_dims[i];

    buffer = ufo_buffer_new (&req, NULL);
    host_array = ufo_buffer_get_host_array (buffer, NULL);
    memcpy (host_array, PyArray_DATA(np_array), ufo_buffer_get_size (buffer));

    return pygobject_new (G_OBJECT (buffer));
}

static PyObject *
ufo_fromarray_inplace (PyObject *self, PyObject *args)
{
    PyObject *py_buffer;
    UfoBuffer *buffer;
    UfoRequisition req;
    PyArrayObject *np_array;
    int np_ndims;
    npy_intp *np_dims;
    float *host_array;

    if (!PyArg_ParseTuple(args, "OO!", &py_buffer, &PyArray_Type, &np_array))
        return NULL;

    buffer = UFO_BUFFER (pygobject_get (py_buffer));
    ufo_buffer_get_requisition (buffer, &req);

    np_ndims = PyArray_NDIM (np_array);
    np_dims = PyArray_DIMS (np_array);

    if (req.n_dims == np_ndims) {
        for (int i = 0; i < np_ndims; i++) {
            if (req.dims[i] != np_dims[i]) {
                resize_buffer (buffer, req.n_dims, np_dims);
                break;
            }
        }
    }
    else {
        resize_buffer (buffer, np_ndims, np_dims);
    }

    host_array = ufo_buffer_get_host_array (buffer, NULL);
    memcpy (host_array, PyArray_DATA (np_array), ufo_buffer_get_size (buffer));
    return Py_BuildValue("");
}

static PyMethodDef exported_methods[] = {
    {"asarray", ufo_asarray, METH_VARARGS, "Convert UfoBuffer to Numpy array"},
    {"fromarray", ufo_fromarray, METH_VARARGS, "Convert Numpy array to UfoBuffer"},
    {"fromarray_inplace", ufo_fromarray_inplace, METH_VARARGS, "Convert Numpy array to UfoBuffer in-place"},
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
