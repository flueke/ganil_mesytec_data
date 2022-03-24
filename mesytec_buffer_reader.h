#ifndef MESYTEC_BUFFER_READER_H
#define MESYTEC_BUFFER_READER_H

#include "mesytec_data.h"
#include "mesytec_experimental_setup.h"
#include <assert.h>
#include <ios>
#include <ostream>
#include <cstring>
#include <ctime>

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
      bool got_tgv{false};
      static const size_t last_buf_store_size=200;// 4 * nwords
      std::array<uint8_t,last_buf_store_size> store_end_of_last_buffer;
      std::array<uint32_t,4> tgv_data;
      size_t tgv_index;

   public:
      void initialise_readout()
      {
         mesytec_setup.readout.begin_readout();
      }

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
         got_tgv=false;
         mesytec_setup.readout.begin_readout();
         tgv_index=0;
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

      void dump_end_last_buffer(std::ostream& output)
      {
         dump_buffer(store_end_of_last_buffer.data(), last_buf_store_size, last_buf_store_size/4, output, "", true);
      }

      void dump_buffer(const uint8_t* _buf, size_t _buf_size, size_t nwords, std::ostream& output, const std::string& what, bool ignore_current_position=false)
      {
         // Dump decoded (i.e. byte swapped) buffer to ostream, nwords 32-bit words before and after current position
         // (but do not depass start or end of buffer)
         //
         // If ignore_current_position=true, we dump the nwords first words of the buffer given to _buf
         //
         // Indicate current position in buffer (in case of problems: 'what' gives exception message)

         auto start = (uint8_t*)(buf_pos - nwords*4);
         if(start < _buf) start=const_cast<uint8_t*>(_buf);
         auto end = (uint8_t*)(buf_pos + nwords*4);
         if(end > _buf+_buf_size) end = const_cast<uint8_t*>(_buf)+_buf_size;
         if(ignore_current_position) // dump beginning of buffer
         {
            start = const_cast<uint8_t*>(_buf);
            end = (uint8_t*)(_buf + nwords*4);
            if(end > _buf+_buf_size) end = const_cast<uint8_t*>(_buf)+_buf_size;
         }

         int words_to_read = (end-start)/4+1;
         auto my_buf_pos = start;
         while(words_to_read--)
         {
            bool current_loc = false;
            if(!ignore_current_position && my_buf_pos == buf_pos) current_loc=true;
            auto next_word = read_data_word(my_buf_pos);
            output << std::hex << std::showbase << next_word << " " << decode_type(next_word);
            if(current_loc) output << " <<<<<< " << what;
            if(my_buf_pos == start) output << " - first word";
            else if(my_buf_pos == end) output << " - last word";
            if(my_buf_pos == _buf) output << " - START BUFFER";
            else if(my_buf_pos == _buf+_buf_size) output << " - END BUFFER";
            output << std::endl;
            my_buf_pos+=4;
         }
      }

      template<typename CallbackFunction>
      uint32_t read_buffer_collate_events(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         // To be used with a raw mvme data stream in order to sort and collate different module data
         // i.e. in Narval receiver actor. The data may look like this:
         //
         // Read nbytes bytes from the buffer [must be a multiple of 4, i.e. only 4-byte words]
         //
         // Decode Mesytec MDPP data in the buffer, collate all module data.
         // When a complete event is ready the callback function is called with the event as
         // argument. Suitable signature for callback could be
         //
         //    void callback((mesytec::mdpp::event& Event);
         //
         // Straight after the call, the event will be deleted, so don't bother keeping a copy of a
         // reference to it, any data must be copied/moved in the callback function.
         //
         // Returns the number of complete collated events were parsed from the buffer, i.e. the number of times
         // the callback function was called without throwing an exception.


         assert(nbytes%4==0); // the buffer should only contain 32-bit words

         buf_pos = const_cast<uint8_t*>(_buf);
         bytes_left_in_buffer = nbytes;

         total_number_events_parsed = 0;

         while(bytes_left_in_buffer)
         {
            auto next_word = read_data_word(buf_pos);

            if(is_event_header(next_word))
            {
               mdpp::module_data tmp{next_word};
               // check readout sequence
               got_header = mesytec_setup.readout.is_next_module(tmp.module_id);

               if(got_header) {
                  // when we meet a new module in the readout, we store any previously read data
                  // for a previous module
                  if(mod_data.module_id) {
                     event.add_module_data(mod_data);
                  }
                  mod_data=std::move(tmp);
                  got_tgv = (mesytec_setup.get_module(mod_data.module_id).firmware == TGV);
                  if(got_tgv) {
                     tgv_index=0;
                  }
               }
            }
            else if(got_tgv && is_tgv_data(next_word)) {
               // this can never happen: if we read TGV data we must have first read the TGV header!
               if(!got_header) throw(std::runtime_error("Read TGV data without first reading header"));

               //mod_data.add_data(next_word); do not store TGV data in event: the timestamp will be put in the MFMFrame header
               tgv_data[tgv_index]=next_word;
               ++tgv_index;
            }
            else if(is_mdpp_data(next_word))
            {
               // this can never happen: if we read data without first reading an events header,
               // we don't know which module sent the data!
               if(!got_header) throw(std::runtime_error("Read MDPP data without first reading header"));
               mod_data.add_data(next_word);
            }

            if(mesytec_setup.readout.readout_complete())
            {
               // the end of the readout of all modules is signalled by the END_READOUT dummy module

               got_header=false;

               // store any previously read data for a previous module
               if(mod_data.module_id) {
                  event.add_module_data(mod_data);
               }

               // check TGV data
               if(!(tgv_data[0] & data_flags::tgv_data_ready_mask))
               {
                  std::cout << "*** WARNING *** Got BAD TIMESTAMP from TGV (TGV NOT READY) *** WARNING ***" << std::endl;
                  event.tgv_ts_lo = 0;
                  event.tgv_ts_mid = 0;
                  event.tgv_ts_hi = 0;
               }
               else
               {
                  // get 3 centrum timestamp words from TGV data
                  event.tgv_ts_lo = (tgv_data[1] &= data_flags::tgv_data_mask_lo);
                  event.tgv_ts_mid = (tgv_data[2] &= data_flags::tgv_data_mask_lo);
                  event.tgv_ts_hi = (tgv_data[3] &= data_flags::tgv_data_mask_lo);
               }

               // clear module data ready for next full readout
               mod_data.clear();

               got_tgv=false;

               // in case callback function 'aborts' (output buffer full) and we have to keep the event for later
               storing_last_complete_event=true;

               // call callback function
               F(event);

               storing_last_complete_event=false;// successful callback

               // reset event after sending/encapsulating
               event.clear();

               // begin new readout cycle
               mesytec_setup.readout.begin_readout();

               ++total_number_events_parsed;
            }

            // advance to next 32-bit word in buffer
            buf_pos+=4;
            bytes_left_in_buffer-=4;
         }

         return total_number_events_parsed;
      }

      uint32_t dump_data_stream(const uint8_t* _buf, size_t nbytes)
      {
         assert(nbytes%4==0);

         std::cout << "=============================================================================" << std::endl;
         std::cout << "       *************         READING NEW BUFFER         *************        " << std::endl;
         std::cout << "=============================================================================" << std::endl;
         buf_pos = const_cast<uint8_t*>(_buf);
         bytes_left_in_buffer = nbytes;

         while(bytes_left_in_buffer)
         {
            auto next_word = read_data_word(buf_pos);

            if(is_frame_header(next_word)) {
               std::cout << decode_frame_header(next_word) << std::endl;
               if(is_system_unix_time(next_word))
               {
                  // print date & time in next word
                  buf_pos+=4;
                  bytes_left_in_buffer-=4;
                  next_word = read_data_word(buf_pos);
                  time_t now = next_word;
                  char mbstr[100];
                  if (std::strftime(mbstr, sizeof(mbstr), "%A %c", std::localtime(&now))) {
                      std::cout << "Time is now : " << mbstr << '\n';
                  }
               }
            }
            else if(!is_fill_word(next_word))
            {
               std::cout << std::hex << std::showbase << next_word << " " << decode_type(next_word) << std::endl;
            }
            buf_pos+=4;
            bytes_left_in_buffer-=4;
         }
         std::cout << "=============================================================================" << std::endl;
         std::cout << "          *************         END OF BUFFER         *************          " << std::endl;
         std::cout << "=============================================================================" << std::endl;
         return 0;
      }

      void treat_header(u32 header_word, const std::string& tab="")
      {
         std::cout << tab << decode_frame_header(header_word) << std::endl;
         auto frame_info = extract_frame_info(header_word);
         auto buf_pos_end = buf_pos + 4*frame_info.len;
         while(buf_pos < buf_pos_end)
         {
            buf_pos+=4;
            bytes_left_in_buffer-=4;
            auto next_word = read_data_word(buf_pos);
            if(is_frame_header(next_word)) treat_header(next_word,tab+"\t");
            else
               std::cout << tab << std::hex << std::showbase << next_word << " " << decode_type(next_word) << std::endl;
         }
      }

      template<typename CallbackFunction>
      void read_event_in_buffer(const uint8_t* _buf, size_t nbytes, CallbackFunction F, u8 mfm_frame_rev)
      {
         // Decode buffers encapsulated in MFM frames, based on the frame revision id
         //
         // rev. 0: buffers included 'End-of-Event' words (which could in actual fact be StackFrame headers etc.),
         //         as well as module headers even for modules with no data
         //
         // rev. 1: buffers only contain module header and data words for modules which fire/produce data
         switch(mfm_frame_rev)
         {
            case 0:
               read_event_in_buffer_v0(_buf,nbytes,F);
               break;
            case 1:
               read_event_in_buffer_v1(_buf,nbytes,F);
               break;
            default:
               throw std::runtime_error("unknown MFM frame revision");
         }
      }

      template<typename CallbackFunction>
      void read_event_in_buffer_v1(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         // To be used to read one (and only one) collated event contained in the buffer encapsulated in MFM frames.
         //
         // This is for MFM frames with revision number 1.
         //
         // Read nbytes bytes from the buffer and call the callback function with the event as
         // argument. Callback signature must be
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
         mod_data.clear();

         while(words_to_read--)
         {
            auto next_word = read_data_word(buf_pos);

            if(is_event_header(next_word))
            {
               // add previously read module to event
               if(mod_data.module_id) event.add_module_data(mod_data);

               // new module
               mod_data = mdpp::module_data{next_word};
               // in principle maximum event size is 255 32-bit words i.e. 1 header + 254 following words
               if(mod_data.data_words>=254) std::cerr << "Header indicates " << mod_data.data_words << " words in this event..." << std::endl;
            }
            else if(is_mdpp_data(next_word)) {
               auto& mod = mesytec_setup.get_module(mod_data.module_id);
               mod.set_data_word(next_word);
               mod_data.add_data( mod.data_type(), mod.channel_number(), mod.channel_data(), next_word);
            }
            buf_pos+=4;
         }
         // add last read module to event
         if(mod_data.module_id) event.add_module_data(mod_data);

         // read all data - call function
         F(event);
      }
      template<typename CallbackFunction>
      void read_event_in_buffer_v0(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         // To be used to read one (and only one) collated event contained in the buffer,
         // for example to read data encapsulated in MFM frames.
         //
         // This is for MFM frames with revision number 0.
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
            if(is_event_header(next_word))
            {
               mod_data = mdpp::module_data{next_word};
               // in principle maximum event size is 255 32-bit words i.e. 1 header + 254 following words
               if(mod_data.data_words>=254) std::cerr << "Header indicates " << mod_data.data_words << " words in this event..." << std::endl;
               got_header = true;
               reading_data = false;
            }
            else if(is_mdpp_data(next_word)) {
               reading_data=true;
               auto& mod = mesytec_setup.get_module(mod_data.module_id);
               mod.set_data_word(next_word);
               mod_data.add_data( mod.data_type(), mod.channel_number(), mod.channel_data(), next_word);
            }
            // due to the confusion between 'end of event' and 'frame header' words in revision 0,
            // here we replace the original test 'if(is_end_of_event...' with 'if(is_end_of_event || is_frame_header...'
            // which corresponds to the effective behaviour of the code at the time when revision 0
            // frames were produced & written.
            else if((got_header || reading_data) && (is_end_of_event(next_word) || is_frame_header(next_word))) // ignore 2nd, 3rd, ... EOE
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
         // begin new readout cycle
         mesytec_setup.readout.begin_readout();
         // advance position in buffer
         buf_pos+=4;
      }
      uint8_t* get_buffer_position() const { return buf_pos; }
      size_t get_remaining_bytes_in_buffer() const { return bytes_left_in_buffer; }
   };
}
#endif // MESYTEC_BUFFER_READER_H
