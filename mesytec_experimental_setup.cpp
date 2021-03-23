#include "mesytec_experimental_setup.h"
#include <fstream>

namespace mesytec
{
   void experimental_setup::read_crate_map(const std::string &mapfile)
   {
      // Read crate set up from a file which contains a line for each module,
      // with each module defined by the 4 parameters
      //   name - module-id - number of channels - firmware
      //
      // For example:
      //
      // MDPP-16 0x0  16  SCP
      // MDPP-32 0x10 32  QDC
      //

      std::ifstream _mapfile;
      _mapfile.open(mapfile);
      if(!_mapfile.good())
      {
         std::cout << "Error in <mesytec::experimental_setup::read_crate_map> : failed to open file "
                   << mapfile << std::endl;
      }
      std::string name, firm;
      uint8_t modid, nchan;
      std::map<std::string,mesytec::firmware_t> firmwares;
      firmwares["SCP"] = mesytec::SCP;
      firmwares["QDC"] = mesytec::QDC;
      firmwares["CSI"] = mesytec::CSI;
      std::vector<module> modules;
      do
      {
         _mapfile >> name >> modid >> nchan >> firm;
         if(_mapfile.good())
         {
            crate_map[modid] = {name, modid, nchan, firmwares[firm]};
         }
      }
      while(_mapfile.good());
      _mapfile.close();
   }

   void experimental_setup::read_detector_correspondence(const std::string &mapfile)
   {
      // Read association between module, channel and detector from a file which
      // contains a line for each detector:
      //   mod-id  -  channel  - detector
      //
      // For example:
      //
      // 0x0   0x0    SI_0601
      // 0x10  0x0    CSI_0601
      //

      std::ifstream _mapfile;
      _mapfile.open(mapfile);
      if(!_mapfile.good())
      {
         std::cout << "Error in <mesytec::experimental_setup::read_detector_correspondence> : failed to open file "
                   << mapfile << std::endl;
      }
      std::string detname;
      uint8_t modid, nchan;
      do
      {
         _mapfile >> modid >> nchan >> detname;
         if(_mapfile.good())
         {
            crate_map[modid].get_channel_map()[nchan] = detname;
         }
      }
      while(_mapfile.good());
      _mapfile.close();
   }
}
