#include <windows.h>

#define PROJECT_NAME                    "Traymond"
#define PROJECT_NAME_LC                 "traymond"
#define PROJECT_NAME_UC                 "TRAYMOND"
#define VERSION_MAJOR                   2
#define VERSION_MINOR                   1
#define VERSION_PATCH                   1
#define VERSION_BUILD                   102
#define _VERSTR(number)                 __VERSTR(number)
#define __VERSTR(number)                #number
#define MAKEVERSION                     _VERSTR(VERSION_MAJOR) "." _VERSTR(VERSION_MINOR) "." _VERSTR(VERSION_PATCH)
#define MAKEFULLVERSION                 MAKEVERSION "." _VERSTR(VERSION_BUILD)
#define POPUP_CLASS                     "POPUP_WINDOW"

#define IDC_STATIC                      -1
#define IDM_EXIT                        1
#define IDM_RESTORE_ALL_WINDOWS         2
#define IDM_OPTIONS                     3
#define IDM_RESTORE_LAST_WINDOW         4
#define IDC_NEW                         11
#define IDC_SAVE                        12
#define IDC_REMOVE                      13
#define IDD_OPTIONS                     101
#define IDD_RULES                       103
#define IDD_ICONS                       104
#define IDI_TRAYMOND                    201
#define IDC_HOTKEY                      1000
#define IDC_HOTKEY_LIST                 1001
#define IDC_CHECK_USE_WIN               1002
#define IDC_CHECK_AUTORUN               1003
#define IDC_COMBO_HIDE_TYPE             1004
#define IDM_POPUP                       1005
#define IDC_LIST_RULES                  1005
#define IDC_CHECK_AUTO_HIDING           1006
#define IDC_BUTTON_RULES                1007
#define IDC_EDIT_TEXT                   1009
#define IDC_EDIT_CLASS                  1010
#define IDC_EDIT_PATH                   1011
#define IDC_CHECK_REGEX_TEXT            1012
#define IDC_CHECK_REGEX_CLASS           1013
#define IDC_CHECK_REGEX_PATH            1014
#define IDC_EDIT_NAME                   1015
#define IDC_COMBO_WINDOWS               1016
#define IDC_ICON_LIST                   1019
#define IDC_CHECK_SHOW_NOTIFICATION     1020

#define IDS_BEGIN                       2001
#define IDS_HOTKEY_ERROR                2001
#define IDS_MUTEX_ERROR                 2002
#define IDS_ALREADY_RUNNING             2003
#define IDS_SAVE_FILE_ERROR             2004
#define IDS_TOO_MANY_HIDDEN_WINDOWS     2005
#define IDS_RESTORE_FROM_UNEXPECTED_TERMINATION 2006
#define IDS_INVALID_HOTKEY              2007
#define IDS_HIDING_WINDOW               2008
#define IDS_TRAY                        2009
#define IDS_MENU                        2010
#define IDS_HOTKEY_HIDE                 2011
#define IDS_HOTKEY_MENU                 2012
#define IDS_COL_KEY                     2013
#define IDS_COL_ACTION                  2014
#define IDS_ACT_1                       2015
#define IDS_ACT_2                       2016
#define IDS_ACT_3                       2017
#define IDS_NEW_RULE                    2018
#define IDS_UNSAVED                     2019
#define IDS_RULE_INFO_REQUIRED          2020
#define IDS_INVALID_REGEX               2021
#define IDS_END                         2022
#define IDS_MAX_SIZE                    (IDS_END - IDS_BEGIN)
