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

#include <glib.h>
#include "config.h"
#include <ufo/ufo.h>

#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

/**
 * Build a opencl-kernel that sums *num_inputs* streams slice wise.
 *
 * @param num_inputs Number of input-streams
 * @return std::string containing an opencl-kernel
 */
std::string build_kernel(unsigned int num_inputs){
    if (num_inputs < 1 or num_inputs > 99999)
        throw std::invalid_argument("num_inputs must be between 0 and 99999.");

    std::string kernel = "kernel void test_input(";
    for(unsigned int i = 0; i < num_inputs; ++i){
        kernel.append("global float *a");
        char number[8];
        sprintf(number, "%06i", i);
        kernel.append(number);
        kernel.append(", ");
    }
    kernel.append("global float *result){\n");
    kernel.append("\tsize_t idx = get_global_id(1) * get_global_size(0) + get_global_id(0);\n");
    kernel.append("\tresult[idx] = ");
    for(unsigned int i = 0; i < num_inputs; ++i){
        kernel.append("a");
        char number[8];
        sprintf(number, "%06i", i);
        kernel.append(number);
        kernel.append("[idx]");
        if (i < num_inputs - 1) {
            kernel.append(" + ");
        }
        else{
            kernel.append(";\n");
        }
    }
    kernel.append("}\n");
    return kernel;
}
/**
 * Creates a graph with *n* dummy-data inputs.
 *
 * The inputs are connected to a opencl-node, that sums the inputs slice wise.
 * Then the summed data is put in a null-sink.
 *
 * @param n Number of inputs
 */
static void test_n_inputs(unsigned int n){
    UfoTaskGraph *graph;
    UfoBaseScheduler *scheduler;
    UfoPluginManager *manager;

#if !(GLIB_CHECK_VERSION(2, 36, 0))
    g_type_init ();
#endif

    graph = UFO_TASK_GRAPH(ufo_task_graph_new());
    manager = ufo_plugin_manager_new();
    scheduler = ufo_scheduler_new();

    auto opencl_kernel = ufo_plugin_manager_get_task(manager, "opencl", nullptr);
    if (opencl_kernel == nullptr)
        throw std::runtime_error("Can not create task 'opencl'.");
    g_object_set (G_OBJECT (opencl_kernel),
                  "source", build_kernel(n).c_str(),
                  "kernel", "test_input",
                  NULL);

    auto sink = ufo_plugin_manager_get_task(manager, "null", nullptr);
    if (sink == nullptr)
        throw std::runtime_error("Can not create task 'null'.");

    std::vector<UfoTaskNode *> readers;
    readers.resize(n);
    std::size_t input_node_counter = 0;
    for (auto& reader: readers) {
        reader = ufo_plugin_manager_get_task(manager, "dummy-data", nullptr);
        if (reader == nullptr)
            throw std::runtime_error("Can not create task 'dummy-data'.");
        g_object_set (G_OBJECT (reader),
                      "width", 256,
                      "height", 256,
                      "number", 100,
                      NULL);
        ufo_task_graph_connect_nodes_full(graph, reader, opencl_kernel, input_node_counter);
        input_node_counter++;
    }

    ufo_task_graph_connect_nodes(graph, opencl_kernel, sink);
    ufo_base_scheduler_run(scheduler, graph, nullptr);

    /* Destroy all objects */
    for (const auto& reader: readers) {
        g_object_unref(reader);
    }
    g_object_unref(graph);
    g_object_unref(scheduler);
    g_object_unref(manager);
    g_object_unref(opencl_kernel);
    g_object_unref(sink);

}

/**
 * Tests a graph with a node, featuring 1 to UFO_MAX_INPUT_NODES.
 */
static void test_max_inputs() {
    for(int i = 1; i <= UFO_MAX_INPUT_NODES; ++i){
        try {
            test_n_inputs(i);
        }
        catch (const std::exception& e){
            std::cout << e.what() << std::endl;
            g_assert(false);
        }
    }
}


extern "C" void test_add_max_input_nodes(void) {
        g_test_add_func("/no-opencl/scheduler/max_input_nodes", test_max_inputs);
}
