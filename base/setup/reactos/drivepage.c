/*
 *  ReactOS applications
 *  Copyright (C) 2004-2008 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 * COPYRIGHT:   See COPYING in the top level directory
 * PROJECT:     ReactOS GUI first stage setup application
 * FILE:        base/setup/reactos/drivepage.c
 * PROGRAMMERS: Matthias Kupfer
 *              Dmitry Chapyshev (dmitry@reactos.org)
 */

#include "reactos.h"
#include <shlwapi.h>

#include <math.h> // For pow()

// #include <ntdddisk.h>
#include <ntddstor.h>
#include <ntddscsi.h>

#include "resource.h"

/* GLOBALS ******************************************************************/

#define IDS_LIST_COLUMN_FIRST IDS_PARTITION_NAME
#define IDS_LIST_COLUMN_LAST  IDS_PARTITION_STATUS

#define MAX_LIST_COLUMNS (IDS_LIST_COLUMN_LAST - IDS_LIST_COLUMN_FIRST + 1)
static const UINT column_ids[MAX_LIST_COLUMNS] = {IDS_LIST_COLUMN_FIRST, IDS_LIST_COLUMN_FIRST + 1, IDS_LIST_COLUMN_FIRST + 2, IDS_LIST_COLUMN_FIRST + 3};
static const INT  column_widths[MAX_LIST_COLUMNS] = {200, 90, 60, 60};
static const INT  column_alignment[MAX_LIST_COLUMNS] = {LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_RIGHT};

/* FUNCTIONS ****************************************************************/

static INT_PTR CALLBACK
MoreOptDlgProc(HWND hwndDlg,
               UINT uMsg,
               WPARAM wParam,
               LPARAM lParam)
{
    PSETUPDATA pSetupData;

    /* Retrieve pointer to the global setup data */
    pSetupData = (PSETUPDATA)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            /* Save pointer to the global setup data */
            pSetupData = (PSETUPDATA)lParam;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)pSetupData);

            CheckDlgButton(hwndDlg, IDC_INSTFREELDR, BST_CHECKED);
            SetDlgItemTextW(hwndDlg, IDC_PATH,
                            pSetupData->USetupData.InstallationDirectory);
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                {
                    GetDlgItemTextW(hwndDlg, IDC_PATH,
                                    pSetupData->USetupData.InstallationDirectory,
                                    ARRAYSIZE(pSetupData->USetupData.InstallationDirectory));
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }

    return FALSE;
}


#define PARTITION_SIZE_INPUT_FIELD_LENGTH 9
/* Restriction for MaxSize */
#define PARTITION_MAXSIZE (pow(10, (PARTITION_SIZE_INPUT_FIELD_LENGTH - 1)) - 1)

typedef struct _PARTCREATE_CTX
{
    // PSETUPDATA pSetupData;
    PPARTLIST PartitionList;
    PPARTENTRY PartEntry;
    ULONG MaxSize;
} PARTCREATE_CTX, *PPARTCREATE_CTX;

static INT_PTR
CALLBACK
PartitionDlgProc(
    _In_ HWND hDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam)
{
    PPARTCREATE_CTX PartCreateCtx;

    /* Retrieve dialog context pointer */
    PartCreateCtx = (PPARTCREATE_CTX)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            PPARTENTRY PartEntry;
            PDISKENTRY DiskEntry;
            ULONG Index = 0;
            PCWSTR FileSystemName;
            INT nSel;
            PCWSTR DefaultFs;
            ULONG MaxSize;

            /* Save dialog context pointer */
            PartCreateCtx = (PPARTCREATE_CTX)lParam;
            SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)PartCreateCtx);

            /* Retrieve the selected partition */
            PartEntry = PartCreateCtx->PartEntry;
            DiskEntry = PartEntry->DiskEntry;

            /* List the well-known filesystems */
            while (GetRegisteredFileSystems(Index++, &FileSystemName))
            {
                // ComboBox_InsertString()
                SendDlgItemMessageW(hDlg, IDC_FSTYPE, CB_INSERTSTRING, -1, (LPARAM)FileSystemName);
            }

#if 0
            // FIXME: Use "DefaultFs"?
            nSel = SendDlgItemMessageW(hDlg, IDC_FSTYPE, CB_FINDSTRINGEXACT, 0, (LPARAM)PartEntry->FileSystem);
#else
            /* By default select the "FAT" file system */
            DefaultFs = L"FAT";
            nSel = SendDlgItemMessageW(hDlg, IDC_FSTYPE, CB_FINDSTRINGEXACT, 0, (LPARAM)DefaultFs);
#endif
            if (nSel == CB_ERR)
                nSel = 0;
            SendDlgItemMessageW(hDlg, IDC_FSTYPE, CB_SETCURSEL, (WPARAM)nSel, 0);

            /* If the partition is originally formatted,
             * add the 'Keep existing filesystem' entry. */
            if (!(PartEntry->New || PartEntry->FormatState == Unformatted))
            {
                // ComboBox_InsertString()
                SendDlgItemMessageW(hDlg, IDC_FSTYPE, CB_INSERTSTRING, -1, (LPARAM)L"Existing filesystem");
            }


            MaxSize = (PartEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector) / MB;  /* in MBytes (rounded) */
            if (MaxSize > PARTITION_MAXSIZE)
                MaxSize = PARTITION_MAXSIZE;
            PartCreateCtx->MaxSize = MaxSize;

            SendDlgItemMessageW(hDlg, IDC_UPDOWN_PARTSIZE, UDM_SETRANGE32, (WPARAM)1, (LPARAM)MaxSize);
            SendDlgItemMessageW(hDlg, IDC_UPDOWN_PARTSIZE, UDM_SETPOS32, 0, (LPARAM)MaxSize);
            // SetDlgItemInt(hDlg, IDC_EDIT_PARTSIZE, MaxSize, FALSE);

            // TODO: If space is logical, or we are not MBR, disable and hide IDC_CHECK_MBREXTPART
            CheckDlgButton(hDlg, IDC_CHECK_MBREXTPART, BST_UNCHECKED);
            break;
        }

        case WM_NOTIFY:
        {
#if 0
            LPPSHNOTIFY lppsn = (LPPSHNOTIFY)lParam;

            if ((lppsn->hdr.code == UDN_DELTAPOS) &&
                (lppsn->hdr.idFrom == IDC_UPDOWN_PARTSIZE))
            {
                LPNMUPDOWN lpnmud = (LPNMUPDOWN)lParam;
                // DWORD wwidth;

                /****/lpnmud = lpnmud;/****/
                // wwidth = lpnmud->iPos + lpnmud->iDelta;

                // /* Be sure that the (new) screen buffer sizes are in the correct range */
                // wwidth = min(max(wwidth , 1), 0xFFFF);

                // ConInfo->ScreenBufferSize.X = (SHORT)swidth;

                // PropSheet_Changed(GetParent(hDlg), hDlg);
            }
#endif
            break;
        }

        case WM_COMMAND:
        {
            if (HIWORD(wParam) != BN_CLICKED)
                break;

            switch (LOWORD(wParam))
            {
            case IDC_CHECK_MBREXTPART:
            {
                /* Check for MBR-extended (container) partition */
                // BST_UNCHECKED or BST_INDETERMINATE => FALSE
                if (IsDlgButtonChecked(hDlg, IDC_CHECK_MBREXTPART) == BST_CHECKED)
                {
                    /* It is, disable formatting options */
                    // EnableDlgItem(hDlg, IDC_EDIT_WINDOW_POS_LEFT, FALSE); // FIXME: disable the static label as well
                    EnableDlgItem(hDlg, IDC_FSTYPE, FALSE);
                    EnableDlgItem(hDlg, IDC_CHECK_QUICKFMT, FALSE);
                }
                else
                {
                    /* It is not, re-enable formatting options */
                    // EnableDlgItem(hDlg, IDC_EDIT_WINDOW_POS_LEFT, TRUE); // FIXME: enable the static label as well
                    EnableDlgItem(hDlg, IDC_FSTYPE, TRUE);
                    EnableDlgItem(hDlg, IDC_CHECK_QUICKFMT, TRUE);
                }
                break;
            }

            case IDC_CHECK_QUICKFMT:
                break;

            case IDOK:
            {
                PPARTENTRY PartEntry = PartCreateCtx->PartEntry;
                PDISKENTRY DiskEntry = PartEntry->DiskEntry;
                /*ULONGLONG*/ ULONG PartSize;
                ULONGLONG SectorCount;

                // PartSize = GetDlgItemInt(hDlg, IDC_EDIT_PARTSIZE, NULL, FALSE);
                PartSize = (ULONG)SendDlgItemMessageW(hDlg, IDC_UPDOWN_PARTSIZE, UDM_SETPOS32, 0, (LPARAM)NULL);
                PartSize = min(max(PartSize, 1), PartCreateCtx->MaxSize);

                /* Convert to bytes */
                if (PartSize == PartCreateCtx->MaxSize)
                {
                    /* Use all of the unpartitioned disk space */
                    SectorCount = PartEntry->SectorCount.QuadPart;
                }
                else
                {
                    /* Calculate the sector count from the size in MB */
                    SectorCount = PartSize * MB / DiskEntry->BytesPerSector;

                    /* But never get larger than the unpartitioned disk space */
                    if (SectorCount > PartEntry->SectorCount.QuadPart)
                        SectorCount = PartEntry->SectorCount.QuadPart;
                }

                if (IsDlgButtonChecked(hDlg, IDC_CHECK_MBREXTPART) != BST_CHECKED)
                {
                    CreatePartition(PartCreateCtx->PartitionList,
                                    PartEntry,
                                    SectorCount,
                                    FALSE);
                }
                else
                {
                    CreateExtendedPartition(PartCreateCtx->PartitionList,
                                            PartEntry,
                                            SectorCount);
                }

                // TODO: Consider doing something else if CreatePartition() fails?
                EndDialog(hDlg, IDOK);
                return TRUE;
            }

            case IDCANCEL:
            {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            }
        }
    }
    return FALSE;
}


BOOL
CreateTreeListColumns(
    IN HINSTANCE hInstance,
    IN HWND hWndTreeList,
    IN const UINT* pIDs,
    IN const INT* pColsWidth,
    IN const INT* pColsAlign,
    IN UINT nNumOfColumns)
{
    UINT i;
    TLCOLUMN tlC;
    WCHAR szText[50];

    /* Create the columns */
    tlC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    tlC.pszText = szText;

    /* Load the column labels from the resource file */
    for (i = 0; i < nNumOfColumns; i++)
    {
        tlC.iSubItem = i;
        tlC.cx = pColsWidth[i];
        tlC.fmt = pColsAlign[i];

        LoadStringW(hInstance, pIDs[i], szText, ARRAYSIZE(szText));

        if (TreeList_InsertColumn(hWndTreeList, i, &tlC) == -1)
            return FALSE;
    }

    return TRUE;
}

// unused
VOID
DisplayStuffUsingWin32Setup(HWND hwndDlg)
{
    HDEVINFO h;
    HWND hList;
    SP_DEVINFO_DATA DevInfoData;
    DWORD i;

    h = SetupDiGetClassDevs(&GUID_DEVCLASS_DISKDRIVE, NULL, NULL, DIGCF_PRESENT);
    if (h == INVALID_HANDLE_VALUE)
        return;

    hList = GetDlgItem(hwndDlg, IDC_PARTITION);
    DevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i=0; SetupDiEnumDeviceInfo(h, i, &DevInfoData); i++)
    {
        DWORD DataT;
        LPTSTR buffer = NULL;
        DWORD buffersize = 0;

        while (!SetupDiGetDeviceRegistryProperty(h,
                                                 &DevInfoData,
                                                 SPDRP_DEVICEDESC,
                                                 &DataT,
                                                 (PBYTE)buffer,
                                                 buffersize,
                                                 &buffersize))
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                if (buffer) LocalFree(buffer);
                buffer = LocalAlloc(LPTR, buffersize * 2);
            }
            else
            {
                return;
            }
        }
        if (buffer)
        {
            SendMessageW(hList, LB_ADDSTRING, (WPARAM)0, (LPARAM)buffer);
            LocalFree(buffer);
        }
    }
    SetupDiDestroyDeviceInfoList(h);
}


HTLITEM
TreeListAddItem(
    _In_ HWND hTreeList,
    _In_opt_ HTLITEM hParent,
    _In_opt_ HTLITEM hInsertAfter,
    _In_ LPCWSTR lpText,
    _In_ INT iImage,
    _In_ INT iSelectedImage,
    _In_ LPARAM lParam)
{
    TLINSERTSTRUCTW Insert;

    ZeroMemory(&Insert, sizeof(Insert));

    Insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    Insert.hParent = hParent;
    Insert.hInsertAfter = (hInsertAfter ? hInsertAfter : TVI_LAST);
    Insert.item.pszText = (LPWSTR)lpText;
    Insert.item.iImage = iImage;
    Insert.item.iSelectedImage = iSelectedImage;
    Insert.item.lParam = lParam;

    // Insert.item.mask |= TVIF_STATE;
    // Insert.item.stateMask = TVIS_OVERLAYMASK;
    // Insert.item.state = INDEXTOOVERLAYMASK(1);

    return TreeList_InsertItem(hTreeList, &Insert);
}

LPARAM
TreeListGetItemData(
    _In_ HWND hTreeList,
    _In_ HTLITEM hItem)
{
    TLITEMW tlItem;

    tlItem.mask = TVIF_PARAM;
    tlItem.hItem = hItem;

    TreeList_GetItem(hTreeList, &tlItem);

    return tlItem.lParam;
}

PPARTENTRY
GetSelectedPartition(
    _In_ HWND hTreeList,
    _Out_opt_ HTLITEM* phItem)
{
    HTLITEM hItem, hParentItem;
    PPARTENTRY PartEntry;

    hItem = TreeList_GetSelection(hTreeList);
    if (!hItem)
        return NULL;

    hParentItem = TreeList_GetParent(hTreeList, hItem);
    /* May or may not be a PPARTENTRY: this is a PPARTENTRY only when hParentItem != NULL */
    PartEntry = (PPARTENTRY)TreeListGetItemData(hTreeList, hItem);
    if (!hParentItem || !PartEntry)
        return NULL;

    if (phItem)
        *phItem = hItem;

    return PartEntry;
}


VOID
GetPartitionTypeString(
    IN PPARTENTRY PartEntry,
    OUT PSTR strBuffer,
    IN ULONG cchBuffer)
{
    if (PartEntry->PartitionType == PARTITION_ENTRY_UNUSED)
    {
        StringCchCopyA(strBuffer, cchBuffer,
                       "Unused" /* MUIGetString(STRING_FORMATUNUSED) */);
    }
    else if (IsContainerPartition(PartEntry->PartitionType))
    {
        StringCchCopyA(strBuffer, cchBuffer,
                       "Extended Partition" /* MUIGetString(STRING_EXTENDED_PARTITION) */);
    }
    else
    {
        UINT i;

        /* Do the table lookup */
        if (PartEntry->DiskEntry->DiskStyle == PARTITION_STYLE_MBR)
        {
            for (i = 0; i < ARRAYSIZE(MbrPartitionTypes); ++i)
            {
                if (PartEntry->PartitionType == MbrPartitionTypes[i].Type)
                {
                    StringCchCopyA(strBuffer, cchBuffer,
                                   MbrPartitionTypes[i].Description);
                    return;
                }
            }
        }
#if 0 // TODO: GPT support!
        else if (PartEntry->DiskEntry->DiskStyle == PARTITION_STYLE_GPT)
        {
            for (i = 0; i < ARRAYSIZE(GptPartitionTypes); ++i)
            {
                if (IsEqualPartitionType(PartEntry->PartitionType,
                                         GptPartitionTypes[i].Guid))
                {
                    StringCchCopyA(strBuffer, cchBuffer,
                                   GptPartitionTypes[i].Description);
                    return;
                }
            }
        }
#endif

        /* We are here because the partition type is unknown */
        if (cchBuffer > 0) *strBuffer = '\0';
    }

    if ((cchBuffer > 0) && (*strBuffer == '\0'))
    {
        StringCchPrintfA(strBuffer, cchBuffer,
                         // MUIGetString(STRING_PARTTYPE),
                         "Type 0x%02x",
                         PartEntry->PartitionType);
    }
}

static
HTLITEM
PrintPartitionData(
    IN HWND hWndList,
    IN PPARTLIST List,
    IN HTLITEM htiParent,
    IN PDISKENTRY DiskEntry,
    IN PPARTENTRY PartEntry)
{
    LARGE_INTEGER PartSize;
    HTLITEM htiPart;
    CHAR PartTypeString[32];
    PCHAR PartType = PartTypeString;
    WCHAR LineBuffer[128];

    /* Volume name */
    if (PartEntry->IsPartitioned == FALSE)
    {
        StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                         // MUIGetString(STRING_UNPSPACE),
                         L"Unpartitioned space");
    }
    else
    {
        StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                         // MUIGetString(STRING_HDDINFOUNK5),
                         L"%s (%c%c)",
                         *PartEntry->VolumeLabel ? PartEntry->VolumeLabel : L"Partition",
                         (PartEntry->DriveLetter == 0) ? L'-' : PartEntry->DriveLetter,
                         (PartEntry->DriveLetter == 0) ? L'-' : L':');
    }

    htiPart = TreeListAddItem(hWndList, htiParent, NULL,
                              LineBuffer, 1, 1,
                              (LPARAM)PartEntry);

    /* Determine partition type */
    *LineBuffer = 0;
    if (PartEntry->IsPartitioned)
    {
        PartTypeString[0] = '\0';
        if (PartEntry->New == TRUE)
        {
            PartType = "New (Unformatted)"; // MUIGetString(STRING_UNFORMATTED);
        }
        else if (PartEntry->IsPartitioned == TRUE)
        {
            GetPartitionTypeString(PartEntry,
                                   PartTypeString,
                                   ARRAYSIZE(PartTypeString));
            PartType = PartTypeString;
        }

        StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                         L"%S",
                         PartType);
    }
    TreeList_SetItemText(hWndList, htiPart, 1, LineBuffer);

    /* Format the disk size in KBs, MBs, etc... */
    PartSize.QuadPart = PartEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
    if (StrFormatByteSizeW(PartSize.QuadPart, LineBuffer, ARRAYSIZE(LineBuffer)) == NULL)
    {
        /* We failed for whatever reason, do the hardcoded way */
        PWCHAR Unit;

#if 0
        if (PartSize.QuadPart >= 10 * GB) /* 10 GB */
        {
            PartSize.QuadPart = RoundingDivide(PartSize.QuadPart, GB);
            // Unit = MUIGetString(STRING_GB);
            Unit = L"GB";
        }
        else
#endif
        if (PartSize.QuadPart >= 10 * MB) /* 10 MB */
        {
            PartSize.QuadPart = RoundingDivide(PartSize.QuadPart, MB);
            // Unit = MUIGetString(STRING_MB);
            Unit = L"MB";
        }
        else
        {
            PartSize.QuadPart = RoundingDivide(PartSize.QuadPart, KB);
            // Unit = MUIGetString(STRING_KB);
            Unit = L"KB";
        }

        StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                         L"%6lu %s",
                         PartSize.u.LowPart,
                         Unit);
    }
    TreeList_SetItemText(hWndList, htiPart, 2, LineBuffer);

    /* Volume status */
    *LineBuffer = 0;
    if (PartEntry->IsPartitioned)
    {
        StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                         // MUIGetString(STRING_HDDINFOUNK5),
                         PartEntry->BootIndicator ? L"Active" : L"");
    }
    TreeList_SetItemText(hWndList, htiPart, 3, LineBuffer);

    return htiPart;
}

static
VOID
PrintDiskData(
    IN HWND hWndList,
    IN PPARTLIST List,
    IN PDISKENTRY DiskEntry)
{
    BOOL Success;
    HANDLE hDevice;
    PCHAR DiskName = NULL;
    ULONG Length = 0;
    PPARTENTRY PrimaryPartEntry, LogicalPartEntry;
    PLIST_ENTRY PrimaryEntry, LogicalEntry;
    ULARGE_INTEGER DiskSize;
    HTLITEM htiDisk, htiPart;
    WCHAR LineBuffer[128];
    UCHAR outBuf[512];

    StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                     // L"\\Device\\Harddisk%lu\\Partition%lu",
                     L"\\\\.\\PhysicalDrive%lu",
                     DiskEntry->DiskNumber);

    hDevice = CreateFileW(
                LineBuffer,                         // device interface name
                GENERIC_READ /*| GENERIC_WRITE*/,   // dwDesiredAccess
                FILE_SHARE_READ | FILE_SHARE_WRITE, // dwShareMode
                NULL,                               // lpSecurityAttributes
                OPEN_EXISTING,                      // dwCreationDistribution
                0,                                  // dwFlagsAndAttributes
                NULL                                // hTemplateFile
                );
    if (hDevice != INVALID_HANDLE_VALUE)
    {
        STORAGE_PROPERTY_QUERY Query;

        Query.PropertyId = StorageDeviceProperty;
        Query.QueryType  = PropertyStandardQuery;

        Success = DeviceIoControl(hDevice,
                                  IOCTL_STORAGE_QUERY_PROPERTY,
                                  &Query,
                                  sizeof(Query),
                                  &outBuf,
                                  sizeof(outBuf),
                                  &Length,
                                  NULL);
        if (Success)
        {
            PSTORAGE_DEVICE_DESCRIPTOR devDesc = (PSTORAGE_DEVICE_DESCRIPTOR)outBuf;
            if (devDesc->ProductIdOffset)
            {
                DiskName = (PCHAR)&outBuf[devDesc->ProductIdOffset];
                Length -= devDesc->ProductIdOffset;
                DiskName[min(Length, strlen(DiskName))] = 0;
                // ( i = devDesc->ProductIdOffset; p[i] != 0 && i < Length; i++ )
            }
        }

        CloseHandle(hDevice);
    }

    if (DiskName && *DiskName)
    {
        if (DiskEntry->DriverName.Length > 0)
        {
            StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                             // MUIGetString(STRING_HDDINFO_1),
                             L"Harddisk %lu (%S) (Port=%hu, Bus=%hu, Id=%hu) on %wZ",
                             DiskEntry->DiskNumber,
                             DiskName,
                             DiskEntry->Port,
                             DiskEntry->Bus,
                             DiskEntry->Id,
                             &DiskEntry->DriverName);
        }
        else
        {
            StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                             // MUIGetString(STRING_HDDINFO_2),
                             L"Harddisk %lu (%S) (Port=%hu, Bus=%hu, Id=%hu)",
                             DiskEntry->DiskNumber,
                             DiskName,
                             DiskEntry->Port,
                             DiskEntry->Bus,
                             DiskEntry->Id);
        }
    }
    else
    {
        if (DiskEntry->DriverName.Length > 0)
        {
            StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                             // MUIGetString(STRING_HDDINFO_1),
                             L"Harddisk %lu (Port=%hu, Bus=%hu, Id=%hu) on %wZ",
                             DiskEntry->DiskNumber,
                             DiskEntry->Port,
                             DiskEntry->Bus,
                             DiskEntry->Id,
                             &DiskEntry->DriverName);
        }
        else
        {
            StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                             // MUIGetString(STRING_HDDINFO_2),
                             L"Harddisk %lu (Port=%hu, Bus=%hu, Id=%hu)",
                             DiskEntry->DiskNumber,
                             DiskEntry->Port,
                             DiskEntry->Bus,
                             DiskEntry->Id);
        }
    }

    htiDisk = TreeListAddItem(hWndList, NULL, NULL,
                              LineBuffer, 0, 0,
                              (LPARAM)DiskEntry);

    /* Disk type: MBR, GPT or RAW (Uninitialized) */
    TreeList_SetItemText(hWndList, htiDisk, 1,
                         DiskEntry->DiskStyle == PARTITION_STYLE_MBR ? L"MBR" :
                         DiskEntry->DiskStyle == PARTITION_STYLE_GPT ? L"GPT" :
                                                                       L"RAW");

    /* Format the disk size in KBs, MBs, etc... */
    DiskSize.QuadPart = DiskEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
    if (StrFormatByteSizeW(DiskSize.QuadPart, LineBuffer, ARRAYSIZE(LineBuffer)) == NULL)
    {
        /* We failed for whatever reason, do the hardcoded way */
        PWCHAR Unit;

        if (DiskSize.QuadPart >= 10 * GB) /* 10 GB */
        {
            DiskSize.QuadPart = RoundingDivide(DiskSize.QuadPart, GB);
            // Unit = MUIGetString(STRING_GB);
            Unit = L"GB";
        }
        else
        {
            DiskSize.QuadPart = RoundingDivide(DiskSize.QuadPart, MB);
            if (DiskSize.QuadPart == 0)
                DiskSize.QuadPart = 1;
            // Unit = MUIGetString(STRING_MB);
            Unit = L"MB";
        }

        StringCchPrintfW(LineBuffer, ARRAYSIZE(LineBuffer),
                         L"%6lu %s",
                         DiskSize.u.LowPart,
                         Unit);
    }
    TreeList_SetItemText(hWndList, htiDisk, 2, LineBuffer);


    /* Print partition lines */
    for (PrimaryEntry = DiskEntry->PrimaryPartListHead.Flink;
         PrimaryEntry != &DiskEntry->PrimaryPartListHead;
         PrimaryEntry = PrimaryEntry->Flink)
    {
        PrimaryPartEntry = CONTAINING_RECORD(PrimaryEntry, PARTENTRY, ListEntry);

        htiPart = PrintPartitionData(hWndList, List, htiDisk,
                                     DiskEntry, PrimaryPartEntry);

        if (IsContainerPartition(PrimaryPartEntry->PartitionType))
        {
            for (LogicalEntry = DiskEntry->LogicalPartListHead.Flink;
                 LogicalEntry != &DiskEntry->LogicalPartListHead;
                 LogicalEntry = LogicalEntry->Flink)
            {
                LogicalPartEntry = CONTAINING_RECORD(LogicalEntry, PARTENTRY, ListEntry);

                PrintPartitionData(hWndList, List, htiPart,
                                   DiskEntry, LogicalPartEntry);
            }

            /* Expand the extended partition node */
            TreeList_Expand(hWndList, htiPart, TVE_EXPAND);
        }
    }

    /* Expand the disk node */
    TreeList_Expand(hWndList, htiDisk, TVE_EXPAND);
}

static VOID
InitPartitionList(
    _In_ HINSTANCE hInstance,
    _In_ HWND hWndList)
{
    HIMAGELIST hSmall;

    TreeList_SetExtendedStyleEx(hWndList, TVS_EX_FULLROWMARK, TVS_EX_FULLROWMARK);
    // TreeList_SetExtendedStyleEx(hWndList, TVS_EX_FULLROWITEMS, TVS_EX_FULLROWITEMS);

    CreateTreeListColumns(hInstance,
                          hWndList,
                          column_ids,
                          column_widths,
                          column_alignment,
                          MAX_LIST_COLUMNS);

    /* Create the ImageList */
    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
                              GetSystemMetrics(SM_CYSMICON),
                              ILC_COLOR32 | ILC_MASK, // ILC_COLOR24
                              1, 1);

    /* Add event type icons to the ImageList */
    ImageList_AddIcon(hSmall, LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_DISKDRIVE)));
    ImageList_AddIcon(hSmall, LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_PARTITION)));

    /* Assign the ImageList to the List View */
    TreeList_SetImageList(hWndList, hSmall, TVSIL_NORMAL);
}

static VOID
DrawPartitionList(
    _In_ HWND hWndList,
    _In_ PPARTLIST List)
{
    PLIST_ENTRY Entry;
    PDISKENTRY DiskEntry;

    /* Clear the list first */
    TreeList_DeleteAllItems(hWndList);

    /* Insert all the detected disks and partitions */
    for (Entry = List->DiskListHead.Flink;
         Entry != &List->DiskListHead;
         Entry = Entry->Flink)
    {
        DiskEntry = CONTAINING_RECORD(Entry, DISKENTRY, ListEntry);

        /* Print disk entry */
        PrintDiskData(hWndList, List, DiskEntry);
    }
}

static VOID
CleanupPartitionList(
    _In_ HWND hWndList)
{
    HIMAGELIST hSmall;

    hSmall = TreeList_GetImageList(hWndList, TVSIL_NORMAL);
    TreeList_SetImageList(hWndList, NULL, TVSIL_NORMAL);
    ImageList_Destroy(hSmall);
}

INT_PTR
CALLBACK
DriveDlgProc(
    HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    PSETUPDATA pSetupData;
    HWND hList;

    /* Retrieve pointer to the global setup data */
    pSetupData = (PSETUPDATA)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            /* Save pointer to the global setup data */
            pSetupData = (PSETUPDATA)((LPPROPSHEETPAGE)lParam)->lParam;
            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)pSetupData);

            /*
             * Keep the "Next" button disabled. It will be enabled only
             * when the user selects a valid partition.
             */
            PropSheet_SetWizButtons(GetParent(hwndDlg), PSWIZB_BACK);

            hList = GetDlgItem(hwndDlg, IDC_PARTITION);

            InitPartitionList(pSetupData->hInstance, hList);

            // DisplayStuffUsingWin32Setup(hwndDlg);
            DrawPartitionList(hList, pSetupData->PartitionList);

            // TODO: Enable/Disable/Show/Hide buttons
            break;
        }

        case WM_DESTROY:
        {
            hList = GetDlgItem(hwndDlg, IDC_PARTITION);
            CleanupPartitionList(hList);
            return TRUE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_PARTMOREOPTS:
                {
                    DialogBoxParamW(pSetupData->hInstance,
                                    MAKEINTRESOURCEW(IDD_BOOTOPTIONS),
                                    hwndDlg,
                                    MoreOptDlgProc,
                                    (LPARAM)pSetupData);
                    break;
                }

                case IDC_INITDISK:
                {
                    // TODO: Implement disk partitioning initialization
                    break;
                }

                case IDC_PARTCREATE:
                {
                    INT_PTR ret;
                    PARTCREATE_CTX PartCreateCtx;

                    hList = GetDlgItem(hwndDlg, IDC_PARTITION);

                    // PartCreateCtx.pSetupData = pSetupData;
                    PartCreateCtx.PartitionList = pSetupData->PartitionList;
                    PartCreateCtx.PartEntry = GetSelectedPartition(hList, NULL);
                    PartCreateCtx.MaxSize = 0;

                    ret = DialogBoxParamW(pSetupData->hInstance,
                                          MAKEINTRESOURCEW(IDD_PARTITION),
                                          hwndDlg,
                                          PartitionDlgProc,
                                          (LPARAM)&PartCreateCtx);

                    /* If we created a partition... */
                    if (ret == IDOK)
                    {
                        TVFIND tvf;
                        HTLITEM index;

                        /* ... redraw the list */
                        DrawPartitionList(hList, pSetupData->PartitionList);

                        /* Give the focus on and select the created partition */
                        tvf.uFlags = TVIF_PARAM;
                        tvf.lParam = (LPARAM)PartCreateCtx.PartEntry;
                        index = TreeList_FindItem(hList, TVI_ROOT, &tvf);
                        if (index) // > 0
                        {
                            // TreeList_SetFocusItem(hList, 1, 1);
                            TreeList_SelectItem(hList, index);
                        }
                    }

                    break;
                }

                case IDC_PARTDELETE:
                {
                    PPARTENTRY PartEntry;
                    HTLITEM hItem;
                    LPCWSTR pszWarnMsg;

                    hList = GetDlgItem(hwndDlg, IDC_PARTITION);

                    PartEntry = GetSelectedPartition(hList, &hItem);
                    if (!PartEntry)
                    {
                        // If the button was clicked, a partition
                        // should have been selected first...
                        ASSERT(FALSE);
                        break;
                    }

                    // FIXME: Localize strings

                    if (!PartEntry->LogicalPartition &&
                        IsContainerPartition(PartEntry->PartitionType))
                    {
                        /* MBR-extended (container) partition: show different message */
                        pszWarnMsg = L"Are you sure you want to delete the selected extended partition and ALL the logical partitions it contains?";
                    }
                    else
                    {
                        pszWarnMsg = L"Are you sure you want to delete the selected partition?";
                        // L"Are you sure you want to delete partition\n%s\non disk\n%s\n?"
                    }

                    /* If the user really wants to delete the partition... */
                    if (MessageBoxW(GetParent(hwndDlg),
                                    pszWarnMsg,
                                    L"Delete partition?",
                                    MB_YESNO | MB_ICONWARNING) == IDYES)
                    {
                        /* ... make it so! */
                        if (DeletePartition(pSetupData->PartitionList,
                                            PartEntry,
                                            NULL /*&PartEntry*/))
                        {
                            // FIXME: This works, but the problem is that
                            // we don't update the list with new unpartitioned
                            // space. So for the time being, just "redraw"
                            // the entire list by re-enumerating everything...
                        #if 0
                            TreeList_DeleteItem(hList, hItem);
                        #else
                            DrawPartitionList(hList, pSetupData->PartitionList);
                        #endif
                        }
                    }

                    break;
                }
            }
            break;
        }

        case WM_NOTIFY:
        {
            LPNMHDR lpnm = (LPNMHDR)lParam;

            // On Vista+ we can use TVN_ITEMCHANGED instead, with NMTVITEMCHANGE* pointer
            if (lpnm->idFrom == IDC_PARTITION && lpnm->code == TVN_SELCHANGED)
            {
                LPNMTREEVIEW pnmv = (LPNMTREEVIEW)lParam;

                // if (pnmv->uChanged & TVIF_STATE) /* The state has changed */
                if (pnmv->itemNew.mask & TVIF_STATE)
                {
                    /* The item has been (de)selected */
                    // if (pnmv->uNewState & TVIS_SELECTED)
                    if (pnmv->itemNew.state & TVIS_SELECTED)
                    {
                        HTLITEM hParentItem = TreeList_GetParent(lpnm->hwndFrom, pnmv->itemNew.hItem);
                        /* May or may not be a PPARTENTRY: this is a PPARTENTRY only when hParentItem != NULL */

                        if (!hParentItem)
                        {
                            /* Hard disk */
                            PDISKENTRY DiskEntry = (PDISKENTRY)pnmv->itemNew.lParam;
                            ASSERT(DiskEntry);

                            ShowWindow(GetDlgItem(hwndDlg, IDC_INITDISK), SW_SHOW);
                            ShowWindow(GetDlgItem(hwndDlg, IDC_PARTCREATE), SW_HIDE);
                            ShowWindow(GetDlgItem(hwndDlg, IDC_PARTDELETE), SW_HIDE);
                        #if 1 // FIXME: Init disk not implemented yet!
                            EnableDlgItem(hwndDlg, IDC_INITDISK,
                                          DiskEntry->DiskStyle == PARTITION_STYLE_RAW);
                        #else
                            EnableDlgItem(hwndDlg, IDC_INITDISK, FALSE);
                        #endif
                            EnableDlgItem(hwndDlg, IDC_PARTCREATE, FALSE);
                            EnableDlgItem(hwndDlg, IDC_PARTDELETE, FALSE);
                            goto DisableWizNext;
                        }
                        else
                        {
                            /* Partition or unpartitioned space */
                            PPARTENTRY PartEntry = (PPARTENTRY)pnmv->itemNew.lParam;
                            ASSERT(PartEntry);

                            ShowWindow(GetDlgItem(hwndDlg, IDC_INITDISK), SW_HIDE);
                            ShowWindow(GetDlgItem(hwndDlg, IDC_PARTCREATE), SW_SHOW);
                            ShowWindow(GetDlgItem(hwndDlg, IDC_PARTDELETE), SW_SHOW);
                            EnableDlgItem(hwndDlg, IDC_INITDISK, FALSE);
                            EnableDlgItem(hwndDlg, IDC_PARTCREATE, !PartEntry->IsPartitioned);
                            EnableDlgItem(hwndDlg, IDC_PARTDELETE,  PartEntry->IsPartitioned);

                            if (PartEntry->IsPartitioned &&
                                !IsContainerPartition(PartEntry->PartitionType) /* alternatively: PartEntry->PartitionNumber != 0 */ &&
                                // !PartEntry->New &&
                                (PartEntry->FormatState == Preformatted /* || PartEntry->FormatState == Formatted */))
                            {
                                PropSheet_SetWizButtons(GetParent(hwndDlg), PSWIZB_BACK | PSWIZB_NEXT);
                            }
                            else
                            {
                                goto DisableWizNext;
                            }
                        }
                    }
                    else
                    {
DisableWizNext:
                        /*
                         * Keep the "Next" button disabled. It will be enabled only
                         * when the user selects a valid partition.
                         */
                        PropSheet_SetWizButtons(GetParent(hwndDlg), PSWIZB_BACK);
                    }
                }

                break;
            }

            switch (lpnm->code)
            {
#if 0
                case PSN_SETACTIVE:
                {
                    /*
                     * Keep the "Next" button disabled. It will be enabled only
                     * when the user selects a valid partition.
                     */
                    PropSheet_SetWizButtons(GetParent(hwndDlg), PSWIZB_BACK);
                    break;
                }
#endif

                case PSN_QUERYINITIALFOCUS:
                {
                    /* Give the focus on and select the first item */
                    hList = GetDlgItem(hwndDlg, IDC_PARTITION);
                    // TreeList_SetFocusItem(hList, 1, 1);
                    TreeList_SelectItem(hList, 1);
                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LONG_PTR)hList);
                    return TRUE;
                }

                case PSN_QUERYCANCEL:
                {
                    if (MessageBoxW(GetParent(hwndDlg),
                                    pSetupData->szAbortMessage,
                                    pSetupData->szAbortTitle,
                                    MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        /* Go to the Terminate page */
                        PropSheet_SetCurSelByID(GetParent(hwndDlg), IDD_RESTARTPAGE);
                    }

                    /* Do not close the wizard too soon */
                    SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }

                case PSN_WIZNEXT: /* Set the selected data */
                {
                    NTSTATUS Status;
                    PPARTENTRY PartEntry;

                    PartEntry = GetSelectedPartition(GetDlgItem(hwndDlg, IDC_PARTITION), NULL);
                    if (!PartEntry)
                    {
                        /* Fail and don't continue the installation */
                        SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, -1);
                        return TRUE;
                    }

                    Status = InitDestinationPaths(&pSetupData->USetupData,
                                                  NULL, // pSetupData->USetupData.InstallationDirectory,
                                                  PartEntry);
                    if (!NT_SUCCESS(Status))
                    {
                        DisplayMessage(GetParent(hwndDlg), MB_ICONERROR, L"Error", L"InitDestinationPaths() failed with status 0x%08lx\n", Status);

                        /* Fail and don't continue the installation */
                        SetWindowLongPtrW(hwndDlg, DWLP_MSGRESULT, -1);
                        return TRUE;
                    }

                    break;
                }

                default:
                    break;
            }
        }
        break;

        default:
            break;
    }

    return FALSE;
}

/* EOF */
