#ifndef MESYTEC_DATA_H
#define MESYTEC_DATA_H

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>
#include "mesytec_experimental_setup.h"

namespace mesytec
{
   const uint16_t mfm_frame_type = 0x4adf;

   struct channel_data
   {
      std::string data_type; /// "adc", "tdc", "qdc_short", "qdc_long", "trig0", "trig1"
      uint32_t data_word;
      uint16_t data;
      uint8_t channel;

      channel_data()=default;
      ~channel_data()=default;
      channel_data(uint32_t _dw)
         : data_word{std::move(_dw)}
      {}
      channel_data(std::string _type, uint8_t _chan, uint16_t _data, uint32_t _dw)
         : data_type{std::move(_type)}, data_word{std::move(_dw)}, data{std::move(_data)}, channel{std::move(_chan)}
      {}
      channel_data(channel_data&& other)
         : data_type{std::move(other.data_type)}, data_word{std::move(other.data_word)}, data{std::move(other.data)}, channel{std::move(other.channel)}
      {}
      channel_data(const channel_data&) = delete;
      channel_data& operator=(const channel_data&) = delete;
      channel_data& operator=(channel_data&& other)
      {
         if(this != &other)
         {
            data_type=std::move(other.data_type); data=std::move(other.data); channel=std::move(other.channel); data_word=std::move(other.data_word);
         }
         return *this;
      }
      void ls(const mesytec::experimental_setup &cfg, uint8_t mod_id) const
      {
         auto mod = cfg.get_module(mod_id);
         mod.set_data_word(data_word);
         mod.print_mdpp_data();
      }
      void add_data_to_buffer(std::vector<uint32_t>& buf) const
      {
         buf.push_back(data_word);
      }
   };

   struct module_data
   {
      std::vector<channel_data> data;
      uint32_t event_counter : 30;
      uint32_t header_word;
      uint32_t eoe_word;
      uint16_t data_words : 10; // number of data items + 1 EOE
      uint8_t module_id{0};

      void clear()
      {
         data.clear();
         event_counter=0;
         header_word=0;
         data_words=0;
         module_id=0;
         eoe_word=0;
      }

      module_data(uint32_t _header_word)
         : header_word{_header_word}, data_words{length_of_data(_header_word)},
           module_id{mesytec::module_id(_header_word)}
      {
         data.reserve(data_words); // max number of data words for 1 module
      }
      module_data()=default;
      ~module_data()=default;
      module_data(module_data&& other)
         : data{std::move(other.data)}, event_counter{other.event_counter},
           header_word{other.header_word},
           eoe_word{other.eoe_word}, data_words{other.data_words}, module_id{other.module_id}
      {}
      module_data(const module_data&) = delete;
      module_data& operator=(const module_data&) = delete;
      module_data& operator=(module_data&& other)
      {
         if(this != &other)
         {
            data=std::move(other.data); event_counter=other.event_counter;
            data_words=other.data_words; module_id=other.module_id;
            header_word=other.header_word; eoe_word=other.eoe_word;
         }
         return *this;
      }
      void add_data(std::string type, uint8_t channel, uint16_t datum, uint32_t data_word)
      {
         data.emplace_back(type,channel,datum,data_word);
      }
      void add_data(uint32_t data_word)
      {
         data.emplace_back(data_word);
      }
      void ls(const mesytec::experimental_setup & cfg) const
      {
         std::cout << " Module-ID=" << std::hex << std::showbase << (unsigned int)module_id << std::dec;
         auto mod = cfg.get_module(module_id);
         std::cout << " " << mod.name;
         if(mod.is_mdpp_module())
         {
            std::cout << "  [data words:" << data.size() << "]\n";
            for(auto& d : data) d.ls(cfg,module_id);
         }
         else if(mod.is_mvlc_scaler())
         {
            // 4 data words of 16 bits (least significant is first word) => 64 bit scaler
            assert(data.size()==4);
            uint64_t x=0;
            int i=0;
            for(auto& d : data) x += ((uint64_t)d.data_word)<<(16*(i++));
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
   };

   struct event
   {
      std::vector<module_data> modules = std::vector<module_data>(50);
      uint32_t event_counter;
      uint16_t tgv_ts_lo,tgv_ts_mid,tgv_ts_hi;

      void clear()
      {
         modules.clear();
      }
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
   };
}
#endif // READ_LISTFILE_H
