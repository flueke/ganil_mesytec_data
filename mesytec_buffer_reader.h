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
      mdpp::event event;
      mdpp::module_data mod_data;
      bool got_header=false;
      bool reading_data=false;
      bool storing_last_complete_event=false;
      uint32_t last_complete_event_counter=0;
      uint8_t* buf_pos=nullptr;
      size_t bytes_left_in_buffer=0;
      uint32_t total_number_events_parsed;
   public:
      void reset()
      {
         // reset buffer reader to initial state, before reading any buffers
         event.clear();
         mod_data.clear();
         got_header=false;
         reading_data=false;
         storing_last_complete_event=false;
         last_complete_event_counter=0;
         buf_pos=nullptr;
         bytes_left_in_buffer=0;
         total_number_events_parsed=0;
      }

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
      uint32_t read_buffer_collate_events(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         // To be used with a raw mvme data stream in order to sort and collate different module data
         // according to the event counter in each module's EOE i.e. in Narval receiver actor.
         //
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

         auto is_mdpp16 = [](const mdpp::module_data& md){ return md.module_id==0x0; };
         auto is_mdpp32 = [](const mdpp::module_data& md){ return md.module_id==0x10; };
         bool got_mdpp16{false},got_mdpp32{false};

         while(words_to_read--)
         {
            auto next_word = read_data_word(buf_pos);
            if(is_header(next_word))
            {
               if(got_header) throw(std::runtime_error("Read another header straight after first"));
               else if(reading_data) throw(std::runtime_error("Read header while reading data, no EOE"));
               mod_data = mdpp::module_data{next_word};
               if(is_mdpp16(mod_data)&&(got_mdpp16||got_mdpp32))
                  throw(std::runtime_error("Got MDPP16 data after either MDPP16 or MDPP32 data"));
               if(is_mdpp32(mod_data)&&!got_mdpp16)
                  throw(std::runtime_error("Got MDPP32 data without first reading MDPP16 data"));
               if(is_mdpp16(mod_data)) got_mdpp16=true;
               else if(is_mdpp32(mod_data)) got_mdpp32=true;
               got_header = true;
               reading_data = false;
            }
            else if(is_mdpp_data(next_word)) {
               if(!got_header) throw(std::runtime_error("Read data without first reading header"));
               reading_data=true;
               mod_data.add_data(next_word);
            }
            else if((got_header || reading_data) && is_end_of_event(next_word)) // ignore 2nd, 3rd, ... EOE
            {
               got_header=false;
               reading_data=false;
               mod_data.event_counter = event_counter(next_word);
               mod_data.eoe_word = next_word;

               if(got_mdpp16 && !got_mdpp32) event.event_counter = mod_data.event_counter;
               event.add_module_data(mod_data);
               // have we received data (at least a header) for every module in the setup?
               // if so then the event is complete and can be encapsulated in an MFMFrame (for example)
               if(got_mdpp16 && got_mdpp32)
               {
                  // store event counter in case callback function 'aborts' (output buffer full)
                  // and we have to keep the event for later
                  storing_last_complete_event=true;
                  // call callback function
                  F(event);
                  storing_last_complete_event=false;// successful callback
                  // reset event after sending/encapsulating
                  event.clear();
                  got_mdpp16=got_mdpp32=false;
                  ++total_number_events_parsed;
               }
            }
            buf_pos+=4;
            bytes_left_in_buffer-=4;
         }
         return total_number_events_parsed;
      }
      template<typename CallbackFunction>
      void read_event_in_buffer(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         // To be used to read one (and only one) collated event contained in the buffer,
         // for example to read data encapsulated in MFM frames.
         //
         // Read nbytes bytes from the buffer and call the callback function with the event as
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
         mdpp::event event;

         while(words_to_read--)
         {
            auto next_word = read_data_word(buf_pos);
            if(is_header(next_word))
            {
               mod_data = mdpp::module_data{next_word};
               got_header = true;
               reading_data = false;
            }
            else if(is_mdpp_data(next_word)) {
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
               event.event_counter = mod_data.event_counter;
               event.add_module_data(mod_data);
            }
            buf_pos+=4;
         }
         // read all data - call function
         F(event);
      }
      uint32_t get_total_events_parsed() const { return total_number_events_parsed; }
      bool is_storing_last_complete_event() const { return storing_last_complete_event; }
      template<typename CallbackFunction>
      void cleanup_last_complete_event(CallbackFunction F)
      {
         // call user function for event saved from last time
         F(event);
         // reset event
         event.clear();
         storing_last_complete_event=false;
         // advance position in buffer
         buf_pos+=4;
      }
      uint8_t* get_buffer_position() const { return buf_pos; }
      size_t get_remaining_bytes_in_buffer() const { return bytes_left_in_buffer; }
   };
}
#endif // MESYTEC_BUFFER_READER_H
