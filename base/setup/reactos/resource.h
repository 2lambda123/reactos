#pragma once

/* Bitmaps */
#define IDB_WATERMARK       100
#define IDB_HEADER          101

/* Icons */
#define IDI_MAIN            3000
#define IDI_ROSICON         3001
#define IDI_WINICON         3002
#define IDI_DISKDRIVE       3003
#define IDI_PARTITION       3004


#define IDC_STATIC          -1

/* Dialogs */
#define IDD_STARTPAGE        2000
#define IDC_STARTTITLE       2001

#define IDD_TYPEPAGE         2010
#define IDC_INSTALL          2011
#define IDC_INSTALLTEXT      2012
#define IDC_UPDATE           2013
#define IDC_UPDATETEXT       2014

#define IDD_UPDATEREPAIRPAGE    2020
#define IDC_NTOSLIST            2021
#define IDC_SKIPUPGRADE         2022

#define IDD_DEVICEPAGE       2030
#define IDC_COMPUTER         2031
#define IDC_DISPLAY          2032
#define IDC_KEYBOARD         2033
#define IDC_KEYLAYOUT        2034

#define IDD_DRIVEPAGE           2040
#define IDC_PARTITION           2041
#define IDC_INITDISK            2042
#define IDC_PARTCREATE          2043
#define IDC_PARTDELETE          2044
#define IDC_DEVICEDRIVER        2045
#define IDC_PARTMOREOPTS        2046

#define IDD_SUMMARYPAGE      2050
#define IDC_INSTALLTYPE      2051
#define IDC_INSTALLSOURCE    2052
#define IDC_ARCHITECTURE     2053
// #define IDC_COMPUTER         2031
// #define IDC_DISPLAY          2032
// #define IDC_KEYBOARD         2033
#define IDC_DESTDRIVE       2054
// #define IDC_PATH             2071
#define IDC_CONFIRM_INSTALL 2055

#define IDD_PROCESSPAGE      2060
#define IDC_ACTIVITY         2061
#define IDC_ITEM             2062
#define IDC_PROCESSPROGRESS  2063

#define IDD_RESTARTPAGE      2070
#define IDC_FINISHTITLE      2071
#define IDC_RESTART_PROGRESS 2072

#define IDD_ADVINSTOPTS      2080
#define IDC_PATH             2081
#define IDC_INSTFREELDR      2082

#define IDD_PARTITION           2090
#define IDC_EDIT_PARTSIZE       2091
#define IDC_UPDOWN_PARTSIZE     2092
#define IDC_UNIT                2093
#define IDC_CHECK_MBREXTPART    2094
#define IDC_FSTYPE              2095
#define IDC_CHECK_QUICKFMT      2096


/* Strings */
#define IDS_TYPETITLE        5007
#define IDS_TYPESUBTITLE     5008
#define IDS_DEVICETITLE      5012
#define IDS_DEVICESUBTITLE   5013
#define IDS_DRIVETITLE       5019
#define IDS_DRIVESUBTITLE    5020
#define IDS_PROCESSTITLE     5029
#define IDS_PROCESSSUBTITLE  5030
#define IDS_RESTARTTITLE     5035
#define IDS_RESTARTSUBTITLE  5036
#define IDS_ABORTSETUP       5039
#define IDS_ABORTSETUP2      5040
#define IDS_SUMMARYTITLE     5050
#define IDS_SUMMARYSUBTITLE  5051
#define IDS_NO_TXTSETUP_SIF  5053
#define IDS_CAPTION          5054

#define IDS_INSTALLATION_NAME   5100
#define IDS_INSTALLATION_PATH   5101
#define IDS_INSTALLATION_VENDOR 5102

#define IDS_PARTITION_NAME   5200
#define IDS_PARTITION_TYPE   5201
#define IDS_PARTITION_SIZE   5202
#define IDS_PARTITION_STATUS 5203

// WARNING: These IDs *MUST* stay in increasing order!
#define IDS_BOOTLOADER_NOINST       5300
#define IDS_BOOTLOADER_REMOVABLE    5301
#define IDS_BOOTLOADER_SYSTEM       5302
#define IDS_BOOTLOADER_MBRVBR       5303
#define IDS_BOOTLOADER_VBRONLY      5304
