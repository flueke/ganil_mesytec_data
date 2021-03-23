#ifndef MESYTEC_EXPERIMENTAL_SETUP_H
#define MESYTEC_EXPERIMENTAL_SETUP_H

#include "mesytec_module.h"

namespace mesytec
{
   /**
  \class experimental_setup

  Can read chassis config from file.
  Can read correspondance module-channel-detector from file.

  A dummy setup can be initialised with just a total number of modules
  (only information required to collate data from the same event).
 */
   class experimental_setup
   {
      std::map<uint8_t, module> crate_map; /// map module id to module
      uint8_t total_number_modules;
   public:
      experimental_setup()
         : total_number_modules{0}
      {}
      experimental_setup(uint8_t number_modules)
         : total_number_modules{number_modules}
      {
         /// Initialise a dummy set up with just a total number of modules
      }
      experimental_setup(std::vector<module> &&modules)
         : total_number_modules{0}
      {
         // define crate map with a vector of module constructors
         // e.g.
         //
         //         mesytec::experimental_setup setup({
         //            {"MDPP-16", 0x0, 16, mesytec::SCP},
         //            {"MDPP-32", 0x10, 32, mesytec::SCP}
         //         });
         for(auto& mod : modules) crate_map[mod.id] = std::move(mod);
      }

      /// do not ask for information on modules, channels, detectors if true
      bool is_dummy_setup() const
      {
         return total_number_modules>0;
      }
      void read_crate_map(const std::string& mapfile);
      void read_detector_correspondence(const std::string& mapfile);

      module& get_module(uint8_t mod_id){ return crate_map[mod_id]; }
      size_t number_of_modules() const
      {
         return is_dummy_setup() ? total_number_modules : crate_map.size();
      }

      /// Get name of detector associated with channel number
      std::string get_detector(uint8_t modid, uint8_t chan)
      {
         return crate_map[modid][chan];
      }
   };
}
#endif // MESYTEC_EXPERIMENTAL_SETUP_H
