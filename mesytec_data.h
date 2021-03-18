#ifndef MESYTEC_DATA_H
#define MESYTEC_DATA_H

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "mesytec_module.h"

namespace mesytec
{
   struct mdpp_channel_data
   {
      std::string data_type; /// "adc", "tdc", "qdc_short", "qdc_long", "trig0", "trig1"
      uint16_t data;
      uint8_t channel;
      uint32_t data_word;

      mdpp_channel_data()=default;
      ~mdpp_channel_data()=default;
      mdpp_channel_data(std::string _type, uint8_t _chan, uint16_t _data, uint32_t _dw)
         : data_type{std::move(_type)}, data{std::move(_data)}, channel{std::move(_chan)}, data_word{std::move(_dw)}
      {}
      mdpp_channel_data(mdpp_channel_data&& other)
         : data_type{std::move(other.data_type)}, data{std::move(other.data)}, channel{std::move(other.channel)}, data_word{std::move(other.data_word)}
      {}
      mdpp_channel_data(const mdpp_channel_data&) = delete;
      mdpp_channel_data& operator=(const mdpp_channel_data&) = delete;
      mdpp_channel_data& operator=(mdpp_channel_data&& other)
      {
         if(this != &other)
         {
            data_type=std::move(other.data_type); data=std::move(other.data); channel=std::move(other.channel); data_word=std::move(other.data_word);
         }
         return *this;
      }
      void ls() const
      {
         std::cout << "\tCHAN#" << (unsigned int)channel << "\tTYPE=" << data_type << "\tDATA=" << data << std::endl;
      }
      void add_data_to_buffer(std::vector<uint32_t>& buf) const
      {
         buf.push_back(data_word);
      }
   };

   struct mdpp_module_data
   {
      std::vector<mdpp_channel_data> data;
      uint32_t event_counter : 30;
      uint32_t header_word;
      uint16_t data_words : 10; // number of data items + 1 EOE
      uint8_t module_id;
      uint32_t eoe_word;

      mdpp_module_data(uint32_t _header_word)
         : header_word{_header_word}, data_words{length_of_data(_header_word)},
           module_id{mesytec::module_id(_header_word)}
      {
         data.reserve(data_words); // max number of data words for 1 module
      }
      mdpp_module_data()=default;
      ~mdpp_module_data()=default;
      mdpp_module_data(mdpp_module_data&& other)
         : data{std::move(other.data)}, event_counter{other.event_counter},
           header_word{other.header_word}, data_words{other.data_words}, module_id{other.module_id},
           eoe_word{other.eoe_word}
      {}
      mdpp_module_data(const mdpp_module_data&) = delete;
      mdpp_module_data& operator=(const mdpp_module_data&) = delete;
      mdpp_module_data& operator=(mdpp_module_data&& other)
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
      void ls() const
      {
         std::cout << " Module-ID=" << std::hex << std::showbase << (unsigned int)module_id << std::dec;
         if(module_id==0) std::cout << "\t(MDPP-16)";
         else std::cout << "\t(MDPP-32)";
         std::cout << " : event#" << event_counter;
         std::cout << "  [data words:" << data_words-1 << "]\n";
         for(auto& d : data) d.ls();
      }
      void add_data_to_buffer(std::vector<uint32_t>& buf) const
      {
         // reconstruct mesytec data buffer for this module i.e. series of 32 bit words
         //  HEADER - N x DATA - EOE  (in total, N+2 words)
         //
         // if no data words are present for the module, no data is added to the buffer.
         if(data.size()){
            buf.push_back(header_word);
            for(auto& v: data) v.add_data_to_buffer(buf);
            buf.push_back(eoe_word);
         }
      }
      size_t size_of_buffer() const
      {
         // returns size (in 4-byte words) of buffer required to hold all data for this module
         //
         // if no data words are present for the module, this is zero.
         return data.size() ? data.size()+2 : 0;
      }
   };

   struct mdpp_event
   {
      std::vector<mdpp_module_data> modules;
      uint32_t event_counter;

      mdpp_event()
      {
         modules.reserve(21); // max number of modules in vme chassis
      }
      void add_module_data(mdpp_module_data& d){ modules.push_back(std::move(d)); }
      bool is_full(unsigned int number_of_modules) const
      {
         return (modules.size()==number_of_modules);
      }
      void ls() const
      {
         std::cout << " Event# " << event_counter << std::endl;
         for(auto& m: modules) m.ls();
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
