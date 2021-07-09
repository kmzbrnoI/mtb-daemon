#include <QtGlobal>
#include "mtbusb-win-com-discover.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <devguid.h>    // for GUID_DEVCLASS_CDROM etc
#include <setupapi.h>
#include <cfgmgr32.h>   // for MAX_DEVICE_ID_LEN, CM_Get_Parent and CM_Get_Device_ID
#define INITGUID
#include <tchar.h>
#include <stdio.h>

#include <initguid.h>
#include <ntdef.h>
#include <Ntddser.h>

#define __in
#define __out

//#include "c:\WinDDK\7600.16385.1\inc\api\devpkey.h"

// include DEVPKEY_Device_BusReportedDeviceDesc from WinDDK\7600.16385.1\inc\api\devpropdef.h
#ifdef DEFINE_DEVPROPKEY
#undef DEFINE_DEVPROPKEY
#endif
#ifdef INITGUID
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY name
#endif // INITGUID

// include DEVPKEY_Device_BusReportedDeviceDesc from WinDDK\7600.16385.1\inc\api\devpkey.h
DEFINE_DEVPROPKEY(DEVPKEY_Device_BusReportedDeviceDesc,  0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 4);     // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_ContainerId,            0x8c7ed206, 0x3f8a, 0x4827, 0xb3, 0xab, 0xae, 0x9e, 0x1f, 0xae, 0xfc, 0x6c, 2);     // DEVPROP_TYPE_GUID
DEFINE_DEVPROPKEY(DEVPKEY_Device_FriendlyName,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_DeviceDisplay_Category,        0x78c34fc8, 0x104a, 0x4aca, 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57, 0x5a);  // DEVPROP_TYPE_STRING_LIST
DEFINE_DEVPROPKEY(DEVPKEY_Device_LocationInfo,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 15);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_Manufacturer,           0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 13);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_SecuritySDS,            0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 26);    // DEVPROP_TYPE_SECURITY_DESCRIPTOR_STRING

#define ARRAY_SIZE(arr)     (sizeof(arr)/sizeof(arr[0]))

typedef BOOL (WINAPI *FN_SetupDiGetDevicePropertyW)(
  __in       HDEVINFO DeviceInfoSet,
  __in       PSP_DEVINFO_DATA DeviceInfoData,
  __in       const DEVPROPKEY *PropertyKey,
  __out      DEVPROPTYPE *PropertyType,
  __out_opt  PBYTE PropertyBuffer,
  __in       DWORD PropertyBufferSize,
  __out_opt  PDWORD RequiredSize,
  __in       DWORD Flags
);

struct ComInfo {
    QString port;
    QString description;
    ComInfo() = default;
    ComInfo(const QString& port, const QString& description) : port(port), description(description) {}
};

// List all USB devices with some additional information
std::vector<ComInfo> _listDevices(CONST GUID *pClassGuid, LPCTSTR pszEnumerator) {
    std::vector<ComInfo> result;
    unsigned i;
    DWORD dwSize;
    DEVPROPTYPE ulPropertyType;
    CONFIGRET status;
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA DeviceInfoData;
    TCHAR szDeviceInstanceID [MAX_DEVICE_ID_LEN];
    WCHAR szBuffer[4096];
    FN_SetupDiGetDevicePropertyW fn_SetupDiGetDevicePropertyW = (FN_SetupDiGetDevicePropertyW)
        GetProcAddress (GetModuleHandle (TEXT("Setupapi.dll")), "SetupDiGetDevicePropertyW");

    // List all connected USB devices
    hDevInfo = SetupDiGetClassDevs(pClassGuid, pszEnumerator, nullptr,
                                   pClassGuid != nullptr ? DIGCF_PRESENT: DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return {};

    // Find the ones that are driverless
    for (i = 0; ; i++) {
        DeviceInfoData.cbSize = sizeof (DeviceInfoData);
        if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData))
            break;

        status = CM_Get_Device_ID(DeviceInfoData.DevInst, szDeviceInstanceID , MAX_PATH, 0);
        if (status != CR_SUCCESS)
            continue;

        // Retreive the device description as reported by the device itself
        // On Vista and earlier, we can use only SPDRP_DEVICEDESC
        // On Windows 7, the information we want ("Bus reported device description") is
        // accessed through DEVPKEY_Device_BusReportedDeviceDesc
        if (fn_SetupDiGetDevicePropertyW && fn_SetupDiGetDevicePropertyW (hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
                                                                          &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {
            ComInfo comInfo;
            if (!fn_SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
                                             &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
                continue;
            comInfo.description =  QString::fromWCharArray(szBuffer);
            if (!fn_SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_FriendlyName,
                                             &ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
                continue;
            QString friendlyName = QString::fromWCharArray(szBuffer);
            int start = friendlyName.indexOf('(')+1;
            int end = friendlyName.indexOf(')');
            if (start < 0 || end < 0)
                continue;
            comInfo.port = friendlyName.mid(start, end-start);
            result.push_back(comInfo);
        }
    }

    return result;
}

std::vector<QSerialPortInfo> winMtbUsbPorts() {
    std::vector<QSerialPortInfo> result;
    std::vector<ComInfo> ports = _listDevices(&GUID_SERENUM_BUS_ENUMERATOR, nullptr);
    for (const ComInfo& info : ports)
        if (info.description.startsWith("MTB-USB v4"))
            result.push_back(QSerialPortInfo(info.port));
    return result;
}

#endif
