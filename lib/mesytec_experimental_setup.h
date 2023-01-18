#ifndef MESYTEC_EXPERIMENTAL_SETUP_H
#define MESYTEC_EXPERIMENTAL_SETUP_H

#include "mesytec_module.h"

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
   class setup_readout
   {
      std::set<uint8_t> set_of_modids;
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
      /**
         @brief add a module to the setup
         @param id VME address of module (as it appears in data)
       */
      void add_module(uint8_t id)
      {
         set_of_modids.insert(id);
      }
      void set_event_start_marker(uint8_t e){ event_start_marker=e; std::cout << "START_READOUT=" << std::hex << std::showbase << (int)e << std::endl; }
      void set_event_end_marker(uint8_t e){ event_end_marker=e;  std::cout << "END_READOUT=" << std::hex << std::showbase << (int)e << std::endl; }
      void begin_readout()
      {
         readout_state=readout_state_t::waiting_for_event_start;
      }
      /**
         @brief check if module is part of event being read

         This is true if we are currently reading an event (read_event_start=true, read_event_end=false)
         and the module corresponds to a known address in the setup

         @param id VME address of module read from data
         @return true if module is part of event
       */
      bool waiting_to_begin_cycle() const { return  readout_state == readout_state_t::waiting_for_event_start; }
      bool in_readout_cycle() const { return  readout_state == readout_state_t::in_event_readout; }
      void force_state_in_readout_cycle() { readout_state = readout_state_t::in_event_readout; }
      bool readout_complete() const { return  readout_state == readout_state_t::finished_readout; }
      readout_state_t get_readout_state() const { return readout_state; }
      bool is_next_module(uint8_t id)
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
         return set_of_modids.find(id)!=set_of_modids.end();
      }
   };

   /**
  \class experimental_setup

  Can read chassis config from file.
  Can read correspondance module-channel-detector from file.

 */
   class experimental_setup
   {
      std::vector<module> crate_map; /// map module id to module
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


         // dummy map
         std::map<uint8_t,module> modmap;
         uint8_t maxmodid=0;

         for(auto& mod : modules) {
            if(mod.firmware == START_READOUT)
               readout.set_event_start_marker(mod.id);
            else if(mod.firmware == END_READOUT)
               readout.set_event_end_marker(mod.id);
            else
            {
               modmap[mod.id] = std::move(mod);
               if(mod.id>maxmodid) maxmodid=mod.id;
               readout.add_module(mod.id);
            }
         }
         // now set up std::vector of modules with size large enough to contain the largest module address
         crate_map.reserve(maxmodid+1);
         for(auto& m : modmap)
         {
            crate_map[m.second.id] = std::move(m.second);
         }
      }
      void read_crate_map(const std::string& mapfile);
      void read_detector_correspondence(const std::string& mapfile);

      setup_readout readout;

      module& get_module(uint8_t mod_id) { return crate_map[mod_id]; }
      const module& get_module(uint8_t mod_id) const { return crate_map[mod_id]; }
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
}
#endif // MESYTEC_EXPERIMENTAL_SETUP_H
