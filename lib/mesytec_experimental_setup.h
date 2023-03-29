#ifndef MESYTEC_EXPERIMENTAL_SETUP_H
#define MESYTEC_EXPERIMENTAL_SETUP_H

#include "mesytec_module.h"

// #define FLM_USE_RAW_POINTERS

#include "fast_lookup_map.h"

//#define DEBUG 1
#ifdef DEBUG
#include <iostream>
#endif
#include <set>

namespace mesytec
{
   class experimental_setup;
   /**
    \class setup_readout
    @brief utility class to verify sequence of data readout from VME modules in crate
    */
   class setup_readout
   {
      experimental_setup* exp_setup;
      uint8_t event_start_marker;
      uint8_t event_end_marker;
      bool _reading_module;
      bool _dummy_module;

   public:
      /**
         @brief Readout state

         Represents different states of the readout cycle
       */
      enum class readout_state_t
      {
         //! initial state
         waiting_for_event_start,
         //! readout in progress
         in_event_readout,
         //! just read end cycle marker, waiting for EOE
         awaiting_eoe_for_end_readout,
         //! readout terminated
         finished_readout
      } readout_state;
      /**
         @param e dummy module HW address read from crate map file for `START_READOUT`
       */
      void set_event_start_marker(uint8_t e){ event_start_marker=e; std::cout << "START_READOUT=" << std::hex << std::showbase << (int)e << std::endl; }
      /**
         @param e dummy module HW address read from crate map file for `END_READOUT`
       */
      void set_event_end_marker(uint8_t e){ event_end_marker=e;  std::cout << "END_READOUT=" << std::hex << std::showbase << (int)e << std::endl; }
      /**
         @brief initialise cycle ready to begin readout
       */
      void begin_readout()
      {
         readout_state=readout_state_t::waiting_for_event_start;
         _reading_module=false;
         _dummy_module=false;
      }
      /**
         @return true if still in initial state, i.e. waiting to encounter `START_READOUT` dummy module in data stream
       */
      bool waiting_to_begin_cycle() const { return  readout_state == readout_state_t::waiting_for_event_start; }
      /**
         @return true if readout cycle is in progress
       */
      bool in_readout_cycle() const { return  (readout_state == readout_state_t::in_event_readout)
               || (readout_state == readout_state_t::awaiting_eoe_for_end_readout); }
      /**
         force the state to be that of readout cycle in progress
       */
      void force_state_in_readout_cycle() { readout_state = readout_state_t::in_event_readout; }
      /**
         @return true when readout cycle is completed (i.e. after reading `END_READOUT` dummy module + EOE from data stream)
       */
      bool readout_complete() const { return  readout_state == readout_state_t::finished_readout; }
      /**
         @return current state of readout cycle
       */
      readout_state_t get_readout_state() const { return readout_state; }
      inline bool is_next_module(uint8_t id);
      bool reading_module() const { return _reading_module; }
      bool dummy_module() const { return _dummy_module; }
      void module_end_of_event() {
         _reading_module=false;
         _dummy_module=false;
         if(readout_state == readout_state_t::awaiting_eoe_for_end_readout)
         {
            // we just read EOE for dummy 'end readout' module. readout is complete.
            readout_state = readout_state_t::finished_readout;
         }
      }
      /**
         @brief initialise readout cycle handler for given experimental setup
         @param s pointer to experimental setup description
       */
      setup_readout(experimental_setup* s) : exp_setup{s} {}
   };

   /**
  \class experimental_setup

  \brief Description of modules contained in VME crate & connected detectors

  The experimental_setup class contains
     + a list of the modules in the crate, with their hardware adress and firmware;
     + [optionally] the name of the detector connected to each channel of each module

  The informations necessary for the description can be read from simple text files with
  the methods read_crate_map() and read_detector_correspondence() .
 */
   class experimental_setup
   {
      mutable fast_lookup_map<uint8_t, module> crate_map;
   public:
      /**
         @class crate_map_not_found
         @brief exception thrown if crate map description file is not found by experimental_setup::read_crate_map
       */
      class crate_map_not_found : public std::runtime_error
      {
      public:
         crate_map_not_found() : runtime_error("crate map not found")
         {}
      };

      experimental_setup()=default;
      experimental_setup(const experimental_setup&)=delete;

      void read_crate_map(const std::string& mapfile);
      void read_detector_correspondence(const std::string& mapfile);

      setup_readout readout{this};

      /**
         @brief has_module
         @param mod_id HW address of module in crate
         @return true if module with given HW address exists in crate
       */
      bool has_module(uint8_t mod_id) const { return crate_map.has_object(mod_id); }

      /**
         @brief get_module
         @param mod_id HW address of module in crate
         @return reference to module with given HW address
       */
      module& get_module(uint8_t mod_id) const { return crate_map[mod_id]; }

      /**
         @brief number_of_modules
         @return total number of modules in crate (including dummy modules corresponding to `MVLC_SCALER` data)
       */
      size_t number_of_modules() const
      {
         return crate_map.size();
      }

      /**
         @brief set_detector_module_channel
         @param modid HW address of module in crate
         @param nchan channel number
         @param detname name of associated detector

         create mapping between module/channel and name of connected physical detector

         used for MDPP modules
       */
      void set_detector_module_channel(uint8_t modid, uint8_t nchan, const std::string& detname)
      {
         get_module(modid)[0][nchan]=detname;
      }

      /**
         @brief set_detector_module_bus_channel
         @param modid HW address of module in crate
         @param nbus bus number
         @param nchan channel (subaddress) number
         @param detname name of associated detector

         create mapping between module/channel and name of connected physical detector

         used for VMMR modules
       */
      void set_detector_module_bus_channel(uint8_t modid, uint8_t nbus, uint8_t nchan, const std::string& detname)
      {
         get_module(modid)[nbus][nchan]=detname;
      }

      /**
         @brief get_detector
         @param modid HW address of module in crate
         @param nchan channel number
         @return name of detector associated with module & channel number (MDPP modules)
       */
      std::string get_detector(uint8_t modid, uint8_t nchan) const
      {
         return get_module(modid)[0][nchan];
      }
      /**
         @brief get_detector
         @param modid HW address of module in crate
         @param nbus bus number
         @param nchan channel number
         @return name of detector associated with module, bus & channel number (VMMR modules)
       */
      std::string get_detector(uint8_t modid, uint8_t nbus, uint8_t nchan) const
      {
         return get_module(modid)[nbus][nchan];
      }
      void print();
   };
   /**
      @param id HW address of next module read from data stream
      @return true if readout cycle is in progress and the HW address corresponds to a module in the setup

      check sequence of readout cycle given HW address of next module in data stream:
         + if we are not in a readout cycle, do nothing until start of event marker `START_READOUT` is found
            - returns true
         + if we are in a readout cycle and read the end of event marker `END_READOUT`, go to cycle finished state
         as soon as next EOE is read, and return true
         + if we are in a readout cycle return true or false depending on whether module id corresponds to a known module which is part of the setup

      After reading id=`START_READOUT` or id=`END_READOUT` dummy_module() returns true.
    */
   bool setup_readout::is_next_module(uint8_t id)
   {
      // if we are not in a readout cycle, do nothing until start of event marker is found
      if(waiting_to_begin_cycle() && id==event_start_marker) {
         readout_state=readout_state_t::in_event_readout;
         _reading_module=_dummy_module=true;
         return true;
      }

      // if we are in a readout cycle, check for end of event marker
      if(in_readout_cycle() && id==event_end_marker)
      {
         readout_state=readout_state_t::awaiting_eoe_for_end_readout;
         _reading_module=_dummy_module=true;
         return true;
      }

      // OK if module id is part of setup
      _dummy_module=false;
      return (_reading_module = exp_setup->has_module(id));
   }

}
#endif // MESYTEC_EXPERIMENTAL_SETUP_H
