#ifndef MESYTEC_EXPERIMENTAL_SETUP_H
#define MESYTEC_EXPERIMENTAL_SETUP_H

#include "mesytec_module.h"

//#define DEBUG 1
#ifdef DEBUG
#include <iostream>
#endif

namespace mesytec
{
   /**
   \struct module_readout_status

   simple structure to store module id and whether it has been read
    */
   struct module_readout_status
   {
      uint8_t id;
      bool read{false};
      module_readout_status(uint8_t _id)
         : id(_id)
      {}
   };

   class wrong_module_sequence : public std::runtime_error
   {
   public:
      wrong_module_sequence() : runtime_error("wrong module sequence")
      {}
   };

   class module_appears_twice : public std::runtime_error
   {
   public:
      module_appears_twice() : runtime_error("module appears twice")
      {}
   };

   /**
    \class setup_readout
    */
   class setup_readout
   {
      std::vector<module_readout_status> mods;
      size_t index;
      size_t next;
      size_t number_of_modules;
   public:
      void add_module(uint8_t id)
      {
         mods.emplace_back(id);
      }
      void begin_readout()
      {
         index=0;
         next=0;
         number_of_modules=mods.size();
         for(auto& m : mods) m.read=false;
      }
      bool is_next_module(uint8_t id)
      {
         return (id == mods[next].id);
      }
      bool next_module_readout_status() const
      {
         return mods[next].read;
      }
      bool accept_module_for_readout(uint8_t id)
      {
#ifdef DEBUG
         std::cout << "accept_module_for_readout:" << std::hex << std::showbase << (int)id << std::endl;
#endif
         if(is_next_module(id) && !next_module_readout_status())
         {
            index=next++;
            mods[index].read=true;
            return true;
         }
//         else
//         {
//            if(!is_next_module(id))
//               throw(wrong_module_sequence());
//            else
//               throw(module_appears_twice());
//         }
         return false;
      }
      bool readout_complete() const
      {
         return next==number_of_modules;
      }
   };

   /**
  \class experimental_setup

  Can read chassis config from file.
  Can read correspondance module-channel-detector from file.

 */
   class experimental_setup
   {
      mutable std::map<uint8_t, module> crate_map; /// map module id to module
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
            crate_map[mod.id] = std::move(mod);
            readout.add_module(mod.id);
         }
      }
      void read_crate_map(const std::string& mapfile);
      void read_detector_correspondence(const std::string& mapfile);

      setup_readout readout;

      module& get_module(uint8_t mod_id) const { return crate_map[mod_id]; }
      size_t number_of_modules() const
      {
         return crate_map.size();
      }

      /// Get name of detector associated with channel number
      std::string get_detector(uint8_t modid, uint8_t chan) const
      {
         return crate_map[modid][chan];
      }
   };
}
#endif // MESYTEC_EXPERIMENTAL_SETUP_H
