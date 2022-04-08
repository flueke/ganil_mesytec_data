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
      bool reading_tgv{false};
      bool reading_mvlc_scaler{false};
      bool reading_mdpp{false};
      static const size_t last_buf_store_size=200;// 4 * nwords
      std::array<uint8_t,last_buf_store_size> store_end_of_last_buffer;
      std::array<uint32_t,4> mesytec_tgv_data;
      uint mesytec_tgv_index,mvlc_scaler_index;
      uint64_t last_timestamp{0};
      double max_timestamp_diff{3600.};
      int buf_copy{200};
      uint8_t last_buffer[200];
      bool have_last_buffer=false;

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
         reading_tgv=false;
         reading_mvlc_scaler=false;
         reading_mdpp=false;
         mesytec_setup.readout.begin_readout();
         mesytec_tgv_index=0;
         last_timestamp=0;
         mvlc_scaler_index=0;
         have_last_buffer=false;
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

      template<typename CallbackFunction>
      uint32_t treat_complete_event(CallbackFunction F)
   {
      // the end of the readout of all modules is signalled by the END_READOUT dummy module

      got_header=false;
      //if(printit) std::cout << "readout complete" << std::endl;

      // check TGV data
      event.tgv_ts_lo = 0;
      event.tgv_ts_mid = 0;
      event.tgv_ts_hi = 0;
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
            event.tgv_ts_lo = (mesytec_tgv_data[1] &= data_flags::tgv_data_mask_lo);
            event.tgv_ts_mid = (mesytec_tgv_data[2] &= data_flags::tgv_data_mask_lo);
            event.tgv_ts_hi = (mesytec_tgv_data[3] &= data_flags::tgv_data_mask_lo);
            //std::cout << event.tgv_ts_hi << " " << event.tgv_ts_mid << " " << event.tgv_ts_lo << std::endl;
         }
      }

      // clear module data ready for next full readout
      mod_data.clear();

      reading_tgv=false;
      reading_mvlc_scaler=false;
      reading_mdpp=false;

      // in case callback function 'aborts' (output buffer full) and we have to keep the event for later
      storing_last_complete_event=true;

//               if(printit)
//               {
//                  event.ls(mesytec_setup);
//               }
      // call callback function
      F(event);
      ++event.event_counter;

      storing_last_complete_event=false;// successful callback

      // reset event after sending/encapsulating
      event.clear();

      // begin new readout cycle
      mesytec_setup.readout.begin_readout();

      ++total_number_events_parsed;
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
         //dump_data_stream(_buf,nbytes);

         buf_pos = const_cast<uint8_t*>(_buf);
         bytes_left_in_buffer = nbytes;

         total_number_events_parsed = 0;

         // if there is a system event unix time tick at the beginning of the frame,
         // no data is to be read until EXT-TS frame and EXT-TS-FRIEND frame follow.
         bool wait_for_ext_ts=false;
         bool seen_ext_ts=false;

//         int nwordsread=0;
         //bool printit=true;
//         std::cout << "=============================================================================" << std::endl;
//         std::cout << "       *************         READING NEW BUFFER         *************        " << std::endl;
//         std::cout << "=============================================================================" << std::endl;

         while(bytes_left_in_buffer)
         {
            auto next_word = read_data_word(buf_pos);
//            ++nwordsread;
//            if(nwordsread>200) printit=false;

//            if(printit) std::cout << std::hex << std::showbase << next_word << std::dec << std::endl;

            // Ignore all frame headers
            if(is_frame_header(next_word))
            {
               if(is_system_unix_time(next_word))
               {
                  if(!seen_ext_ts) wait_for_ext_ts=true;
                  // for system event with unix time tick, also skip the following word (unix date/time)
                  buf_pos+=4;
                  bytes_left_in_buffer-=4;
                  //if(printit) std::cout << "unix-time ignored" << std::endl;
                  if(!bytes_left_in_buffer) break;
               }
               buf_pos+=4;
               bytes_left_in_buffer-=4;
               //if(printit) std::cout << "frame_header ignored" << std::endl;
               continue;
            }
            // Ignore all 'EXT-TS' (0x2a7d00d9) and the following 'friend' frame (0x1xxxx)
            if(is_extended_ts(next_word) || is_exts_friend(next_word))
            {
               // advance to next 32-bit word in buffer
               buf_pos+=4;
               bytes_left_in_buffer-=4;
               seen_ext_ts=true;
               wait_for_ext_ts=false;
               //if(printit) std::cout << "ext-ts ignored" << std::endl;
               continue;
            }
            if(is_event_header(next_word))
            {
//               if(printit){
//                  auto dec_word = decode_type(next_word);
//                  std::cout << std::hex << std::showbase << next_word << " " << dec_word << std::dec << std::endl;
//               }
               mdpp::module_data tmp{next_word};
               // check readout sequence
               got_header = mesytec_setup.readout.is_next_module(tmp.module_id);
               //std::cout << "module-id = " << (int)tmp.module_id << std::endl;
               if(got_header) {
                  wait_for_ext_ts=false;//if a module header is seen at beginning of buffer, do not wait for ext-ts
                  //if(printit) std::cout << " - to be read" << std::endl;
                  mod_data=std::move(tmp);
                  auto firmware = mesytec_setup.get_module(mod_data.module_id).firmware;
                  //std::cout << "firmware = " << (int)firmware << std::endl;
                  reading_mdpp = (firmware == SCP || firmware == QDC);
                  reading_tgv = (firmware == TGV);
                  reading_mvlc_scaler = (firmware == MVLC_SCALER);
                  //if(reading_mdpp) {
                     //std::cout << "reading mdpp !!!" << std::endl;
                  //}
                  if(reading_tgv) {
                     mesytec_tgv_index=0;
                     //std::cout << "reading tgv !!!" << std::endl;
                  }
                  if(reading_mvlc_scaler) {
                     mvlc_scaler_index=0;
                     //std::cout << "reading scaler !!!" << std::endl;
                  }
               }
               else
               {
                  // check for 'truncated event' i.e. start_event marker before finishing previous one
                  if(mesytec_setup.readout.get_readout_state()==setup_readout::readout_state_t::start_event_found_in_readout_cycle)
                  {
                     treat_complete_event(F);
                     mesytec_setup.readout.force_state_in_readout_cycle();
                  }
               }
            }
            else if(!wait_for_ext_ts && reading_tgv) {
               if(is_tgv_data(next_word))
               {
                  if(mesytec_tgv_index>3) std::cout << "***ERROR*** got too much TGV ***ERROR***" << std::endl;
                  else
                  {
                     mesytec_tgv_data[mesytec_tgv_index]=next_word;
                     //if(printit) std::cout << "  tgv data " << std::dec << mesytec_tgv_index << " = " << next_word << std::endl;
                  }
                  ++mesytec_tgv_index;
               }
               else if(is_end_of_event_tgv(next_word)){
                  reading_tgv=false;
                  //if(printit) std::cout << "  finished reading tgv" << std::endl;
                  if(mesytec_tgv_index!=4)
                  {
                     std::cout << "***ERROR*** For TGV I read " << mesytec_tgv_index
                               << " 16 bit words (should be 4):" << std::endl;
                  }
               }
            }
            else if(!wait_for_ext_ts && reading_mvlc_scaler)
            {
               if(is_tgv_data(next_word))// scaler data is also only in the 16 low bits, nothing in the high bits
               {
                  // read data coming from MVLC counters
                  mod_data.add_data(next_word);
                  //if(printit) std::cout << "  scaler data " << std::dec << mvlc_scaler_index << " = " << next_word << std::endl;
                  ++mvlc_scaler_index;
               }
               else if(is_end_of_event_tgv(next_word)){ // same EOE as for TGV
                  reading_mvlc_scaler=false;
                  event.add_module_data(mod_data);
                  //if(printit) std::cout << "  finished reading scaler" << std::endl;
                  if(mvlc_scaler_index!=4)
                  {
                     std::cout << "***ERROR*** For MVLC scaler " <<
                                  std::hex << std::showbase << (int)mod_data.module_id <<
                                  " I read " << std::dec << mvlc_scaler_index
                               << " 16 bit words (should be 4)" << std::endl;

                     int buffer_start_offset = buf_pos - _buf;
                     int start_frame = std::min(12,buffer_start_offset/4);
                     for(int i=-start_frame;i<=0;++i)
                     {
                        auto wor = read_data_word(buf_pos+i*4);
                        auto dec_word = decode_type(wor);
                        std::cout << i << " :: " << std::hex << std::showbase << wor << " " << dec_word << std::dec << std::endl;
                     }
                  }
               }
            }
            else if(reading_mdpp)
            {
               if(is_mdpp_data(next_word))
               {
                  mod_data.add_data(next_word);
                  //if(printit) std::cout << "  mdpp data = " << std::dec << next_word << std::endl;
               }
               else if(is_end_of_event(next_word))
               {
                  event.add_module_data(mod_data);
                  //if(printit) std::cout << "  finished reading mdpp" << std::endl;
                  reading_mdpp=false;
               }
            }

            if(mesytec_setup.readout.readout_complete())
               treat_complete_event(F);

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
         module *current_module;

         while(words_to_read--)
         {
            auto next_word = read_data_word(buf_pos);

            if(is_event_header(next_word))
            {
               // add previously read module to event
               if(mod_data.module_id) event.add_module_data(mod_data);

               // new module
               mod_data = mdpp::module_data{next_word};
               current_module = &mesytec_setup.get_module(mod_data.module_id);
               // in principle maximum event size is 255 32-bit words i.e. 1 header + 254 following words
               if(mod_data.data_words>=254) std::cerr << "Header indicates " << mod_data.data_words << " words in this event..." << std::endl;

               reading_mvlc_scaler = current_module->firmware == MVLC_SCALER;
            }
            else if(reading_mvlc_scaler)
            {
               mod_data.add_data(next_word);
            }
            else if(is_mdpp_data(next_word)) {
               current_module->set_data_word(next_word);
               mod_data.add_data( current_module->data_type(), current_module->channel_number(),
                                  current_module->channel_data(), next_word);
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
