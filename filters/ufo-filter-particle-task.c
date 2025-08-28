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
 *
 * Authored by: Alexandre Lewkowicz (lewkow_a@epita.fr)
 */
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "ufo-filter-particle-task.h"
#include "ufo-ring-coordinates.h"

struct _UfoFilterParticleTaskPrivate {
    unsigned *img;
    size_t num_pixels;
    float threshold;
    float min;
};

/*Maximal number of particles that can be tagged*/
#define nVectMax 100000


static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFilterParticleTask, ufo_filter_particle_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FILTER_PARTICLE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_PARTICLE_TASK, UfoFilterParticleTaskPrivate))

enum {
    PROP_0,
    PROP_MIN,
    PROP_THRESHOLD,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_filter_particle_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FILTER_PARTICLE_TASK, NULL));
}

static void
ufo_filter_particle_task_setup (UfoTask *task,
                                UfoResources *resources,
                                GError **error)
{
}

static void
ufo_filter_particle_task_get_requisition (UfoTask *task,
                                          UfoBuffer **inputs,
                                          UfoRequisition *requisition,
                                          GError **error)
{
    UfoFilterParticleTaskPrivate *priv = UFO_FILTER_PARTICLE_TASK_GET_PRIVATE (task);
    requisition->n_dims = 1;
    ufo_buffer_get_requisition(inputs[0], requisition);
    size_t num_pixels = requisition->dims[0] * requisition->dims[1];

    /* Allocate internal buffer to hold copy of image */
    /* Reuse buffer when size is the smae */
    if (priv->num_pixels != num_pixels) {
        priv->num_pixels = num_pixels;
        priv->img = realloc (priv->img, sizeof (unsigned) * num_pixels);
    }

    /* Output size varies according to  data whithin the input */
    /* I don't know the output size, I'll allocate it myself once I have
     * processed the data */
    requisition->dims[0] = 0;
    requisition->n_dims = 1;
}

static guint
ufo_filter_particle_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_filter_particle_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 2;
}

static UfoTaskMode
ufo_filter_particle_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static void
part_cen_radius(float *img, unsigned *bin, unsigned nc, unsigned nr,
                unsigned pc, double *cx, double *cy, double *rr)
{
    /* nr == number of rows */
    /* nc == number of columns */
    /* pc == pixel count, (i-e nb of detected rings) */
    /* cx, cy, rr, (x, y, r) coordinates of each ring */
    unsigned i,j,k, itop, ibot, jleft, jright;
    unsigned b;
    unsigned *tops,*bots,*lefts,*rights;
    float cenx, radx, ceny, rady, wei;

    tops = malloc((pc+1)*sizeof(unsigned) * 100);
    bots = malloc((pc+1)*sizeof(unsigned) * 100);
    lefts = malloc((pc+1)*sizeof(unsigned) * 100);
    rights = malloc((pc+1)*sizeof(unsigned) * 100);

    for (k=0;k<=pc;k++) {
        tops[k]= (unsigned)nr;  bots[k]=0;  lefts[k]=(unsigned)nc; rights[k]=0;
    }

    /* Find extremities of each clustered pixels */
    for (i=0;i<nr;i++) {
        for(j=0;j<nc;j++) {
            b = bin[i * nc + j];
            if ( tops[b]>i ) tops[b]=i;
            if ( bots[b]<i ) bots[b]=i;
            if ( lefts[b]>j ) lefts[b]=j;
            if ( rights[b]<j ) rights[b]=j;
        }
    }

    /* calculate center of each clustered pixel using img to put weight on each
     * candidate pixel, at the same time calculate radius of the cluster */
    for (k = 0; k < pc; k++) {
        itop=tops[k+1];     ibot=bots[k+1];
        jleft=lefts[k+1];   jright=rights[k+1];

        cenx=0.0;    radx=0.0;
        ceny=0.0;    rady=0.0;
        wei=0.0;

        for (i=itop;i<=ibot;i++) {
            for (j=jleft;j<=jright;j++)
            {
                if (bin[i * nc + j]==k+1) {
                    float value = img[i * nc + j];
                    cenx += (float) j * value;
                    radx = radx + (float) (j * j) * value;
                    ceny = ceny + (float) i * value;
                    rady = rady + (float) (i * i) * value;
                    wei = wei + value;
                }
            }
        }
        cenx=cenx/wei;
        ceny=ceny/wei;
        radx=sqrtf (fabsf ((radx/wei) - cenx*cenx)) + (float) 0.25;
        rady=sqrtf (fabsf ((rady/wei) - ceny*ceny)) + (float) 0.25;

        cx[k] = cenx+1;
        cy[k] = ceny+1;
        rr[k] = sqrt(radx*rady);
    }

    free(tops);  free(bots);  free(lefts);  free(rights);

}

static void
pfind (float *input, unsigned *out, unsigned nc, unsigned nr, unsigned *pcount,
       double threshold)
{
    unsigned pc;
    unsigned ndup;
    unsigned i,j,k;
    int ncase;
    char iodd;
    unsigned ncc;
    unsigned  *dupx,*dupy;
    unsigned *dupindex, *binloc;
    unsigned k1,k2,temp;
 
    unsigned b;
 
    pc=0;
    ndup=0;
 
    dupx = malloc (nVectMax * sizeof (unsigned));
    dupy = malloc (nVectMax * sizeof (unsigned));

    /* 0, 0 is top left of image */
    for(j=0;j<nc;j++) {
        for(i=0;i<nr;i++) {
            /* Bit mask of surrounding pixels */
            ncase=0;
            /* Set if bottom left pixel is set */
            iodd=0;
            if(input[i * nc +  j] > threshold) {
                /* Bottom left pixel */
                if (i < (nr - 1) && j > 0 && input[(i + 1) * nc + (j - 1)] > threshold)
                { ncase += 1; iodd = 1; }
                /* Bottom right pixel */
                if (i < (nr - 1) && j < (nc - 1) && input[(i + 1) * nc + (j + 1)] > threshold)
                    ncase += 2;
                /* Top right pixel */
                if (i > 0 && j < (nc - 1) && input[(i - 1) * nc + (j + 1)] > threshold)
                    ncase += 4;
                /* Top left pixel */
                if (i > 0 && j > 0 && input[(i - 1) * nc + (j - 1)] > threshold)
                    ncase += 8;
                /* Bottom pixel */
                if (i < (nr - 1) && input[(i + 1) * nc + j] > threshold)
                    ncase += 16;
                /* Right pixel */
                if (j < (nc - 1) && input[i * nc + (j + 1)] > threshold)
                    ncase += 32;
                /* Top pixel */
                if (i > 0 && input[(i - 1) * nc + j] > threshold)
                    ncase += 64;
                /* Left pixel */
                if (j > 0 && input[i * nc + (j - 1)] > threshold)
                    ncase += 128;

                /* Pixel has no : Bottom, Right, Top, Left */
                if (ncase <= 15) {
                    //  if (NO1 == 1)
                    //    bin[i][j] = ++pc;
                    //  else
                    out[i * nc + j] = 0;
                }
                /* Pixel has no : Right, Top, Left */
                /* and Pixel has : Bottom */
                else if (ncase <= 31) {
                    if(iodd == 1)
                        out[i * nc + j] = out[(i + 1) * nc + (j - 1)]; /* Clone id */
                    else
                        out[i * nc + j] = ++pc;
                }
                /* Pixel has no : Top, Left, Bottom */
                /* Pixel has : Right */
                else if (ncase <= 47)
                    out[i * nc + j] = ++pc;
                /* Pixel has no : Top, Left */
                /* Pixel has : Right, Bottom */
                else if (ncase <= 63) {
                    if (iodd == 1)
                        out[i * nc + j] = out[(i + 1) * nc + (j - 1)];
                    else
                        out[i * nc + j] = ++pc;
                }
                /* Pixel has no : Left */
                /* Pixel has : Top */
                else if (ncase <= 127)
                    out[i * nc + j] = out[(i - 1) * nc + j];
                /* Pixel has : Left */
                else
                    out[i * nc + j] = out[i * nc + (j - 1)];


                /* Bottom Left && Bottom && Top && not(Left) */
                if ((ncase & 1) && (ncase & 16) && (ncase & 64) && !(ncase & 128)) {
                    /* Find duplicates, and locate them by x, and y list */
                    /* These duplicates shall be joined later */
                    k1 = out[i * nc + j];
                    /* 11
                     *  1
                     * 22
                     * in this case, bottom left was processed later, which
                     * explains the comparison */
                    k2 = out[(i + 1) * nc + (j - 1)];
                    dupx[ndup] = (k1 > k2 ? k2 : k1);
                    dupy[ndup] = (k1 > k2 ? k1 : k2);
                    ndup++;
                }
            }
            else
                out[i * nc + j] = 0;
        }
    }

    dupindex = malloc ((unsigned) (pc + 1) * sizeof (unsigned));
    /* binloc stores the 1D position (on row basis, like matlab) of each
     * candidate block of pixel (Most left-top of block)
     * or MAX_POS_VAL otherwise */
    binloc = malloc ((unsigned) (pc + 1) * sizeof(unsigned));
    for(k=0;k<=pc;k++) {
        binloc[k]= nr * nc;
        dupindex[k] = k;
    }

    for(j=0;j<nc;j++) {
        for(i=0;i<nr;i++) {
            b = out[i * nc + j];
            if(b > 0 && binloc[b] > (j*nr+i) )
                binloc[b]=j*nr+i;
        }
    }

    for(k=0; k < ndup; k++) {
        /* 11
         *  1
         * 22 in this case*/
        if( binloc[dupx[k]] > binloc[dupy[k]] ) {
            /* Dupx has always been afected the smaller id
             * ==> with row wise basis, locations dupx > dupy is always false */
            temp = dupx[k];   dupx[k]=dupy[k];    dupy[k]=temp;
        }
    }

    /* Sort dupx to be in crescent order */
    /* i-e
     * 22
     *  2
     * 33
     *  3
     *111
     * Here dupx = { 2, 1 } ==> Not in crescent order */
    if (ndup != 0) {
        for (k=0;k<ndup-1;k++) {
            for(j=k+1;j<ndup;j++) {
                if(binloc[dupx[k]]>binloc[dupx[j]]) {
                    temp=dupx[k];
                    dupx[k]=dupx[j];
                    dupx[j]=temp;
                    temp=dupy[k];
                    dupy[k]=dupy[j];
                    dupy[j]=temp;
                }
                else if( dupx[k]== dupx[j] && binloc[dupy[k]] > binloc[dupy[j]]) {
                    temp=dupy[k];
                    dupy[k]=dupy[j];
                    dupy[j]=temp;
                }
            }
        }
    }

    /* dupindex gives the new index for each index, when two index are duplicates
     * dupindex will affect those two indexes a same value */
    for(k=0;k<ndup;k++) {
        k1=dupx[k];
        for(j=k;j<ndup;j++) {
            if(dupy[j]== dupy[k]||dupx[j]== dupy[k]) {
                dupindex[ dupx[j] ]=dupindex[k1];
                dupindex[ dupy[j] ]=dupindex[k1];
            }
        }
    }

    /* copy dupindex in dupx */
    for (k=0;k<=pc;k++) {
        dupx[k]= dupindex[k];
        dupy[k]=0;
    }
    /* order dupx in crescent order, is it not already ordered though ?!? */
    for (k=0;k<=pc;k++) {
        for(j=k+1;j<=pc;j++) {
            if(dupx[k]>dupx[j]) {
                temp=dupx[k];
                dupx[k]=dupx[j];
                dupx[j]=temp;
            }
        }
    }

    ncc=0;
    dupy[0]=0;
    /* map dupindex values to values from 1 to number_of_detected_ring, this
     * allows to have a constant step of one, eg: converting {1,3,6} -> {1,2,3} */
    for(k=1;k<=pc;k++) {
        if(dupx[k]>dupx[k-1])
            dupy[ dupx[k] ] = ++ncc;
    }

    *pcount=ncc;
    /* set dupindex to its newly mapped value, (making dupindex have a step of
     * one */
    for (k=0;k<=pc;k++)
        dupindex[k]= dupy[ dupindex[k] ];

    /* Make all duplicates have the same ID */
    for(i=0;i<nr;i++) {
        for(j=0;j<nc;j++)
            out[i * nc + j]= dupindex[ out[i * nc + j] ];
    }

    free(dupx);
    free(dupy);
    free(dupindex);
    free(binloc);

    return;
}

static void
get_radius_metadata(UfoBuffer *src, unsigned *radius)
{
    GValue *value;

    value = ufo_buffer_get_metadata(src, "radius");
    *radius = g_value_get_uint(value);
}

static double
get_max_value(float* img, size_t width, size_t height)
{
    double res = 0;

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            if (res < img[y * width + x])
                res = img[y * width + x];
        }
    }

    return res;
}

static unsigned
min(unsigned a, unsigned b)
{
    return a > b ? b : a;
}

static gboolean
ufo_filter_particle_task_process (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoBuffer *output,
                                  UfoRequisition *requisition)
{
    (void) requisition;
    UfoFilterParticleTaskPrivate *priv = UFO_FILTER_PARTICLE_TASK_GET_PRIVATE (task);
    UfoRequisition req;
    ufo_buffer_get_requisition(inputs[0], &req);
    unsigned pcount = 0;
    float *input = ufo_buffer_get_host_array(inputs[0], NULL);
    unsigned radius;
    get_radius_metadata(inputs[0], &radius);

    /* Give unique ID to each cluster of pixels */
    double max_value = get_max_value(input, req.dims[0], req.dims[1]);
    double threshold = priv->threshold * max_value;

    // When max_value is too low, it is unlikely for there to be any rings
    if (max_value < priv->min) {
        g_print("Filter_particle: min : Ignoring radius %u. Inputs max value is"
                " %f, at least %f expected\n", radius, max_value, priv->min);
        UfoRequisition out_req = {
            .n_dims = 1,
            .dims[0] = 1
        };
        ufo_buffer_resize (output, &out_req);
        float *out = ufo_buffer_get_host_array(output, NULL);
        *out = 0;
        return TRUE;
    }

    pfind (input, priv->img, (unsigned) req.dims[0], (unsigned) req.dims[1], &pcount, threshold);

    /* Compute center of each cluster and store coordinates in cx, cy */
    double *cx = malloc (sizeof (double) * pcount);
    double *cy = malloc (sizeof (double) * pcount);
    double *rr = malloc (sizeof (double) * pcount);
    part_cen_radius(input, priv->img, (unsigned) req.dims[0],
                    (unsigned) req.dims[1], pcount, cx, cy, rr);

    /* a list of pcount UfoRingCoordinate are generated in output */
    /* first element is the number of rings detected */
    UfoRequisition out_req = {
        .n_dims = 1,
        .dims[0] = 1 + pcount * sizeof (UfoRingCoordinate) / sizeof (float),
    };
    ufo_buffer_resize (output, &out_req);
    float *out = ufo_buffer_get_host_array(output, NULL);
    UfoRingCoordinate *res = (UfoRingCoordinate *) (out + 1);
    *out = (float) pcount;

    for (unsigned i = 0; i < pcount; ++i) {
        res[i].x = (float) cx[i];
        res[i].y = (float) cy[i];
        res[i].r = (float) radius;
        unsigned x =  min((unsigned)round(cx[i]), (unsigned) (req.dims[0] - 1));
        unsigned y =  min((unsigned)round(cy[i]), (unsigned) (req.dims[1] - 1));
        res[i].intensity = input[y * req.dims[0] + x];
    }

    free (cx);
    free (cy);
    free (rr);

    return TRUE;
}

static void
ufo_filter_particle_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoFilterParticleTaskPrivate *priv = UFO_FILTER_PARTICLE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MIN:
            priv->min = g_value_get_float(value);
            break;
        case PROP_THRESHOLD:
            priv->threshold = g_value_get_float(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_particle_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoFilterParticleTaskPrivate *priv = UFO_FILTER_PARTICLE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MIN:
            g_value_set_float(value, priv->min);
            break;
        case PROP_THRESHOLD:
            g_value_set_float(value, priv->threshold);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_particle_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_filter_particle_task_parent_class)->finalize (object);
    UfoFilterParticleTaskPrivate *priv = UFO_FILTER_PARTICLE_TASK_GET_PRIVATE (object);
    free (priv->img);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_filter_particle_task_setup;
    iface->get_num_inputs = ufo_filter_particle_task_get_num_inputs;
    iface->get_num_dimensions = ufo_filter_particle_task_get_num_dimensions;
    iface->get_mode = ufo_filter_particle_task_get_mode;
    iface->get_requisition = ufo_filter_particle_task_get_requisition;
    iface->process = ufo_filter_particle_task_process;
}

static void
ufo_filter_particle_task_class_init (UfoFilterParticleTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_filter_particle_task_set_property;
    gobject_class->get_property = ufo_filter_particle_task_get_property;
    gobject_class->finalize = ufo_filter_particle_task_finalize;

    properties[PROP_MIN] =
        g_param_spec_float ("min",
            "Ignore image when it's maximal value is less than min",
            "Ignore image when it's maximal value is less than min",
            0, 1, 0.125f,
            G_PARAM_READWRITE);
    properties[PROP_THRESHOLD] =
        g_param_spec_float ("threshold",
            "Set threshold value which is relative to the image maximum value",
            "Set threshold value which is relative to the image maximum value",
            0, 1, 0.8f,
            G_PARAM_READWRITE);


    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoFilterParticleTaskPrivate));
}

static void
ufo_filter_particle_task_init(UfoFilterParticleTask *self)
{
    self->priv = UFO_FILTER_PARTICLE_TASK_GET_PRIVATE(self);
    self->priv->img = NULL;
    self->priv->num_pixels = 0;
    self->priv->min = 0.125f;
    self->priv->threshold = 0.8f;
}
