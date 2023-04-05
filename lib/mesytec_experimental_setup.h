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
}
#endif // MESYTEC_EXPERIMENTAL_SETUP_H
