#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "vmulticlient.h"

#if __GNUC__
    #define __in
    #define __in_ecount(x)
    typedef void* PVOID;
    typedef PVOID HDEVINFO;
    WINHIDSDI BOOL WINAPI HidD_SetOutputReport(HANDLE, PVOID, ULONG);
#endif

typedef struct _vmulti_client_t
{
    HANDLE hVMulti;
    BYTE vendorReport[VENDOR_REPORT_SIZE];
} vmulti_client_t;

//
// Function prototypes
//

HANDLE
SearchMatchingHwID (
    void
    );

HANDLE
OpenDeviceInterface (
    __in       HDEVINFO                    HardwareDeviceInfo,
    __in       PSP_DEVICE_INTERFACE_DATA   DeviceInterfaceData
    );

BOOLEAN
CheckIfOurDevice(
    HANDLE file
    );

BOOL
HidOutput(
    BOOL useSetOutputReport,
    HANDLE file,
    PCHAR buffer,
    ULONG bufferSize
    );

//
// Copied this structure from hidport.h
//

typedef struct _HID_DEVICE_ATTRIBUTES {

    ULONG           Size;
    //
    // sizeof (struct _HID_DEVICE_ATTRIBUTES)
    //
    //
    // Vendor ids of this hid device
    //
    USHORT          VendorID;
    USHORT          ProductID;
    USHORT          VersionNumber;
    USHORT          Reserved[11];

} HID_DEVICE_ATTRIBUTES, * PHID_DEVICE_ATTRIBUTES;

//
// Implementation
//

pvmulti_client vmulti_alloc(void)
{
    return (pvmulti_client)malloc(sizeof(vmulti_client_t));
}

void vmulti_free(pvmulti_client vmulti)
{
    free(vmulti);
}

BOOL vmulti_connect(pvmulti_client vmulti)
{
    vmulti->hVMulti = SearchMatchingHwID();
    return vmulti->hVMulti != INVALID_HANDLE_VALUE && vmulti->hVMulti != NULL;
}

void vmulti_disconnect(pvmulti_client vmulti)
{
    CloseHandle(vmulti->hVMulti);
}

BOOL vmulti_update_mouse(pvmulti_client vmulti, BYTE button, USHORT x, USHORT y, BYTE wheelPosition)
{
    VMultiReportHeader* pReport = NULL;
    VMultiMouseReport* pMouseReport = NULL;

    if (VENDOR_REPORT_SIZE <= sizeof(VMultiReportHeader) + sizeof(VMultiMouseReport))
    {
        return FALSE;
    }

    //
    // Set the report header
    //

    pReport = (VMultiReportHeader*)vmulti->vendorReport;
    pReport->ReportID = REPORTID_VENDOR_01;
    pReport->ReportLength = sizeof(VMultiMouseReport);

    //
    // Set the input report
    //

    pMouseReport = (VMultiMouseReport*)(vmulti->vendorReport + sizeof(VMultiReportHeader));
    pMouseReport->ReportID = REPORTID_MOUSE;
    pMouseReport->Button = button;
    pMouseReport->XValue = x;
    pMouseReport->YValue = y;
    pMouseReport->WheelPosition = wheelPosition;

    // Send the report
    return HidOutput(FALSE, vmulti->hVMulti, (PCHAR)vmulti->vendorReport, VENDOR_REPORT_SIZE);
}

BOOL vmulti_update_digi(pvmulti_client vmulti, BYTE status, USHORT x, USHORT y)
{
    VMultiReportHeader* pReport = NULL;
    VMultiDigiReport* pDigiReport = NULL;

    if (VENDOR_REPORT_SIZE <= sizeof(VMultiReportHeader) + sizeof(VMultiDigiReport))
    {
        return FALSE;
    }

    //
    // Set the report header
    //

    pReport = (VMultiReportHeader*)vmulti->vendorReport;
    pReport->ReportID = REPORTID_VENDOR_01;
    pReport->ReportLength = sizeof(VMultiDigiReport);

    //
    // Set the input report
    //

    pDigiReport = (VMultiDigiReport*)(vmulti->vendorReport + sizeof(VMultiReportHeader));
    pDigiReport->ReportID = REPORTID_DIGI;
    pDigiReport->Status = status;
    pDigiReport->XValue = x;
    pDigiReport->YValue = y;

    // Send the report
    return HidOutput(FALSE, vmulti->hVMulti, (PCHAR)vmulti->vendorReport, VENDOR_REPORT_SIZE);
}

BOOL vmulti_update_multitouch(pvmulti_client vmulti, PTOUCH pTouch, BYTE actualCount)
{
    VMultiReportHeader* pReport = NULL;
    VMultiMultiTouchReport* pMultiReport = NULL;
    int numberOfTouchesSent = 0;

    if (VENDOR_REPORT_SIZE <= sizeof(VMultiReportHeader) + sizeof(VMultiMultiTouchReport))
        return FALSE;

    //
    // Set the report header
    //

    pReport = (VMultiReportHeader*)vmulti->vendorReport;
    pReport->ReportID = REPORTID_VENDOR_01;
    pReport->ReportLength = sizeof(VMultiMultiTouchReport);

    while (numberOfTouchesSent < actualCount)
    {

        //
        // Set the input report
        //

        pMultiReport = (VMultiMultiTouchReport*)(vmulti->vendorReport + sizeof(VMultiReportHeader));
        pMultiReport->ReportID = REPORTID_MTOUCH;
        memcpy(pMultiReport->Touch, pTouch + numberOfTouchesSent, sizeof(TOUCH));
        if (numberOfTouchesSent <= actualCount - 2)
            memcpy(pMultiReport->Touch + 1, pTouch + numberOfTouchesSent + 1, sizeof(TOUCH));
		else
            memset(pMultiReport->Touch + 1, 0, sizeof(TOUCH));
        if (numberOfTouchesSent == 0)
            pMultiReport->ActualCount = actualCount;
        else
            pMultiReport->ActualCount = 0;

        // Send the report
        if (!HidOutput(TRUE, vmulti->hVMulti, (PCHAR)vmulti->vendorReport, VENDOR_REPORT_SIZE))
            return FALSE;

        numberOfTouchesSent += 2;
    }

    return TRUE;
}


BOOL vmulti_update_joystick(pvmulti_client vmulti, USHORT buttons, BYTE hat, BYTE x, BYTE y, BYTE rx, BYTE ry, BYTE throttle)
{
    VMultiReportHeader* pReport = NULL;
    VMultiJoystickReport* pJoystickReport = NULL;

    if (VENDOR_REPORT_SIZE <= sizeof(VMultiReportHeader) + sizeof(VMultiJoystickReport))
    {
        return FALSE;
    }

    //
    // Set the report header
    //

    pReport = (VMultiReportHeader*)vmulti->vendorReport;
    pReport->ReportID = REPORTID_VENDOR_01;
    pReport->ReportLength = sizeof(VMultiJoystickReport);

    //
    // Set the input report
    //

    pJoystickReport = (VMultiJoystickReport*)(vmulti->vendorReport + sizeof(VMultiReportHeader));
    pJoystickReport->ReportID = REPORTID_JOYSTICK;
    pJoystickReport->Buttons = buttons;
    pJoystickReport->Hat = hat;
    pJoystickReport->XValue = x;
    pJoystickReport->YValue = y;
    pJoystickReport->RXValue = rx;
    pJoystickReport->RYValue = ry;
    pJoystickReport->Throttle = throttle;

    // Send the report
    return HidOutput(FALSE, vmulti->hVMulti, (PCHAR)vmulti->vendorReport, VENDOR_REPORT_SIZE);
}

HANDLE
SearchMatchingHwID (
    void
    )
{
    HDEVINFO                  hardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA  deviceInterfaceData;
    SP_DEVINFO_DATA           devInfoData;
    GUID                      hidguid;
    int                       i;

    HidD_GetHidGuid(&hidguid);

    hardwareDeviceInfo =
            SetupDiGetClassDevs ((LPGUID)&hidguid,
                                            NULL,
                                            NULL, // Define no
                                            (DIGCF_PRESENT |
                                            DIGCF_INTERFACEDEVICE));

    if (INVALID_HANDLE_VALUE == hardwareDeviceInfo){
        printf("SetupDiGetClassDevs failed: %x\n", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    deviceInterfaceData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    //
    // Enumerate devices of this interface class
    //

    printf("\n....looking for our HID device (with UP=0xFF00 "
                "and Usage=0x01)\n");

    for(i=0; SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
                            0, // No care about specific PDOs
                            (LPGUID)&hidguid,
                            i, //
                            &deviceInterfaceData);
                            i++ ){

        //
        // Open the device interface and Check if it is our device
        // by matching the Usage page and Usage from Hid_Caps.
        // If this is our device then send the hid request.
        //

        HANDLE file = OpenDeviceInterface(hardwareDeviceInfo, &deviceInterfaceData);

        if (file != INVALID_HANDLE_VALUE)
        {
            SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);
            return file;
        }

        //
        //device was not found so loop around.
        //

    }

    printf("Failure: Could not find our HID device \n");

    SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);

    return INVALID_HANDLE_VALUE;
}

HANDLE
OpenDeviceInterface (
    __in       HDEVINFO                    hardwareDeviceInfo,
    __in       PSP_DEVICE_INTERFACE_DATA   deviceInterfaceData
    )
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;

    DWORD        predictedLength = 0;
    DWORD        requiredLength = 0;
    HANDLE       file = INVALID_HANDLE_VALUE;

    SetupDiGetDeviceInterfaceDetail(
                            hardwareDeviceInfo,
                            deviceInterfaceData,
                            NULL, // probing so no output buffer yet
                            0, // probing so output buffer length of zero
                            &requiredLength,
                            NULL
                            ); // not interested in the specific dev-node

    predictedLength = requiredLength;

    deviceInterfaceDetailData =
         (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc (predictedLength);

    if (!deviceInterfaceDetailData)
    {
        printf("Error: OpenDeviceInterface: malloc failed\n");
        goto cleanup;
    }

    deviceInterfaceDetailData->cbSize =
                    sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

    if (!SetupDiGetDeviceInterfaceDetail(
                            hardwareDeviceInfo,
                            deviceInterfaceData,
                            deviceInterfaceDetailData,
                            predictedLength,
                            &requiredLength,
                            NULL))
    {
        printf("Error: SetupDiGetInterfaceDeviceDetail failed\n");
        free (deviceInterfaceDetailData);
        goto cleanup;
    }

    file = CreateFile ( deviceInterfaceDetailData->DevicePath,
                            GENERIC_READ | GENERIC_WRITE,
                            0, // FILE_SHARE_READ | FILE_SHARE_READ |
                            NULL, // no SECURITY_ATTRIBUTES structure
                            OPEN_EXISTING, // No special create flags
                            0, // No special attributes
                            NULL); // No template file

    if (INVALID_HANDLE_VALUE == file) {
        printf("Error: CreateFile failed: %d\n", GetLastError());
        goto cleanup;
    }

    if (CheckIfOurDevice(file)){

        goto cleanup;

    }

    CloseHandle(file);

    file = INVALID_HANDLE_VALUE;

cleanup:

    free (deviceInterfaceDetailData);

    return file;

}


BOOLEAN
CheckIfOurDevice(
    HANDLE file)
{
    PHIDP_PREPARSED_DATA Ppd = NULL; // The opaque parser info describing this device
    HIDD_ATTRIBUTES                 Attributes; // The Attributes of this hid device.
    HIDP_CAPS                       Caps; // The Capabilities of this hid device.
    USAGE                           MyUsagePage = 0xff00;
    USAGE                           MyUsage = 0x0001;
    BOOLEAN                         result = FALSE;

    if (!HidD_GetPreparsedData (file, &Ppd))
    {
        printf("Error: HidD_GetPreparsedData failed \n");
        goto cleanup;
    }

    if (!HidD_GetAttributes(file, &Attributes))
    {
        printf("Error: HidD_GetAttributes failed \n");
        goto cleanup;
    }

    if (Attributes.VendorID == VMULTI_VID && Attributes.ProductID == VMULTI_PID)
    {
        if (!HidP_GetCaps (Ppd, &Caps))
        {
            printf("Error: HidP_GetCaps failed \n");
            goto cleanup;
        }

        if ((Caps.UsagePage == MyUsagePage) && (Caps.Usage == MyUsage))
        {
            printf("Success: Found my device.. \n");
            result = TRUE;
        }
    }

cleanup:

    if (Ppd != NULL)
    {
        HidD_FreePreparsedData (Ppd);
    }

    return result;
}

BOOL
HidOutput(
    BOOL useSetOutputReport,
    HANDLE file,
    PCHAR buffer,
    ULONG bufferSize
    )
{
    ULONG bytesWritten;
    if (useSetOutputReport)
    {
        //
        // Send Hid report thru HidD_SetOutputReport API
        //

        if (!HidD_SetOutputReport(file, buffer, bufferSize))
        {
            printf("failed HidD_SetOutputReport %d\n", GetLastError());
            return FALSE;
        }
    }
    else
    {
        if (!WriteFile(file, buffer, bufferSize, &bytesWritten, NULL))
        {
            printf("failed WriteFile %d\n", GetLastError());
            return FALSE;
        }
    }

    return TRUE;
}
