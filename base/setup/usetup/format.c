/*
 *  ReactOS kernel
 *  Copyright (C) 2003 ReactOS Team
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
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS text-mode setup
 * FILE:            base/setup/usetup/format.c
 * PURPOSE:         Filesystem format support functions
 * PROGRAMMER:      Casper S. Hornstrup (chorns@users.sourceforge.net)
 */

/* INCLUDES *****************************************************************/

#include "usetup.h"

#define NDEBUG
#include <debug.h>

static PPROGRESSBAR FormatProgressBar = NULL;

/* FUNCTIONS ****************************************************************/

static
BOOLEAN
NTAPI
FormatCallback(
    _In_ CALLBACKCOMMAND Command,
    _In_ ULONG Modifier,
    _In_ PVOID Argument)
{
    switch (Command)
    {
        case PROGRESS:
        {
            PULONG Percent;

            Percent = (PULONG)Argument;
            DPRINT("%lu percent completed\n", *Percent);

            ProgressSetStep(FormatProgressBar, *Percent);
            break;
        }

#if 0
        case OUTPUT:
        {
            PTEXTOUTPUT Output;
            output = (PTEXTOUTPUT) Argument;
            DPRINT("%s\n", output->Output);
            break;
        }
#endif

        case DONE:
        {
            // PBOOLEAN Success;
            DPRINT("Done\n");
#if 0
            Success = (PBOOLEAN)Argument;
            if (*Success == FALSE)
            {
                DPRINT("FormatEx was unable to complete successfully.\n\n");
            }
#endif
            break;
        }

        default:
            DPRINT("Unknown callback %lu\n", (ULONG)Command);
            break;
    }

    return TRUE;
}

VOID
StartFormat(
    _Inout_ PFORMAT_PARTITION_INFO PartInfo,
    _In_ PFILE_SYSTEM_ITEM SelectedFileSystem)
{
    ASSERT(SelectedFileSystem && SelectedFileSystem->FileSystem);

    // TODO: Think about which values could be defaulted...
    PartInfo->FileSystemName = SelectedFileSystem->FileSystem;
    PartInfo->MediaFlag = FMIFS_HARDDISK;
    PartInfo->Label = NULL;
    PartInfo->QuickFormat = SelectedFileSystem->QuickFormat;
    PartInfo->ClusterSize = 0;
    PartInfo->Callback = FormatCallback;

    FormatProgressBar = CreateProgressBar(6,
                                          yScreen - 14,
                                          xScreen - 7,
                                          yScreen - 10,
                                          10,
                                          24,
                                          TRUE,
                                          MUIGetString(STRING_FORMATTINGPART));

    ProgressSetStepCount(FormatProgressBar, 100);
}

VOID
EndFormat(
    _In_ NTSTATUS Status)
{
    DestroyProgressBar(FormatProgressBar);
    FormatProgressBar = NULL;

    DPRINT("FormatPartition() finished with status 0x%08lx\n", Status);
}

/* EOF */
