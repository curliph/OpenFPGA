/**********************************************************
 * MIT License
 *
 * Copyright (c) 2018 LNIS - The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ***********************************************************************/

/************************************************************************
 * Filename:    check_circuit_library.cpp
 * Created by:   Xifan Tang
 * Change history:
 * +-------------------------------------+
 * |  Date       |    Author   | Notes
 * +-------------------------------------+
 * | 2019/08/12  |  Xifan Tang | Created 
 * +-------------------------------------+
 ***********************************************************************/

/************************************************************************
 * Function to perform fundamental checking for the circuit library 
 * such as 
 * 1. if default circuit models are defined 
 * 2. if any circuit models shared the same name or prefix 
 * 3. if nay circuit model miss mandatory ports 
 ***********************************************************************/

/* Header files should be included in a sequence */
/* Standard header files required go first */
#include "vtr_assert.h"

#include "util.h"

#include "check_circuit_library.h"


/************************************************************************
 * Circuit models have unique names, return the number of errors 
 *  If not found, we give an error
 ***********************************************************************/
static  
size_t check_circuit_library_unique_names(const CircuitLibrary& circuit_lib) {
  size_t num_err = 0;

  for (size_t i = 0; i < circuit_lib.num_models(); ++i) {
    /* Skip for the last element, because the inner loop will access it */
    if (i == circuit_lib.num_models() - 1) {
      continue;
    }
    /* Get the name of reference */
    const std::string& i_name = circuit_lib.model_name(CircuitModelId(i));
    for (size_t j = i + 1; j < circuit_lib.num_models(); ++j) {
      /* Compare the name of candidate */
      const std::string& j_name = circuit_lib.model_name(CircuitModelId(j));
      /* Compare the name and skip for different names */
      if (0 != i_name.compare(j_name)) {
        continue;
      }
      vpr_printf(TIO_MESSAGE_ERROR,
                 "Circuit model(index=%d) and (index=%d) share the same name, which is invalid!\n",
                 i , j, i_name.c_str());
      /* Incremental the counter for errors */
      num_err++;
    }
  }

  return num_err;
}


/************************************************************************
 * Circuit models have unique names, return the number of errors 
 *  If not found, we give an error
 ***********************************************************************/
static 
size_t check_circuit_library_unique_prefix(const CircuitLibrary& circuit_lib) {
  size_t num_err = 0;

  for (size_t i = 0; i < circuit_lib.num_models(); ++i) {
    /* Skip for the last element, because the inner loop will access it */
    if (i == circuit_lib.num_models() - 1) {
      continue;
    }
    /* Get the name of reference */
    const std::string& i_prefix = circuit_lib.model_prefix(CircuitModelId(i));
    for (size_t j = i + 1; j < circuit_lib.num_models(); ++j) {
      /* Compare the name of candidate */
      const std::string& j_prefix = circuit_lib.model_prefix(CircuitModelId(j));
      /* Compare the name and skip for different prefix */
      if (0 != i_prefix.compare(j_prefix)) {
        continue;
      }
      vpr_printf(TIO_MESSAGE_ERROR,
                 "Circuit model(name=%s) and (name=%s) share the same prefix, which is invalid!\n",
                 circuit_lib.model_name(CircuitModelId(i)).c_str(),
                 circuit_lib.model_name(CircuitModelId(j)).c_str(),
                 i_prefix.c_str());
      /* Incremental the counter for errors */
      num_err++;
    }
  }

  return num_err;
}

/************************************************************************
 * A generic function to check the port list of a circuit model in a given type
 *  If not found, we give an error
 ***********************************************************************/
static 
size_t check_circuit_model_required(const CircuitLibrary& circuit_lib,
                                    const enum e_spice_model_type& circuit_model_type_to_check) {
  size_t num_err = 0;

  /* We must have an IOPAD*/
  if ( 0 == circuit_lib.models_by_type(circuit_model_type_to_check).size()) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "At least one %s circuit model is required!\n",
               CIRCUIT_MODEL_TYPE_STRING[size_t(circuit_model_type_to_check)]);
    /* Incremental the counter for errors */
    num_err++;
  }

  return num_err;
}

/************************************************************************
 * A generic function to check the port list of a circuit model in a given type 
 *  If not found, we give an error
 ***********************************************************************/
size_t check_one_circuit_model_port_required(const CircuitLibrary& circuit_lib,
                                             const CircuitModelId& circuit_model, 
                                             const std::vector<enum e_spice_model_port_type>& port_types_to_check) {
  size_t num_err = 0;

  for (const auto& port_type: port_types_to_check) {
    if (0 == circuit_lib.model_ports_by_type(circuit_model, port_type).size()) {
      vpr_printf(TIO_MESSAGE_ERROR,
                 "%s circuit model(name=%s) does not have %s port\n",
                 CIRCUIT_MODEL_TYPE_STRING[size_t(circuit_lib.model_type(circuit_model))], 
                 circuit_lib.model_name(circuit_model).c_str(), 
                 CIRCUIT_MODEL_PORT_TYPE_STRING[size_t(port_type)]);
      /* Incremental the counter for errors */
      num_err++;
    }
  }

  return num_err;
}

/************************************************************************
 * A generic function to check the port size of a given circuit model
 * if the port size does not match, we give an error 
 ***********************************************************************/
size_t check_one_circuit_model_port_size_required(const CircuitLibrary& circuit_lib,
                                                  const CircuitModelId& circuit_model, 
                                                  const CircuitPortId& circuit_port,
                                                  const size_t& port_size_to_check) {

  size_t num_err = 0;

  if (port_size_to_check != circuit_lib.port_size(circuit_port)) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "Port of circuit model(name=%s) does not have a port(type=%s) of size=%d.\n",
               circuit_lib.model_name(circuit_model).c_str(), 
               CIRCUIT_MODEL_PORT_TYPE_STRING[size_t(circuit_lib.port_type(circuit_port))],
               port_size_to_check);
    /* Incremental the counter for errors */
    num_err++;
  }

  return num_err;
}

/************************************************************************
 * A generic function to check the port size of a given circuit model
 * if the number of ports in the given type does not match, we give an error 
 * for each port, if the port size does not match, we give an error 
 ***********************************************************************/
size_t check_one_circuit_model_port_type_and_size_required(const CircuitLibrary& circuit_lib,
                                                           const CircuitModelId& circuit_model, 
                                                           const enum e_spice_model_port_type& port_type_to_check,
                                                           const size_t& num_ports_to_check,
                                                           const size_t& port_size_to_check,
                                                           const bool& include_global_ports) {

  size_t num_err = 0;

  std::vector<CircuitPortId> ports = circuit_lib.model_ports_by_type(circuit_model, port_type_to_check, false == include_global_ports);
  if (num_ports_to_check != ports.size()) {
    vpr_printf(TIO_MESSAGE_ERROR, 
               "Expect %d %s ports for a %s circuit model, but only have %d %s ports!\n",
               num_ports_to_check,
               CIRCUIT_MODEL_PORT_TYPE_STRING[size_t(port_type_to_check)],
               CIRCUIT_MODEL_TYPE_STRING[size_t(circuit_lib.model_type(circuit_model))], 
               CIRCUIT_MODEL_PORT_TYPE_STRING[size_t(port_type_to_check)],
               ports.size());
    num_err++;
  } 
  for (const auto& port : ports) { 
    num_err += check_one_circuit_model_port_size_required(circuit_lib,
                                                          circuit_model, 
                                                          port, port_size_to_check);
  }

  return num_err;
}

/************************************************************************
 * A generic function to check the port list of circuit models in a given type 
 *  If not found, we give an error
 ***********************************************************************/
static 
size_t check_circuit_model_port_required(const CircuitLibrary& circuit_lib,
                                         const enum e_spice_model_type& circuit_model_type_to_check, 
                                         const std::vector<enum e_spice_model_port_type>& port_types_to_check) {
  size_t num_err = 0;

  for (const auto& id : circuit_lib.models_by_type(circuit_model_type_to_check)) {
    num_err += check_one_circuit_model_port_required(circuit_lib, id, port_types_to_check); 
  }

  return num_err;
}

/************************************************************************
 *  A generic function to find the default circuit model with a given type
 *  If not found, we give an error
 ***********************************************************************/
static 
size_t check_required_default_circuit_model(const CircuitLibrary& circuit_lib,
                                            const enum e_spice_model_type& circuit_model_type) {
  size_t num_err = 0;

  if (CircuitModelId::INVALID() == circuit_lib.default_model(circuit_model_type)) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "A default circuit model for the type %s! Try to define it in your architecture file!\n",
               CIRCUIT_MODEL_TYPE_STRING[size_t(circuit_model_type)]);
    exit(1);
  }

  return num_err;
}

/************************************************************************
 * A function to check the port map of FF circuit model
 ***********************************************************************/
size_t check_ff_circuit_model_ports(const CircuitLibrary& circuit_lib,
                                    const CircuitModelId& circuit_model) {
  size_t num_err = 0;

  /* Check the type of circuit model */
  VTR_ASSERT(SPICE_MODEL_FF == circuit_lib.model_type(circuit_model));
  /* Check if we have D, Set and Reset */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_INPUT,
                                                                 3, 1, false);
  /* Check if we have a clock */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_CLOCK,
                                                                 1, 1, false);


  /* Check if we have output */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_OUTPUT,
                                                                 1, 1, false);

  return num_err;
}

/************************************************************************
 * A function to check the port map of SCFF circuit model
 ***********************************************************************/
size_t check_scff_circuit_model_ports(const CircuitLibrary& circuit_lib,
                                      const CircuitModelId& circuit_model) {
  size_t num_err = 0;

  /* Check the type of circuit model */
  VTR_ASSERT(SPICE_MODEL_SCFF == circuit_lib.model_type(circuit_model));

  /* Check if we have D, Set and Reset */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_INPUT,
                                                                 1, 1, false);
  /* Check if we have a clock */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_CLOCK,
                                                                 1, 1, true);


  /* Check if we have output */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_OUTPUT,
                                                                 2, 1, false);

  return num_err;
}

/************************************************************************
 * A function to check the port map of SRAM circuit model
 ***********************************************************************/
size_t check_sram_circuit_model_ports(const CircuitLibrary& circuit_lib,
                                      const CircuitModelId& circuit_model,
                                      const bool& check_blwl) {
  size_t num_err = 0;

  /* Check the type of circuit model */
  VTR_ASSERT(SPICE_MODEL_SRAM == circuit_lib.model_type(circuit_model));

  /* Check if we has 1 output with size 2 */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_OUTPUT,
                                                                 1, 2, false);
  /* basic check finished here */
  if (false == check_blwl) {
    return num_err;
  }

  /* If bl and wl are required, check their existence */
  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_BL,
                                                                 1, 1, false);

  num_err += check_one_circuit_model_port_type_and_size_required(circuit_lib, circuit_model, 
                                                                 SPICE_MODEL_PORT_WL,
                                                                 1, 1, false);

  return num_err;
}

/* Check all the ports make sure, they satisfy the restriction */
static 
size_t check_circuit_library_ports(const CircuitLibrary& circuit_lib) {
  size_t num_err = 0;
  
  /* Check global ports: make sure all the global ports are input ports */
  for (const auto& port : circuit_lib.ports()) {
    if ( (circuit_lib.port_is_global(port)) 
      && (!circuit_lib.is_input_port(port)) ) {
      vpr_printf(TIO_MESSAGE_ERROR,
                 "Circuit port (type=%s) of model (name=%s) is defined as global but not an input port!\n",
                 CIRCUIT_MODEL_PORT_TYPE_STRING[size_t(circuit_lib.port_type(port))],
                 circuit_lib.model_name(port).c_str());
      num_err++;
    }
  }

  /* Check set/reset/config_enable ports: make sure they are all global ports */
  for (const auto& port : circuit_lib.ports()) {
    if ( ( (circuit_lib.port_is_set(port)) 
        || (circuit_lib.port_is_reset(port)) 
        || (circuit_lib.port_is_config_enable(port)) )
      && (!circuit_lib.port_is_global(port)) ) {
      vpr_printf(TIO_MESSAGE_ERROR,
                 "Circuit port (type=%s) of model (name=%s) is defined as a set/reset/config_enable port but  it is not global!\n",
                 CIRCUIT_MODEL_PORT_TYPE_STRING[size_t(circuit_lib.port_type(port))],
                 circuit_lib.model_name(port).c_str());
      num_err++;
    }
  }

  return num_err;
}

/************************************************************************
 * Check points to make sure we have a valid circuit library
 * Detailed checkpoints: 
 * 1. Circuit models have unique names 
 * 2. Circuit models have unique prefix
 * 3. Check IOPADs have input and output ports
 * 4. Check MUXes has been defined and has input and output ports
 * 5. We must have at least one SRAM or SCFF 
 * 6. SRAM must have at least an input and an output ports
 * 7. SCFF must have at least a clock, an input and an output ports
 * 8. FF must have at least a clock, an input and an output ports
 * 9. LUT must have at least an input, an output and a SRAM ports
 * 10. We must have default circuit models for these types: MUX, channel wires and wires
 ***********************************************************************/
void check_circuit_library(const CircuitLibrary& circuit_lib) {
  size_t num_err = 0;

  vpr_printf(TIO_MESSAGE_INFO, "Checking circuit models...\n");

  /* 1. Circuit models have unique names  
   * For each circuit model, we always make sure it does not share any name with any circuit model locating after it
   */
  num_err += check_circuit_library_unique_names(circuit_lib);

  /* 2. Circuit models have unique prefix
   * For each circuit model, we always make sure it does not share any prefix with any circuit model locating after it
   */
  num_err += check_circuit_library_unique_prefix(circuit_lib);

  /* Check global ports */
  num_err += check_circuit_library_ports(circuit_lib);

  /* 3. Check io has been defined and has input and output ports 
   * [a] We must have an IOPAD! 
   * [b] For each IOPAD, we must have at least an input, an output, an INOUT and an SRAM port
   */
  num_err += check_circuit_model_required(circuit_lib, SPICE_MODEL_IOPAD);

  std::vector<enum e_spice_model_port_type> iopad_port_types_required;
  iopad_port_types_required.push_back(SPICE_MODEL_PORT_INPUT);
  iopad_port_types_required.push_back(SPICE_MODEL_PORT_OUTPUT);
  iopad_port_types_required.push_back(SPICE_MODEL_PORT_INOUT);
  iopad_port_types_required.push_back(SPICE_MODEL_PORT_SRAM);

  num_err += check_circuit_model_port_required(circuit_lib, SPICE_MODEL_IOPAD, iopad_port_types_required);

  /* 4. Check mux has been defined and has input and output ports
   * [a] We must have a MUX! 
   * [b] For each MUX, we must have at least an input, an output, and an SRAM port
   */
  num_err += check_circuit_model_required(circuit_lib, SPICE_MODEL_MUX);

  std::vector<enum e_spice_model_port_type> mux_port_types_required;
  mux_port_types_required.push_back(SPICE_MODEL_PORT_INPUT);
  mux_port_types_required.push_back(SPICE_MODEL_PORT_OUTPUT);
  mux_port_types_required.push_back(SPICE_MODEL_PORT_SRAM);

  num_err += check_circuit_model_port_required(circuit_lib, SPICE_MODEL_MUX, mux_port_types_required);

  /* 5. We must have at least one SRAM or SCFF */
  if ( ( 0 == circuit_lib.models_by_type(SPICE_MODEL_SRAM).size())
    && ( 0 == circuit_lib.models_by_type(SPICE_MODEL_SCFF).size()) ) {
    vpr_printf(TIO_MESSAGE_ERROR,
               "At least one %s or %s circuit model is required!\n",
               CIRCUIT_MODEL_TYPE_STRING[size_t(SPICE_MODEL_SRAM)], 
               CIRCUIT_MODEL_TYPE_STRING[size_t(SPICE_MODEL_SCFF)]);
    /* Incremental the counter for errors */
    num_err++;
  }

  /* 6. SRAM must have at least an input and an output ports*/
  std::vector<enum e_spice_model_port_type> sram_port_types_required;
  sram_port_types_required.push_back(SPICE_MODEL_PORT_INPUT);
  sram_port_types_required.push_back(SPICE_MODEL_PORT_OUTPUT);

  num_err += check_circuit_model_port_required(circuit_lib, SPICE_MODEL_SRAM, sram_port_types_required);

  /* 7. SCFF must have at least a clock, an input and an output ports*/
  std::vector<enum e_spice_model_port_type> scff_port_types_required;
  scff_port_types_required.push_back(SPICE_MODEL_PORT_CLOCK);
  scff_port_types_required.push_back(SPICE_MODEL_PORT_INPUT);
  scff_port_types_required.push_back(SPICE_MODEL_PORT_OUTPUT);

  num_err += check_circuit_model_port_required(circuit_lib, SPICE_MODEL_SCFF, scff_port_types_required);

  /* 8. FF must have at least a clock, an input and an output ports*/
  std::vector<enum e_spice_model_port_type> ff_port_types_required;
  ff_port_types_required.push_back(SPICE_MODEL_PORT_CLOCK);
  ff_port_types_required.push_back(SPICE_MODEL_PORT_INPUT);
  ff_port_types_required.push_back(SPICE_MODEL_PORT_OUTPUT);

  num_err += check_circuit_model_port_required(circuit_lib, SPICE_MODEL_FF, ff_port_types_required);

  /* 9. LUT must have at least an input, an output and a SRAM ports*/
  std::vector<enum e_spice_model_port_type> lut_port_types_required;
  lut_port_types_required.push_back(SPICE_MODEL_PORT_SRAM);
  lut_port_types_required.push_back(SPICE_MODEL_PORT_INPUT);
  lut_port_types_required.push_back(SPICE_MODEL_PORT_OUTPUT);

  num_err += check_circuit_model_port_required(circuit_lib, SPICE_MODEL_LUT, lut_port_types_required);

  /* 10. We must have default circuit models for these types: MUX, channel wires and wires */
  num_err += check_required_default_circuit_model(circuit_lib, SPICE_MODEL_MUX);
  num_err += check_required_default_circuit_model(circuit_lib, SPICE_MODEL_CHAN_WIRE);
  num_err += check_required_default_circuit_model(circuit_lib, SPICE_MODEL_WIRE);

  /* If we have any errors, exit */
  vpr_printf(TIO_MESSAGE_INFO,
             "Finished checking circuit library with %d errors!\n",
             num_err);

  if (0 < num_err) {
    exit(1);
  }

  return;
}

/************************************************************************
 * End of file : check_circuit_library.cpp
 ***********************************************************************/
