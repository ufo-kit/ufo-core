/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
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

#define _GNU_SOURCE
#include <sched.h>
#include <ufo/ufo-task-node.h>

/**
 * SECTION:ufo-task-node
 * @Short_description: Node type for tasks
 * @Title: UfoTaskNode
 *
 * The node type that is inserted into a #UfoTaskGraph and keeps common data.
 */

G_DEFINE_TYPE (UfoTaskNode, ufo_task_node, UFO_TYPE_NODE)

#define UFO_TASK_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TASK_NODE, UfoTaskNodePrivate))

enum {
    PROP_0,
    PROP_NUM_PROCESSED,
    N_PROPERTIES
};

struct _UfoTaskNodePrivate {
    gchar           *plugin;
    gchar           *identifier;
    UfoSendPattern   pattern;
    UfoNode         *proc_node;
    UfoGroup        *out_group;
    UfoProfiler     *profiler;
    GList           *in_groups[16];
    GList           *current[16];
    gint             n_expected[16];
    guint            index;
    guint            total;
    guint            num_processed;
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

void
ufo_task_node_setup (UfoTaskNode *node)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    node->priv->num_processed = 0;
}

void
ufo_task_node_set_plugin_name (UfoTaskNode *task_node,
                               const gchar *name)
{
    UfoTaskNodePrivate *priv;

    g_return_if_fail (UFO_IS_TASK_NODE (task_node));
    priv = task_node->priv;

    g_free (priv->plugin);
    priv->plugin = g_strdup (name);

    g_free (priv->identifier);
    priv->identifier = g_strdup_printf ("%s-%p", name, (gpointer) task_node);
}

const gchar *
ufo_task_node_get_plugin_name (UfoTaskNode *task_node)
{
    g_assert (UFO_IS_TASK_NODE (task_node));
    return task_node->priv->plugin;
}

const gchar *
ufo_task_node_get_package_name (UfoTaskNode *task_node)
{
    g_assert (UFO_IS_TASK_NODE (task_node));
    return UFO_TASK_NODE_GET_CLASS(task_node)->get_package_name(task_node);
}

void
ufo_task_node_set_identifier (UfoTaskNode *node,
                              const gchar *identifier)
{
    g_assert (UFO_IS_TASK_NODE (node) && identifier != NULL);
    g_free (node->priv->identifier);
    node->priv->identifier = g_strdup (identifier);
}

const gchar *
ufo_task_node_get_identifier (UfoTaskNode *task_node)
{
    g_assert (UFO_IS_TASK_NODE (task_node));
    return task_node->priv->identifier;
}

void
ufo_task_node_set_send_pattern (UfoTaskNode *node,
                                UfoSendPattern pattern)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    node->priv->pattern = pattern;
}

UfoSendPattern
ufo_task_node_get_send_pattern (UfoTaskNode *node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), 0);
    return node->priv->pattern;
}

void
ufo_task_node_set_num_expected (UfoTaskNode *node,
                                guint pos,
                                gint n_expected)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    g_return_if_fail (pos < 16);
    node->priv->n_expected[pos] = n_expected;
}

gint
ufo_task_node_get_num_expected (UfoTaskNode *node,
                                guint pos)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), 0);
    g_return_val_if_fail (pos < 16, 0);
    return node->priv->n_expected[pos];
}

void
ufo_task_node_set_out_group (UfoTaskNode *node,
                             UfoGroup *group)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    node->priv->out_group = group;
}

/**
 * ufo_task_node_get_out_group:
 * @node: A #UfoTaskNode
 *
 * Get the current out of @node. The out group is used to fetch the ouput buffer
 * for @node using ufo_group_pop_output_buffer().
 *
 * Return value: (transfer full): The out group of @node.
 */
UfoGroup *
ufo_task_node_get_out_group (UfoTaskNode *node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), NULL);
    return node->priv->out_group;
}

void
ufo_task_node_add_in_group (UfoTaskNode *node,
                            guint pos,
                            UfoGroup *group)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    /* TODO: check out-of-bounds condition */
    node->priv->in_groups[pos] = g_list_prepend (node->priv->in_groups[pos], group);
    node->priv->current[pos] = node->priv->in_groups[pos];
}

/**
 * ufo_task_node_reset:
 * @node: A #UfoTaskNode
 *
 * Reset a task node so it can be re-used a second time.
 */
void
ufo_task_node_reset (UfoTaskNode *node)
{
    UfoTaskNodePrivate *priv;

    g_return_if_fail (UFO_IS_TASK_NODE (node));
    priv = UFO_TASK_NODE_GET_PRIVATE (node);
    priv->out_group = NULL;
    priv->proc_node = NULL;

    for (guint i = 0; i < 16; i++) {
        g_list_free (priv->in_groups[i]);
        priv->in_groups[i] = NULL;
    }
}

/**
 * ufo_task_node_get_current_in_group:
 * @node: A #UfoTaskNode
 * @pos: Input position of @node
 *
 * Several nodes can be connected to input @pos of @node. However, at a time
 * @node will fetch only one buffer from all its inputs. This method returns the
 * currently selected input group at @pos.
 *
 * Return value: (transfer full): The current in group of @node for @pos.
 */
UfoGroup *
ufo_task_node_get_current_in_group (UfoTaskNode *node,
                                    guint pos)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), NULL);
    g_assert (pos < 16);
    g_assert (node->priv->current[pos] != NULL);
    return UFO_GROUP (node->priv->current[pos]->data);
}

void
ufo_task_node_switch_in_group (UfoTaskNode *node,
                               guint pos)
{
    UfoTaskNodePrivate *priv;

    g_return_if_fail (UFO_IS_TASK_NODE (node));
    priv = node->priv;
    priv->current[pos] = g_list_next (priv->current[pos]);

    if (priv->current[pos] == NULL)
        priv->current[pos] = g_list_first (priv->in_groups[pos]);
}

void
ufo_task_node_set_proc_node (UfoTaskNode *task_node,
                             UfoNode *proc_node)
{
    g_return_if_fail (UFO_IS_TASK_NODE (task_node) && UFO_IS_NODE (proc_node));
    task_node->priv->proc_node = proc_node;
}

void
ufo_task_node_set_profiler (UfoTaskNode *node,
                            UfoProfiler *profiler)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));

    if (node->priv->profiler)
        g_object_unref (node->priv->profiler);

    g_object_ref (profiler);
    node->priv->profiler = profiler;
}

/**
 * ufo_task_node_get_profiler:
 * @node: A #UfoTaskNode
 *
 * Get the associated profiler of @node.
 *
 * Return value: (transfer full): A #UfoProfiler object.
 */
UfoProfiler *
ufo_task_node_get_profiler (UfoTaskNode *node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), NULL);
    return node->priv->profiler;
}

/**
 * ufo_task_node_get_proc_node:
 * @node: A #UfoTaskNode
 *
 * Get the associated processing node of @node.
 *
 * Return value: (transfer full): A #UfoNode.
 */
UfoNode *
ufo_task_node_get_proc_node (UfoTaskNode *node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), NULL);
    return node->priv->proc_node;
}

void
ufo_task_node_set_partition (UfoTaskNode *node,
                             guint index,
                             guint total)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    g_assert (index < total);
    node->priv->index = index;
    node->priv->total = total;
}

void
ufo_task_node_get_partition (UfoTaskNode *node,
                             guint *index,
                             guint *total)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    *index = node->priv->index;
    *total = node->priv->total;
}

void
ufo_task_node_increase_processed (UfoTaskNode *node)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    node->priv->num_processed++;
}

static UfoNode *
ufo_task_node_copy (UfoNode *node,
                    GError **error)
{
    UfoTaskNode *orig;
    UfoTaskNode *copy;

    copy = UFO_TASK_NODE (UFO_NODE_CLASS (ufo_task_node_parent_class)->copy (node, error));
    orig = UFO_TASK_NODE (node);

    copy->priv->pattern = orig->priv->pattern;

    for (guint i = 0; i < 16; i++)
        copy->priv->n_expected[i] = orig->priv->n_expected[i];

    ufo_task_node_set_plugin_name (copy, orig->priv->plugin);

    return UFO_NODE (copy);
}

static void
ufo_task_node_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    UfoTaskNodePrivate *priv = UFO_TASK_NODE_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_PROCESSED:
            g_value_set_uint (value, priv->num_processed);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_task_node_dispose (GObject *object)
{
    UfoTaskNodePrivate *priv;

    priv = UFO_TASK_NODE_GET_PRIVATE (object);

    if (priv->profiler) {
        g_object_unref (priv->profiler);
        priv->profiler = NULL;
    }

    G_OBJECT_CLASS (ufo_task_node_parent_class)->dispose (object);
}

static void
ufo_task_node_finalize (GObject *object)
{
    UfoTaskNodePrivate *priv;

    priv = UFO_TASK_NODE_GET_PRIVATE (object);
    ufo_task_node_reset (UFO_TASK_NODE (object));
    g_free (priv->plugin);
    g_free (priv->identifier);

    G_OBJECT_CLASS (ufo_task_node_parent_class)->finalize (object);
}


static const gchar *
ufo_task_node_get_package_name_real (UfoTaskNode *task_node)
{
    return NULL;
}

static void
ufo_task_node_class_init (UfoTaskNodeClass *klass)
{
    GObjectClass *oclass;
    UfoNodeClass *nclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->get_property = ufo_task_node_get_property;
    oclass->dispose = ufo_task_node_dispose;
    oclass->finalize = ufo_task_node_finalize;

    nclass = UFO_NODE_CLASS (klass);
    nclass->copy = ufo_task_node_copy;

    klass->get_package_name = ufo_task_node_get_package_name_real;

    properties[PROP_NUM_PROCESSED] =
        g_param_spec_uint ("num-processed",
                           "Number of processed elements",
                           "Number of processed elements",
                           0, G_MAXUINT, 0,
                           G_PARAM_READABLE);

    g_object_class_install_property (oclass, PROP_NUM_PROCESSED, properties[PROP_NUM_PROCESSED]);

    g_type_class_add_private (klass, sizeof(UfoTaskNodePrivate));
}

static void
ufo_task_node_init (UfoTaskNode *self)
{
    self->priv = UFO_TASK_NODE_GET_PRIVATE (self);
    self->priv->plugin = NULL;
    self->priv->identifier = NULL;
    self->priv->pattern = UFO_SEND_SCATTER;
    self->priv->proc_node = NULL;
    self->priv->out_group = NULL;
    self->priv->index = 0;
    self->priv->total = 1;
    self->priv->num_processed = 0;
    self->priv->profiler = ufo_profiler_new ();

    for (guint i = 0; i < 16; i++) {
        self->priv->in_groups[i] = NULL;
        self->priv->current[i] = NULL;
        self->priv->n_expected[i] = -1;
    }
}
