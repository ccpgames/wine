/*
 * Drive management UI code
 *
 * Copyright 2003 Mark Westcott
 * Copyright 2003 Mike Hearn
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <wine/debug.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <shlobj.h>

#include "winecfg.h"
#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(winecfg);

static BOOL updatingUI = FALSE;
static char editWindowLetter; /* drive letter of the drive we are currently editing */


/* returns NULL on failure. caller is responsible for freeing result */
char *getDriveValue(char letter, char *valueName) {
  HKEY hkDrive = 0;
  char *subKeyName;
  char *result = NULL;
  HRESULT hr;
  DWORD bufferSize;
  
  WINE_TRACE("letter=%c, valueName=%s\n", letter, valueName);

  subKeyName = malloc(strlen("Drive X"));
  sprintf(subKeyName, "Drive %c", letter);

  hr = RegOpenKeyEx(configKey, subKeyName, 0, KEY_READ, &hkDrive);
  if (hr != ERROR_SUCCESS) goto end;

  hr = RegQueryValueEx(hkDrive, valueName, NULL, NULL, NULL, &bufferSize);
  if (hr != ERROR_SUCCESS) goto end;

  result = malloc(bufferSize);
  hr = RegQueryValueEx(hkDrive, valueName, NULL, NULL, result, &bufferSize);
  if (hr != ERROR_SUCCESS) goto end;
  
end:
  if (hkDrive) RegCloseKey(hkDrive);
  free(subKeyName);
  return result;
}

void initDriveDlg (HWND hDlg)
{
  int i;
  char *subKeyName = malloc(MAX_NAME_LENGTH);
  int driveCount = 0;
  DWORD sizeOfSubKeyName;

  WINE_TRACE("\n");

  updatingUI = TRUE;

  /* Clear the listbox */
  SendMessageA(GetDlgItem(hDlg, IDC_LIST_DRIVES), LB_RESETCONTENT, 0, 0);
  for (i = 0;
       RegEnumKeyExA(configKey, i, subKeyName, &sizeOfSubKeyName, NULL, NULL, NULL, NULL ) != ERROR_NO_MORE_ITEMS;
       ++i, sizeOfSubKeyName = 50) {
    
    HKEY hkDrive;
    char returnBuffer[MAX_NAME_LENGTH];
    DWORD sizeOfReturnBuffer = sizeof(returnBuffer);
    LONG r;

    if (!strncmp("Drive ", subKeyName, 5)) {
      char driveLetter = '\0';
      char *label;
      char *title;
      int titleLen;
      const char *itemLabel = "Drive %s (%s)";
      int itemIndex;
      
      if (RegOpenKeyExA (configKey, subKeyName, 0, KEY_READ, &hkDrive) != ERROR_SUCCESS)        {
        WINE_ERR("unable to open drive registry key");
        RegCloseKey(configKey);
        return;
      }
      
      /* extract the drive letter, force to upper case */
      driveLetter = subKeyName[strlen(subKeyName)-1];
      if(driveLetter) driveLetter = toupper(driveLetter);
      
      ZeroMemory(returnBuffer, sizeof(*returnBuffer));
      sizeOfReturnBuffer = sizeof(returnBuffer);
      r = RegQueryValueExA(hkDrive, "Label", NULL, NULL, returnBuffer, &sizeOfReturnBuffer);
      if (r == ERROR_SUCCESS) {
        label = malloc(sizeOfReturnBuffer);
        strncpy(label, returnBuffer, sizeOfReturnBuffer);
      } else {
        WINE_WARN("label not loaded: %ld\n", r);
        label = NULL;
      }

      /* We now know the label and drive letter, so we can add to the list. The list items will have the letter associated
       * with them, which acts as the key. We can then use that letter to get/set the properties of the drive. */
      WINE_TRACE("Adding %c (%s) to the listbox\n", driveLetter, label);
      
      if (!label) label = "no label";
      titleLen = strlen(itemLabel) - 1 + strlen(label) - 2 + 1;
      title = malloc(titleLen);
      /* the %s in the item label will be replaced by the drive letter, so -1, then
	 -2 for the second %s which will be expanded to the label, finally + 1 for terminating #0 */
      snprintf(title, titleLen, "Drive %c (%s)", driveLetter, label);
      
      /* the first SendMessage call adds the string and returns the index, the second associates that index with it */
      itemIndex = SendMessageA(GetDlgItem(hDlg, IDC_LIST_DRIVES), LB_ADDSTRING ,(WPARAM) -1, (LPARAM) title);
      SendMessageA(GetDlgItem(hDlg, IDC_LIST_DRIVES), LB_SETITEMDATA, itemIndex, (LPARAM) driveLetter);
      
      free(title);
      free(label);

      driveCount++;
	
    }
  }
  
  WINE_TRACE("loaded %d drives\n", driveCount);
  
  free(subKeyName);  
  updatingUI = FALSE;
  return;
}

/******************************************************************************/
/*  The Drive Editing Dialog                                                  */
/******************************************************************************/
#define DRIVE_MASK_BIT(B) 1<<(toupper(B)-'A')

typedef struct{
  char *sCode;
  char *sDesc;
} code_desc_pair;

static code_desc_pair type_pairs[] = {
  {"hd", "Local hard disk"},
  {"network", "Network share" },
  {"floppy", "Floppy disk"},
  {"cdrom", "CD-ROM"}
};
#define DRIVE_TYPE_DEFAULT 1

static code_desc_pair fs_pairs[] = {
  {"win95", "Long file names"},
  {"msdos", "MS-DOS 8 character file names"},
  {"unix", "UNIX file names"}
};
 
#define DRIVE_FS_DEFAULT 0


void fill_drive_droplist(long mask, char currentLetter, HWND hDlg)
{
  int i;
  int selection;
  int count;
  int next_letter;
  char sName[4] = "A:";

  for( i=0, count=0, selection=-1, next_letter=-1; i <= 'Z'-'A'; ++i ) {
    if( mask & DRIVE_MASK_BIT('A'+i) ) {
      sName[0] = 'A' + i;
      SendDlgItemMessage( hDlg, IDC_COMBO_LETTER, CB_ADDSTRING, 0, (LPARAM) sName );
      
      if( toupper(currentLetter) == 'A' + i ) {
	selection = count;
      }
      
      if( i >= 2 && next_letter == -1){ /*default drive is first one of C-Z */
	next_letter = count;
      }
      
      count++;
    }
  }
  
  if( selection == -1 ) {
    selection = next_letter;
  }
  
  SendDlgItemMessage( hDlg, IDC_COMBO_LETTER, CB_SETCURSEL, selection, 0 );
}

/* if bEnable is 1 then we are editing a CDROM, so can enable all controls, otherwise we want to disable
 * "detect from device" and "serial number", but still allow the user to manually set the path. The UI
 * for this could be somewhat better -mike
 */
void enable_labelserial_box(HWND hDlg, int bEnable)
{
  EnableWindow( GetDlgItem( hDlg, IDC_RADIO_AUTODETECT ), bEnable );
  EnableWindow( GetDlgItem( hDlg, IDC_EDIT_DEVICE ), bEnable );
  EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_BROWSE_DEVICE ), bEnable );
  EnableWindow( GetDlgItem( hDlg, IDC_EDIT_SERIAL ), bEnable );
  EnableWindow( GetDlgItem( hDlg, IDC_STATIC_SERIAL ), bEnable );
  EnableWindow( GetDlgItem( hDlg, IDC_STATIC_LABEL ), bEnable );
}

/* This function produces a mask for each drive letter that isn't currently used. Each bit of the long result
 * represents a letter, with A being the least significant bit, and Z being the most significant.
 *
 * To calculate this, we loop over each letter, and see if we can get a drive entry for it. If so, we
 * set the appropriate bit. At the end, we flip each bit, to give the desired result.
 *
 * The letter parameter is always marked as being available. This is so the edit dialog can display the
 * currently used drive letter alongside the available ones.
 */
long drive_available_mask(char letter)
{
  long result = 0;
  char curLetter;
  char *slop;
  
  WINE_TRACE("\n");
 
  for (curLetter = 'A'; curLetter < 'Z'; curLetter++) {
    slop = getDriveValue(curLetter, "Path");
    if (slop != NULL) {
      result |= DRIVE_MASK_BIT(curLetter);
      free(slop);
    }
  }
  
  result = ~result; 
  result |= DRIVE_MASK_BIT(letter);
  
  WINE_TRACE( "finished drive letter loop with %lx\n", result );
  return result;
}


void refreshDriveEditDialog(HWND hDlg) {
  char *path;
  char *type;
  char *fs;
  char *serial;
  char *label;
  char *device;
  int i, selection;

  updatingUI = TRUE;
  
  /* Drive letters */
  fill_drive_droplist( drive_available_mask( editWindowLetter ), editWindowLetter, hDlg );

  /* path */
  path = getDriveValue(editWindowLetter, "Path");
  if (path) {
    SetWindowText(GetDlgItem(hDlg, IDC_EDIT_PATH), path);
  } else WINE_WARN("no Path field?\n");
  
  /* drive type */
  type = getDriveValue(editWindowLetter, "Type");
  if (type) {
    for(i = 0, selection = -1; i < sizeof(type_pairs)/sizeof(code_desc_pair); i++) {
      SendDlgItemMessage(hDlg, IDC_COMBO_TYPE, CB_ADDSTRING, 0,
			 (LPARAM) type_pairs[i].sDesc);
      if(strcasecmp(type_pairs[i].sCode, type) == 0){
	selection = i;
      }
    }
  
    if( selection == -1 ) selection = DRIVE_TYPE_DEFAULT;
    SendDlgItemMessage(hDlg, IDC_COMBO_TYPE, CB_SETCURSEL, selection, 0);
  } else WINE_WARN("no Type field?\n");

  
  /* FileSystem name handling */
  fs = getDriveValue(editWindowLetter, "FileSystem");
  if (fs) {
    for( i=0, selection=-1; i < sizeof(fs_pairs)/sizeof(code_desc_pair); i++) {
      SendDlgItemMessage(hDlg, IDC_COMBO_NAMES, CB_ADDSTRING, 0,
			 (LPARAM) fs_pairs[i].sDesc);
      if(strcasecmp(fs_pairs[i].sCode, fs) == 0){
	selection = i;
      }
    }
  
    if( selection == -1 ) selection = DRIVE_FS_DEFAULT;
    SendDlgItemMessage(hDlg, IDC_COMBO_NAMES, CB_SETCURSEL, selection, 0);
  } else WINE_WARN("no FileSystem field?\n");


  /* removeable media properties */
  serial = getDriveValue(editWindowLetter, "Serial");
  if (serial) {
    SendDlgItemMessage(hDlg, IDC_EDIT_SERIAL, WM_SETTEXT, 0,(LPARAM)serial);
  } else WINE_WARN("no Serial field?\n");

  label = getDriveValue(editWindowLetter, "Label");
  if (label) {
    SendDlgItemMessage(hDlg, IDC_EDIT_LABEL, WM_SETTEXT, 0,(LPARAM)label);
  } else WINE_WARN("no Label field?\n");

  device = getDriveValue(editWindowLetter, "Device");
  if (device) {
    SendDlgItemMessage(hDlg, IDC_EDIT_DEVICE, WM_SETTEXT, 0,(LPARAM)device);
  } else WINE_WARN("no Device field?\n");

  if( strcmp("cdrom", type) == 0 ||
      strcmp("floppy", type) == 0) {
    if( (strlen( device ) == 0) &&
	((strlen( serial ) > 0) || (strlen( label ) > 0)) ) {
      selection = IDC_RADIO_ASSIGN;
    }
    else {
      selection = IDC_RADIO_AUTODETECT;
    }
    
    enable_labelserial_box( hDlg, 1 );
  }
  else {
    enable_labelserial_box( hDlg, 0 );
    selection = IDC_RADIO_ASSIGN;
  }

  CheckRadioButton( hDlg, IDC_RADIO_AUTODETECT, IDC_RADIO_ASSIGN, selection );
  SendDlgItemMessage(hDlg, IDC_EDIT_PATH, WM_SETTEXT, 0,(LPARAM)path);
  
  if (path) free(path);
  if (type) free(type);
  if (fs) free(fs);
  if (serial) free(serial);
  if (label) free(label);
  if (device) free(device);
  
  updatingUI = FALSE;
  
  return;
}

INT_PTR CALLBACK DriveEditDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  int selection;

  switch (uMsg)  {
      case WM_INITDIALOG: {
	editWindowLetter = (char) lParam;
	refreshDriveEditDialog(hDlg);
      }

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
	  case IDC_COMBO_TYPE:
	    switch( HIWORD(wParam)) {
		case CBN_SELCHANGE:
		  selection = SendDlgItemMessage( hDlg, IDC_COMBO_TYPE, CB_GETCURSEL, 0, 0);
		  if( selection == 2 || selection == 3 ) { /* cdrom or floppy */
		    enable_labelserial_box( hDlg, 1 );
		  }
		  else {
		    enable_labelserial_box( hDlg, 0 );
		  }
		  break;
	    }
	    break;
	    
	  case ID_BUTTON_OK: break;
	  
	  /* Fall through. */
	  
	  case ID_BUTTON_CANCEL:
	    EndDialog(hDlg, wParam);
	    return TRUE;
      }
  }
  return FALSE;
}


INT_PTR CALLBACK
DriveDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  int selection = -1;

  switch (uMsg) {
      case WM_COMMAND:
	switch (LOWORD(wParam)) {
	    case IDC_LIST_DRIVES:
	      switch(HIWORD( wParam )) {
		  case LBN_DBLCLK:
		    selection = -1;
		    break;
	      }

	    case IDC_BUTTON_ADD:
	      /* temporarily disabled, awaiting rewrite for transactional design (need to fill in defaults smartly, wizard?) */
	      break;

	    case IDC_BUTTON_REMOVE:
	      /* temporarily disabled, awaiting rewrite */
	      break;
	      
	    case IDC_BUTTON_EDIT:
	      if (HIWORD(wParam) == BN_CLICKED) {
		int nItem = SendMessage(GetDlgItem(hDlg, IDC_LIST_DRIVES),  LB_GETCURSEL, 0, 0);
		char letter = SendMessage(GetDlgItem(hDlg, IDC_LIST_DRIVES), LB_GETITEMDATA, nItem, 0);
		
		DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DRIVE_EDIT2), NULL, (DLGPROC) DriveEditDlgProc, (LPARAM) letter);
	      }
	      break;
	      
	}
	break;
	
      case WM_NOTIFY: switch(((LPNMHDR)lParam)->code) {
	    case PSN_KILLACTIVE:
	      SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
	      break;
	    case PSN_APPLY:
	      SetWindowLong(hDlg, DWL_MSGRESULT, PSNRET_NOERROR);
	      break;
	    case PSN_SETACTIVE:
	      initDriveDlg (hDlg);
	      break;
	}
	break;

  }

  return FALSE;
}
