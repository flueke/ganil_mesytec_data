#ifndef MESYTEC_MODULE_H
#define MESYTEC_MODULE_H

#include <cstdint>
#include <vector>
#include <iostream>
#include <map>

namespace mesytec
{
   namespace data_flags
   {
      const uint32_t header_found_mask = 0xF0000000;
      const uint32_t header_found = 0x40000000;
      const uint32_t data_length_mask = 0x000003FF;
      const uint32_t module_setng_mask = 0x0000FC00;
      const uint32_t module_setng_div = 0x400;
      const uint32_t module_id_mask = 0x00FF0000;
      const uint32_t module_id_div = 0x10000;
      const uint32_t mdpp_data_mask = 0xF0000000;
      const uint32_t madc_data_mask = 0xFF800000;
      const uint32_t mdpp_data = 0x10000000;
      const uint32_t madc_data = 0x04000000;
      const uint32_t channel_mask_mdpp16 = 0x000F0000;
      const uint32_t channel_mask_mdpp32 = 0x001F0000;
      const uint32_t channel_div = 0x0010000;
      const uint32_t channel_flag_mask_mdpp32 = 0x00600000;
      const uint32_t channel_flag_div_mdpp32 = 0x00200000;
      const uint32_t channel_flag_mask_mdpp16 = 0x00300000;
      const uint32_t channel_flag_div_mdpp16 = 0x00100000;
      const uint32_t mdpp_flags_mask = 0x0FC00000;
      const uint32_t mdpp_flags_div = 0x0040000;
      const uint32_t eoe_found_mask = 0xC0000000;
      const uint32_t eoe_event_counter_mask = 0x3FFFFFFF;
      const uint32_t data_mask = 0x0000FFFF;
      const uint32_t fill_word_found = 0x00000000;
      const uint32_t extended_ts_mask = 0xF0000000;
      const uint32_t extended_ts = 0x20000000;
   };

   enum firmware_t
   {
      SCP,
      QDC,
      CSI
   };

   struct end_of_buffer : public std::runtime_error
   {
      end_of_buffer(const std::string& what)
         : std::runtime_error(what)
      {}
   };

   struct module
   {
      std::string name;
      uint8_t id;
      firmware_t firmware;
      uint32_t channel_mask,channel_flag_mask,channel_flag_div;
      uint32_t DATA;
      std::map<uint8_t,std::string> channel_map;

      module() = default;
      module(const std::string& _name, uint8_t _id, uint8_t nchan, firmware_t F)
         : name{_name}, id{_id}, firmware{F}
      {
         if(nchan==16)
         {
            // mdpp-16
            channel_mask = data_flags::channel_mask_mdpp16;
            channel_flag_mask = data_flags::channel_flag_mask_mdpp16;
            channel_flag_div = data_flags::channel_flag_div_mdpp16;
         }
         else
         {
            // mdpp-32
            channel_mask = data_flags::channel_mask_mdpp32;
            channel_flag_mask = data_flags::channel_flag_mask_mdpp32;
            channel_flag_div = data_flags::channel_flag_div_mdpp32;
         }
      }
      void set_data_word(uint32_t data){ DATA = data; }
      uint8_t channel_number() const
      {
          return (DATA & channel_mask) / data_flags::channel_div;
      }

      unsigned int channel_data() const
      {
          return (DATA & data_flags::data_mask);
      }
      uint8_t channel_flags() const
      {
         // =0 : data is ADC or QDC_long
         // =1 : data is TDC
         // =3 : data is QDC_short
         // =2 : data is trigger time
          return (DATA & channel_flag_mask)/channel_flag_div;
      }
      void print_mdpp_data() const
      {
          printf("== MDPP-DATA :: %s [%#04x]  chan_number = %02d    %s = %5d\n",
                     name.c_str(), id, channel_number(), data_type().c_str(), channel_data());
      }
      std::string data_type() const
      {
         // =0 : data is ADC or QDC_long
         // =1 : data is TDC
         // =3 : data is QDC_short
         // =2 : data is trigger time

         switch(channel_flags())
         {
         case 0:
            return firmware==QDC ? "qdc_long" : "adc";
         case 1:
            return "tdc";
         case 2:
            return "trig";
         case 3:
            return "qdc_short";
         }
         return "unknown";
      }
      auto& get_channel_map() { return channel_map; }

      /// Get name of detector associated with channel number
      auto operator[](uint8_t nchan){ return channel_map[nchan]; }
   };

   std::map<uint8_t, module> define_setup(std::vector<module>&& modules);
   uint32_t read_data_word(std::istream& data);
   uint32_t read_data_word(const uint8_t* data);
   bool is_header(uint32_t DATA);
   bool is_end_of_event(uint32_t DATA);
   bool is_mdpp_data(uint32_t DATA);
   bool is_fill_word(uint32_t DATA);
   bool is_extended_ts(uint32_t DATA);
   void print_type(uint32_t DATA);
   uint16_t length_of_data(uint32_t DATA);
   uint8_t module_id(uint32_t DATA);
   unsigned int module_setting(uint32_t DATA);
   void print_header(uint32_t DATA);
   unsigned int extended_timestamp(uint32_t DATA);
   unsigned int event_counter(uint32_t DATA);
   void print_eoe(uint32_t DATA);
   void print_ext_ts(uint32_t DATA);
}
#endif // MESYTEC_MODULE_H
