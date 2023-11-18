/*
 * PROJECT:         ReactOS DiskPart
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            base/system/diskpart/partlist.c
 * PURPOSE:         Manages all the partitions of the OS in an interactive way.
 * PROGRAMMERS:     Eric Kohl
 */

/* INCLUDES *******************************************************************/

#include "diskpart.h"
#include <ntddscsi.h>

#define NDEBUG
#include <debug.h>

/* GLOBALS ********************************************************************/

LIST_ENTRY VolumeListHead;

/* FUNCTIONS THAT CAME FROM PARTLIST TODO USE THEM ****************************/

/****
 ** VOLUME-specific
 ****/
// static
VOID
AssignDriveLetters(
    IN PPARTLIST List)
{
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    PLIST_ENTRY Entry1;
    PLIST_ENTRY Entry2;
    WCHAR Letter;

    Letter = L'C';

    /* Assign drive letters to primary partitions */
    for (Entry1 = List->DiskListHead.Flink;
         Entry1 != &List->DiskListHead;
         Entry1 = Entry1->Flink)
    {
        DiskEntry = CONTAINING_RECORD(Entry1, DISKENTRY, ListEntry);

        for (Entry2 = DiskEntry->PrimaryPartListHead.Flink;
             Entry2 != &DiskEntry->PrimaryPartListHead;
             Entry2 = Entry2->Flink)
        {
            PartEntry = CONTAINING_RECORD(Entry2, PARTENTRY, ListEntry);

            PartEntry->DriveLetter = 0;

            if (PartEntry->IsPartitioned &&
                !IsContainerPartition(PartEntry->PartitionType))
            {
                ASSERT(PartEntry->PartitionType != PARTITION_ENTRY_UNUSED);

                if (IsRecognizedPartition(PartEntry->PartitionType) ||
                    PartEntry->SectorCount.QuadPart != 0LL)
                {
                    if (Letter <= L'Z')
                    {
                        PartEntry->DriveLetter = Letter;
                        Letter++;
                    }
                }
            }
        }
    }

    /* Assign drive letters to logical drives */
    for (Entry1 = List->DiskListHead.Flink;
         Entry1 != &List->DiskListHead;
         Entry1 = Entry1->Flink)
    {
        DiskEntry = CONTAINING_RECORD(Entry1, DISKENTRY, ListEntry);

        for (Entry2 = DiskEntry->LogicalPartListHead.Flink;
             Entry2 != &DiskEntry->LogicalPartListHead;
             Entry2 = Entry2->Flink)
        {
            PartEntry = CONTAINING_RECORD(Entry2, PARTENTRY, ListEntry);

            PartEntry->DriveLetter = 0;

            if (PartEntry->IsPartitioned)
            {
                ASSERT(PartEntry->PartitionType != PARTITION_ENTRY_UNUSED);

                if (IsRecognizedPartition(PartEntry->PartitionType) ||
                    PartEntry->SectorCount.QuadPart != 0LL)
                {
                    if (Letter <= L'Z')
                    {
                        PartEntry->DriveLetter = Letter;
                        Letter++;
                    }
                }
            }
        }
    }
}



NTSTATUS
DetectFileSystem(
    _Inout_ PPARTENTRY PartEntry) // FIXME: Replace by volume entry
{
    NTSTATUS Status;
    PDISKENTRY DiskEntry;
    PVOLENTRY VolumeEntry;
    HANDLE PartitionHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatusBlock;
    UNICODE_STRING Name;
    WCHAR PathBuffer[MAX_PATH];
    UCHAR LabelBuffer[sizeof(FILE_FS_VOLUME_INFORMATION) + 256 * sizeof(WCHAR)];
    PFILE_FS_VOLUME_INFORMATION LabelInfo = (PFILE_FS_VOLUME_INFORMATION)LabelBuffer;

    DiskEntry = PartEntry->DiskEntry;
    VolumeEntry = PartEntry->Volume;

    /* Specify the partition as initially unformatted */
    VolumeEntry->FormatState = Unformatted;
    VolumeEntry->FileSystem[0] = L'\0';

    /* Initialize the partition volume label */
    RtlZeroMemory(VolumeEntry->VolumeLabel, sizeof(VolumeEntry->VolumeLabel));

#if 0
    if (!IsRecognizedPartition(PartEntry->PartitionType))
    {
        /* Unknown partition, hence unknown format (may or may not be actually formatted) */
        VolumeEntry->FormatState = UnknownFormat;
        return STATUS_SUCCESS;
    }
#endif

    /* Try to open the volume so as to mount it */
    RtlStringCchPrintfW(PathBuffer, ARRAYSIZE(PathBuffer),
                        L"\\Device\\Harddisk%lu\\Partition%lu",
                        DiskEntry->DiskNumber,
                        PartEntry->PartitionNumber);
    RtlInitUnicodeString(&Name, PathBuffer);

    InitializeObjectAttributes(&ObjectAttributes,
                               &Name,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    PartitionHandle = NULL;
    Status = NtOpenFile(&PartitionHandle,
                        FILE_READ_DATA | SYNCHRONIZE,
                        &ObjectAttributes,
                        &IoStatusBlock,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("NtOpenFile() failed, Status 0x%08lx\n", Status);
    }

    if (PartitionHandle)
    {
        ASSERT(NT_SUCCESS(Status));

        /* We don't have a FS, try to guess one */
        Status = InferFileSystem(NULL, PartitionHandle,
                                 VolumeEntry->FileSystem,
                                 sizeof(VolumeEntry->FileSystem));
        if (!NT_SUCCESS(Status))
            DPRINT1("InferFileSystem() failed, Status 0x%08lx\n", Status);
    }
    if (*VolumeEntry->FileSystem)
    {
        ASSERT(PartitionHandle);

        /*
         * Handle partition mounted with RawFS: it is
         * either unformatted or has an unknown format.
         */
        if (wcsicmp(VolumeEntry->FileSystem, L"RAW") == 0)
        {
            /*
             * True unformatted partitions on NT are created with their
             * partition type set to either one of the following values,
             * and are mounted with RawFS. This is done this way since we
             * are assured to have FAT support, which is the only FS that
             * uses these partition types. Therefore, having a partition
             * mounted with RawFS and with these partition types means that
             * the FAT FS was unable to mount it beforehand and thus the
             * partition is unformatted.
             * However, any partition mounted by RawFS that does NOT have
             * any of these partition types must be considered as having
             * an unknown format.
             */
            if (PartEntry->PartitionType == PARTITION_FAT_12 ||
                PartEntry->PartitionType == PARTITION_FAT_16 ||
                PartEntry->PartitionType == PARTITION_HUGE   ||
                PartEntry->PartitionType == PARTITION_XINT13 ||
                PartEntry->PartitionType == PARTITION_FAT32  ||
                PartEntry->PartitionType == PARTITION_FAT32_XINT13)
            {
                VolumeEntry->FormatState = Unformatted;
            }
            else
            {
                /* Close the partition before dismounting */
                NtClose(PartitionHandle);
                PartitionHandle = NULL;
                /*
                 * Dismount the partition since RawFS owns it, and set its
                 * format to unknown (may or may not be actually formatted).
                 */
                DismountVolume(PartEntry);
                VolumeEntry->FormatState = UnknownFormat;
                VolumeEntry->FileSystem[0] = L'\0';
            }
        }
        else
        {
            VolumeEntry->FormatState = Formatted;
        }
    }
    else
    {
        VolumeEntry->FormatState = UnknownFormat;
    }

    /* Retrieve the partition volume label */
    if (PartitionHandle)
    {
        Status = NtQueryVolumeInformationFile(PartitionHandle,
                                              &IoStatusBlock,
                                              &LabelBuffer,
                                              sizeof(LabelBuffer),
                                              FileFsVolumeInformation);
        if (NT_SUCCESS(Status))
        {
            /* Copy the (possibly truncated) volume label and NULL-terminate it */
            RtlStringCbCopyNW(VolumeEntry->VolumeLabel, sizeof(VolumeEntry->VolumeLabel),
                              LabelInfo->VolumeLabel, LabelInfo->VolumeLabelLength);
        }
        else
        {
            DPRINT1("NtQueryVolumeInformationFile() failed, Status 0x%08lx\n", Status);
        }
    }

    /* Close the partition */
    if (PartitionHandle)
        NtClose(PartitionHandle);

    return STATUS_SUCCESS;
}



/* FUNCTIONS ******************************************************************/

#if 0 // FIXME
//
// FIXME: Improve
//
static
VOID
GetVolumeExtents(
    _In_ HANDLE VolumeHandle,
    _In_ PVOLENTRY VolumeEntry)
{
    DWORD dwBytesReturned = 0, dwLength, i;
    PVOLUME_DISK_EXTENTS pExtents;
    BOOL bResult;
    DWORD dwError;

    dwLength = sizeof(VOLUME_DISK_EXTENTS);
    pExtents = RtlAllocateHeap(ProcessHeap, HEAP_ZERO_MEMORY, dwLength);
    if (pExtents == NULL)
        return;

    bResult = DeviceIoControl(VolumeHandle,
                              IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                              NULL,
                              0,
                              pExtents,
                              dwLength,
                              &dwBytesReturned,
                              NULL);
    if (!bResult)
    {
        dwError = GetLastError();

        if (dwError != ERROR_MORE_DATA)
        {
            RtlFreeHeap(ProcessHeap, 0, pExtents);
            return;
        }
        else
        {
            dwLength = sizeof(VOLUME_DISK_EXTENTS) + ((pExtents->NumberOfDiskExtents - 1) * sizeof(DISK_EXTENT));
            RtlFreeHeap(ProcessHeap, 0, pExtents);
            pExtents = RtlAllocateHeap(ProcessHeap, HEAP_ZERO_MEMORY, dwLength);
            if (pExtents == NULL)
            {
                return;
            }

            bResult = DeviceIoControl(VolumeHandle,
                                      IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                      NULL,
                                      0,
                                      pExtents,
                                      dwLength,
                                      &dwBytesReturned,
                                      NULL);
            if (!bResult)
            {
                RtlFreeHeap(ProcessHeap, 0, pExtents);
                return;
            }
        }
    }

    for (i = 0; i < pExtents->NumberOfDiskExtents; i++)
        VolumeEntry->Size.QuadPart += pExtents->Extents[i].ExtentLength.QuadPart;

    VolumeEntry->pExtents = pExtents;
}

//
// FIXME: Improve
//
static
VOID
GetVolumeType(
    _In_ HANDLE VolumeHandle,
    _In_ PVOLENTRY VolumeEntry)
{
    FILE_FS_DEVICE_INFORMATION DeviceInfo;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS Status;

    Status = NtQueryVolumeInformationFile(VolumeHandle,
                                          &IoStatusBlock,
                                          &DeviceInfo,
                                          sizeof(FILE_FS_DEVICE_INFORMATION),
                                          FileFsDeviceInformation);
    if (!NT_SUCCESS(Status))
        return;

    switch (DeviceInfo.DeviceType)
    {
        case FILE_DEVICE_CD_ROM:
        case FILE_DEVICE_CD_ROM_FILE_SYSTEM:
            VolumeEntry->VolumeType = VOLUME_TYPE_CDROM;
            break;

        case FILE_DEVICE_DISK:
        case FILE_DEVICE_DISK_FILE_SYSTEM:
            if (DeviceInfo.Characteristics & FILE_REMOVABLE_MEDIA)
                VolumeEntry->VolumeType = VOLUME_TYPE_REMOVABLE;
            else
                VolumeEntry->VolumeType = VOLUME_TYPE_PARTITION;
            break;

        default:
            VolumeEntry->VolumeType = VOLUME_TYPE_UNKNOWN;
            break;
    }
}

//
// FIXME: Improve
//
static
VOID
AddVolumeToList(
    ULONG ulVolumeNumber,
    PWSTR pszVolumeName)
{
    PVOLENTRY VolumeEntry;
    HANDLE VolumeHandle;

    DWORD dwError, dwLength;
    WCHAR szPathNames[MAX_PATH + 1];
    WCHAR szVolumeName[MAX_PATH + 1];
    WCHAR szFilesystem[MAX_PATH + 1];

    DWORD  CharCount            = 0;
    size_t Index                = 0;

    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING Name;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Status;

    DPRINT("AddVolumeToList(%S)\n", pszVolumeName);

    VolumeEntry = RtlAllocateHeap(ProcessHeap,
                                  HEAP_ZERO_MEMORY,
                                  sizeof(VOLENTRY));
    if (VolumeEntry == NULL)
        return;

    VolumeEntry->VolumeNumber = ulVolumeNumber;
    wcscpy(VolumeEntry->VolumeName, pszVolumeName);

    Index = wcslen(pszVolumeName) - 1;

    pszVolumeName[Index] = L'\0';

    CharCount = QueryDosDeviceW(&pszVolumeName[4], VolumeEntry->DeviceName, ARRAYSIZE(VolumeEntry->DeviceName)); 

    pszVolumeName[Index] = L'\\';

    if (CharCount == 0)
    {
        RtlFreeHeap(ProcessHeap, 0, VolumeEntry);
        return;
    }

    DPRINT("DeviceName: %S\n", VolumeEntry->DeviceName);

    RtlInitUnicodeString(&Name, VolumeEntry->DeviceName);

    InitializeObjectAttributes(&ObjectAttributes,
                               &Name,
                               0,
                               NULL,
                               NULL);

    Status = NtOpenFile(&VolumeHandle,
                        SYNCHRONIZE,
                        &ObjectAttributes,
                        &Iosb,
                        0,
                        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT);
    if (NT_SUCCESS(Status))
    {
        GetVolumeType(VolumeHandle, VolumeEntry);
        GetVolumeExtents(VolumeHandle, VolumeEntry);
        NtClose(VolumeHandle);
    }

    if (GetVolumeInformationW(pszVolumeName,
                              szVolumeName,
                              MAX_PATH + 1,
                              NULL, //  [out, optional] LPDWORD lpVolumeSerialNumber,
                              NULL, //  [out, optional] LPDWORD lpMaximumComponentLength,
                              NULL, //  [out, optional] LPDWORD lpFileSystemFlags,
                              szFilesystem,
                              MAX_PATH + 1))
    {
        VolumeEntry->pszLabel = RtlAllocateHeap(ProcessHeap,
                                                0,
                                                (wcslen(szVolumeName) + 1) * sizeof(WCHAR));
        if (VolumeEntry->pszLabel)
            wcscpy(VolumeEntry->pszLabel, szVolumeName);

        VolumeEntry->pszFilesystem = RtlAllocateHeap(ProcessHeap,
                                                     0,
                                                     (wcslen(szFilesystem) + 1) * sizeof(WCHAR));
        if (VolumeEntry->pszFilesystem)
            wcscpy(VolumeEntry->pszFilesystem, szFilesystem);
    }
    else
    {
        dwError = GetLastError();
        if (dwError == ERROR_UNRECOGNIZED_VOLUME)
        {
            VolumeEntry->pszFilesystem = RtlAllocateHeap(ProcessHeap,
                                                         0,
                                                         (3 + 1) * sizeof(WCHAR));
            if (VolumeEntry->pszFilesystem)
                wcscpy(VolumeEntry->pszFilesystem, L"RAW");
        }
    }

    if (GetVolumePathNamesForVolumeNameW(pszVolumeName,
                                         szPathNames,
                                         ARRAYSIZE(szPathNames),
                                         &dwLength))
    {
        VolumeEntry->DriveLetter = szPathNames[0];
    }

    InsertTailList(&VolumeListHead,
                   &VolumeEntry->ListEntry);
}
#endif

//
// FIXME: Improve; for the moment a temporary function is written below.
//
#if 0
NTSTATUS
CreateVolumeList(
    _Out_ PLIST_ENTRY VolumeListHead)
{
    BOOL Success;
    HANDLE hVolume = INVALID_HANDLE_VALUE;
    ULONG ulVolumeNumber = 0;
    WCHAR szVolumeName[MAX_PATH];

    InitializeListHead(VolumeListHead);

    hVolume = FindFirstVolumeW(szVolumeName, ARRAYSIZE(szVolumeName));
    if (hVolume == INVALID_HANDLE_VALUE)
    {
        return STATUS_UNSUCCESSFUL;
    }

    AddVolumeToList(ulVolumeNumber++, szVolumeName);

    for (;;)
    {
        Success = FindNextVolumeW(hVolume, szVolumeName, ARRAYSIZE(szVolumeName));
        if (!Success)
        {
            break;
        }

        AddVolumeToList(ulVolumeNumber++, szVolumeName);
    }

    FindVolumeClose(hVolume);

    return STATUS_SUCCESS;
}
#else
NTSTATUS
CreateVolumeList(
    _Out_ PLIST_ENTRY VolumeListHead)
{
}
#endif


// FIXME: Temporary function?
PVOLENTRY
TempAttachVolumeToPartition(
    _In_ PPARTENTRY PartEntry)
{
    PVOLENTRY VolumeEntry;

    VolumeEntry = RtlAllocateHeap(ProcessHeap,
                                  HEAP_ZERO_MEMORY,
                                  sizeof(VOLENTRY));
    if (!VolumeEntry)
        return NULL;

    /* Specify the partition as initially unformatted */
    VolumeEntry->FormatState = Unformatted;
    VolumeEntry->FileSystem[0] = L'\0';

    /* Initialize the partition volume label */
    RtlZeroMemory(VolumeEntry->VolumeLabel, sizeof(VolumeEntry->VolumeLabel));

    return VolumeEntry;
}



//
// FIXME: Improve, see also DestroyVolumeList
//
#if 0
VOID
RemoveVolume(
    _In_ PVOLENTRY VolumeEntry)
{
    RemoveEntryList(&VolumeEntry->ListEntry);

    if (VolumeEntry->pszLabel)
        RtlFreeHeap(ProcessHeap, 0, VolumeEntry->pszLabel);

    if (VolumeEntry->pszFilesystem)
        RtlFreeHeap(ProcessHeap, 0, VolumeEntry->pszFilesystem);

    if (VolumeEntry->pExtents)
        RtlFreeHeap(ProcessHeap, 0, VolumeEntry->pExtents);

    /* Release volume entry */
    RtlFreeHeap(ProcessHeap, 0, VolumeEntry);
}
#else
VOID
RemoveVolume(
    _In_ PVOLENTRY VolumeEntry)
{
    // VolumeEntry->FormatState = Unformatted;
    // VolumeEntry->FileSystem[0] = L'\0';
    // VolumeEntry->DriveLetter = 0;
    // RtlZeroMemory(VolumeEntry->VolumeLabel, sizeof(VolumeEntry->VolumeLabel));
    RtlFreeHeap(ProcessHeap, 0, VolumeEntry);
}
#endif

//
// TODO: Improve, see also RemoveVolume
//
VOID
DestroyVolumeList(
    _In_ PLIST_ENTRY VolumeListHead)
{
    PLIST_ENTRY Entry;
    PVOLENTRY VolumeEntry;

    /* Release volume info */
    while (!IsListEmpty(VolumeListHead))
    {
        Entry = RemoveHeadList(VolumeListHead);
        VolumeEntry = CONTAINING_RECORD(Entry, VOLENTRY, ListEntry);

        if (VolumeEntry->pszLabel)
            RtlFreeHeap(ProcessHeap, 0, VolumeEntry->pszLabel);

        if (VolumeEntry->pszFilesystem)
            RtlFreeHeap(ProcessHeap, 0, VolumeEntry->pszFilesystem);

        if (VolumeEntry->pExtents)
            RtlFreeHeap(ProcessHeap, 0, VolumeEntry->pExtents);

        /* Release volume entry */
        RtlFreeHeap(ProcessHeap, 0, VolumeEntry);
    }
}


NTSTATUS
DismountVolume(
    _In_ PPARTENTRY PartEntry) // FIXME: Replace by volume entry
{
    NTSTATUS Status;
    NTSTATUS LockStatus;
    UNICODE_STRING Name;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatusBlock;
    HANDLE PartitionHandle;
    WCHAR Buffer[MAX_PATH];

    /* Check whether the partition is valid and was mounted by the system */
    if (!PartEntry->IsPartitioned ||
        IsContainerPartition(PartEntry->PartitionType)   ||
        !IsRecognizedPartition(PartEntry->PartitionType) ||
        PartEntry->FormatState == UnknownFormat ||
        // NOTE: If FormatState == Unformatted but *FileSystem != 0 this means
        // it has been usually mounted with RawFS and thus needs to be dismounted.
        !*PartEntry->FileSystem ||
        PartEntry->PartitionNumber == 0)
    {
        /* The partition is not mounted, so just return success */
        return STATUS_SUCCESS;
    }

    ASSERT(PartEntry->PartitionType != PARTITION_ENTRY_UNUSED);

    /* Open the volume */
    RtlStringCchPrintfW(Buffer, ARRAYSIZE(Buffer),
                        L"\\Device\\Harddisk%lu\\Partition%lu",
                        PartEntry->DiskEntry->DiskNumber,
                        PartEntry->PartitionNumber);
    RtlInitUnicodeString(&Name, Buffer);

    InitializeObjectAttributes(&ObjectAttributes,
                               &Name,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = NtOpenFile(&PartitionHandle,
                        GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                        &ObjectAttributes,
                        &IoStatusBlock,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("ERROR: Cannot open volume %wZ for dismounting! (Status 0x%lx)\n", &Name, Status);
        return Status;
    }

    /* Lock the volume */
    LockStatus = NtFsControlFile(PartitionHandle,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &IoStatusBlock,
                                 FSCTL_LOCK_VOLUME,
                                 NULL,
                                 0,
                                 NULL,
                                 0);
    if (!NT_SUCCESS(LockStatus))
    {
        DPRINT1("WARNING: Failed to lock volume! Operations may fail! (Status 0x%lx)\n", LockStatus);
    }

    /* Dismount the volume */
    Status = NtFsControlFile(PartitionHandle,
                             NULL,
                             NULL,
                             NULL,
                             &IoStatusBlock,
                             FSCTL_DISMOUNT_VOLUME,
                             NULL,
                             0,
                             NULL,
                             0);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to unmount volume (Status 0x%lx)\n", Status);
    }

    /* Unlock the volume */
    LockStatus = NtFsControlFile(PartitionHandle,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &IoStatusBlock,
                                 FSCTL_UNLOCK_VOLUME,
                                 NULL,
                                 0,
                                 NULL,
                                 0);
    if (!NT_SUCCESS(LockStatus))
    {
        DPRINT1("Failed to unlock volume (Status 0x%lx)\n", LockStatus);
    }

    /* Close the volume */
    NtClose(PartitionHandle);

    return Status;
}


//
// TODO: Improve. For example, do this calculation lookup while
// listing the volumes during list creation, then here, just do
// a quick lookup (for example each VOLENTRY could contain a
// linked-list to the partition(s) on which it is based upon).
//
PVOLENTRY
GetVolumeFromPartition(
    _In_ PLIST_ENTRY VolumeListHead,
    _In_ PPARTENTRY PartEntry)
{
    PLIST_ENTRY Entry;
    PVOLENTRY VolumeEntry;
    ULONG i;

    if (!PartEntry || !PartEntry->DiskEntry)
        return NULL;

    for (Entry = VolumeListHead->Flink;
         Entry != VolumeListHead;
         Entry = Entry->Flink)
    {
        VolumeEntry = CONTAINING_RECORD(Entry, VOLENTRY, ListEntry);

        if (VolumeEntry->pExtents == NULL)
            return NULL;

        for (i = 0; i < VolumeEntry->pExtents->NumberOfDiskExtents; i++)
        {
            if (VolumeEntry->pExtents->Extents[i].DiskNumber == PartEntry->DiskEntry->DiskNumber)
            {
                if ((VolumeEntry->pExtents->Extents[i].StartingOffset.QuadPart == PartEntry->StartSector.QuadPart * PartEntry->DiskEntry->BytesPerSector) &&
                    (VolumeEntry->pExtents->Extents[i].ExtentLength.QuadPart == PartEntry->SectorCount.QuadPart * PartEntry->DiskEntry->BytesPerSector))
                {
                    return VolumeEntry;
                }
            }
        }
    }

    return NULL;
}






PPARTENTRY
GetNextUnformattedPartition(
    _In_ PPARTLIST List,
    _In_opt_ PPARTENTRY CurrentPart)
{
    /* Continue enumerating while we have something */
    while ((CurrentPart = GetNextDataPartition(List, CurrentPart)))
    {
        ASSERT(CurrentPart->IsPartitioned);
        if (CurrentPart->New /**/&& (CurrentPart->FormatState == Unformatted)/**/)
        {
            /* Found a candidate, return it */
            return CurrentPart;
        }
    }

    return NULL;
}

PPARTENTRY
GetNextUncheckedPartition(
    _In_ PPARTLIST List,
    _In_opt_ PPARTENTRY CurrentPart)
{
    /* Continue enumerating while we have something */
    while ((CurrentPart = GetNextDataPartition(List, CurrentPart)))
    {
        ASSERT(CurrentPart->IsPartitioned);
        if (CurrentPart->NeedsCheck)
        {
            /* Found a candidate, return it */
            ASSERT(CurrentPart->FormatState == Formatted);
            return CurrentPart;
        }
    }

    return NULL;
}

/* EOF */
