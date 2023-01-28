#include "mesytec_buffer_reader.h"
#include "mesytec_data.h"

void analysis_event(mesytec::event& ev, mesytec::experimental_setup& config)
{
   // Function called for each complete event read by mesytec::buffer_reader below

   for(auto& mod_dat : ev.get_module_data()) // loop over data from each module in crate (mesytec::module_data)
   {
      // Examples of available information on module data:
      mod_dat.get_module_id();                                     // => address in crate
      if(config.has_module(mod_dat.get_module_id()))               // => sanity check: does data come from a known module?
      {
         auto& mod = config.get_module(mod_dat.get_module_id());   // => find module (mesytec::module) in crate config
         if( mod.is_mdpp_module() ) { /* data from MDPP */ }
         if( mod.is_vmmr_module() ) { /* data from VMMR */ }

         for(auto& chan_dat : mod_dat.get_channel_data())          // Treat data from module item by item (mesytec::channel_data)
         {
            auto b = chan_dat.get_bus_number();         // => for VMMR: bus number (for MDPP=0)
            auto c = chan_dat.get_channel_number();     // => for VMMR: bus subaddress (0-127), for MDPP: channel number
            auto e = chan_dat.get_data();               // => 16 bit data
            auto t = chan_dat.get_data_type();          // => data type (mesytec_module::datatype_t)
            if( t == mesytec::module::ADC )
               { /* ADC data */ }
            else if( t == mesytec::module::TDC )
               { /* channel time (relative to window of interest) :
                          N.B. for VMMR, only bus number is defined in this case (c = 0) */ }
            /* N.B. chan_dat.get_data_type_name() returns "adc", "tdc", "QDC_long", etc. */
         }
      }
   }
}

int main()
{
   mesytec::buffer_reader reader;
   // read files containing crate map & bus/channel/detector correspondence
   reader.read_crate_map("");
   reader.read_detector_correspondence("");

   const uint8_t* buf;             // buffer containing data e.g. read from file
   size_t nbytes;                  // size of buffer in bytes
   bool events_to_read{true};      // some condition such as 'have we reached the end of the file?'

   while(events_to_read)
      reader.read_event_in_buffer(buf, nbytes, analysis_event);
}
