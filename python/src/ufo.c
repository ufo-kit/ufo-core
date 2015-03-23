#include <Python.h>
#include <pygobject.h>
#include <numpy/arrayobject.h>
#include <ufo/ufo.h>


static PyObject *
asarray (PyObject *self, PyObject *args)
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
        np_dim_size[i] = req.dims[req.n_dims - 1 - i];

    np_array = PyArray_NewFromDescr (&PyArray_Type, PyArray_DescrFromType (NPY_FLOAT32),
                                     req.n_dims, np_dim_size, NULL, host_array, 0, NULL);

    return np_array;
}

static void
resize_buffer (UfoBuffer *buffer, guint np_ndims, npy_intp *np_dim_size)
{
    UfoRequisition req;

    req.n_dims = np_ndims;

    for (int i = 0; i < np_ndims; i++)
        req.dims[i] = np_dim_size[np_ndims - 1 - i];

    ufo_buffer_resize (buffer, &req);
}

static PyObject *
fromarray (PyObject *self, PyObject *args)
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
        req.dims[i] = np_dims[np_ndims - 1 - i];

    buffer = ufo_buffer_new (&req, NULL);
    host_array = ufo_buffer_get_host_array (buffer, NULL);
    memcpy (host_array, PyArray_DATA (np_array), ufo_buffer_get_size (buffer));

    return pygobject_new (G_OBJECT (buffer));
}

static PyObject *
fromarray_inplace (PyObject *self, PyObject *args)
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
            if (req.dims[i] != np_dims[np_ndims - 1 - i]) {
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

static PyObject *
empty_like (PyObject *self, PyObject *args)
{
    PyArrayObject *np_array;
    guint np_ndims;
    npy_intp *np_dims;
    UfoRequisition req;

    if (!PyArg_ParseTuple (args, "O!", &PyArray_Type, &np_array))
        return NULL;

    np_ndims = PyArray_NDIM (np_array);
    np_dims = PyArray_DIMS (np_array);

    req.n_dims = np_ndims;

    for (guint i = 0; i < np_ndims; i++)
        req.dims[i] = np_dims[np_ndims - 1 - i];

    return pygobject_new (G_OBJECT (ufo_buffer_new (&req, NULL)));
}

static PyMethodDef exported_methods[] = {
    {"asarray",             asarray,            METH_VARARGS, "Convert UfoBuffer to Numpy array"},
    {"fromarray",           fromarray,          METH_VARARGS, "Convert Numpy array to UfoBuffer"},
    {"fromarray_inplace",   fromarray_inplace,  METH_VARARGS, "Convert Numpy array to UfoBuffer in-place"},
    {"empty_like",          empty_like,         METH_VARARGS, "Create UfoBuffer with dimensions of NumPy array"},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
init_ufo(void)
{
    PyObject *module = Py_InitModule("_ufo", exported_methods);

    if (module == NULL)
        return;

    import_array();
    pygobject_init (-1, -1, -1);
}
