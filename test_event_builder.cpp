#include "mesytec_data.h"
#include "mesytec_experimental_setup.h"

int main()
{
   mesytec::experimental_setup mesytec_setup({
                     {"CSI-01", 0x3, 32, mesytec::QDC}
   });
   mesytec_setup.set_detector_module_channel(0x3,0,"CSI_0601");
   mesytec_setup.set_detector_module_channel(0x3,1,"CSI_0602");
   mesytec_setup.set_detector_module_channel(0x3,2,"CSI_0603");
   mesytec_setup.set_detector_module_channel(0x3,3,"CSI_0604");
   mesytec_setup.set_detector_module_channel(0x3,4,"CSI_0605");
   mesytec_setup.set_detector_module_channel(0x3,5,"CSI_0606");
   mesytec_setup.print();

   mesytec::mdpp::event event;
   //   0x4002000b HEADER module 0x2
   //   0xf301003e EOE
   //   0x4003000b HEADER module 0x3
   //   0x10220c01 MDPP-DATA
   //   0x10020134 MDPP-DATA
   //   0x10620063 MDPP-DATA
   //   0xf301003e EOE
   //   0xf520000c EOE
   //   0x4004000b HEAD
   mesytec::mdpp::module_data mod_data{0x4003000b};
   mod_data.add_data(0x10220c01);
   mod_data.add_data(0x10020134);
   mod_data.add_data(0x10620063);
   event.add_module_data(mod_data);

   event.ls(mesytec_setup);
}
