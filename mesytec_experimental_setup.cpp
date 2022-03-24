#include "mesytec_experimental_setup.h"
#include <fstream>
#include <sstream>

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
      // MDPP-16,0x0,16,SCP
      // MDPP-32,0x10,32,QDC
      //
      // Also, dummy modules must be present representing 'START_READOUT' and 'END_READOUT' markers in data

      std::ifstream _mapfile;
      _mapfile.open(mapfile);
      if(!_mapfile.good())
      {
         std::cerr << "Error in <mesytec::experimental_setup::read_crate_map> : failed to open file "
                   << mapfile << std::endl;
         throw crate_map_not_found();
      }
      std::string name, firm;
      uint8_t modid, nchan;
      std::map<std::string,mesytec::firmware_t> firmwares;
      firmwares["SCP"] = mesytec::SCP;
      firmwares["QDC"] = mesytec::QDC;
      firmwares["CSI"] = mesytec::CSI;
      firmwares["TGV"] = mesytec::TGV;
      firmwares["START_READOUT"] = mesytec::START_READOUT;
      firmwares["END_READOUT"] = mesytec::END_READOUT;
      do
      {
         std::string dummy;
         size_t count=0;
         std::getline(_mapfile,name,',');
         if(!_mapfile.good()) break;
         std::getline(_mapfile,dummy,',');
         modid = std::stoi(dummy,&count,0);
         std::getline(_mapfile,dummy,',');
         nchan = std::stoi(dummy);
         std::getline(_mapfile,firm);
         if(_mapfile.good())
         {
            if(firmwares[firm]==START_READOUT)
               readout.set_event_start_marker(modid);
            else if(firmwares[firm]==END_READOUT)
               readout.set_event_end_marker(modid);
            else
            {
               crate_map[modid] = module{name, modid, nchan, firmwares[firm]};
               readout.add_module(modid);
            }
         }
      }
      while(_mapfile.good());
      _mapfile.close();

      for(auto& m : crate_map)
      {
         printf("Module id = %#05x  firmware = %d  name = %s\n", m.second.id, m.second.firmware, m.second.name.c_str());
      }
   }

   void mesytec::experimental_setup::print()
   {
      for(auto& m : crate_map)
      {
         for(auto& d : m.second.get_channel_map())
         {
            printf("mod=%#x  chan=%d   det=%s\n", m.second.id, d.first, d.second.c_str());
         }
      }
   }

   void experimental_setup::read_detector_correspondence(const std::string &mapfile)
   {
      // Read association between module, channel and detector from a file which
      // contains a line for each detector:
      //   mod-id  -  channel  - detector
      //
      // For example:
      //
      // 0x0,0,SI_0601
      // 0x10,2,CSI_0603
      //

      std::ifstream _mapfile;
      _mapfile.open(mapfile);
      if(!_mapfile.good())
      {
         std::cout << "Error in <mesytec::experimental_setup::read_detector_correspondence> : failed to open file "
                   << mapfile << std::endl;
      }
      std::string detname,dummy;
      uint8_t modid, nchan;
      size_t count=0;
      do
      {
         std::getline(_mapfile,dummy,',');
         if(!_mapfile.good()) break;
         modid=std::stoi(dummy,&count,0);
         std::getline(_mapfile,dummy,',');
         nchan=std::stoi(dummy);
         std::getline(_mapfile,detname);
         if(_mapfile.good())
         {
            crate_map[modid].get_channel_map()[nchan] = detname;
         }
      }
      while(_mapfile.good());
      _mapfile.close();

      print();
   }
}
