#ifndef MESYTEC_BUFFER_READER_H
#define MESYTEC_BUFFER_READER_H

#include "mesytec_data.h"
#include "mesytec_experimental_setup.h"
#include <assert.h>

namespace mesytec
{
   class buffer_reader
   {
      experimental_setup mesytec_setup;
      std::map<uint32_t,mdpp::event> event_map;
      mdpp::module_data mod_data;
      bool got_header=false;
      bool reading_data=false;
      bool storing_last_complete_event=false;
      uint32_t last_complete_event_counter=0;
      uint8_t* buf_pos=nullptr;
      size_t bytes_left_in_buffer=0;
      uint32_t total_number_events_parsed;
      bool stop_processing_flag=false;
   public:
      buffer_reader() = default;
      buffer_reader(experimental_setup& setup)
         : mesytec_setup{setup}
      {}
      void define_setup(experimental_setup& setup)
      {
         mesytec_setup = setup;
      }
      const experimental_setup& get_setup() const { return mesytec_setup; }
      template<typename CallbackFunction>
      uint32_t read_buffer(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         // Read nbytes bytes from the buffer [must be a multiple of 4, i.e. only 4-byte words]
         //
         // Decode Mesytec MDPP data in the buffer, collate all module data with the same event counter
         // (in their EOE word). When a complete event is ready (i.e. after reading data for each module
         // in the setup given to the constructor) the callback function is called with the event as
         // argument. Suitable signature for callback could be
         //
         //    void callback((mesytec::mdpp::event& Event);
         //
         // Straight after the call, the event will be deleted, so don't bother keeping a copy of a
         // reference to it, any data must be copied/moved in the callback function.
         //
         // Returns the number of complete collated events were parsed from the buffer, i.e. the number of times
         // the callback function was called without throwing an exception.

         assert(nbytes%4==0);

         int words_to_read = nbytes/4;

         buf_pos = const_cast<uint8_t*>(_buf);
         bytes_left_in_buffer = nbytes;

         total_number_events_parsed = 0;

         auto number_of_modules = mesytec_setup.number_of_modules();
         while(words_to_read--)
         {
            auto next_word = read_data_word(buf_pos);
            if(is_header(next_word))
            {
               if(got_header) throw(std::runtime_error("Read another header straight after first"));
               else if(reading_data) throw(std::runtime_error("Read header while reading data, no EOE"));
               mod_data = mdpp::module_data{next_word};
               got_header = true;
               reading_data = false;
            }
            else if(is_mdpp_data(next_word)) {
               if(!got_header) throw(std::runtime_error("Read data without first reading header"));
               reading_data=true;
               if(mesytec_setup.is_dummy_setup())
                   mod_data.add_data(next_word);
               else
               {
                   auto& mod = mesytec_setup.get_module(mod_data.module_id);
                   mod.set_data_word(next_word);
                   mod_data.add_data( mod.data_type(), mod.channel_number(), mod.channel_data(), next_word);
               }
            }
            else if((got_header || reading_data) && is_end_of_event(next_word)) // ignore 2nd, 3rd, ... EOE
            {
               got_header=false;
               reading_data=false;
               mod_data.event_counter = event_counter(next_word);
               mod_data.eoe_word = next_word;
               mdpp::event& event = event_map[mod_data.event_counter];
               event.event_counter = mod_data.event_counter;
               event.add_module_data(mod_data);
               // have we received data (at least a header) for every module in the setup?
               // if so then the event is complete and can be encapsulated in an MFMFrame (for example)
               if(event.is_full(number_of_modules))
               {
                  // store event counter in case callback function 'aborts' (output buffer full)
                  // and we have to keep the event for later
                  storing_last_complete_event=true;
                  last_complete_event_counter=event.event_counter;
                  // call callback function
                  F(event);
                  storing_last_complete_event=false;// successful callback
                  // delete event after sending/encapsulating
                  auto it = event_map.find(event.event_counter);
                  event_map.erase(it);
                  ++total_number_events_parsed;
               }
            }
            buf_pos+=4;
            bytes_left_in_buffer-=4;
         }
         return total_number_events_parsed;
      }
      uint32_t get_total_events_parsed() const { return total_number_events_parsed; }
      bool is_storing_last_complete_event() const { return storing_last_complete_event; }
      template<typename CallbackFunction>
      void cleanup_last_complete_event(CallbackFunction F)
      {
         // find last complete event in map
         auto it = event_map.find(last_complete_event_counter);
         // call user function for event
         F((*it).second);
         // erase event
         event_map.erase(it);
         storing_last_complete_event=false;
         // advance position in buffer
         buf_pos+=4;
      }
      uint8_t* get_buffer_position() const { return buf_pos; }
      size_t get_remaining_bytes_in_buffer() const { return bytes_left_in_buffer; }
      void stop() { stop_processing_flag=true; }
      void go() { stop_processing_flag=false; }
      bool can_process() const { return !stop_processing_flag; }
   };
}
#endif // MESYTEC_BUFFER_READER_H
