#ifndef MESYTEC_DATA_H
#define MESYTEC_DATA_H

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>
#include "mesytec_experimental_setup.h"

#define MESYTEC_DATA_NO_MDPP_NAMESPACE
#define MESYTEC_DATA_NO_PUBLIC_MEMBERS

namespace mesytec
{
   const uint16_t mfm_frame_type = 0x4adf;

   /**
      @class channel_data
      @brief a single item of data read from a module, corresponding to a given bus number, channel number and data type
    */
   class channel_data
   {
      uint32_t data_word;
      uint16_t data;
      uint8_t bus_number;
      uint8_t channel;
      module::datatype_t data_type;
   public:
      channel_data()=default;
      ~channel_data()=default;
      channel_data(uint32_t _dw)
         : data_word{std::move(_dw)}
      {}
      channel_data(module::datatype_t _type, uint8_t _chan, uint16_t _data, uint32_t _dw)
         : data_type{_type}, data_word{_dw}, data{_data}, bus_number{0}, channel{_chan}
      {}
      channel_data(module::datatype_t _type, uint8_t _bus, uint8_t _chan, uint16_t _data, uint32_t _dw)
         : data_type{_type}, data_word{_dw}, data{_data}, bus_number{_bus}, channel{_chan}
      {}
      channel_data(channel_data&& other)=default;
      channel_data(const channel_data&) = delete;
      channel_data& operator=(const channel_data&) = delete;
      channel_data& operator=(channel_data&& other)=default;
      void ls(const mesytec::experimental_setup &cfg, uint8_t mod_id) const
      {
         auto& mod = cfg.get_module(mod_id);
         mod.set_data_word(data_word);
         mod.print_data();
      }
      void add_data_to_buffer(std::vector<uint32_t>& buf) const
      {
         buf.push_back(data_word);
      }

      /**
         @return the full 32-bit data word read from the datastream corresponding to this data item
       */
      uint32_t get_data_word() const { return data_word; }
      /**
         @return the actual data (adc, qdc, etc.) associated with this data item
       */
      uint16_t get_data() const { return data; }
      /**
         @return the bus index associated with this data item
       */
      uint8_t get_bus_number() const { return bus_number; }
      /**
         @return the channel/subaddress associated with this data item
       */
      uint8_t get_channel_number() const { return channel; }
      /**
         @return the type of data associated with this data item
       */
      module::datatype_t get_data_type() const { return data_type; }
   };

   /**
      @class module_data
      @brief collects together all channel_data read from a single module in one event

      Iterating over the data from the module is simple to do:
      ~~~~{.cpp}
      void read_module_data(mesytec::module_data& mod)
      {
          for(auto& chan : mod.get_channel_data())
          {
             \// work with the individual channel_data objects
          }
      }
      ~~~~
   */
   class module_data
   {
      friend class buffer_reader;

      std::vector<channel_data> data;
      uint32_t event_counter;
      uint32_t header_word;
      uint32_t eoe_word;
      uint16_t data_words; // number of data items + 1 EOE
      uint8_t module_id{UNKNOWN};
   public:
      /**
         dump contents as list of 32 bit data words (starting with header)
       */
      void dump_module_data() const
      {
         std::cout << std::hex << std::showbase << header_word << std::endl;
         for(auto& v :data) std::cout << v.get_data_word() << std::endl;
      }

      uint32_t get_header_word() const { return header_word; }

      /**
         @return HW address of module in VME crate
       */
      uint8_t get_module_id() const { return module_id; }
      /**
         @return reference to vector containing all data from channels of this module (channel_data)
       */
      const std::vector<channel_data> & get_channel_data() const { return data; }
      void clear()
      {
         data.clear();
         event_counter=0;
         header_word=0;
         data_words=0;
         module_id=0;
         eoe_word=0;
      }

      /**
         @return number of data words announced by header (not counting the EOE)
       */
      uint16_t get_number_of_data_words() const
      {
         //assert(data_words);
         return data_words ? data_words-1 : 0;
      }
      void set_header_word(uint32_t _header_word, firmware_t firmware)
      {
         clear();
         header_word=_header_word;
         module_id = mesytec::module_id(_header_word);
         switch(firmware)
         {
         case MDPP_QDC:
         case MDPP_SCP:
         case MDPP_CSI:
            data_words = length_of_data_mdpp(_header_word);
            break;

         case VMMR:
            data_words = length_of_data_vmmr(_header_word);
            break;

         case TGV:
         case MVLC_SCALER:
            data_words = 4;
            break;

         default:
            data_words = 0;
         }
         if(data_words) data.reserve(data_words); // max number of data words for 1 module
      }
      module_data()=default;
      ~module_data()=default;
      module_data(module_data&&)=default;
      module_data(const module_data&) = delete;
      module_data& operator=(const module_data&) = delete;
      module_data& operator=(module_data&&)=default;
      void add_data(module::datatype_t type, uint8_t channel, uint16_t datum, uint32_t data_word)
      {
         data.emplace_back(type,channel,datum,data_word);
      }
      void add_data(module::datatype_t type, uint8_t busnum, uint8_t channel, uint16_t datum, uint32_t data_word)
      {
         data.emplace_back(type,busnum,channel,datum,data_word);
      }
      void add_data(uint32_t data_word)
      {
         data.emplace_back(data_word);
      }
      void ls(const mesytec::experimental_setup & cfg) const
      {
         auto& mod = cfg.get_module(module_id);
         if(mod.is_mdpp_module() || mod.is_vmmr_module())
         {
            //std::cout << "  [data words:" << data.size() << "]\n";
            for(auto& d : data) d.ls(cfg,module_id);
         }
         else if(mod.is_mvlc_scaler())
         {
            // 4 data words of 16 bits (least significant is first word) => 64 bit scaler
            assert(data.size()==4);
            uint64_t x=0;
            int i=0;
            for(auto& d : data) x += ((uint64_t)d.get_data_word())<<(16*(i++));
            std::cout << " = " << std::hex << std::showbase << x << std::endl;
         }
         else
            std::cout << std::endl;
      }
      void add_data_to_buffer(std::vector<uint32_t>& buf) const
      {
         // reconstruct mesytec data buffer for this module i.e. series of 32 bit words
         //  HEADER - N x DATA  (in total, N+1 words)
         //
         // if no data words are present for the module, no data is added to buffer
         // (not even the header)

         if(data.size())
         {
            buf.push_back(header_word);
            for(auto& v: data) v.add_data_to_buffer(buf);
         }
      }
      size_t size_of_buffer() const
      {
         // returns size (in 4-byte words) of buffer required to hold all data for this module
         // corresponding to header + N data words.
         //
         // if no data words are present for the module, this is 0
         return data.size() ? data.size()+1 : 0;
      }
      bool has_data() const { return data.size()>0; }
   };

   /**
      @class event
      @brief collection of data read out from the VME crate

      An event is a collection of module_data objects, each of which contains the data read from each module,
      in the form of a collection of channel_data objects.

      Iterating over the data of the event is simple to do:
      ~~~~{.cpp}
      void read_event(mesytec::event& ev)
      {
         for(auto& mod : ev.get_module_data())
         {
            for(auto& chan : mod.get_channel_data())
            {
               \// work with the individual channel_data objects
            }
         }
      }
      ~~~~
    */
   class event
   {
      friend class buffer_reader;

      std::vector<module_data> modules;
      uint32_t event_counter;
      uint16_t tgv_ts_lo,tgv_ts_mid,tgv_ts_hi;
   public:
      /**
         @return the least significant 16-bit word of the TGV timestamp data (bits 0-15)
       */
      uint16_t get_tgv_ts_lo() const { return tgv_ts_lo; }
      /**
         @return the middle 16-bit word of the TGV timestamp data (bits 16-31)
       */
      uint16_t get_tgv_ts_mid() const { return tgv_ts_mid; }
      /**
         @return the most significant 16-bit word of the TGV timestamp data (bits 32-47)
       */
      uint16_t get_tgv_ts_hi() const { return tgv_ts_hi; }
      /**
         @return 32-bit mesytec event counter
       */
      uint32_t get_event_counter() const { return event_counter; }
      event()
      {
         // reserve capacity for data from up to 25 modules (> capacity of 1 VME crate)
         // to avoid reallocations as data is 'pushed back' in to the vector
         modules.reserve(25);
      }
      void clear()
      {
         modules.clear();
      }
      /**
        @return reference to the collection of module_data objects
       */
      const std::vector<module_data>& get_module_data() const { return modules; }

      void add_module_data(module_data& d){ modules.push_back(std::move(d)); }
      bool is_full(unsigned int number_of_modules) const
      {
         return (modules.size()==number_of_modules);
      }
      void ls(const mesytec::experimental_setup & cfg) const
      {
         std::cout << " Event# " << event_counter << std::endl;
         for(auto& m: modules) m.ls(cfg);
      }
      size_t size_of_buffer() const
      {
         // returns size (in 4-byte words) of buffer required to hold all data for this event
         size_t s=0;
         for(auto& v : modules) s+=v.size_of_buffer();
         return s;
      }
      std::vector<uint32_t> get_output_buffer() const
      {
         // return full representation of all data for event

         std::vector<uint32_t> buf;
         buf.reserve(size_of_buffer());
         for(auto& m : modules) m.add_data_to_buffer(buf);
         return buf;
      }
      bool has_data() const { return modules.size()>0; }
   };
}
#endif // READ_LISTFILE_H
