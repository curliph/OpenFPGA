/********************************************************************
 * This file includes functions that are used to generate 
 * a Verilog module of a pre-configured FPGA fabric 
 *******************************************************************/
#include <fstream>
#include <ctime>

#include "vtr_assert.h"
#include "device_port.h"
#include "util.h"

#include "bitstream_manager_utils.h"

#include "fpga_x2p_naming.h"
#include "fpga_x2p_utils.h"
#include "fpga_x2p_benchmark_utils.h"

#include "verilog_global.h"
#include "verilog_writer_utils.h"
#include "verilog_preconfig_top_module.h"

/********************************************************************
 * Print module declaration and ports for the pre-configured
 * FPGA top module
 * The module ports do exactly match the input benchmark 
 *******************************************************************/
static 
void print_verilog_preconfig_top_module_ports(std::fstream& fp, 
                                              const std::string& circuit_name,
                                              const std::vector<t_logical_block>& L_logical_blocks) {

  /* Validate the file stream */
  check_file_handler(fp);

  /* Module declaration */ 
  fp << "module " << circuit_name << std::string(formal_verification_top_module_postfix);
  fp << " (" << std::endl;
 
  /* Add module ports */
  size_t port_counter = 0;

  /* Port type-to-type mapping */
  std::map<enum logical_block_types, enum e_dump_verilog_port_type> port_type2type_map;
  port_type2type_map[VPACK_INPAD] = VERILOG_PORT_INPUT;
  port_type2type_map[VPACK_OUTPAD] = VERILOG_PORT_OUTPUT;
  
  /* Print all the I/Os of the circuit implementation to be tested*/
  for (const t_logical_block& lb : L_logical_blocks) {
    /* We only care I/O logical blocks !*/
    if ( (VPACK_INPAD != lb.type) && (VPACK_OUTPAD != lb.type) ) {
      continue;
    }
    if (0 < port_counter) { 
      fp << "," << std::endl;
    }
    /* Both input and output ports have only size of 1 */
    BasicPort module_port(std::string(std::string(lb.name) + std::string(formal_verification_top_module_port_postfix)), 1); 
    fp << generate_verilog_port(port_type2type_map[lb.type], module_port);  

    /* Update port counter */
    port_counter++;
  }

  fp << ");" << std::endl;

  /* Add an empty line as a splitter */
  fp << std::endl;
}

/********************************************************************
 * Print internal wires for the pre-configured FPGA top module
 * The internal wires are tailored for the ports of FPGA top module
 * which will be different in various configuration protocols
 *******************************************************************/
static 
void print_verilog_preconfig_top_module_internal_wires(std::fstream& fp,
                                                       const ModuleManager& module_manager,
                                                       const ModuleId& top_module) {
  /* Validate the file stream */
  check_file_handler(fp);

  /* Global ports of top-level module  */
  print_verilog_comment(fp, std::string("----- Global ports of FPGA fabric -----"));
  for (const BasicPort& global_port : module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_GLOBAL_PORT)) {
    fp << generate_verilog_port(VERILOG_PORT_WIRE, global_port) << ";" << std::endl;
  }
  /* Add an empty line as a splitter */
  fp << std::endl;

  /* Datapath I/Os of FPGA fabric */
  print_verilog_comment(fp, std::string("----- I/Os of FPGA fabric -----"));
  for (const BasicPort& gpio_port : module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_GPIO_PORT)) {
    fp << generate_verilog_port(VERILOG_PORT_WIRE, gpio_port) << ";" << std::endl;
  }
  /* Add an empty line as a splitter */
  fp << std::endl;

  /* Datapath I/Os of FPGA fabric */
  print_verilog_comment(fp, std::string("----- Configuration protocols of FPGA fabric -----"));
  for (const BasicPort& input_port : module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_INPUT_PORT)) {
    fp << generate_verilog_port(VERILOG_PORT_WIRE, input_port) << ";" << std::endl;
  }
  /* Add an empty line as a splitter */
  fp << std::endl;
}

/********************************************************************
 * Print an instance of the FPGA top-level module
 *******************************************************************/
static 
void print_verilog_preconfig_top_instance(std::fstream& fp,
                                          const ModuleManager& module_manager,
                                          const ModuleId& top_module) {
  /* Validate the file stream */
  check_file_handler(fp);

  /* Include defined top-level module */
  print_verilog_comment(fp, std::string("----- FPGA top-level module to be capsulated -----"));

  /* Create an empty port-to-port name mapping, because we use default names */
  std::map<std::string, BasicPort> port2port_name_map;

  /* Use explicit port mapping for a clean instanciation */
  print_verilog_module_instance(fp, module_manager, top_module, 
                                std::string(formal_verification_top_module_uut_name), 
                                port2port_name_map, true); 

  /* Add an empty line as a splitter */
  fp << std::endl;
}

/********************************************************************
 * Connect global ports of FPGA top module to constants except:
 * 1. operating clock, which should be wired to the clock port of 
 * this pre-configured FPGA top module
 *******************************************************************/
static 
void print_verilog_preconfig_top_module_connect_global_ports(std::fstream& fp,
                                                             const ModuleManager& module_manager,
                                                             const ModuleId& top_module,
                                                             const CircuitLibrary& circuit_lib,
                                                             const std::vector<CircuitPortId>& global_ports,
                                                             const std::vector<std::string>& benchmark_clock_port_names) {
  /* Validate the file stream */
  check_file_handler(fp);

  print_verilog_comment(fp, std::string("----- Begin Connect Global ports of FPGA top module -----"));

  /* Global ports of the top module in module manager do not carry any attributes, 
   * such as is_clock, is_set, etc. 
   * Therefore, for each global port in the top module, we find the circuit port in the circuit library
   * which share the same name. We can access to the attributes.
   * To gurantee the correct link between global ports in module manager and those in circuit library
   * We have performed some critical check in check_circuit_library() for global ports,
   * where we guarantee all the global ports share the same name must have the same attributes.
   * So that each global port with the same name is unique!
   */
  for (const BasicPort& module_global_port : module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_GLOBAL_PORT)) {
    CircuitPortId linked_circuit_port_id = CircuitPortId::INVALID();
    /* Find the circuit port with the same name */
    for (const CircuitPortId& circuit_port_id : global_ports) {
      if (0 != module_global_port.get_name().compare(circuit_lib.port_lib_name(circuit_port_id))) {
        continue;
      }
      linked_circuit_port_id = circuit_port_id;
      break;
    }
    /* Must find one valid circuit port */
    VTR_ASSERT(CircuitPortId::INVALID() != linked_circuit_port_id);
    /* Port size should match! */
    VTR_ASSERT(module_global_port.get_width() == circuit_lib.port_size(linked_circuit_port_id));
    /* Now, for operating clock port, we should wire it to the clock of benchmark! */
    if  ( (SPICE_MODEL_PORT_CLOCK == circuit_lib.port_type(linked_circuit_port_id)) 
       && (false == circuit_lib.port_is_prog(linked_circuit_port_id)) ) {
      /* Wiring to each pin of the global port: benchmark clock is always 1-bit */
      for (const size_t& pin : module_global_port.pins()) {  
        for (const std::string& clock_port_name : benchmark_clock_port_names) {  
          BasicPort module_clock_pin(module_global_port.get_name(), pin, pin);
          BasicPort benchmark_clock_pin(clock_port_name + std::string(formal_verification_top_module_port_postfix), 1);
          print_verilog_wire_connection(fp, module_clock_pin, benchmark_clock_pin, false);
        }
      } 
      /* Finish, go to the next */
      continue;
    } 

    /* For other ports, give an default value */
    std::vector<size_t> default_values(module_global_port.get_width(), circuit_lib.port_default_value(linked_circuit_port_id)); 
    print_verilog_wire_constant_values(fp, module_global_port, default_values);
  }

  print_verilog_comment(fp, std::string("----- End Connect Global ports of FPGA top module -----"));

  /* Add an empty line as a splitter */
  fp << std::endl;
}

/********************************************************************
 * This function adds stimuli to I/Os of FPGA fabric
 * 1. For mapped I/Os, this function will wire them to the input ports
 *    of the pre-configured FPGA top module
 * 2. For unmapped I/Os, this function will assign a constant value
 *    by default 
 *******************************************************************/
static 
void print_verilog_preconfig_top_module_connect_ios(std::fstream& fp,
                                                    const ModuleManager& module_manager,
                                                    const ModuleId& top_module,
                                                    const std::vector<t_logical_block>& L_logical_blocks,
                                                    const vtr::Point<size_t>& device_size,
                                                    const std::vector<std::vector<t_grid_tile>>& L_grids, 
                                                    const std::vector<t_block>& L_blocks) {
  /* Validate the file stream */
  check_file_handler(fp);

  /* In this function, we support only 1 type of I/Os */
  VTR_ASSERT(1 == module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_GPIO_PORT).size());
  BasicPort module_io_port = module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_GPIO_PORT)[0];

  /* Keep tracking which I/Os have been used */
  std::vector<bool> io_used(module_io_port.get_width(), false);

  /* See if this I/O should be wired to a benchmark input/output */
  /* Add signals from blif benchmark and short-wire them to FPGA I/O PADs
   * This brings convenience to checking functionality  
   */
  print_verilog_comment(fp, std::string("----- Link BLIF Benchmark I/Os to FPGA I/Os -----"));
  for (const t_logical_block& io_lb : L_logical_blocks) {
    /* We only care I/O logical blocks !*/
    if ( (VPACK_INPAD != io_lb.type) && (VPACK_OUTPAD != io_lb.type) ) {
      continue;
    }

    /* Find the index of the mapped GPIO in top-level FPGA fabric */
    size_t io_index = find_benchmark_io_index(io_lb, device_size, L_grids, L_blocks);

    /* Ensure that IO index is in range */
    BasicPort module_mapped_io_port = module_io_port; 
    /* Set the port pin index */ 
    VTR_ASSERT(io_index < module_mapped_io_port.get_width());
    module_mapped_io_port.set_width(io_index, io_index);

    /* Create the port for benchmark I/O, due to BLIF benchmark, each I/O always has a size of 1 */
    BasicPort benchmark_io_port(std::string(std::string(io_lb.name)+ std::string(formal_verification_top_module_port_postfix)), 1); 

    print_verilog_comment(fp, std::string("----- Blif Benchmark inout " + std::string(io_lb.name) + " is mapped to FPGA IOPAD " + module_mapped_io_port.get_name() + "[" + std::to_string(io_index) + "] -----"));
    print_verilog_wire_connection(fp, module_mapped_io_port, benchmark_io_port, false);

    /* Mark this I/O has been used/wired */
    io_used[io_index] = true;
  }

  /* Add an empty line as a splitter */
  fp << std::endl;

  /* Wire the unused iopads to a constant */
  print_verilog_comment(fp, std::string("----- Wire unused FPGA I/Os to constants -----"));
  for (size_t io_index = 0; io_index < io_used.size(); ++io_index) {
    /* Bypass used iopads */
    if (true == io_used[io_index]) {
      continue;
    }

    /* Wire to a contant */
    BasicPort module_unused_io_port = module_io_port; 
    /* Set the port pin index */ 
    module_unused_io_port.set_width(io_index, io_index);

    std::vector<size_t> default_values(module_unused_io_port.get_width(), verilog_default_signal_init_value);
    print_verilog_wire_constant_values(fp, module_unused_io_port, default_values);
  }

  /* Add an empty line as a splitter */
  fp << std::endl;
}

/********************************************************************
 * Impose the bitstream on the configuration memories
 *******************************************************************/
static 
void print_verilog_preconfig_top_module_load_bitstream(std::fstream& fp,
                                                       const ModuleManager& module_manager,
                                                       const ModuleId& top_module,
                                                       const BitstreamManager& bitstream_manager,
                                                       const std::vector<ConfigBitId>& fabric_bitstream) {
  print_verilog_comment(fp, std::string("----- Begin load bitstream to configuration memories -----"));

  for (const ConfigBitId& config_bit_id : fabric_bitstream) {
    ConfigBlockId bit_parent_block = bitstream_manager.bit_parent_block(config_bit_id);
    /* Build the hierarchical path of the configuration bit in modules */   
    std::vector<ConfigBlockId> block_hierarchy = find_bitstream_manager_block_hierarchy(bitstream_manager, bit_parent_block);
    /* Drop the first block, which is the top module, it should be replaced by the instance name here */
    /* Ensure that this is the module we want to drop! */
    VTR_ASSERT(0 == module_manager.module_name(top_module).compare(bitstream_manager.block_name(block_hierarchy[0])));
    block_hierarchy.erase(block_hierarchy.begin());
    /* Build the full hierarchy path */
    std::string bit_hierarchy_path(formal_verification_top_module_uut_name);
    for (const ConfigBlockId& temp_block : block_hierarchy) {
      bit_hierarchy_path += std::string(".");
      bit_hierarchy_path += bitstream_manager.block_name(temp_block);
    }
    bit_hierarchy_path += std::string(".");
    bit_hierarchy_path += generate_configuration_chain_data_out_name();

    /* Find the bit index in the parent block */
    BasicPort config_port(bit_hierarchy_path, 
                          bitstream_manager.bit_index_in_parent_block(config_bit_id),
                          bitstream_manager.bit_index_in_parent_block(config_bit_id));

    /* Wire it to the configuration bit */
    std::vector<size_t> default_values(config_port.get_width(), bitstream_manager.bit_value(config_bit_id));
    print_verilog_wire_constant_values(fp, config_port, default_values);
  }

  print_verilog_comment(fp, std::string("----- End load bitstream to configuration memories -----"));
}


/********************************************************************
 * Top-level function to generate a Verilog module of 
 * a pre-configured FPGA fabric.
 *
 *   Pre-configured FPGA fabric
 *                        +--------------------------------------------
 *                        |
 *                        |          FPGA fabric
 *                        |          +-------------------------------+
 *                        |          |                               |
 *                        |  0/1---->|FPGA global ports              |
 *                        |          |                               |
 *   benchmark_clock----->|--------->|FPGA_clock                     |
 *                        |          |                               |
 *   benchmark_inputs---->|--------->|FPGA mapped I/Os               |
 *                        |          |                               |
 *   benchmark_outputs<---|<---------|FPGA mapped I/Os               |
 *                        |          |                               |
 *                        |  0/1---->|FPGA unmapped I/Os             |
 *                        |          |                               |
 *   fabric_bitstream---->|--------->|Internal_configuration_ports   |
 *                        |          +-------------------------------+
 *                        |
 *                        +-------------------------------------------
 *
 * Note: we do NOT put this module in the module manager.
 * Because, it is not a standard module, where we force configuration signals 
 * This module is a wrapper for the FPGA fabric to be compatible in 
 * the port map of input benchmark.
 * It includes wires to force constant values to part of FPGA datapath I/Os
 * All these are hard to implement as a module in module manager
 *******************************************************************/
void print_verilog_preconfig_top_module(const ModuleManager& module_manager,
                                        const BitstreamManager& bitstream_manager,
                                        const std::vector<ConfigBitId>& fabric_bitstream,
                                        const CircuitLibrary& circuit_lib,
                                        const std::vector<CircuitPortId>& global_ports,
                                        const std::vector<t_logical_block>& L_logical_blocks,
                                        const vtr::Point<size_t>& device_size,
                                        const std::vector<std::vector<t_grid_tile>>& L_grids, 
                                        const std::vector<t_block>& L_blocks,
                                        const std::string& circuit_name,
                                        const std::string& verilog_fname,
                                        const std::string& verilog_dir) {
  vpr_printf(TIO_MESSAGE_INFO, 
             "Writing pre-configured FPGA top-level Verilog netlist for design %s...", 
             circuit_name.c_str());

  /* Start time count */
  clock_t t_start = clock();

  /* Create the file stream */
  std::fstream fp;
  fp.open(verilog_fname, std::fstream::out | std::fstream::trunc);

  /* Validate the file stream */
  check_file_handler(fp);

  /* Generate a brief description on the Verilog file*/
  std::string title = std::string("Verilog netlist for pre-configured FPGA fabric by design: ") + circuit_name;
  print_verilog_file_header(fp, title); 

  /* Print preprocessing flags and external netlists */
  print_verilog_include_defines_preproc_file(fp, verilog_dir);

  print_verilog_include_netlist(fp, std::string(verilog_dir + std::string(defines_verilog_simulation_file_name)));  

  /* Print module declaration and ports */
  print_verilog_preconfig_top_module_ports(fp, circuit_name, L_logical_blocks);

  /* Find the top_module */
  ModuleId top_module = module_manager.find_module(generate_fpga_top_module_name());
  VTR_ASSERT(true == module_manager.valid_module_id(top_module));

  /* Print internal wires */
  print_verilog_preconfig_top_module_internal_wires(fp, module_manager, top_module);

  /* Instanciate FPGA top-level module */
  print_verilog_preconfig_top_instance(fp, module_manager, top_module);

  /* Find clock ports in benchmark */
  std::vector<std::string> benchmark_clock_port_names = find_benchmark_clock_port_name(L_logical_blocks);

  /* Connect FPGA top module global ports to constant or benchmark global signals! */
  print_verilog_preconfig_top_module_connect_global_ports(fp, module_manager, top_module,
                                                          circuit_lib, global_ports,
                                                          benchmark_clock_port_names);

  /* Connect I/Os to benchmark I/Os or constant driver */
  print_verilog_preconfig_top_module_connect_ios(fp, module_manager, top_module,
                                                 L_logical_blocks, device_size, L_grids, 
                                                 L_blocks);

  /* Assign FPGA internal SRAM/Memory ports to bitstream values */
  print_verilog_preconfig_top_module_load_bitstream(fp, module_manager, top_module,
                                                    bitstream_manager, fabric_bitstream);

  /* Testbench ends*/
  print_verilog_module_end(fp, std::string(circuit_name) + std::string(formal_verification_top_module_postfix));

  /* Close the file stream */
  fp.close();

  /* End time count */
  clock_t t_end = clock();

  float run_time_sec = (float)(t_end - t_start) / CLOCKS_PER_SEC;
  vpr_printf(TIO_MESSAGE_INFO, 
             "took %g seconds\n", 
             run_time_sec);  
}