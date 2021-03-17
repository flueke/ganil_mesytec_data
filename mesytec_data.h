#ifndef MESYTEC_DATA_H
#define MESYTEC_DATA_H

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

namespace mesytec
{
   struct mdpp_channel_data
   {
      std::string data_type; /// "adc", "tdc", "qdc_short", "qdc_long", "trig0", "trig1"
      uint16_t data;
      uint8_t channel;

      mdpp_channel_data()=default;
      ~mdpp_channel_data()=default;
      mdpp_channel_data(std::string _type, uint8_t _chan, uint16_t _data)
         : data_type{std::move(_type)}, data{std::move(_data)}, channel{std::move(_chan)}
      {}
      mdpp_channel_data(mdpp_channel_data&& other)
         : data_type{std::move(other.data_type)}, data{std::move(other.data)}, channel{std::move(other.channel)}
      {}
      mdpp_channel_data(const mdpp_channel_data&) = delete;
      mdpp_channel_data& operator=(const mdpp_channel_data&) = delete;
      mdpp_channel_data& operator=(mdpp_channel_data&& other)
      {
         if(this != &other)
         {
            data_type=std::move(other.data_type); data=std::move(other.data); channel=std::move(other.channel);
         }
         return *this;
      }
      void ls() const
      {
         std::cout << "\tCHAN#" << (unsigned int)channel << "\tTYPE=" << data_type << "\tDATA=" << data << std::endl;
      }
   };

   struct mdpp_module_data
   {
      std::vector<mdpp_channel_data> data;
      uint32_t event_counter : 30;
      uint16_t data_words : 10;
      uint8_t module_id;

      mdpp_module_data(uint8_t mod_id, uint16_t dat_len)
         : data_words{dat_len}, module_id{mod_id}
      {
         data.reserve(dat_len); // max number of data words for 1 module
      }
      mdpp_module_data()=default;
      ~mdpp_module_data()=default;
      mdpp_module_data(mdpp_module_data&& other)
         : data{std::move(other.data)}, event_counter{other.event_counter},
           data_words{other.data_words}, module_id{other.module_id}
      {}
      mdpp_module_data(const mdpp_module_data&) = delete;
      mdpp_module_data& operator=(const mdpp_module_data&) = delete;
      mdpp_module_data& operator=(mdpp_module_data&& other)
      {
         if(this != &other)
         {
            data=std::move(other.data); event_counter=other.event_counter;
            data_words=other.data_words; module_id=other.module_id;
         }
         return *this;
      }
      void add_data(std::string type, uint8_t channel, uint16_t datum)
      {
         data.emplace_back(type,channel,datum);
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
   };

}
#endif // READ_LISTFILE_H
