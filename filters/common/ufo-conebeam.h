/*
 * Copyright (C) 2015-2017 Karlsruhe Institute of Technology
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

#ifndef UFO_CONEBEAM_H
#define UFO_CONEBEAM_H

#include <glib.h>
#include <glib-object.h>

typedef enum {
    UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_X,
    UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Y,
    UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Z,
    UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_X,
    UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Y,
    UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Z,
    UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_X,
    UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Y,
    UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Z,
    UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_X,
    UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Y,
    UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Z,
    UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_X,
    UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Y,
    UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Z,
    UFO_UNI_RECO_PARAMETER_CENTER_POSITION_X,
    UFO_UNI_RECO_PARAMETER_CENTER_POSITION_Z,
    UFO_UNI_RECO_PARAMETER_Z
} UfoUniRecoParameter;

typedef struct {
    gchar *name;
    guint burst;
    guint max_regcount;
} UfoUniRecoNodeProps;

GHashTable *ufo_get_node_props_table (void);

#endif
