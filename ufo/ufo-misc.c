/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#ifdef WITH_PYTHON
#include <Python.h>
#endif

#include <ufo/ufo.h>

/**
 * ufo_signal_emit:
 * @instance: (type GObject.Object): the instance the signal is being emitted on.
 * @signal_id: the signal id
 * @detail: the detail
 * @...: parameters to be passed to the signal, followed by a
 *  location for the return value. If the return type of the signal
 *  is #G_TYPE_NONE, the return value location can be omitted.
 *
 * Emits a signal just like g_signal_emit(). In case ufo-core is compiled with
 * Python-support, the GIL will be locked before signalling.
 *
 * Note that g_signal_emit() resets the return value to the default
 * if no handlers are connected, in contrast to g_signal_emitv().
 */
void
ufo_signal_emit (gpointer instance,
                 guint signal_id,
                 GQuark detail,
                 ...)
{
    va_list var_args;

#ifdef WITH_PYTHON
    PyGILState_STATE state = PyGILState_Ensure ();
#endif

    va_start (var_args, detail);
    g_signal_emit_valist (instance, signal_id, detail, var_args);
    va_end (var_args);

#ifdef WITH_PYTHON
    PyGILState_Release (state);
#endif
}
