/****************************************************************************
SteveHAL_Windows.h
(C) 2023 Jac Goudsmit
MIT License.

This file declares a Windows-specific Hardware Abstraction Layer for Steve.
****************************************************************************/

#ifndef _STEVEHAL_WINDOWS_H
#define _STEVEHAL_WINDOWS_H

#ifdef _WIN32

/////////////////////////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "ftd2xx.h"
#include "libmpsse_spi.h"
#include "SteveHAL.h"

#pragma comment(lib, "libmpsse.lib")

/////////////////////////////////////////////////////////////////////////////
// STEVE HAL FOR WINDOWS
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
// This implements a Steve Hardware Abstraction Layer for Windows using the
// MPSSE library from FTDI. It can be used with the C232HM-DDHSL-0
// cable (which uses the FTDI FT232H at 3.3V) to control a display directly
// from a Windows application.
//
// Other FTDI chip sets should work too such as the FT4222.
//
// When using a Crystalfontz CFA10098 evaluation/interface board, connect
// the C232HM-DDHSL-0 cable as shown in the CFA10098 manual:
// (1) VCC: Read Below!
// (2) GND: Read Below!
// (3) SCK: Orange
// (4) MOSI: Yellow
// (5) MISO: Green
// (6) GPIO0: N/C
// (7) GPIO1: N/C
// (8) GND: Black
// (9) !CS: Brown
// (10): !INT: Purple
// (11): !PD: Blue
// (12): GPIO2: N/C
// (13): GND: N/C
// Grey, White and possibly Red wires are unused.
// Note: The CrystalFontz display evaluation kits use the same wiring colors
// between the Arduino and the CFA10098 breakout board as the wires that
// are attached to the C232HM-DDHSL-0.
// 
// IMPORTANT: The Red wire from the C232HM-DDHSL-0 cable can be used on pin
// 1 to supply SOME of the displays that are available from CrystalFontz,
// such as the CFA480128 series, because they use 3.3V as power voltage and
// don't use much current. However for most devices (especially bigger
// displays), you should NOT connect the red wire to pin 1 of the CFA10098,
// but should supply the power some other way. For example, the CFA800480
// requires 5V (not 3.3V) and 128 mA which cannot be supplied by the
// C232HM-DDHSL-0 cable. Hint: Check out the CrystalFontz evaluation kit for
// your display of choice. If the evaluation kit has pin 1 connected to
// the 5V pin of the Arduino, you can't supply the display from the
// C232HM-DDHSL-0 cable.
// 
// Check the documentation of your display and the documentation of your
// USB-SPI cable for information about power requirements and capabilities.
// The author will not take responsibility for hardware that failed for
// any reason. See the LICENSE file.
class SteveHAL_Windows_MPSSE : public SteveHAL
{
protected:
  //-------------------------------------------------------------------------
  // Data
  DWORD             _channel;           // MPSSE channel to use
  UINT32            _clockRate;         // Clock frequency to use

  FT_HANDLE         _ftHandle;          // Handle to the channel
  bool              _selected;          // True if EVE chip is selected

  const static size_t _cacheSize = 128; // Size of cache buffer
  BYTE              _cache[_cacheSize]; // Write cache buffer
  size_t            _cacheIndex;        // Number of bytes in cache

public:
  //-------------------------------------------------------------------------
  // Constructor
  SteveHAL_Windows_MPSSE(
    DWORD channel,                      // MPSSE channel to use
    UINT32 clockrate)                   // Clock frequency to use
  {
    _channel = channel;
    _clockRate = clockrate;

    _ftHandle = 0;
    _selected = true;

    _cacheIndex = 0;
  }

protected:
  //-------------------------------------------------------------------------
  // Initialize the hardware
  virtual bool Begin() override         // Returns true if successful
  {
    bool result = false;

    if (!_ftHandle)
    {
      FT_DEVICE_LIST_INFO_NODE devList;
      DWORD u;
      DWORD channels;
      FT_STATUS status;

      Init_libMPSSE();

      status = SPI_GetNumChannels(&channels);
      for (u = 0; u < channels; u++)
      {
        status = SPI_GetChannelInfo(u, &devList);
        printf("SPI_GetNumChannels returned %u for channel %u\n", status, u);
        /*print the dev info*/
        printf("      VID/PID: 0x%04x/0x%04x\n", devList.ID >> 16, devList.ID & 0xffff);
        printf("      SerialNumber: %s\n", devList.SerialNumber);
        printf("      Description: %s\n", devList.Description);
      }

      if (_channel < channels)
      {
        status = SPI_OpenChannel(_channel, &_ftHandle);
        if (status != FT_OK)
        {
          fprintf(stderr, "Channel %u failed to open status %u\n", _channel, status);
        }
        else
        {
          result = true;
        }
      }
      else
      {
        fprintf(stderr, "Not enough channels found (wanted >%u got %u)\n", _channel, channels);
      }
    }

    return result;
  }

protected:
  //-------------------------------------------------------------------------
  // Initialize the communication
  virtual void Init(
    bool slow = false) override         // True=use slow speed for early init
  {
    FT_STATUS status;

    if (slow)
    {
      UINT32 rate = _clockRate;
      if (slow && (rate > 8000000))
      {
        rate = 8000000;
      }

      ChannelConfig channelConf = { 0 };
      channelConf.ClockRate = rate;
      channelConf.LatencyTimer = 10;
      channelConf.configOptions = SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;

      status = SPI_InitChannel(_ftHandle, &channelConf);
      if (status != FT_OK)
      {
        fprintf(stderr, "Channel %u failed to initialize SPI status %u\n", _channel, status);
        exit(-3);
      }

/*
      status = FT_SetUSBParameters(_ftHandle, 64, 64);
      if (status != FT_OK)
      {
        fprintf(stderr, "Channel %u failed to setup USB parameters %u\n", _channel, status);
        exit(-3);
      }
*/
    }
  }

protected:
  //-------------------------------------------------------------------------
  // Pause or resume communication
  virtual void Pause(
    bool pause) override                // True=pause, false=resume
  {
    //printf("Pause is not supported at this time\n");
  }

protected:
  //-------------------------------------------------------------------------
  // Turn the power on or off
  virtual void Power(
    bool enable) override               // True=on (!PD high) false=off/reset
  {
    // Temporarily change the CS output to DBUS7 (Blue)
    SPI_ChangeCS(_ftHandle, SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS7 | SPI_CONFIG_OPTION_CS_ACTIVELOW);

    // Change the pin
    SPI_ToggleCS(_ftHandle, !enable);

    // Change CS back to pin DBUS3 (Orange)
    SPI_ChangeCS(_ftHandle, SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW);

    _cacheIndex = 0;
  }

protected:
  //-------------------------------------------------------------------------
  // Send write cache buffer
  void SendCache()
  {
    if (_cacheIndex)
    {
      DWORD sizeTransferred;

      SPI_Write(_ftHandle, _cache, _cacheIndex, &sizeTransferred, 0);

      _cacheIndex = 0;
    }
  }

protected:
  //-------------------------------------------------------------------------
  // Store data in cache
  size_t WriteToCache(LPCVOID buf, size_t size)
  {
    size_t result = 0;
    const UCHAR *block = (const UCHAR *)buf;
    size_t remsize = size;

    while (remsize)
    {
      size_t blocksize = remsize;

      if (_cacheIndex + blocksize > _cacheSize)
      {
        blocksize = _cacheSize - _cacheIndex;
      }

      if (blocksize)
      {
        memcpy(&_cache[_cacheIndex], block, blocksize);
        block += blocksize;
        _cacheIndex += blocksize;
        remsize -= blocksize;
        result += blocksize;
      }

      if (_cacheIndex == _cacheSize)
      {
        SendCache();
      }
    }

    return result;
  }

protected:
  //-------------------------------------------------------------------------
  // Select or de-select the chip
  virtual bool Select(
    bool enable) override               // True=select (!CS low) false=de-sel
  {
    bool result = (enable != _selected);

    if (result)
    {
      if (!enable)
      {
        SendCache();
      }

      SPI_ToggleCS(_ftHandle, !!enable);

      _selected = enable;
    }

    return result;
  }

protected:
  //-------------------------------------------------------------------------
  // Transfer data to and from the EVE chip
  virtual uint8_t Transfer(             // Returns received byte
    uint8_t value) override             // Byte to send
  {
    fprintf(stderr, "BUG: You shouldn't get here");
    exit(-3);
  }

protected:
  //-------------------------------------------------------------------------
  // Send an 8-bit value
  virtual void Send8(
    uint8_t value)                      // Value to send
  {
    WriteToCache(&value, 1);
  }

protected:
  //-------------------------------------------------------------------------
  // Send a 16-bit value in little-endian format
  //
  // The least significant byte is sent first.
  virtual void Send16(
    uint16_t value)                     // Value to send
  {
    WriteToCache(&value, 2);
  }

protected:
  //-------------------------------------------------------------------------
  // Send a 24 bit value in BIG ENDIAN format
  virtual void Send24BE(
    uint32_t value)                     // Value to send (MSB ignored)
  {
    UINT32 buf = ((value >> 16) & 0xFF) | (value & 0x00FF00) | ((value & 0xFF) << 16);

    WriteToCache(&buf, 3);
  }

protected:
  //-------------------------------------------------------------------------
  // Send a 32-bit value in little-endian format
  //
  // The least significant byte is sent first.
  virtual void Send32(
    uint32_t value)                     // Value to send
  {
    WriteToCache(&value, 4);
  }

protected:
  //-------------------------------------------------------------------------
  // Send data from a RAM buffer to the chip
  virtual uint32_t SendBuffer(          // Returns number of bytes sent
    const uint8_t *buffer,              // Buffer to send
    uint32_t len)                       // Number of bytes to send
  {
    return WriteToCache(buffer, len);
  }

protected:
  //-------------------------------------------------------------------------
  // Receive an 8-bit value
  virtual uint8_t Receive8()            // Returns incoming value
  {
    uint8_t result;
    DWORD sizeTransferred;

    if (_cacheIndex)
    {
      SendCache();
    }

    // NOTE: Little-endian system assumed.
    if (FT_OK != SPI_Read(_ftHandle, &result, 1, &sizeTransferred, 0))
    {
      fprintf(stderr, "SPI_Read failed");
      exit(-3);
    }

    return result;
  }

protected:
  //-------------------------------------------------------------------------
  // Receive a 16-bit value in little-endian format
  //
  // The least significant byte is received first.
  virtual uint16_t Receive16()          // Returns incoming value
  {
    uint16_t result;
    DWORD sizeTransferred;

    if (_cacheIndex)
    {
      SendCache();
    }

    // NOTE: Little-endian system assumed.
    if (FT_OK != SPI_Read(_ftHandle, (uint8_t *)&result, 2, &sizeTransferred, 0))
    {
      fprintf(stderr, "SPI_Read failed");
      exit(-3);
    }

    return result;
  }

protected:
  //-------------------------------------------------------------------------
  // Receive a 32-bit value in little-endian format
  //
  // The least significant byte is received first.
  virtual uint32_t Receive32()          // Returns incoming value
  {
    uint32_t result;
    DWORD sizeTransferred;

    if (_cacheIndex)
    {
      SendCache();
    }

    // NOTE: Little-endian system assumed.
    if (FT_OK != SPI_Read(_ftHandle, (uint8_t *)&result, 4, &sizeTransferred, 0))
    {
      fprintf(stderr, "SPI_Read failed");
      exit(-3);
    }

    return result;
  }

protected:
  //-------------------------------------------------------------------------
  // Receive a buffer
  virtual uint32_t ReceiveBuffer(       // Returns number of bytes received
    uint8_t *buffer,                    // Buffer to receive to
    uint32_t len)                       // Number of bytes to receive
  {
    DWORD sizeTransferred;

    if (_cacheIndex)
    {
      SendCache();
    }

    if (FT_OK != SPI_Read(_ftHandle, buffer, len, &sizeTransferred, 0))
    {
      fprintf(stderr, "SPI_Read failed");
      exit(-3);
    }

    return sizeTransferred;
  }

protected:
  //-------------------------------------------------------------------------
  // Wait for at least the requested time
  virtual void Delay(
    uint32_t ms) override               // Number of milliseconds to wait
  {
    Sleep(ms);
  }
};

/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////

#endif
#endif
