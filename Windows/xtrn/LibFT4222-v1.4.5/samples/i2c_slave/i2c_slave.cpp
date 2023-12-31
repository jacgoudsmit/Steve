/**
 * @file i2c_slave.cpp
 *
 * @author FTDI
 * @date 2015-01-08
 *
 * Copyright © 2011 Future Technology Devices International Limited
 * Company Confidential
 *
 * Rivision History:
 * 1.0 - initial version
 */

//------------------------------------------------------------------------------
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

//------------------------------------------------------------------------------
// include FTDI libraries
//
#include "ftd2xx.h"
#include "LibFT4222.h"


std::vector< FT_DEVICE_LIST_INFO_NODE > g_FT4222DevList;

//------------------------------------------------------------------------------
inline std::string DeviceFlagToString(DWORD flags)
{
    std::string msg;
    msg += (flags & 0x1)? "DEVICE_OPEN" : "DEVICE_CLOSED";
    msg += ", ";
    msg += (flags & 0x2)? "High-speed USB" : "Full-speed USB";
    return msg;
}

void ListFtUsbDevices()
{
    FT_STATUS ftStatus = 0;

    DWORD numOfDevices = 0;
    ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

    for(DWORD iDev=0; iDev<numOfDevices; ++iDev)
    {
        FT_DEVICE_LIST_INFO_NODE devInfo;
        memset(&devInfo, 0, sizeof(devInfo));

        ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type,
                                        &devInfo.ID, &devInfo.LocId,
                                        devInfo.SerialNumber,
                                        devInfo.Description,
                                        &devInfo.ftHandle);

        if (FT_OK == ftStatus)
        {
            const std::string desc = devInfo.Description;
            if(desc == "FT4222" || desc == "FT4222 A")
            {
                g_FT4222DevList.push_back(devInfo);
            }
        }
    }
}

//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
int main(int argc, char const *argv[])
{
    ListFtUsbDevices();

    if(g_FT4222DevList.empty()) {
        printf("No FT4222 device is found!\n");
        return 0;
    }

    const FT_DEVICE_LIST_INFO_NODE& devInfo = g_FT4222DevList[0];

    printf("Open Device\n");
    printf("  Flags= 0x%x, (%s)\n", devInfo.Flags, DeviceFlagToString(devInfo.Flags).c_str());
    printf("  Type= 0x%x\n",        devInfo.Type);
    printf("  ID= 0x%x\n",          devInfo.ID);
    printf("  LocId= 0x%x\n",       devInfo.LocId);
    printf("  SerialNumber= %s\n",  devInfo.SerialNumber);
    printf("  Description= %s\n",   devInfo.Description);
    printf("  ftHandle= 0x%x\n",    devInfo.ftHandle);


    FT_HANDLE ftHandle = NULL;

    FT_STATUS ftStatus;
    ftStatus = FT_OpenEx((PVOID)g_FT4222DevList[0].LocId, FT_OPEN_BY_LOCATION, &ftHandle);
    if (FT_OK != ftStatus)
    {
        printf("Open a FT4222 device failed!\n");
        return 0;
    }


    printf("\n\n");
    printf("Init FT4222 as I2C slave\n");
    ftStatus = FT4222_I2CSlave_Init(ftHandle);
    if (FT_OK != ftStatus)
    {
        printf("Init FT4222 as I2C slave device failed!\n");
        return 0;
    }

    // set i2c slave address
    ftStatus = FT4222_I2CSlave_SetAddress(ftHandle, 0x40);
    if (FT_OK != ftStatus)
    {
        printf("Init FT4222 as I2C slave device failed!\n");
        return 0;
    }
    

    // TODO:
    //    Start to work as I2C slave, and read/write data
    //    1. write i2c slave , it will cache the data and wait for i2c master access
    //    1. read i2c slave , it will get data from i2c master . If master didn't send data, 
    //    FT4222_I2CSlave_Write
    //    FT4222_I2CSlave_Read


    uint8 sent_data[] = {0x1A, 0x2B, 0x3C, 0x4D};
    uint16 sizeTransferred = 0;
    uint16 rxSize;

    printf("I2C slave write data... \n");
    
    ftStatus = FT4222_I2CSlave_Write(ftHandle, sent_data, sizeof(sent_data), &sizeTransferred);
    if (FT_OK != ftStatus)
    {
        printf("I2C slave write error\n");
    }

    // Check how much data needs to receive 
    // you can set FT_EVENT_RXCHAR evnt with FT_SetEventNotification to improve the rx performance
    if(FT4222_I2CSlave_GetRxStatus(ftHandle, &rxSize) == FT_OK)
    {
        if(rxSize>0)
        {
            uint8 *pRead_data = (uint8 *)malloc(rxSize) ;          
            printf("I2C slave read data... \n");

            if(FT4222_I2CSlave_Read(ftHandle,pRead_data, rxSize, &sizeTransferred)!= FT_OK)
            {
                printf("I2C slave read error\n");
            }
            free(pRead_data);
        }        
    }
    else
    {
        printf("I2C slave get status error\n");
    }

    printf("UnInitialize FT4222\n");
    FT4222_UnInitialize(ftHandle);

    printf("Close FT device\n");
    FT_Close(ftHandle);
    return 0;
}
