#ifndef MESYTEC_BUFFER_READER_H
#define MESYTEC_BUFFER_READER_H

#include "mesytec_data.h"
#include "mesytec_experimental_setup.h"
#include <assert.h>
#include <ios>
#include <ostream>
#include <cstring>
#include <ctime>
#include <array>

#define MESYTEC_DATA_BUFFER_READER_NO_DEFINE_SETUP
#define MESYTEC_DATA_BUFFER_READER_CALLBACK_WITH_EVENT_AND_SETUP

// #define DEBUG

namespace mesytec
{
   /**
      @class buffer_reader
      @brief parse mesytec data in buffers

      For an example of use, see example_analysis.cpp
    */
   class buffer_reader
   {
      experimental_setup mesytec_setup;
      event mesy_event;
      module_data mod_data;
      bool got_header=false;
      bool reading_data=false;
      bool storing_last_complete_event=false;
      uint32_t last_complete_event_counter=0;
      uint8_t* buf_pos=nullptr;
      size_t bytes_left_in_buffer=0;
      uint32_t total_number_events_parsed;
      bool reading_tgv{false};
      bool reading_mvlc_scaler{false};
      bool reading_mdpp{false};
      bool reading_vmmr{false};
      static const size_t last_buf_store_size=200;// 4 * nwords
      std::array<uint8_t,last_buf_store_size> store_end_of_last_buffer;
      std::array<uint32_t,4> mesytec_tgv_data;
      uint mesytec_tgv_index,mvlc_scaler_index;
      uint64_t last_timestamp{0};
      double max_timestamp_diff{3600.};
      int buf_copy{200};
      uint8_t last_buffer[200];
      bool have_last_buffer=false;

      template<typename CallbackFunction>
      void treat_complete_event(CallbackFunction F)
      {
         // the end of the readout of all modules is signalled by the END_READOUT dummy module

         got_header=false;
         //if(printit) std::cout << "readout complete" << std::endl;

         // check TGV data
         mesy_event.tgv_ts_lo = 0;
         mesy_event.tgv_ts_mid = 0;
         mesy_event.tgv_ts_hi = 0;
         //std::cout << "tgv = " << mesytec_tgv_index << " scaler = " << mvlc_scaler_index << std::endl;
         if(mesytec_tgv_index==4){
            if(!(mesytec_tgv_data[0] & data_flags::tgv_data_ready_mask))
            {
               std::cout << "*** WARNING *** Got BAD TIMESTAMP from TGV (TGV NOT READY) *** WARNING ***" << std::endl;
               for(int i=0; i<mesytec_tgv_index; ++i)
                  std::cout << std::dec << i << " : " << std::hex << std::showbase << mesytec_tgv_data[i] << std::endl;
            }
            else
            {
               //if(printit) std::cout << "TGV data is OK" << std::endl;
               // get 3 centrum timestamp words from TGV data
               mesy_event.tgv_ts_lo = (mesytec_tgv_data[1] &= data_flags::tgv_data_mask_lo);
               mesy_event.tgv_ts_mid = (mesytec_tgv_data[2] &= data_flags::tgv_data_mask_lo);
               mesy_event.tgv_ts_hi = (mesytec_tgv_data[3] &= data_flags::tgv_data_mask_lo);
               //std::cout << event.tgv_ts_hi << " " << event.tgv_ts_mid << " " << event.tgv_ts_lo << std::endl;
            }
         }

         // clear module data ready for next full readout
         mod_data.clear();

         reading_tgv=false;
         reading_mvlc_scaler=false;
         reading_mdpp=false;
         reading_vmmr=false;

         // in case callback function 'aborts' (output buffer full) and we have to keep the event for later
         storing_last_complete_event=true;

         // do not call callback (and do not increment event counter) if event contains no data
         if(mesy_event.has_data()){
#ifdef DEBUG
            std::cout << "calling callback\n";
#endif
            F(mesy_event,mesytec_setup);
            ++mesy_event.event_counter;
         }
         else
            std::cout << "callback not called : empty event\n";

         storing_last_complete_event=false;// successful callback

         // reset event after sending/encapsulating
         mesy_event.clear();

         // begin new readout cycle
         mesytec_setup.readout.begin_readout();

         ++total_number_events_parsed;
      }

      /**
             Decode buffers encapsulated in MFM frames with frame revision id=1:
                 + buffers only contain module header and data words for modules which fire/produce data
             */
      template<typename CallbackFunction>
      void read_event_in_buffer_v1(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         assert(nbytes%4==0);

         int words_to_read = nbytes/4;
         buf_pos = const_cast<uint8_t*>(_buf);
         event mesy_event;
         mod_data.clear();
         module *current_module;
         while(words_to_read--)
         {
            auto next_word = read_data_word(buf_pos);

            if(is_module_header(next_word))
            {
               // add previously read module to event
               if(mod_data.module_id) mesy_event.add_module_data(mod_data);

               // new module
               current_module = &mesytec_setup.get_module(module_id(next_word));
               auto firmware = current_module->firmware;
               mod_data.set_header_word(next_word,firmware);

               reading_mvlc_scaler = current_module->firmware == MVLC_SCALER;
            }
            else if(reading_mvlc_scaler)
            {
               mod_data.add_data(next_word);
            }
            else if(is_mdpp_data(next_word)||is_vmmr_data(next_word)) {
               current_module->set_data_word(next_word);
               if(current_module->firmware == VMMR)
                  mod_data.add_data( current_module->get_data_type(), current_module->get_bus_number(), current_module->get_channel_number(),
                                     current_module->get_channel_data(), next_word);
               else
                  mod_data.add_data( current_module->get_data_type(), current_module->get_channel_number(),
                                     current_module->get_channel_data(), next_word);
            }
            buf_pos+=4;
         }
         // add last read module to event
         if(mod_data.module_id) mesy_event.add_module_data(mod_data);

         // read all data - call function
         F(mesy_event,mesytec_setup);
      }
      /**
             Decode buffers encapsulated in MFM frames, with frame revision id=0:
                + buffers included 'End-of-Event' words (which could in actual fact be StackFrame headers etc.),
                  as well as module headers even for modules with no data
             */
      template<typename CallbackFunction>
      void read_event_in_buffer_v0(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {
         assert(nbytes%4==0);

         int words_to_read = nbytes/4;
         buf_pos = const_cast<uint8_t*>(_buf);
         event mesy_event;

         while(words_to_read--)
         {
            auto next_word = read_data_word(buf_pos);
            if(is_module_header(next_word))
            {
               auto firmware = mesytec_setup.get_module(module_id(next_word)).firmware;
               mod_data.set_header_word(next_word,firmware);
               got_header = true;
               reading_data = false;
            }
            else if(is_mdpp_data(next_word)) {
               reading_data=true;
               auto& mod = mesytec_setup.get_module(mod_data.module_id);
               mod.set_data_word(next_word);
               mod_data.add_data( mod.get_data_type(), mod.get_channel_number(), mod.get_channel_data(), next_word);
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
               mesy_event.event_counter = mod_data.event_counter;
               mesy_event.add_module_data(mod_data);
            }
            buf_pos+=4;
         }
         // read all data - call function
         F(mesy_event,mesytec_setup);
      }
   public:
      void initialise_readout()
      {
         mesytec_setup.readout.begin_readout();
      }

      void reset()
      {
         // reset buffer reader to initial state, before reading any buffers
         mesy_event.clear();
         mod_data.clear();
         got_header=false;
         reading_data=false;
         storing_last_complete_event=false;
         last_complete_event_counter=0;
         buf_pos=nullptr;
         bytes_left_in_buffer=0;
         total_number_events_parsed=0;
         reading_tgv=false;
         reading_mvlc_scaler=false;
         reading_mdpp=false;
         reading_vmmr=false;
         mesytec_setup.readout.begin_readout();
         mesytec_tgv_index=0;
         last_timestamp=0;
         mvlc_scaler_index=0;
         have_last_buffer=false;
      }

      buffer_reader() = default;
      /**
               read and set up description of experimental configuration i.e. the VME crate

               for definition of file format, see mesytec::experimental_setup::read_crate_map()

               @param map_file full path to file containing definitions of modules in VME crate
             */
      void read_crate_map(const std::string& map_file)
      {
         mesytec_setup.read_crate_map(map_file);
      }
      /**
               read and set up correspondence between electronics channels and detectors

               for definition of file format, see mesytec::experimental_setup::read_detector_correspondence()

               @param det_cor_file full file to path containing module/bus/channel/detector associations
             */
      void read_detector_correspondence(const std::string& det_cor_file)
      {
         mesytec_setup.read_detector_correspondence(det_cor_file);
      }
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

      void rollback_function(int rollback,int bytes_from_start,int bytes_to_end)
      {
         auto in_buf = buf_pos;
         auto rstart=rollback;
         auto rend=rollback;
         if(rstart*4 > bytes_from_start)
         {
            rstart = bytes_from_start/4;
            std::cout << "ROLLBACK FROM START OF BUFFER: rstart=" << std::dec << -rstart << std::endl;
            if(have_last_buffer)
            {
               std::cout << "HERE IS THE END OF THE PREVIOUS BUFFER" << std::endl;
               for(int i=0; i<buf_copy; i+=4)
               {
                  auto frame = read_data_word(last_buffer+i);
                  if(is_frame_header(frame)) {
                     std::cout << std::dec << i << ":" << decode_frame_header(frame)<< std::dec << std::endl;
                  }
                  else
                  {
                     auto dec_word = decode_type(frame);
                     std::cout << std::dec << i << ":" << std::hex << std::showbase << frame << " " << dec_word << std::dec << std::endl;
                  }
               }
            }
         }
         if(rend*4 > bytes_to_end)
         {
            std::cout << "ROLLBACK TO END OF BUFFER" << std::dec << rend << std::endl;
            rend = bytes_to_end/4;
         }
         for(int i=-rstart; i<=rend; ++i)
         {
            auto frame = read_data_word(in_buf+i*4);
            if(is_frame_header(frame)) {
               std::cout << std::dec << i << ":" << decode_frame_header(frame)<< std::dec << std::endl;
            }
            else
            {
               auto dec_word = decode_type(frame);
               std::cout << std::dec << i << ":" << std::hex << std::showbase << frame << " " << dec_word << std::dec << std::endl;
            }
         }
      }

      /**
             Used with a raw mvme data stream in order to sort and collate different module data
             i.e. in Narval receiver actor.

             When a complete event is ready the callback function F is called with the event and
             the description of the setup as
             arguments. Suitable signature for the callback function F is

             ~~~~{.cpp}
                void callback(mesytec::event&, mesytec::experimental_setup&);
             ~~~~

             (it can also of course be implemented with a lambda capture or a functor object).

             Straight after the call, the event will be deleted, so don't bother keeping a copy of a
             reference to it, any data must be treated/copied/moved in the callback function.

             Returns the number of complete collated events were parsed from the buffer, i.e. the number of times
             the callback function was called without throwing an exception.

             @param _buf pointer to the beginning of the buffer
             @param nbytes size of buffer in bytes
             @param F function to call each time a complete event is ready
             */
      template<typename CallbackFunction>
      uint32_t read_buffer_collate_events(const uint8_t* _buf, size_t nbytes, CallbackFunction F)
      {

         assert(nbytes%4==0); // the buffer should only contain 32-bit words

         buf_pos = const_cast<uint8_t*>(_buf);
         bytes_left_in_buffer = nbytes;

         total_number_events_parsed = 0;

         while(bytes_left_in_buffer)
         {
            auto next_word = read_data_word(buf_pos);
#ifdef DEBUG
            std::cout << std::hex << std::showbase << next_word << " : ";
#endif
            if(mesytec_setup.readout.in_readout_cycle() && mesytec_setup.readout.reading_module())
            {
               // we are currently reading data for a VME module.
               // check if we just read the EOE for the module
               if(is_end_of_event(next_word))
               {
#ifdef DEBUG
                  std::cout << "EOE :";
#endif
                  if(!mesytec_setup.readout.dummy_module())// not a dummy module, i.e. 'START' or 'END' markers
                  {
                     if(mod_data.has_data()) {
                        mesy_event.add_module_data(mod_data);
#ifdef DEBUG
                        std::cout << " added data to event";
#endif
                     }
#ifdef DEBUG
                     else
                        std::cout << " no data added to event";
#endif
                  }

                  mesytec_setup.readout.module_end_of_event();
                  if(mesytec_setup.readout.readout_complete())
                  {
#ifdef DEBUG
                     std::cout << " readout complete : treat_complete_event : ";
#endif
                     treat_complete_event(F);
                  }
#ifdef DEBUG
                  else
                     std::cout << std::endl;
#endif
               }
               else if(!mesytec_setup.readout.dummy_module())
               {
                  // read data from VME module ?
                  if(is_mesytec_data(next_word))
                  {
#ifdef DEBUG
                     std::cout << "DATA\n";
#endif
                     mod_data.add_data(next_word);
                  }
               }
            }
            else if(is_module_header(next_word))
            {
#ifdef DEBUG
               std::cout << "HEADER : id = " << std::hex << std::showbase << (int) module_id(next_word);
#endif
               got_header = mesytec_setup.readout.is_next_module(module_id(next_word));

               if(got_header){
                  if(!mesytec_setup.readout.dummy_module())
                  {
                     // not a dummy module, i.e. 'START' or 'END' markers
                     auto firmware = mesytec_setup.get_module(module_id(next_word)).firmware;
#ifdef DEBUG
                     std::cout << " : real\n";
#endif
                     mod_data.set_header_word(next_word,firmware);
                  }
#ifdef DEBUG
                  else
                     std::cout << " : dummy\n";
#endif
               }
#ifdef DEBUG
               else
                  std::cout << " : unknown module header\n";
#endif
            }
#ifdef DEBUG
            else
               std::cout << std::endl;
#endif
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

         bool tgv = false;
         int tgv_i=0;

         bool merde=false;
         int lines_to_print;


         bool writing=true;

         while(bytes_left_in_buffer)
         {
            //            if((nbytes-bytes_left_in_buffer)==200)
            //            {
            //                  std::cout << "\n============================================================\n";
            //                  writing=false;
            //            }
            //            else if(bytes_left_in_buffer==200)
            //            {
            //               writing=true;
            //            }
            //            if(!writing)
            //            {
            //               buf_pos+=4;
            //               bytes_left_in_buffer-=4;
            //               continue;
            //            }
            auto next_word = read_data_word(buf_pos);

            //            if(merde)
            //            {
            //               --lines_to_print;
            //               if(!lines_to_print) throw(std::runtime_error("SHIIIIIIIIIIIIIIIITTTTT"));
            //            }
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
            //else if(!is_fill_word(next_word))
            {
               auto dec_word = decode_type(next_word);
               //if(dec_word!="MDPP-DATA"
               //    && dec_word!="END-OF-EVENT")
               std::cout << std::hex << std::showbase << next_word << " " << dec_word << std::dec << std::endl;
               //               if(dec_word=="TGV")
               //               {
               //                  tgv=true;
               //                  tgv_i=0;
               //               }
               //               else if(tgv && is_tgv_data(next_word))
               //               {
               //                  ++tgv_i;
               //               }
               //               else if(tgv && dec_word=="END-OF-EVENT")
               //               {
               //                  if(tgv_i!=4){
               //                     std::cout << "Got " << tgv_i << " bits of timestamp" << std::endl;
               //                     merde=true;
               //                     lines_to_print=20;
               //                  }
               //                  tgv=false;
               //                  std::cout << "I READ " << tgv_i << " TGV DATA WORDS" << std::endl;
               //               }
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

      /**
             @param _buf pointer to the beginning of the buffer
             @param nbytes size of buffer in bytes
             @param F function to call when parsed event is ready
             @param mfm_frame_rev revision number of the MFM frame [default: 1]

             To be used to read one (and only one) event contained in the buffer extracted from an MFM data frame.

             When a complete event is ready the callback function F is called with the event and
             the description of the setup as
             arguments. Suitable signature for the callback function F is

             ~~~~{.cpp}
                void callback(mesytec::event&, mesytec::experimental_setup&);
             ~~~~

             (it can also of course be implemented with a lambda capture or a functor object).

             Straight after the call, the event will be deleted, so don't bother keeping a copy of a
             reference to it, any data must be treated/copied/moved in the callback function.
      */
      template<typename CallbackFunction>
      void read_event_in_buffer(const uint8_t* _buf, size_t nbytes, CallbackFunction F, u8 mfm_frame_rev = 1)
      {
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

      uint8_t* get_buffer_position() const { return buf_pos; }
      size_t get_remaining_bytes_in_buffer() const { return bytes_left_in_buffer; }
      uint32_t get_total_events_parsed() const { return total_number_events_parsed; }
      bool is_storing_last_complete_event() const { return storing_last_complete_event; }
      template<typename CallbackFunction>
      void cleanup_last_complete_event(CallbackFunction F)
      {
         // call user function for event saved from last time
         F(mesy_event,mesytec_setup);
         // reset event
         mesy_event.clear();
         storing_last_complete_event=false;
         // begin new readout cycle
         mesytec_setup.readout.begin_readout();
         // advance position in buffer
         buf_pos+=4;
      }
   };
}
#endif // MESYTEC_BUFFER_READER_H
