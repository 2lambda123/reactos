/*
 * PROJECT:         ReactOS Bootsector
 * LICENSE:         GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * FILE:            boot/freeldr/bootsect/fat32.S
 * PURPOSE:         Implementing boot sector for NTFS
 * COPYRIGHT:       Copyright 2020 Sylvain Deverre (deverre.sylv@gmail.com)
 */

/* INCLUDES ******************************************************************/

#include <asm.inc>
#include <freeldr/include/arch/pc/x86common.h>

.code16

//ORG HEX(7c00)

start:
    jmp short main
    nop
OEMName:
    .ASCII "NTFS    "   // NTFS signature
BytesPerSector:
    .word 512          
SectsPerCluster:
    .byte 8
ReservedSectors:
    .word 0             // Unused by NTFS
NumberOfFats:
    .byte 0             // Unused by NTFS
MaxRootEntries:
    .word 0             // Unused by NTFS
TotalSectors:
    .word 0             // Unused by NTFS
MediaDescriptor:
    .byte HEX(0f8)
SectorsPerFat:
    .word 0             // Unused by NTFS
SectorsPerTrack:
    .word 0             // Should contain disk geometry
NumberOfHeads:
    .word 0             // Should contain disk geometry
HiddenSectors:
    .long 0             // Filled by format program
TotalSectorsBig:
    .long 0             // Unused by NTFS
// NTFS inserted info
BootDrive:
    .byte HEX(80)
CurrentHead:
    .byte 0
BootSignature:
    .byte HEX(80)
Unused:
    .byte 0
VolumeSectorCount:
    .quad 0            // Must be patched by format program !
MftLocation:
    .quad 0            // Must be patched by format program !
MftMirrorLocation:
    .quad 0            // Must be patched by format program !
ClustersPerMftRecord:
    .long 0
ClustersPerIndexRecord:
    .long 0
VolumeSerialNumber:
    .quad 0
Checksum:
    .long 0

main:
    xor ax,ax               // Setup segment registers
    mov ds,ax               // Make DS correct
    mov es,ax               // Make ES correct
    mov ss,ax               // Make SS correct
    mov sp, HEX(7c00)
    mov bp, sp              // Setup a stack

    cmp byte ptr [BootDrive], HEX(0ff)    // If they have specified a boot drive then use it
    jne GetDriveParameters

    mov byte ptr [BootDrive], dl          // Save the boot drive

GetDriveParameters:
    mov  ah, 8
    mov  dl, byte ptr [BootDrive]               // Get boot drive in dl
    int  HEX(13)                                // Request drive parameters from the bios
    jnc  CalcDriveSize                          // If the call succeeded then calculate the drive size

    // If we get here then the call to the BIOS failed
    // so just set CHS equal to the maximum addressable
    // size
    mov  cx, HEX(0ffff)
    mov  dh, cl

CalcDriveSize:
    // Now that we have the drive geometry
    // lets calculate the drive size
    mov  bl, ch         // Put the low 8-bits of the cylinder count into BL
    mov  bh, cl         // Put the high 2-bits in BH
    shr  bh, 6          // Shift them into position, now BX contains the cylinder count
    and  cl, HEX(3f)    // Mask off cylinder bits from sector count
    // CL now contains sectors per track and DH contains head count
    movzx eax, dh       // Move the heads into EAX
    movzx ebx, bx       // Move the cylinders into EBX
    movzx ecx, cl       // Move the sectors per track into ECX
    inc   eax           // Make it one based because the bios returns it zero based
    inc   ebx           // Make the cylinder count one based also
    mul   ecx           // Multiply heads with the sectors per track, result in edx:eax
    mul   ebx           // Multiply the cylinders with (heads * sectors) [stored in edx:eax already]

    // We now have the total number of sectors as reported
    // by the bios in eax, so store it in our variable
    mov dword ptr ds:[BiosCHSDriveSize], eax

LoadExtraBootCode:
    // First we have to load our extra boot code at
    // next sector into memory at [0000:7e00h]
    mov  eax, HEX(1)
    xor  edx, edx
    mov  cx, 4
    xor  bx, bx
    mov  es, bx                                 // Read sector to [0000:7e00h]
    mov  bx, HEX(7e00)
    call ReadSectors
    jmp  StartSearch


// Reads logical sectors into [ES:BX]
// EDX:EAX has logical sector number to read
// CX has number of sectors to read
ReadSectors:
    push es
    add  eax, dword ptr [HiddenSectors]   // Add offset from the disk beginning
    test edx, edx
    jnz  ReadSectorsLBA
    cmp  eax, dword ptr ds:[BiosCHSDriveSize]   // Check if they are reading a sector outside CHS range
    jae  ReadSectorsLBA                         // Yes - go to the LBA routine
                                                // If at all possible we want to use LBA routines because
                                                // They are optimized to read more than 1 sector per read

    pushad                                      // Save logical sector number & sector count

CheckInt13hExtensions:                          // Now check if this computer supports extended reads
    mov  ah, HEX(41)                            // AH = 41h
    mov  bx, HEX(55aa)                          // BX = 55AAh
    mov  dl, byte ptr [BootDrive]               // DL = drive (80h-FFh)
    int  HEX(13)                                // IBM/MS INT 13 Extensions - INSTALLATION CHECK
    jc   ReadSectorsCHS                         // CF set on error (extensions not supported)
    cmp  bx, HEX(0aa55)                         // BX = AA55h if installed
    jne  ReadSectorsCHS
    test cl,1                                   // CX = API subset support bitmap
    jz   ReadSectorsCHS                         // Bit 0, extended disk access functions (AH=42h-44h,47h,48h) supported

    popad                                       // Restore sector count & logical sector number

ReadSectorsLBA:
    pushad                                      // Save logical sector number & sector count

    cmp  cx, 64                                 // Since the LBA calls only support 0x7F sectors at a time we will limit ourselves to 64
    jbe  ReadSectorsSetupDiskAddressPacket      // If we are reading less than 65 sectors then just do the read
    mov  cx, 64                                 // Otherwise read only 64 sectors on this loop iteration

ReadSectorsSetupDiskAddressPacket:
    mov word ptr ds:[LBASectorsRead],cx
    push edx
    push eax                                // Put 64-bit logical block address on stack
    push es                                 // Put transfer segment on stack
    push bx                                 // Put transfer offset on stack
    push cx                                 // Set transfer count
    push 16                                 // Set size of packet to 10h
    mov  si, sp                             // Setup disk address packet on stack

    mov  dl, byte ptr [BootDrive]           // Drive number
    mov  ah, HEX(42)                        // Int 13h, AH = 42h - Extended Read
    int  HEX(13)                            // Call BIOS
    jc   PrintDiskError                     // If the read failed then abort

    add  sp, 16                             // Remove disk address packet from stack

    popad                                   // Restore sector count & logical sector number

    push bx
    mov  ebx, dword ptr ds:[LBASectorsRead]
    add  eax, ebx                           // Increment sector to read
    adc  edx, 0
    shl  ebx, 5
    mov  dx, es
    add  dx, bx                             // Setup read buffer for next sector
    mov  es, dx
    pop  bx

    sub  cx, word ptr ds:[LBASectorsRead]
    jnz  ReadSectorsLBA                     // Read next sector

    pop es
    ret

LBASectorsRead:
    .long    0


// Reads logical sectors into [ES:BX]
// EAX has logical sector number to read
// CX has number of sectors to read
ReadSectorsCHS:
    popad                                        // Get logical sector number & sector count off stack

ReadSectorsCHSLoop:
    pushad
    xor  edx, edx
    movzx ecx, word ptr [SectorsPerTrack]
    div  ecx                                    // Divide logical by SectorsPerTrack
    inc  dl                                     // Sectors numbering starts at 1 not 0
    mov  cl, dl                                 // Sector in CL
    mov  edx, eax
    shr  edx, 16
    div  word ptr [NumberOfHeads]               // Divide logical by number of heads
    mov  dh, dl                                 // Head in DH
    mov  dl, byte ptr [BootDrive]               // Drive number in DL
    mov  ch, al                                 // Cylinder in CX
    ror  ah, 2                                  // Low 8 bits of cylinder in CH, high 2 bits
                                                //  in CL shifted to bits 6 & 7
    or   cl, ah                                 // Or with sector number
    mov  ax, HEX(0201)
    int  HEX(13)    // DISK - READ SECTORS INTO MEMORY
                     // AL = number of sectors to read, CH = track, CL = sector
                     // DH = head, DL = drive, ES:BX -> buffer to fill
                     // Return: CF set on error, AH = status (see AH=01h), AL = number of sectors read

    jc   PrintDiskError                         // If the read failed then abort

    popad

    inc  eax                                    // Increment Sector to Read

    mov  dx, es
    add  dx, 32                                 // Increment read buffer for next sector
    mov  es, dx

    loop ReadSectorsCHSLoop                     // Read next sector

    pop es
    ret

// Displays a disk error message
// And reboots
PrintDiskError:
    mov  si, offset msgDiskError        // Bad boot disk message
    call PutChars                       // Display it

    jmp  Reboot

// Displays a file system error message
// And reboots
PrintFileSystemError:
    mov  si, offset msgFileSystemError  // FreeLdr not found message
    call PutChars                       // Display it

Reboot:
    mov  si, offset msgAnyKey           // Press any key message
    call PutChars                       // Display it
    xor  ax, ax
    int  HEX(16)                        // Wait for a keypress
    int  HEX(19)                        // Reboot

PutChars:
    lodsb
    or   al, al
    jz   short Done
    mov  ah, HEX(0e)
    mov  bx, 7
    int  HEX(10)
    jmp  short PutChars
Done:
    ret

msgDiskError:
    .ascii "Disk error", CR, LF, NUL
msgFileSystemError:
    .ascii "File system error", CR, LF, NUL
msgAnyKey:
    .ascii "Press any key to restart", CR, LF, NUL

SectsPerMFT:
    .word 0
    
MFTStartSector:
    .quad 0

BiosCHSDriveSize:
    .long 0

.org 509 // Pad to 509 bytes
BootPartition:
    .byte 1

BootFlagSignature:
    .word HEX(0aa55)    // BootSector signature

// End of Bootsector
// Next sector starts as sector 2, since boot sector is 
// as a file under NTFS ($Boot, which has inode number 7)
// and takes the two first clusters of the volume (which means
// 16 sectors, checked on Windows and mkfs.ntfs from ntfsutils)

#define ATTRIBUTE_DATA HEX(80)
#define ATTRIBUTE_INDEX_ROOT HEX(90)
#define ATTRIBUTE_INDEX_ALLOCATION HEX(A0)
#define FILE_MAGIC HEX(454c4946)
#define INDX_MAGIC HEX(58444e49)
#define NTFS_FILE_ROOT 5
StartSearch:
    // Compute MFT start sector
    mov   eax, dword ptr [MftLocation]
    mov   cl, byte ptr [SectsPerCluster]
    movzx ecx, cx
    mul   ecx
    mov   dword ptr [MFTStartSector], eax

    // Compute size of MFT entry in sectors
    xor   ax, ax
    mov   al, byte ptr [ClustersPerMftRecord]
    test  al, al
    js    NegativeOffset
    mov   cx, word ptr [SectsPerCluster]
    mul   cx
    mov   word ptr [SectsPerMFT], cx
    jmp   SearchNext
    NegativeOffset:
    // ClustersPerMftRecord is negative, so we need to perform 1 << (-ClustersPerMftRecord)
    // to get the number of bytes needed for a MFT entry
    not   al
    inc   al
    mov   cl, al
    xor   ax, ax
    inc   ax
    shl   ax, cl

    // But here want to store sectors, so we need to divide by BytesPerSector
    xor   dx, dx
    mov   cx, word ptr [BytesPerSector]
    div   cx
    mov   word ptr [SectsPerMFT], ax

    SearchNext:
    mov   bp, sp
    sub   sp, HEX(10)

    // Read Root Directory MFT into [2000:0]
    mov   ax, HEX(2000)
    mov   es, ax
    xor   bx, bx
    xor   edx, edx
    mov   eax, NTFS_FILE_ROOT
    call  ReadInode

    // Finds freeldr.sys into index root's B-Tree
    // and return its MFT index
    xor   ax, ax
    call  ExploreIndexRoot

    // Read the MFT entry of freeldr.sys into [A00:0]
    push  eax
    mov   ax, HEX(A00)
    mov   es, ax
    pop   eax
    call  ReadInode

    xor   ax, ax
    mov   ebx, HEX(30)
    call  FindAttributeHdr
    mov   si, ax
    // Move to the attribute data
    add   si, word ptr es:[si + HEX(14)]

    // We don't support compressed, sparse or encrypted Freeldr
    test  dword ptr es:[si + HEX(38)], HEX(4a00)
    jnz   CompressedFreeldr

    // Compute size in clusters
    mov   eax, dword ptr es:[si + HEX(28)]
    mov   cl, byte ptr [SectsPerCluster]
    xor   edx, edx
    div   ecx
    mov   cx, word ptr [BytesPerSector]
    movzx ecx, cx
    xor   edx, edx
    div   ecx
    mov   edx, eax

    push  ax
    mov   si, offset msgLoading
    call  PutChars
    pop   ax

    xor   ax, ax
    mov   ebx, HEX(80)
    call  FindAttributeHdr
    mov   si, ax
    add   ax, word ptr es:[si + HEX(20)]
    xor   ecx, ecx
    xor   bx, bx
    push  FREELDR_BASE / 16
    pop   fs
    FreeLdrLoad:
        pushad
        call  ReadNonResidentAttribute
        popad
        mov   bx, fs
        add   bx, HEX(100)
        mov   fs, bx
        xor   bx, bx
        inc   ecx
        cmp   ecx, edx
        jbe   FreeLdrLoad

    mov    dl, byte ptr [BootDrive]
    mov    dh, byte ptr [BootPartition]
    ljmp16 0, FREELDR_BASE

// Error message if Freeldr is compressed, encrypted or sparse
CompressedFreeldr:
    mov   si, offset msgFreeldrCompressed
    call  PutChars
    jmp   Reboot

// Finds Freeldr.sys into the directory tree and returns the
// inode index
// INPUT:
//  - ES:[AX] address of the MFT record
// OUTPUT:
//  - EDX:EAX MFT number of the found file
ExploreIndexRoot:
    #define fileRecordBase 2
    #define fileRecord 4
    #define indexRootData 6
    #define allocationRunList 8

    push  bp
    mov   bp, sp
    push  bx
    push  si
    push  di
    sub   sp, HEX(10)
    mov   word ptr [bp - fileRecordBase], es
    mov   word ptr [bp - fileRecord], ax

    mov   ebx, ATTRIBUTE_INDEX_ROOT
    call  FindAttributeHdr
    test  ax, ax
    jz    PrintFileSystemError         // fail if no INDEX_ROOT

    mov   si, ax
    cmp   byte ptr es:[si + 8], HEX(0)
    jnz   PrintFileSystemError         // fail if attribute is non-resident

    add   si, word ptr es:[si + HEX(14)]
    cmp   dword ptr es:[si], HEX(30)
    jnz   PrintFileSystemError                  // fail if FILE_NAME attribute isn't indexed

    mov   word ptr [bp - indexRootData], si
    mov   ax, word ptr [bp - fileRecord]

    test  byte ptr es:[si + HEX(0c)], 1
    jz    ExploreNext                          // Skip index allocation lookup if we don't have children
    
    mov   ebx, ATTRIBUTE_INDEX_ALLOCATION
    call  FindAttributeHdr
    test  ax, ax
    jz    PrintFileSystemError                  // No INDEX_ALLOCATION found

    mov   si, ax
    cmp   byte ptr es:[si + 8], 1
    jnz   PrintFileSystemError                  // Fail if attribute is resident (shouldn't be)

    add   si, word ptr es:[si + HEX(20)]
    mov   word ptr [bp - allocationRunList], si // save run list

    ExploreNext:
    mov   si, word ptr [bp - indexRootData]
    lea   si, [si + 32]                         // get the first INDEX_ENTRY

    // Main search loop. We browse the B-Tree which contains directory entries
    // SI contains the current index entry.
    NodeCheck:
        test  word ptr es:[si + HEX(0C)], 2
        jnz   NodeCheckLastNode

        mov   cl, byte ptr es:[si + HEX(50)]
        movzx cx, cl
        lea   si, [si + HEX(52)]
        mov   di, offset FreeLdr
        call  CompareWideStrInsensitive
        lea   si, [si - HEX(52)]
        test  ax, ax
        jz    RootIndexFound
        test  word ptr es:[si + HEX(0C)], 1
        jz    ContinueSearch

        test  ax, HEX(f000)
        jnz   LookupChildNode                  // if result < 0 then explore child node

        ContinueSearch:
        add   si, word ptr es:[si + 8]
        jmp   NodeCheck

        RootIndexFound:
        mov   eax, dword ptr es:[si]           // We found root entry, return with its MFT number
        mov   dx, word ptr es:[si + 4]         // Return high part into edx.
                                               // We take only the first word, the high word contains the
                                               // sequence number
        movzx edx, dx
        jmp   ExitIndexTree
    
        NodeCheckLastNode:
        test  word ptr es:[si + HEX(0C)], 1
        jz    PrintFreeldrError

        LookupChildNode:
        // Take the right LCN at the end of the index record
        add   si, word ptr es:[si + 8]
        mov   ecx, dword ptr es:[si - 8]

        // Read the fixed up LCN
        mov   bx, word ptr [bp - allocationRunList]
        mov   ax, word ptr [bp - fileRecordBase]
        mov   es, ax
        mov   ax, HEX(9000)
        mov   fs, ax
        mov   ax, bx
        xor   bx, bx
        call  ReadINDX

        // Go again to the loop but with the child node list
        mov   ax, HEX(9000)
        mov   es, ax
        mov   si, 0
        add   si, es:[si + HEX(18)] // Go to the first node
        lea   si, [si + HEX(18)]
        jmp   NodeCheck

    ExitIndexTree:
    pop   di
    pop   si
    mov   sp, bp
    pop   bp
    ret

// 64-bit multiplication
// EDX:EAX operand
// ECX operator
Multiply64:
    push bp
    mov  bp, sp
    sub  sp, 12
    // Save the high part of the multiplication
    mov  dword ptr [bp - 4], edx

    // Multiply the low part and save the result
    mul  ecx
    mov  dword ptr[bp - 8], eax
    mov  dword ptr[bp - 12], edx

    // Multiply the high part and add the carry
    mov  eax, dword ptr [bp - 4]
    mul  ecx
    add  eax, dword ptr [bp - 12]

    // Format correctly the number
    mov  edx, eax
    mov  eax, dword ptr [bp - 8]
    mov  sp, bp
    pop  bp
    ret

// Compare case-insensitive strings
// [ES:SI] - the source file name
// [DS:DI] - the destination file name
// CX - compare length
CompareWideStrInsensitive:
    push bx
    push si
    push di
    movzx cx, cl
    CmpLoop:
        mov ax, word ptr es:[si]
        cmp ax, 'a'
        jl NoUpper
        cmp ax, 'z'
        jg NoUpper
        sub ax, HEX(20)
        NoUpper:
        mov bx, word ptr ds:[di]
        sub bx, ax
        test bx, bx
        jnz  CompareFail
        add  si, 2
        add  di, 2
        dec cx
        jnz CmpLoop
    CompareFail:
    mov ax, bx
    pop di
    pop si
    pop bx
    ret

// Reads a NTFS cluster
// INPUT:
//  - EDX:EAX     : cluster number to read
// OUTPUT:
//  - ES:BX  : address to read
ReadCluster:
    push  ecx
    mov   cl, byte ptr [SectsPerCluster]   // Convert clusters to sectors
    movzx ecx, cx
    call  Multiply64
    call  ReadSectors
    pop   ecx
    ret

// Reads a MFT entry
// INPUT:
//  - EDX:EAX      : MFT inode
// OUTPUT:
//  - ES:[BX] : address to read
ReadInode:
    push  ecx
    push  si
    push  di

    push  edx
    mov   cx, word ptr [SectsPerMFT]       // Get the correct number of sectors for the FILE entry
    movzx ecx, cx
    mul   ecx
    movzx eax, ax
    add   eax, dword ptr [MFTStartSector]  // Add it to the start of the MFT
    mov   cx, word ptr [SectsPerMFT]
    movzx ecx, cx
    pop   edx
    call  ReadSectors
    
    cmp   dword ptr es:[bx], FILE_MAGIC // Ensure we get a valid FILE record
    jnz   PrintFileSystemError

    call  ApplyFixups

    pop   di
    pop   si
    pop   ecx
    ret

#define UpdateSequenceOffset 4
#define UpdateSequenceLen 6
// Apply fixups to INDX and FILE records
// INPUT:
//  - ES:[BX] - pointer to the record to fixup
ApplyFixups:
    push  si
    push  di
    mov   si, bx
    add   si, word ptr es:[bx + UpdateSequenceOffset]
    xor   cx, cx
    inc   cx
    FixupLoop:
        cmp  cx, word ptr es:[bx + UpdateSequenceLen]
        jz  EndFixupLoop
        mov   si, bx
        add   si, word ptr es:[bx + UpdateSequenceOffset]
        mov   ax, word ptr es:[si]     // Get first fixup value

        mov   di, cx
        shl   di, 9
        add   di, bx
        sub   di, 2
        cmp   ax, word ptr es:[di]     // Check fixup value
        jnz   PrintFileSystemError     // Fixup is corrupted, so print error
        inc   cx
        add   si, cx
        mov   ax, word ptr es:[si]     // Apply fixup
        mov   word ptr es:[di], ax

        jmp   FixupLoop
    EndFixupLoop:

    pop di
    pop si
    ret

// Reads a non-resident attribute
// INPUT:
//  - ES:[AX] : Address of the data runs
//  - ECX     : LCN to read
// OUTPUT:
//  - FS:[BX] : Address to write to
ReadNonResidentAttribute:
    #define currentLCN 4
    #define offsetWrite 8
    #define startReadLCN HEX(0C)
    push  bp
    mov   bp, sp
    sub   sp, HEX(10)
    push  edx
    push  si
    mov   dword ptr [bp - currentLCN], 0          // Store the current LCN
    mov   word ptr [bp - offsetWrite], bx
    mov   dword ptr [bp - startReadLCN], ecx
    mov   si, ax
    xor   edx, edx
    RunLoop:
        mov   al, byte ptr es:[si]
        test  al, al
        jz    NotFound
        call  UnpackRun
        add   dword ptr [bp - currentLCN], eax
        cmp   dword ptr [bp - startReadLCN], ecx
        jb    FoundRun
        sub   dword ptr [bp - startReadLCN], ecx  // Decrement the cluster
        jmp   RunLoop
    FoundRun:
    mov   ebx, dword ptr [bp - currentLCN]
    mov   ecx, dword ptr [bp - startReadLCN]
    add   ebx, ecx

    push es
    mov  ax, fs
    mov  es, ax
    mov  eax, ebx
    mov  bx, word ptr [bp - offsetWrite]
    call ReadCluster
    pop  es
    jmp  RunSearchEnd
    NotFound:
    xor  ax, ax
    RunSearchEnd:
    pop  si
    pop  edx
    mov  sp, bp
    pop  bp
    ret

// Decodes a run in the runlist
// INPUT:
//  - ES:[SI] : address of the run
// OUTPUT:
//  - EAX : Unpacked LCN
//  - ECX : Unpacked run length (in sectors)
//  - SI  : Next run in the run list
UnpackRun:
    push bp
    mov  bp, sp
    sub  sp, HEX(10)
    push ebx

    // Unpack run header
    mov  bl, byte ptr es:[si]
    inc  si

    mov  bh, bl
    shr  bh, 4 
    and  bl, 7 
    mov  byte ptr [bp-2], bh     // BH contains the LCN length
    mov  byte ptr [bp-1], bl     // BL contains the number of cluster length
    
    mov  al, bl
    call UnpackLen
    mov  dword ptr [bp - 8], ebx

    mov  al, byte ptr [bp-2]
    call UnpackLen
    mov  cl, byte ptr es:[si-1]  // Fixup sign if last byte is > 255
    test cl, HEX(80)
    jz   NoSign
    not  eax
    add  ebx, eax

    NoSign:
    mov  eax, ebx
    mov  ecx, dword ptr [bp - 8]
    pop  ebx
    mov  sp, bp
    pop  bp
    ret


// Auxiliary function that unpacks n bytes in the memory
// INPUT:
//  - AL  : size to unpack (max 4 bytes)
// OUTPUT:
//  - EAX : the mask used to unpack (for negative number fixup)
//  - EBX : the unpacked number
//  - SI  : Next byte to read
UnpackLen:
    push  cx
    movzx ax, al

    // Read the whole DWORD and then compute a mask to remove
    // unneeded bytes to get correct size
    xor   ebx, ebx
    mov   ebx, dword ptr es:[si]
    add   si, ax

    cmp   al, 4
    jnz   UnpackLen_not4
    xor   eax, eax
    dec   eax
    jmp   UnpackLen_mask

UnpackLen_not4:
    mov   cl, al    // Compute mask (2^(8*len) - 1)
    shl   cl, 3
    xor   eax, eax
    inc   eax
    shl   eax, cl
    dec   eax

UnpackLen_mask:
    and   ebx, eax // Apply mask
    pop   cx
    ret

// Reads an INDX sector and applies fixups
// INPUT:
//  - ES:[AX] : Address of the data runs
//  - ECX     : LCN to read
// OUTPUT:
//  - FS:[BX] : Address to write to
ReadINDX:
    push  es
    push  bx
    call  ReadNonResidentAttribute
    test  ax, ax
    jz    PrintFileSystemError
    cmp   dword ptr fs:[0], INDX_MAGIC
    jnz   PrintFileSystemError  // jump if not valid
    pop   bx
    
    mov   ax, fs
    mov   es, ax
    call  ApplyFixups
    pop   es
    ret

// Finds an attribute header into the MFT
// INPUT:
//  - ES:[AX] : pointer to the MFT entry
//  - EBX     : type to find
// OUTPUT:
//  - ES:[AX] : Pointer to the attribute header in the MFT entry
FindAttributeHdr:
    push  cx
    push  si
    push  edx
    mov   si, ax
    mov   cx, word ptr es:[si+HEX(14)]  // Get offset attribute
    add   si, cx
    FindAttributeHdrLoop:
        mov   edx, dword ptr es:[si]    // Get attribute type 
        cmp   edx, ebx                
        jz    AttrFound
        cmp   edx, HEX(ffffffff)
        jz    AttrNotFound
        add   cx, word ptr es:[si + 4]  // Add size of the attribute
        add   si, word ptr es:[si + 4] 
        jmp   FindAttributeHdrLoop
    
    AttrNotFound:
    // Attribute not found, reset the offset
    xor   cx, cx
    AttrFound:
    mov   ax, cx
    pop   edx
    pop   si
    pop   cx
    ret

PrintFreeldrError:
    mov  si, offset msgFreeldr
    call PutChars
    jmp  Reboot

FreeLdr:
    .word 'F', 'R', 'E', 'E', 'L', 'D', 'R', '.', 'S', 'Y', 'S'
msgFreeldr:
    .ascii "FreeLdr not found, cannot boot", CR, LF, NUL
msgLoading:
    .ascii "Loading FreeLoader...", CR, LF, NUL
msgFreeldrCompressed:
    .ascii "freeldr.sys is a sparse, compressed or encrypted file, cannot boot", CR, LF, NUL
.endcode16

END
