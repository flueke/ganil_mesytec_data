#ifndef MESYTEC_BUFFER_READER_H
#define MESYTEC_BUFFER_READER_H

#include "mesytec_data.h"

#include <assert.h>

namespace mesytec
{
   class mesytec_buffer_reader
   {
      std::map<uint8_t,mesytec_module> mesytec_setup;
      std::map<uint32_t,mdpp_event> event_map;
      mdpp_module_data mod_data;
      bool got_header=false;
      bool reading_data=false;
   public:
      mesytec_buffer_reader(std::map<uint8_t, mesytec_module> setup)
         : mesytec_setup{setup}
      {}
      template<typename CallbackFunction>
      void read_buffer(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         // Read nbytes bytes from the buffer [must be a multiple of 4, i.e. only 4-byte words]
         //
         // Decode Mesytec MDPP data in the buffer, collate all module data with the same event counter
         // (in their EOE word). When a complete event is ready (i.e. after reading data for each module
         // in the setup given to the constructor) the callback function is called with the event as
         // argument. Suitable signature for callback could be
         //
         //    void callback((mesytec::mdpp_event& Event);
         //
         // Straight after the call, the event will be deleted, so don't bother keeping a copy of a
         // reference to it, any data must be copied/moved in the callback function.

         assert(nbytes%4==0);

         int words_to_read = nbytes/4;
         uint8_t* buf = const_cast<uint8_t*>(_buf);
         int number_of_modules = mesytec_setup.size();
         while(words_to_read--)
         {
            auto next_word = read_data_word(buf);
            if(is_header(next_word))
            {
               if(got_header) throw(std::runtime_error("Read another header straight after first"));
               else if(reading_data) throw(std::runtime_error("Read header while reading data, no EOE"));
               mod_data = mdpp_module_data{next_word};
               got_header = true;
               reading_data = false;
            }
            else if(is_mdpp_data(next_word)) {
               if(!got_header) throw(std::runtime_error("Read data without first reading header"));
               reading_data=true;
               auto& module = mesytec_setup[mod_data.module_id];
               module.set_data_word(next_word);
               mod_data.add_data( module.data_type(), module.channel_number(), module.channel_data(), next_word);
            }
            else if((got_header || reading_data) && is_end_of_event(next_word)) // ignore 2nd, 3rd, ... EOE
            {
               got_header=false;
               reading_data=false;
               mod_data.event_counter = event_counter(next_word);
               mod_data.eoe_word = next_word;
               mdpp_event& event = event_map[mod_data.event_counter];
               event.event_counter = mod_data.event_counter;
               event.add_module_data(mod_data);
               // have we received data (at least a header) for every module in the setup?
               // if so then the event is complete and can be encapsulated in an MFMFrame (for example)
               if(event.is_full(number_of_modules))
               {
                  // call callback function
                  F(event);
                  // delete event after sending/encapsulating
                  auto it = event_map.find(event.event_counter);
                  event_map.erase(it);
               }
            }
            buf+=4;
         }
      }
   };
}
#endif // MESYTEC_BUFFER_READER_H
