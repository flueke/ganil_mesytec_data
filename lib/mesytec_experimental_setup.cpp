#include "mesytec_experimental_setup.h"
#include <fstream>
#include <sstream>

namespace mesytec
{
   void experimental_setup::read_crate_map(const std::string &mapfile)
   {
      /// \param[in] mapfile full path to file containing crate map description
      ///
      /// Read crate set up from a file which contains a line for each module,
      /// with each module defined by the 4 parameters
      ///   + name - module-id - number of channels - firmware (for MDPP modules)
      ///   + name - module-id - number of buses - VMMR (for VMMR modules)
      ///
      /// For example:
      ///~~~
      /// MDPP-01,0x0,16,MDPP_SCP
      /// MDPP-02,0x10,32,MDPP_QDC
      /// VMMR-01,0x20,8,VMMR
      ///~~~
      ///
      /// In addition, dummy modules in this file are used to indicate:
      ///    + the fake module id's used for `START_READOUT` and `END_READOUT` markers in data
      ///    + the fake module id's used for any scaler data coming from the MVLC controller
      ///
      /// | Module type | firmware | name |
      /// |-------------|----------|------|
      /// | MDPP        | SCP      | MDPP_SCP |
      /// | ^           | ^        | SCP |
      /// | ^           | QDC      | MDPP_QDC |
      /// | ^           | ^        | QDC |
      /// | VMMR        | VMMR     | VMMR |
      /// | TGV         | TGV      | TGV |
      /// | MVLC_SCALER | MVLC_SCALER | MVLC_SCALER |
      /// | START_READOUT | START_READOUT | START_READOUT |
      /// | END_READOUT | END_READOUT | END_READOUT |


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
      firmwares["UNKNOWN"] = mesytec::UNKNOWN;
      firmwares["MDPP_SCP"] = mesytec::MDPP_SCP;
      firmwares["MDPP_QDC"] = mesytec::MDPP_QDC;
      firmwares["MDPP_CSI"] = mesytec::MDPP_CSI;
      firmwares["SCP"] = mesytec::MDPP_SCP;
      firmwares["QDC"] = mesytec::MDPP_QDC;
      firmwares["CSI"] = mesytec::MDPP_CSI;
      firmwares["VMMR"] = mesytec::VMMR;
      firmwares["TGV"] = mesytec::TGV;
      firmwares["START_READOUT"] = mesytec::START_READOUT;
      firmwares["END_READOUT"] = mesytec::END_READOUT;
      firmwares["MVLC_SCALER"] = mesytec::MVLC_SCALER;

      // dummy map
      std::map<uint8_t,module> modmap;
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
            if(firmwares[firm]==UNKNOWN)
            {
               throw(std::runtime_error("module "+name+" in crate map with unknown firmware "+firm));
            }
            if(firmwares[firm]==START_READOUT)
               readout.set_event_start_marker(modid);
            else if(firmwares[firm]==END_READOUT)
               readout.set_event_end_marker(modid);
            else
            {
               modmap[modid] = module{name, modid, nchan, firmwares[firm]};
               crate_map.add_id(modid);
            }
         }
      }
      while(_mapfile.good());
      _mapfile.close();

      // now set up fast_lookup_map
      for(auto& m : modmap)
      {
         crate_map.add_object(m.first,std::move(m.second));
      }

      for(auto& mod : crate_map)
      {
         printf("Module id = %#05x  firmware = %d  name = %s\n", mod.id, mod.firmware, mod.name.c_str());
      }
   }

   void mesytec::experimental_setup::print()
   {
      for(auto& m : crate_map) m.print();
   }

   void experimental_setup::read_detector_correspondence(const std::string &mapfile)
   {
      /// Read association between module, channel and detector from a file which
      /// contains a line for each detector:
      ///    + `mod-id  -  channel  - detector`    (for MDPP modules)
      ///    + `mod-id  -  bus  - channel  - detector` (for VMMR modules)
      ///
      /// For example (the `mod-id` values correspond to those in the example crate map description file
      /// used in read_crate_map() ):
      ///
      ///~~~
      /// 0x0,0,SI_0601
      /// 0x10,2,CSI_0603
      /// 0x20,1,63,PISTA_DE_63
      ///~~~

      std::ifstream _mapfile;
      _mapfile.open(mapfile);
      if(!_mapfile.good())
      {
         std::cout << "Error in <mesytec::experimental_setup::read_detector_correspondence> : failed to open file "
                   << mapfile << std::endl;
      }
      std::string detname,dummy;
      uint8_t modid, nbus, nchan;
      size_t count=0;
      int line_number=0;
      do
      {
         ++line_number;
         std::getline(_mapfile,dummy,',');
         if(!_mapfile.good()) break;
         modid=std::stoi(dummy,&count,0);
         if(get_module(modid).is_vmmr_module())
         {
            // for VMMR, read bus number before channel subaddress
            std::getline(_mapfile,dummy,',');
            nbus=std::stoi(dummy,&count,0);
         }
         try {
            std::getline(_mapfile,dummy,',');
            nchan=std::stoi(dummy,&count,0);
         } catch (std::exception& e) {
            std::string ewhat = e.what();
            throw(std::runtime_error("problem decoding detector correspondence line "
                                     + std::to_string(line_number) + ". expected channel number, read this:" + dummy
                                     + " [exception: " + ewhat));
         }
         std::getline(_mapfile,detname);
         if(_mapfile.good())
         {
            if(get_module(modid).is_vmmr_module())
               set_detector_module_bus_channel(modid,nbus,nchan,detname);
            else
               set_detector_module_channel(modid,nchan,detname);
         }
      }
      while(_mapfile.good());
      _mapfile.close();
   }

}
