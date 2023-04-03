#ifndef MESYTEC_BUFFER_READER_H
#define MESYTEC_BUFFER_READER_H

//#undef NDEBUG

#include "mesytec_data.h"
#include "mesytec_experimental_setup.h"
#include <cassert>
#include <ios>
#include <ostream>
#include <cstring>
#include <ctime>
#include <array>

#define MESYTEC_DATA_BUFFER_READER_NO_DEFINE_SETUP
#define MESYTEC_DATA_BUFFER_READER_CALLBACK_WITH_EVENT_AND_SETUP

//#define DEBUG

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
   };
}
#endif // MESYTEC_BUFFER_READER_H
