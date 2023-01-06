#ifndef MESYTEC_MODULE_DATA_INTERPRETER_H
#define MESYTEC_MODULE_DATA_INTERPRETER_H

#include <cstdint>

namespace mesytec {

   /**
      @class module_data_interpreter
      @brief Abstract base class for module-dependent interpretation of mesytec data
    */
   class module_data_interpreter
   {
   public:
      virtual uint8_t channel_number(uint32_t DATA) const = 0;
   };

   /**
      @class mdpp_data_interpreter
      @brief Interprets data received from MDPP modules
    */
   class mdpp_data_interpreter : public module_data_interpreter
   {
   protected:
      uint32_t channel_mask;
      uint32_t channel_div = 0x0010000;
   public:
      /// @returns channel number corresponding to data word
      uint8_t channel_number(uint32_t DATA) const override
      {
          return (DATA & channel_mask) / channel_div;
      }
   };

   /**
      @class mdpp16_data_interpreter
      @brief Interprets data received from MDPP-16 modules
    */
   class mdpp16_data_interpreter : public mdpp_data_interpreter
   {
   public:
      mdpp16_data_interpreter() { channel_mask = 0x000F0000; }
   };

   /**
      @class mdpp32_data_interpreter
      @brief Interprets data received from MDPP-32 modules
    */
   class mdpp32_data_interpreter : public mdpp_data_interpreter
   {
   public:
      mdpp32_data_interpreter() { channel_mask = 0x001F0000; }
   };

   /**
      @class vmmr_data_interpreter
      @brief Interprets data received from VMMR modules
    */
   class vmmr_data_interpreter : public module_data_interpreter
   {
   };
} // namespace mesytec

#endif // MESYTEC_MODULE_DATA_INTERPRETER_H
