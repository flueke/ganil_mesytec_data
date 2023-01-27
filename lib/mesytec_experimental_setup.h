#ifndef MESYTEC_EXPERIMENTAL_SETUP_H
#define MESYTEC_EXPERIMENTAL_SETUP_H

#include "mesytec_module.h"

#define FLM_USE_RAW_POINTERS

#include "fast_lookup_map.h"

//#define DEBUG 1
#ifdef DEBUG
#include <iostream>
#endif
#include <set>

namespace mesytec
{
   /**
    \class setup_readout
    */
   class experimental_setup;
   class setup_readout
   {
      experimental_setup* exp_setup;
      uint8_t event_start_marker;
      uint8_t event_end_marker;

   public:
      enum class readout_state_t
      {
         waiting_for_event_start,
         in_event_readout,
         finished_readout,
         start_event_found_in_readout_cycle
      } readout_state;
      void set_event_start_marker(uint8_t e){ event_start_marker=e; std::cout << "START_READOUT=" << std::hex << std::showbase << (int)e << std::endl; }
      void set_event_end_marker(uint8_t e){ event_end_marker=e;  std::cout << "END_READOUT=" << std::hex << std::showbase << (int)e << std::endl; }
      void begin_readout()
      {
         readout_state=readout_state_t::waiting_for_event_start;
      }
      bool waiting_to_begin_cycle() const { return  readout_state == readout_state_t::waiting_for_event_start; }
      bool in_readout_cycle() const { return  readout_state == readout_state_t::in_event_readout; }
      void force_state_in_readout_cycle() { readout_state = readout_state_t::in_event_readout; }
      bool readout_complete() const { return  readout_state == readout_state_t::finished_readout; }
      readout_state_t get_readout_state() const { return readout_state; }
      inline bool is_next_module(uint8_t id);
      setup_readout(experimental_setup* s) : exp_setup{s} {}
   };

   /**
  \class experimental_setup

  Can read chassis config from file.
  Can read correspondance module-channel-detector from file.

 */
   class experimental_setup
   {
      mutable fast_lookup_map<uint8_t, module> crate_map; /// map module id to module
   public:
      class crate_map_not_found : public std::runtime_error
      {
      public:
         crate_map_not_found() : runtime_error("crate map not found")
         {}
      };

      experimental_setup() {}
      experimental_setup(std::vector<module> &&modules)
      {
         // define crate map with a vector of module constructors
         // e.g.
         //
         //         mesytec::experimental_setup setup({
         //            {"MDPP-16", 0x0, 16, mesytec::SCP},
         //            {"MDPP-32", 0x10, 32, mesytec::SCP}
         //         });
         for(auto& mod : modules) {
            if(mod.firmware == START_READOUT)
               readout.set_event_start_marker(mod.id);
            else if(mod.firmware == END_READOUT)
               readout.set_event_end_marker(mod.id);
            else
            {
               crate_map.add_id(mod.id);
            }
         }
         for(auto& mod : modules) {
            if(mod.firmware != START_READOUT && mod.firmware != END_READOUT)
               crate_map.add_object(mod.id,mod);
         }
      }
      void read_crate_map(const std::string& mapfile);
      void read_detector_correspondence(const std::string& mapfile);

      setup_readout readout{this};

      bool has_module(uint8_t mod_id) const { return crate_map.has_object(mod_id); }
      module& get_module(uint8_t mod_id) const { return crate_map[mod_id]; }
      size_t number_of_modules() const
      {
         return crate_map.size();
      }

      void set_detector_module_channel(uint8_t modid, uint8_t nchan, const std::string& detname)
      {
         get_module(modid).get_channel_map()[nchan] = detname;
      }
      /// Get name of detector associated with channel number
      std::string get_detector(uint8_t modid, uint8_t chan)
      {
         return crate_map[modid][chan];
      }
      void print();
   };
   bool setup_readout::is_next_module(uint8_t id)
   {
      // if we are not in a readout cycle, do nothing until start of event marker is found
      if(waiting_to_begin_cycle())
      {
         if(id==event_start_marker) readout_state=readout_state_t::in_event_readout;
         return false;
      }

      // if we are in a readout cycle, check for end of event marker
      if(in_readout_cycle() && id==event_end_marker)
      {
         readout_state=readout_state_t::finished_readout;
         return false;
      }

      // we may meet a start event marker before completing the previous event
      // (truncated events?) in this case we need to store whatever we got from the
      // previous event and start another one
      if(in_readout_cycle() && id==event_start_marker)
      {
         readout_state=readout_state_t::start_event_found_in_readout_cycle;
         return false;
      }

      // OK if module id is part of setup
      return exp_setup->has_module(id);
   }

}
#endif // MESYTEC_EXPERIMENTAL_SETUP_H
