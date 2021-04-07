/*
 * Project 64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 zilmar (zilmar@emulation64.com) and 
 * Jabo (jabo@emulation64.com).
 *
 * pj64 homepage: www.pj64.net
 *
 * Permission to use, copy, modify and distribute Project64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Project64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Project64 or software derived from Project64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */

#include <Windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <time.h>
#include <tchar.h>
#include "main.h"
#include "cpu.h"
#include "plugin.h"
#include "resource.h"
#include "RomTools_Common.h"

#define NoOfSortKeys		3
#define RB_FileName			0
#define RB_InternalName		1
#define RB_GoodName			2
#define RB_Status			3
#define RB_RomSize			4
#define RB_CoreNotes		5
#define RB_PluginNotes		6
#define RB_UserNotes		7
#define RB_CartridgeID		8
#define RB_Manufacturer		9
#define RB_Country			10
#define RB_Developer		11
#define RB_Crc1				12
#define RB_Crc2				13
#define RB_CICChip			14
#define RB_ReleaseDate		15
#define RB_Genre			16
#define RB_Players			17
#define RB_ForceFeedback	18
#define COLOR_TEXT			0
#define COLOR_SELECTED_TEXT	1
#define COLOR_HIGHLIGHTED	2

typedef struct {
	char     szFullFileName[MAX_PATH+1];
	char     Status[60];
	char     FileName[200];
	char     InternalName[22];
	char     GoodName[200];
	char     CartID[3];
	char     PluginNotes[250];
	char     CoreNotes[250];
	char     UserNotes[250];
	char     Developer[30];
	char     ReleaseDate[30];
	char     Genre[15];
	int		 Players;
	int      RomSize;
	BYTE     Manufacturer;
	BYTE     Country;
	DWORD    CRC1;
	DWORD    CRC2;
	int      CicChip;
	char     ForceFeedback[15];
} ROM_INFO;

typedef struct {
	int    ListCount;
	int    ListAlloc;
	ROM_INFO *List;
} ITEM_LIST;

typedef struct {
	int    Key[NoOfSortKeys];
	BOOL   KeyAscend[NoOfSortKeys];
} SORT_FIELDS;

typedef struct {
	char *status_name;
	COLORREF HighLight;
	COLORREF Text;
	COLORREF SelectedText;
} COLOR_ENTRY;

typedef struct {
	COLOR_ENTRY *List;
	int count;
	int max_allocated;
} COLOR_CACHE;


void GetSortField(int Index, char *ret, int max);
void LoadRomList();
void RomList_SortList();
void RomList_SelectFind(char *match);
void FillRomExtensionInfo(ROM_INFO *pRomInfo);
BOOL FillRomInfo(ROM_INFO *pRomInfo);
void SetSortAscending(BOOL Ascending, int Index);
void SetSortField(char * FieldName, int Index);
void SaveRomList();
void AddToColorCache(COLOR_ENTRY color);
COLORREF GetColor(char *status, int selection);
int ColorIndex(char *status);
void ClearColorCache();
void SetColors(char *status);
void FillRomList(char *Directory);

char CurrentRBFileName[MAX_PATH+1] = {""};

ROMBROWSER_FIELDS RomBrowserFields[] =
{
	"File Name",              -1, RB_FileName,      218, RB_FILENAME,
	"Internal Name",          -1, RB_InternalName,  200, RB_INTERNALNAME,
	"Good Name",               0, RB_GoodName,      218, RB_GOODNAME,
	"Status",                  1, RB_Status,         92, RB_STATUS,
	"Rom Size",               -1, RB_RomSize,       100, RB_ROMSIZE,
	"Notes (Core)",            2, RB_CoreNotes,     120, RB_NOTES_CORE,
	"Notes (default plugins)", 3, RB_PluginNotes,   188, RB_NOTES_PLUGIN,
	"Notes (User)",           -1, RB_UserNotes,     100, RB_NOTES_USER,
	"Cartridge ID",           -1, RB_CartridgeID,   100, RB_CART_ID,
	"Manufacturer",           -1, RB_Manufacturer,  100, RB_MANUFACTUER,
	"Country",                -1, RB_Country,       100, RB_COUNTRY,
	"Developer",              -1, RB_Developer,     100, RB_DEVELOPER,
	"CRC1",                   -1, RB_Crc1,          100, RB_CRC1,
	"CRC2",                   -1, RB_Crc2,          100, RB_CRC2,
	"CIC Chip",               -1, RB_CICChip,       100, RB_CICCHIP,
	"Release Date",           -1, RB_ReleaseDate,   100, RB_RELEASE_DATE,
	"Genre",                  -1, RB_Genre,         100, RB_GENRE,
	"Players",                -1, RB_Players,       100, RB_PLAYERS,
	"Force Feedback",          4, RB_ForceFeedback, 100, RB_FORCE_FEEDBACK,
};

HWND hRomList = NULL;
int NoOfFields = sizeof(RomBrowserFields) / sizeof(RomBrowserFields[0]),
 FieldType[(sizeof(RomBrowserFields) / sizeof(RomBrowserFields[0])) + 1];
BOOL scanning = FALSE, cancelled = TRUE, pendingUpdate = FALSE;
HANDLE gLVMutex = NULL;
HANDLE romThread;

ITEM_LIST ItemList = {0};
COLOR_CACHE ColorCache = {0};
SORT_FIELDS gSortFields = {0};

void AddRomToList (char *RomLocation) {
	
	DWORD wait_result = WaitForSingleObject(gLVMutex, INFINITE);

	// Mutex ownership acquired, okay to continue
	if (wait_result == WAIT_OBJECT_0) {
		if (ItemList.ListAlloc == ItemList.ListCount) {
			ItemList.ListAlloc += 50;
			ItemList.List = (ROM_INFO *)realloc(ItemList.List, ItemList.ListAlloc * sizeof(ROM_INFO));
		}

		// Only continue if reallocation was successful
		if (ItemList.List != NULL) {
			memset(&ItemList.List[ItemList.ListCount], 0, sizeof(ItemList.List[ItemList.ListCount]));
			strncpy(ItemList.List[ItemList.ListCount].szFullFileName, RomLocation, MAX_PATH);
			
			if (FillRomInfo(&ItemList.List[ItemList.ListCount]))
				ItemList.ListCount += 1;
		}
	}
	
	// No longer require mutual exclusion
	if (!ReleaseMutex(gLVMutex))
		MessageBox(NULL, "Failed to release a mutex???", "Error", MB_OK);

	// Corruption? Mutex was released by operating system, just bail and reuse memory allocation error
	if (wait_result == WAIT_ABANDONED || ItemList.List == NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		ExitThread(0);
	}
}

void CreateRomListControl (HWND hParent) {
	DWORD dwStyle;
	hRomList = CreateWindowEx( WS_EX_CLIENTEDGE,WC_LISTVIEW,NULL,
					WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER |
					LVS_OWNERDATA | LVS_OWNERDRAWFIXED | LVS_SINGLESEL | LVS_REPORT,
					0,0,0,0,hParent,(HMENU)IDC_ROMLIST,hInst,NULL);
	
	// Double buffering! Useful to keep the flicker down
	dwStyle = ListView_GetExtendedListViewStyle(hRomList);
	dwStyle |= LVS_EX_DOUBLEBUFFER;
	ListView_SetExtendedListViewStyle(hRomList, dwStyle);

	// Will be used to control when the listview can be updated
	gLVMutex = CreateMutex(NULL, FALSE, NULL);

	ResetRomBrowserColomuns();
	LoadRomList();
}

void FixRomBrowserColumnLang (void) {
	ResetRomBrowserColomuns();
}

void HideRomBrowser (void) {
	DWORD X, Y;
	long Style;

	if (CPURunning) { return; }	
	if (hRomList == NULL) { return; }

	IgnoreMove = TRUE;
	if (IsRomBrowserMaximized()) { ShowWindow(hMainWindow,SW_RESTORE); }
	ShowWindow(hMainWindow,SW_HIDE);
	Style = GetWindowLong(hMainWindow,GWL_STYLE);
	Style = Style &	~(WS_SIZEBOX | WS_MAXIMIZEBOX);
	SetWindowLong(hMainWindow,GWL_STYLE,Style);
	if (GetStoredWinPos( "Main", &X, &Y ) ) {
		SetWindowPos(hMainWindow,NULL,X,Y,0,0, SWP_NOZORDER | SWP_NOSIZE);		 
	}			
	EnableWindow(hRomList,FALSE);
	ShowWindow(hRomList,SW_HIDE);
	SetupPlugins(hMainWindow);
	
	SendMessage(hMainWindow,WM_USER + 17,0,0);
	ShowWindow(hMainWindow,SW_SHOW);
	IgnoreMove = FALSE;
}

BOOL IsRomBrowserMaximized (void) {
	return Settings_ReadBool(APPS_NAME, "Rom Browser Page", "Maximized", FALSE);
}

BOOL IsSortAscending (int Index) {
	char Search[200];
	sprintf(Search, "Sort Ascending %d", Index);
	return Settings_ReadBool(APPS_NAME, "Rom Browser Page", Search, FALSE);
}

void LoadRomList (void) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE] ,dir[_MAX_DIR];
	char fname[_MAX_FNAME],ext[_MAX_EXT];
	char FileName[_MAX_PATH];
	int Size, count;
	DWORD dwRead;
	HANDLE hFile;

	GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
	_splitpath( path_buffer, drive, dir, fname, ext );
	sprintf(FileName,"%s%s%s",drive,dir,ROC_NAME);

	hFile = CreateFile(FileName,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		RefreshRomBrowser();
		return;
	}
	Size = 0;
	ReadFile(hFile,&Size,sizeof(Size),&dwRead,NULL);
	if (Size != sizeof(ROM_INFO) || dwRead != sizeof(Size)) {
		CloseHandle(hFile);
		RefreshRomBrowser();
		return;
	}
	FreeRomBrowser();
	ReadFile(hFile,&ItemList.ListCount,sizeof(ItemList.ListCount),&dwRead,NULL);
	if(ItemList.ListCount == 0)	{
		CloseHandle(hFile);
		RefreshRomBrowser();
		return;
	}
	ItemList.List = (ROM_INFO *)malloc(ItemList.ListCount * sizeof(ROM_INFO));
	ItemList.ListAlloc = ItemList.ListCount;
	ReadFile(hFile,ItemList.List,sizeof(ROM_INFO) * ItemList.ListCount,&dwRead,NULL);
	CloseHandle(hFile);

	ListView_SetItemCount(hRomList, 0);

	for (count = 0; count < ItemList.ListCount; count++) {
		SetColors(ItemList.List[count].Status);
		// Update any items that may be out of date
		FillRomExtensionInfo(&ItemList.List[count]);
	}

	RomList_SortList();
	ListView_SetItemCount(hRomList, ItemList.ListCount);
	RomList_SelectFind(LastRoms[0]);
}

void LoadRomBrowserColumnInfo (void) {
	char String[200];
	int count;

	for (count = 0; count < NoOfFields; count++) {
		// Column Position
		RomBrowserFields[count].Pos = Settings_ReadInt(APPS_NAME, "Rom Browser", RomBrowserFields[count].Name, RomBrowserFields[count].Pos);

		// Column Width
		sprintf(String, "%s.Width", RomBrowserFields[count].Name);
		RomBrowserFields[count].ColWidth = Settings_ReadInt(APPS_NAME, "Rom Browser", String, RomBrowserFields[count].ColWidth);
	}
	FixRomBrowserColumnLang();
}

void FillRomExtensionInfo(ROM_INFO *pRomInfo) {
	char Identifier[100], *read;

	RomIDPreScanned(Identifier, &pRomInfo->CRC1, &pRomInfo->CRC2, &pRomInfo->Country);

	//Rom Notes
	if (RomBrowserFields[RB_UserNotes].Pos >= 0) {
		Settings_Read(RDN_NAME, Identifier, "Note", "", &read);
		strncpy(pRomInfo->UserNotes, read, sizeof(pRomInfo->UserNotes));
		if (read) free(read);
	}
	
	//Rom Extension info
	if (RomBrowserFields[RB_Developer].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "Developer", "", &read);
		strncpy(pRomInfo->Developer, read, sizeof(pRomInfo->Developer));
		if (read) free(read);
	}
	
	if (RomBrowserFields[RB_ReleaseDate].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "ReleaseDate", "", &read);
		strncpy(pRomInfo->ReleaseDate, read, sizeof(pRomInfo->ReleaseDate));
		if (read) free(read);
	}
	
	if (RomBrowserFields[RB_Genre].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "Genre", "", &read);
		strncpy(pRomInfo->Genre, read, sizeof(pRomInfo->Genre));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_Players].Pos >= 0)
		pRomInfo->Players = Settings_ReadInt(RDI_NAME, Identifier, "Players", 1);
		
	if (RomBrowserFields[RB_ForceFeedback].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "ForceFeedback", "", &read);
		strncpy(pRomInfo->ForceFeedback, read, sizeof(pRomInfo->ForceFeedback));
		if (read) free(read);
	}

	//Rom Settings
	if (RomBrowserFields[RB_GoodName].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Name", GS(RB_NOT_GOOD_FILE), &read);
		strncpy(pRomInfo->GoodName, read, sizeof(pRomInfo->GoodName));
		if (read) free(read);
	}
	
	if (RomBrowserFields[RB_Status].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Status", Default_RomStatus, &read);
		strncpy(pRomInfo->Status, read, sizeof(pRomInfo->Status));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_CoreNotes].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Core Note", "", &read);
		strncpy(pRomInfo->CoreNotes, read, sizeof(pRomInfo->CoreNotes));
		if (read) free(read);
	}
	
	if (RomBrowserFields[RB_PluginNotes].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Plugin Note", "", &read);
		strncpy(pRomInfo->PluginNotes, read, sizeof(pRomInfo->PluginNotes));
		if (read) free(read);
	}

	SetColors(pRomInfo->Status);
}

BOOL FillRomInfo(ROM_INFO *pRomInfo) {
	BYTE RomData[0x1000];
	
	if (!LoadDataFromRomFile(pRomInfo->szFullFileName, RomData, sizeof(RomData), &pRomInfo->RomSize))
		return FALSE;

	_splitpath(pRomInfo->szFullFileName, NULL, NULL, pRomInfo->FileName, NULL);

	GetRomName(pRomInfo->InternalName, RomData);
	GetRomCartID(pRomInfo->CartID, RomData);
	GetRomManufacturer(&pRomInfo->Manufacturer, RomData);
	GetRomCountry(&pRomInfo->Country, RomData);
	GetRomCRC1(&pRomInfo->CRC1, RomData);
	GetRomCRC2(&pRomInfo->CRC2, RomData);
	pRomInfo->CicChip = GetRomCicChipID(RomData);
	
	FillRomExtensionInfo(pRomInfo);
	return TRUE;
}

int GetRomBrowserSize ( DWORD * nWidth, DWORD * nHeight ) {
	*nWidth = Settings_ReadInt(APPS_NAME, "Rom Browser Page", "Width", -1);
	*nHeight = Settings_ReadInt(APPS_NAME, "Rom Browser Page", "Height", -1);

	if (*nWidth == -1 || *nHeight == -1)
		return FALSE;

	return TRUE;
}

void GetSortField(int Index, char *ret, int max) {
	char String[200], *read;

	sprintf(String, "Sort Field %d", Index);
	Settings_Read(APPS_NAME, "Rom Browser Page", String, "", &read);
	strncpy(ret, read, max);
	if (read) free(read);
}

void RefreshRomBrowser (void) {
	// Update to not scan if the rom list is hidden
	if (hRomList && !IsWindowEnabled(hRomList)) {
		FreeRomBrowser();
		SaveRomList();
		pendingUpdate = TRUE;
		return;
	}
	if (!hRomList) { return; }
	if (scanning) {
		MessageBox(NULL, "Still Scanning!", "Wait up!", MB_OK);
		return;
	}
	ListView_SetItemCount(hRomList, 0);
	FreeRomBrowser();
	
	romThread = CreateThread(NULL, 0, RefreshRomBrowserMT, NULL, 0, NULL);
}

void ResetRomBrowserColomuns (void) {
	int Column, index;
	LV_COLUMN lvColumn;
	char szString[300];

	//SaveRomBrowserColumnInfo();
    memset(&lvColumn,0,sizeof(lvColumn));
	lvColumn.mask = LVCF_FMT;
	while (ListView_GetColumn(hRomList,0,&lvColumn)) {
		ListView_DeleteColumn(hRomList,0);
	}

	//Add Colomuns
	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.pszText = szString;

	for (Column = 0; Column < NoOfFields; Column ++) {
		for (index = 0; index < NoOfFields; index ++) {
			if (RomBrowserFields[index].Pos == Column) { break; }
		}
		if (index == NoOfFields || RomBrowserFields[index].Pos != Column) {
			FieldType[Column] = -1;
			break;
		}
		FieldType[Column] = RomBrowserFields[index].ID;
		lvColumn.cx = RomBrowserFields[index].ColWidth;
		strncpy(szString, GS(RomBrowserFields[index].LangID), sizeof(szString));
		ListView_InsertColumn(hRomList, Column, &lvColumn);
	}
}

void ResizeRomListControl (WORD nWidth, WORD nHeight) {
	if (IsWindow(hRomList)) {
		if (IsWindow(hStatusWnd)) {
			RECT rc;

			GetWindowRect(hStatusWnd, &rc);
			nHeight -= (WORD)(rc.bottom - rc.top);
		}
		MoveWindow(hRomList, 0, 0, nWidth, nHeight, TRUE);
	}
}

void RomList_ColumnSortList(LPNMLISTVIEW pnmv) {
	int index, iItem;
	char selected_filename[261], String[200];

	// Save the previously selected item 
	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	if (iItem != -1)
		strcpy(selected_filename, ItemList.List[iItem].szFullFileName);

	for (index = 0; index < NoOfFields; index++) {
		if (RomBrowserFields[index].Pos == pnmv->iSubItem) { break; }
	}
	if (NoOfFields == index) { return; }
	GetSortField(0, String, sizeof(String));
	if (_stricmp(String, RomBrowserFields[index].Name) == 0) {
		SetSortAscending(!IsSortAscending(0),0);
	} else {
		int count;

		for (count = NoOfSortKeys - 1; count > 0; count --) {
			GetSortField(count - 1, String, sizeof(String));
			SetSortField (String, count);
			SetSortAscending(IsSortAscending(count - 1), count);
		}
		SetSortField (RomBrowserFields[index].Name, 0);
		SetSortAscending(TRUE,0);
	}
	RomList_SortList();
	ListView_RedrawItems(hRomList, 0, ItemList.ListCount);

	// Make sure the last item selected is still selected
	if (iItem != -1) {
		RomList_SelectFind(selected_filename);
	}
}

int __cdecl RomList_Compare (const void *a, const void *b) {
	ROM_INFO *pRomInfo1, *pRomInfo2;
	int count, compare;

	pRomInfo1 = (ROM_INFO *)a;
	pRomInfo2 = (ROM_INFO *)b;

	for (count = 0; count < NoOfSortKeys; count++) {
		pRomInfo1 = gSortFields.KeyAscend[count] ? (ROM_INFO *)a : (ROM_INFO *)b;
		pRomInfo2 = gSortFields.KeyAscend[count] ? (ROM_INFO *)b : (ROM_INFO *)a;

		switch (gSortFields.Key[count]) {
		case RB_FileName:
			compare = (int)lstrcmpi(pRomInfo1->FileName, pRomInfo2->FileName);
			break;
		case RB_InternalName:
			compare = (int)lstrcmpi(pRomInfo1->InternalName, pRomInfo2->InternalName);
			break;
		case RB_GoodName:
			compare = (int)lstrcmpi(pRomInfo1->GoodName, pRomInfo2->GoodName);
			break;
		case RB_Status:
			compare = (int)lstrcmpi(pRomInfo1->Status, pRomInfo2->Status);
			break;
		case RB_RomSize:
			compare = pRomInfo1->RomSize - pRomInfo2->RomSize;
			break;
		case RB_CoreNotes:
			compare = (int)lstrcmpi(pRomInfo1->CoreNotes, pRomInfo2->CoreNotes);
			break;
		case RB_PluginNotes:
			compare = (int)lstrcmpi(pRomInfo1->PluginNotes, pRomInfo2->PluginNotes);
			break;
		case RB_UserNotes:
			compare = (int)lstrcmpi(pRomInfo1->UserNotes, pRomInfo2->UserNotes);
			break;
		case RB_CartridgeID:
			compare = (int)lstrcmpi(pRomInfo1->CartID, pRomInfo2->CartID);
			break;
		case RB_Manufacturer:
			compare = (int)pRomInfo1->Manufacturer - (int)pRomInfo2->Manufacturer;
			break;
		case RB_Country:
			{
				char junk1[50], junk2[50];
				CountryCodeToString(junk1, pRomInfo1->Country, sizeof(junk1));
				CountryCodeToString(junk2, pRomInfo2->Country, sizeof(junk2));
				compare = lstrcmpi(junk1, junk2);
				break;
			}
		case RB_Developer:
			compare = (int)lstrcmpi(pRomInfo1->Developer, pRomInfo2->Developer);
			break;
		case RB_Crc1:
			compare = (int)pRomInfo1->CRC1 - (int)pRomInfo2->CRC1;
			break;
		case RB_Crc2:
			compare = (int)pRomInfo1->CRC2 - (int)pRomInfo2->CRC2;
			break;
		case RB_CICChip:
			compare = pRomInfo1->CicChip - pRomInfo2->CicChip;
			break;
		case RB_ReleaseDate:
			compare = (int)lstrcmpi(pRomInfo1->ReleaseDate, pRomInfo2->ReleaseDate);
			break;
		case RB_Players:
			compare = pRomInfo1->Players - pRomInfo2->Players;
			break;
		case RB_ForceFeedback:
			compare = (int)lstrcmpi(pRomInfo1->ForceFeedback, pRomInfo2->ForceFeedback);
			break;
		case RB_Genre:
			compare = (int)lstrcmpi(pRomInfo1->Genre, pRomInfo2->Genre);
			break;
		default:
			compare = 0;
			break;
		}

		if (compare != 0)
			return compare;
	}
	return 0;
} 

void RomList_GetDispInfo(LPNMHDR pnmh) {
	LV_DISPINFO * lpdi = (LV_DISPINFO *)pnmh;
	ROM_INFO * pRomInfo = &ItemList.List[lpdi->item.iItem];

	// Do not continue if the request does not contain a valid text field (Windows XP was having issues here)
	if (!(lpdi->item.mask & LVIF_TEXT))
		return;

	switch(FieldType[lpdi->item.iSubItem]) {
	case RB_FileName: strncpy(lpdi->item.pszText, pRomInfo->FileName, lpdi->item.cchTextMax); break;
	case RB_InternalName: strncpy(lpdi->item.pszText, pRomInfo->InternalName, lpdi->item.cchTextMax); break;
	case RB_GoodName: strncpy(lpdi->item.pszText, pRomInfo->GoodName, lpdi->item.cchTextMax); break;
	case RB_CoreNotes: strncpy(lpdi->item.pszText, pRomInfo->CoreNotes, lpdi->item.cchTextMax); break;
	case RB_PluginNotes: strncpy(lpdi->item.pszText, pRomInfo->PluginNotes, lpdi->item.cchTextMax); break;
	case RB_Status: strncpy(lpdi->item.pszText, pRomInfo->Status, lpdi->item.cchTextMax); break;
	case RB_RomSize: sprintf(lpdi->item.pszText,"%.1f MBit",(float)pRomInfo->RomSize/0x20000); break;
	case RB_CartridgeID: strncpy(lpdi->item.pszText, pRomInfo->CartID, lpdi->item.cchTextMax); break;
	case RB_Manufacturer:
		switch (pRomInfo->Manufacturer) {
		case 'N':strncpy(lpdi->item.pszText, "Nintendo", lpdi->item.cchTextMax); break;
		case 0:  strncpy(lpdi->item.pszText, "None", lpdi->item.cchTextMax); break;
		default: sprintf(lpdi->item.pszText, "(Unknown %c (%X))", pRomInfo->Manufacturer,pRomInfo->Manufacturer); break;
		}
		break;
	case RB_Country: {
		char junk[50];
		CountryCodeToString(junk, pRomInfo->Country, 50);
		strncpy(lpdi->item.pszText, junk, lpdi->item.cchTextMax);
		break; }
	case RB_Crc1: sprintf(lpdi->item.pszText,"0x%08X",pRomInfo->CRC1); break;
	case RB_Crc2: sprintf(lpdi->item.pszText,"0x%08X",pRomInfo->CRC2); break;
	case RB_CICChip: 
		if (pRomInfo->CicChip < 0) { 
			sprintf(lpdi->item.pszText,"Unknown CIC Chip"); 
		} else {
			sprintf(lpdi->item.pszText,"CIC-NUS-61%2d",pRomInfo->CicChip); 
		}
		break;
	case RB_UserNotes: strncpy(lpdi->item.pszText, pRomInfo->UserNotes, lpdi->item.cchTextMax); break;
	case RB_Developer: strncpy(lpdi->item.pszText, pRomInfo->Developer, lpdi->item.cchTextMax); break;
	case RB_ReleaseDate: strncpy(lpdi->item.pszText, pRomInfo->ReleaseDate, lpdi->item.cchTextMax); break;
	case RB_Genre: strncpy(lpdi->item.pszText, pRomInfo->Genre, lpdi->item.cchTextMax); break;
	case RB_Players: sprintf(lpdi->item.pszText,"%d",pRomInfo->Players); break;
	case RB_ForceFeedback: strncpy(lpdi->item.pszText, pRomInfo->ForceFeedback, lpdi->item.cchTextMax); break;
	default: strncpy(lpdi->item.pszText, " ", lpdi->item.cchTextMax);
	}
	if(lpdi->item.pszText == NULL)
		return;
	if (strlen(lpdi->item.pszText) == 0) { strcpy(lpdi->item.pszText," "); }
}

void RomList_PopupMenu(LPNMHDR pnmh) {
	HMENU hMenu = LoadMenu(hInst,MAKEINTRESOURCE(IDR_POPUP));
	HMENU hPopupMenu = GetSubMenu(hMenu,0);
	POINT Mouse;
	LONG iItem;

	GetCursorPos(&Mouse);

	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	if (iItem != -1) { 
		strcpy(CurrentRBFileName, ItemList.List[iItem].szFullFileName);
	} else {
		strcpy(CurrentRBFileName, "");
	}
	
	//Fix up menu
	MenuSetText(hPopupMenu, 0, GS(POPUP_PLAY), NULL);
	MenuSetText(hPopupMenu, 2, GS(MENU_REFRESH), NULL);
	MenuSetText(hPopupMenu, 3, GS(MENU_CHOOSE_ROM), NULL);
	MenuSetText(hPopupMenu, 5, GS(POPUP_INFO), NULL);
	MenuSetText(hPopupMenu, 7, GS(POPUP_SETTINGS), NULL);
	MenuSetText(hPopupMenu, 8, GS(POPUP_CHEATS), NULL);

	if (strlen(CurrentRBFileName) == 0) {
		DeleteMenu(hPopupMenu,8,MF_BYPOSITION);
		DeleteMenu(hPopupMenu,7,MF_BYPOSITION);
		DeleteMenu(hPopupMenu,6,MF_BYPOSITION);
		DeleteMenu(hPopupMenu,5,MF_BYPOSITION);
		DeleteMenu(hPopupMenu,4,MF_BYPOSITION);
		DeleteMenu(hPopupMenu,1,MF_BYPOSITION);
		DeleteMenu(hPopupMenu,0,MF_BYPOSITION);
	} else {
		if (BasicMode && !RememberCheats) { DeleteMenu(hPopupMenu,8,MF_BYPOSITION); }
		if (BasicMode) { DeleteMenu(hPopupMenu,7,MF_BYPOSITION); }
		if (BasicMode && !RememberCheats) { DeleteMenu(hPopupMenu,6,MF_BYPOSITION); }
	}
	
	TrackPopupMenu(hPopupMenu, 0, Mouse.x, Mouse.y, 0,hMainWindow, NULL);
	DestroyMenu(hMenu);
}

void RomList_SetFocus (void) {
	if (!RomListVisible()) { return; }
	SetFocus(hRomList);
}

void RomList_OpenRom(LPNMHDR pnmh) {
	DWORD ThreadID;
	LONG iItem;

	(*pnmh).idFrom;

	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	if (iItem == -1) { return; }

	strcpy(CurrentFileName, ItemList.List[iItem].szFullFileName);
	CreateThread(NULL, 0, OpenChosenFile, NULL, 0, &ThreadID);	
}

void RomList_SortList (void) {
	char SortField[200];
	int count, index;

	for (count = 0; count < NoOfSortKeys; count ++) {
		GetSortField(count, SortField, sizeof(SortField));
		for (index = 0; index < NoOfFields; index++) {
			if (_stricmp(RomBrowserFields[index].Name, SortField) == 0) { break; }
		}
		// Global variables used, qsort does not allow passing of variables
		gSortFields.Key[count] = index;
		gSortFields.KeyAscend[count] = IsSortAscending(count);
	}
	
	qsort(ItemList.List, ItemList.ListCount, sizeof(*ItemList.List), RomList_Compare);
}

void RomListDrawItem (LPDRAWITEMSTRUCT ditem) {
	RECT rcItem, rcDraw;
	ROM_INFO * pRomInfo;
	char String[300];
	BOOL bSelected;
	HBRUSH hBrush;
    LV_COLUMN lvc; 
	int nColumn;

	bSelected = (ListView_GetItemState(hRomList, ditem->itemID, -1) & LVIS_SELECTED);
	pRomInfo = &ItemList.List[ditem->itemID];
	if (bSelected) {
		hBrush = CreateSolidBrush(GetColor(pRomInfo->Status, COLOR_HIGHLIGHTED));
		SetTextColor(ditem->hDC, GetColor(pRomInfo->Status, COLOR_SELECTED_TEXT));
	} else {
		hBrush = (HBRUSH)(COLOR_WINDOW + 1);
		SetTextColor(ditem->hDC, GetColor(pRomInfo->Status, COLOR_TEXT));
	}
	FillRect( ditem->hDC, &ditem->rcItem,hBrush);	
	SetBkMode( ditem->hDC, TRANSPARENT );
	
	//Draw
	ListView_GetItemRect(hRomList,ditem->itemID,&rcItem,LVIR_LABEL);
	ListView_GetItemText(hRomList,ditem->itemID, 0, String, sizeof(String)); 
	memcpy(&rcDraw,&rcItem,sizeof(RECT));
	rcDraw.right -= 3;
	DrawText(ditem->hDC, String, strlen(String), &rcDraw, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);	
	
    memset(&lvc,0,sizeof(lvc));
	lvc.mask = LVCF_FMT | LVCF_WIDTH; 
	for(nColumn = 1; ListView_GetColumn(hRomList,nColumn,&lvc); nColumn += 1) {		
		rcItem.left = rcItem.right; 
        rcItem.right += lvc.cx; 

		ListView_GetItemText(hRomList,ditem->itemID, nColumn, String, sizeof(String)); 
		memcpy(&rcDraw,&rcItem,sizeof(RECT));
		rcDraw.right -= 3;
		DrawText(ditem->hDC, String, strlen(String), &rcDraw, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);
	}

	DeleteObject(hBrush);
}

void RomListNotify(LPNMHDR pnmh) {
	switch (pnmh->code) {
	case LVN_GETDISPINFO:
		{
			DWORD wait_result = WaitForSingleObject(gLVMutex, INFINITE);
			
			// Got control of the mutex, that means nothing else is using the listview control
			if (wait_result == WAIT_OBJECT_0) {
				RomList_GetDispInfo(pnmh);
				
				if (!ReleaseMutex(gLVMutex))
					MessageBox(NULL, "Failed to release a mutex???", "Error!", MB_OK);
			}
			
			// Abandoned mutex, why???
			if (wait_result == WAIT_ABANDONED) {
				DisplayError(GS(MSG_MEM_ALLOC_ERROR));
				ExitThread(0);
			}
		}
		break;
	case LVN_COLUMNCLICK:
		RomList_ColumnSortList((LPNMLISTVIEW)pnmh);
		break;
	case NM_RETURN:
		RomList_OpenRom(pnmh);
		break;
	case NM_DBLCLK:
		RomList_OpenRom(pnmh);
		break;
	case NM_RCLICK:
		RomList_PopupMenu(pnmh);
		break;
	}
}

BOOL RomListVisible(void) {
	if (hRomList == NULL) { return FALSE; }
	return (IsWindowVisible(hRomList));
}

void SaveRomBrowserColumnInfo (void) {
	int Column, index;
	LV_COLUMN lvColumn;
	char String[200], String2[200];

	memset(&lvColumn,0,sizeof(lvColumn));
	lvColumn.mask = LVCF_WIDTH;
	
	for (Column = 0;ListView_GetColumn(hRomList, Column, &lvColumn); Column++) {
		for (index = 0; index < NoOfFields; index++) {
			if (RomBrowserFields[index].Pos == Column) { break; }
		}
		
		RomBrowserFields[index].ColWidth = lvColumn.cx;
		sprintf(String, "%s.Width", RomBrowserFields[index].Name);
		sprintf(String2, "%d", lvColumn.cx);
		Settings_Write(APPS_NAME, "Rom Browser", String, String2);
	}
}

void SaveRomBrowserColumnPosition (int index, int Position) {
	char szPos[10];

	sprintf(szPos, "%d", Position);
	Settings_Write(APPS_NAME, "Rom Browser", RomBrowserFields[index].Name, szPos);
}

void SaveRomList (void) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE] ,dir[_MAX_DIR];
	char fname[_MAX_FNAME],ext[_MAX_EXT];
	char FileName[_MAX_PATH];
	DWORD dwWritten;
	HANDLE hFile;
	int Size;

	GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
	_splitpath( path_buffer, drive, dir, fname, ext );
	sprintf(FileName,"%s%s%s",drive,dir,ROC_NAME);
	
	hFile = CreateFile(FileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	Size = sizeof(ROM_INFO);
	WriteFile(hFile,&Size,sizeof(Size),&dwWritten,NULL);
	WriteFile(hFile,&ItemList.ListCount,sizeof(ItemList.ListCount),&dwWritten,NULL);
	WriteFile(hFile,ItemList.List,Size * ItemList.ListCount,&dwWritten,NULL);
	CloseHandle(hFile);
}

// Windows 7 and higher has a bugged SHBrowseForFolder when using BIF_NEWDIALOGSTYLE
// This is a work-around, it enumerates through the child windows until the tree view of the dialog is hit and then
//	ensures the selected element is visible.
// This behavior is not apparent in XP or Vista and it is suggested to use the new FolderBrowserDialog
static BOOL CALLBACK EnumCallback(HWND hWndChild, LPARAM lParam)
{
	char szClass[MAX_PATH];
	HTREEITEM hNode;
	if (GetClassName(hWndChild, szClass, sizeof(szClass)) && strcmp(szClass, "SysTreeView32") == 0) {
		hNode = TreeView_GetSelection(hWndChild);    // found the tree view window
		TreeView_EnsureVisible(hWndChild, hNode);   // ensure its selection is visible
		return(FALSE);   // done; stop enumerating
	}
	return(TRUE);       // continue enumerating
}

// This is sort of a hack, it's designed to force SHBrowseForFolder to scroll down to the selected folder
// It seems to not have worked until BFFM_SELCHANGED was included
int CALLBACK SelectRomDirCallBack(HWND hwnd,DWORD uMsg,DWORD lp, DWORD lpData) {
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		// lpData is TRUE since you are passing a path.
		// It would be FALSE if you were passing a pidl.
		if (lpData)
			SendMessage((HWND)hwnd, BFFM_SETSELECTION, TRUE, lpData);
		break;

	case BFFM_SELCHANGED:
		EnumChildWindows(hwnd, EnumCallback, NULL);
		break;
	}
	return 0;
}

void SelectRomDir(void) {
	char Buffer[MAX_PATH], Directory[MAX_PATH], RomDirectory[MAX_PATH + 1];
	LPITEMIDLIST pidl;
	BROWSEINFO bi = { 0 };	// Initialization to 0 prevents XP crash

	Settings_GetDirectory(RomDir, RomDirectory, sizeof(RomDirectory));

	bi.hwndOwner = hMainWindow;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = Buffer;
	bi.lpszTitle = GS(SELECT_ROM_DIR);
	bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = (BFFCALLBACK)SelectRomDirCallBack;
	bi.lParam = (DWORD)RomDirectory;

	CoInitialize(NULL);

	if ((pidl = SHBrowseForFolder(&bi)) != NULL) {
		if (SHGetPathFromIDList(pidl, Directory)) {
			int len = strlen(Directory);

			if (Directory[len - 1] != '\\') {
				strcat(Directory,"\\");
			}
			SetRomDirectory(Directory);
			Settings_Write(APPS_NAME, "Directories", "Use Default Rom", "False");
			RefreshRomBrowser();
		}
	}

	CoUninitialize();
}

void SetRomBrowserMaximized (BOOL Maximized) {
	Settings_Write(APPS_NAME, "Page Setup", "Rom Browser Maximized", Maximized ? STR_TRUE : STR_FALSE);
}

void SetRomBrowserSize ( int nWidth, int nHeight ) {
	char String[100];

	sprintf(String, "%d", nWidth);
	Settings_Write(APPS_NAME, "Rom Browser Page", "Width", String);
	sprintf(String, "%d", nHeight);
	Settings_Write(APPS_NAME, "Rom Browser Page", "Height", String);
}

void SetSortAscending (BOOL Ascending, int Index) {
	char String[200];

	sprintf(String, "Sort Ascending %d", Index);
	Settings_Write(APPS_NAME, "Rom Browser Page", String, Ascending ? STR_TRUE : STR_FALSE);
}

void SetSortField (char * FieldName, int Index) {
	char String[200];

	sprintf(String, "Sort Field %d", Index);
	Settings_Write(APPS_NAME, "Rom Browser Page", String, FieldName);
}

void FillRomList (char *Directory) {
	char FullPath[MAX_PATH + 1], FileName[MAX_PATH + 1], SearchSpec[MAX_PATH + 1];
	char drive[_MAX_DRIVE], dir[_MAX_DIR], ext[_MAX_EXT];
	WIN32_FIND_DATA fd;
	HANDLE hFind;

	strcpy(SearchSpec, Directory);
	if (SearchSpec[strlen(Directory) - 1] != '\\')
		strcat(SearchSpec, "\\");
	strcat(SearchSpec, "*.*");

	hFind = FindFirstFile(SearchSpec, &fd);
	if (hFind == INVALID_HANDLE_VALUE) { return; }
	do {
		// Force a stop of the scanning
		if (!scanning)
			break;

		if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
			continue;

		strcpy(FullPath, Directory);
		if (FullPath[strlen(Directory) - 1] != '\\') { strcat(FullPath,"\\"); }
		strcat(FullPath, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (Recursion)
				FillRomList(FullPath);
			continue;
		}
		_splitpath( FullPath, drive, dir, FileName, ext );
		if (_stricmp(ext, ".zip") == 0 || _stricmp(ext, ".v64") == 0 || _stricmp(ext, ".z64") == 0 || _stricmp(ext, ".n64") == 0 ||
			_stricmp(ext, ".rom") == 0 || _stricmp(ext, ".jap") == 0 || _stricmp(ext, ".pal") == 0 || _stricmp(ext, ".usa") == 0 ||
			_stricmp(ext, ".eur") == 0 || _stricmp(ext, ".bin") == 0)
			AddRomToList(FullPath);
	} while (FindNextFile(hFind, &fd));

	FindClose(hFind);
}

void ShowRomList(HWND hParent) {
	DWORD X, Y, Width, Height;
	long Style;
	int iItem;

	if (CPURunning) { return; }
	if (hRomList != NULL && IsWindowVisible(hRomList)) { return; }

	SetupMenu(hMainWindow);
	IgnoreMove = TRUE;
	SetupPlugins(hHiddenWin);
	ShowWindow(hMainWindow,SW_HIDE);
	if (hRomList == NULL) {
		CreateRomListControl(hParent);
	} else {
		EnableWindow(hRomList,TRUE);
	}
	if (!GetRomBrowserSize(&Width,&Height)) { Width = 640; Height= 480; }
	ChangeWinSize ( hMainWindow, Width, Height, NULL );
	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	ListView_EnsureVisible(hRomList, iItem, FALSE);

	ShowWindow(hRomList,SW_SHOW);
	InvalidateRect(hParent,NULL,TRUE);
	Style = GetWindowLong(hMainWindow,GWL_STYLE) | WS_SIZEBOX | WS_MAXIMIZEBOX;
	SetWindowLong(hMainWindow,GWL_STYLE,Style);
	if (!GetStoredWinPos( "Main.RomList", &X, &Y ) ) {
  		X = (GetSystemMetrics( SM_CXSCREEN ) - Width) / 2;
		Y = (GetSystemMetrics( SM_CYSCREEN ) - Height) / 2;
	}		
	SetWindowPos(hMainWindow,HWND_NOTOPMOST,X,Y,0,0,SWP_NOSIZE);		 
	if (IsRomBrowserMaximized()) { 
		ShowWindow(hMainWindow,SW_MAXIMIZE); 
	} else {
		ShowWindow(hMainWindow,SW_SHOW);
		DrawMenuBar(hMainWindow);
		ChangeWinSize ( hMainWindow, Width, Height, NULL );
	}
	IgnoreMove = FALSE;
	SetupMenu(hMainWindow);
	
	SetFocus(hRomList);

	if (pendingUpdate) {
		RefreshRomBrowser();
		pendingUpdate = FALSE;
	}
}

void FreeRomBrowser ( void )
{
	if (ItemList.ListAlloc != 0) {
		free(ItemList.List);
		ItemList.ListAlloc = 0;
		ItemList.ListCount = 0;
		ItemList.List = NULL;
	}
	
	ClearColorCache();
}

void AddToColorCache(COLOR_ENTRY color) {

	// Allocate more memory if there is not enough to store the new entry.
	if (ColorCache.count == ColorCache.max_allocated) {
		COLOR_ENTRY *temp;
		const int increase = ColorCache.max_allocated + 5;

		temp = (COLOR_ENTRY *)realloc(ColorCache.List, sizeof(COLOR_ENTRY) * increase);

		if (temp == NULL)
			return;

		ColorCache.List = temp;
		ColorCache.max_allocated = increase;
	}

	ColorCache.List[ColorCache.count] = color;
	ColorCache.count++;
}

COLORREF GetColor(char *status, int selection) {
	int i = ColorIndex(status);

	switch(selection) {
	case COLOR_SELECTED_TEXT:
		if (i == -1)
			return RGB(0xFF, 0xFF, 0xFF);
		return ColorCache.List[i].SelectedText;
	case COLOR_HIGHLIGHTED:
		if (i == -1) 
			return RGB(0, 0, 0);
		return ColorCache.List[i].HighLight;
	default:
		if (i == -1)
			return RGB(0, 0, 0);
		return ColorCache.List[i].Text;
	}
}

int ColorIndex(char *status) {
	int i;

	for (i = 0; i < ColorCache.count; i++)
		if (strcmp(status, ColorCache.List[i].status_name) == 0)
			return i;

	return -1;
}

void ClearColorCache() {
	int i;
	
	for (i = 0; i < ColorCache.count; i++) {
		free(ColorCache.List[i].status_name);
	}

	free(ColorCache.List);

	ColorCache.count = 0;
	ColorCache.List = NULL;
	ColorCache.max_allocated = 0;
}

void SetColors(char *status) {
	int count;
	COLOR_ENTRY colors;
	char String[100], *read;

	if (ColorIndex(status) == -1) {
		sprintf(String, "%s", status);
		Settings_Read(RDS_NAME, "Rom Status", String, "000000", &read);
		count = (AsciiToHex(read) & 0xFFFFFF);
		if (read) free(read);
		colors.Text = (count & 0x00FF00) | ((count >> 0x10) & 0xFF) | ((count & 0xFF) << 0x10);

		sprintf(String,"%s.Sel", status);
		Settings_Read(RDS_NAME, "Rom Status", String, "FFFFFF", &read);
		count = (AsciiToHex(read) & 0xFFFFFF);
		if (read) free(read);
		if (count < 0) {
			colors.HighLight = COLOR_HIGHLIGHT + 1;
		} else {
			count = (count & 0x00FF00) | ((count >> 0x10) & 0xFF) | ((count & 0xFF) << 0x10);
			colors.HighLight = count;
		}		
		
		sprintf(String,"%s.Seltext", status);
		Settings_Read(RDS_NAME, "Rom Status", String, "FFFFFF", &read);
		count = (AsciiToHex(read) & 0xFFFFFF);
		if (read) free(read);
		colors.SelectedText = (count & 0x00FF00) | ((count >> 0x10) & 0xFF) | ((count & 0xFF) << 0x10);

		colors.status_name = (char *)malloc(strlen(status) + 1);
		strcpy(colors.status_name, status);
		AddToColorCache(colors);
	}
}

LRESULT RomList_FindItem(NMHDR* lParam) {
	NMLVFINDITEM* findInfo;
	int currentPos, col, startPos;

	findInfo = (NMLVFINDITEM*)lParam;

	// Search criteria is not supported, only works with strings for now
	if (((findInfo->lvfi.flags) & LVFI_STRING) == 0)
		return -1;

	// Fetch first column, thats what we are searching by
	for (col = 0; col < NoOfFields; col++) {
		if (RomBrowserFields[col].Pos == 0)
			break;
	}

	startPos = findInfo->iStart;

	// Either the last item was selected or nothing was, start at the top
	if (startPos >= ItemList.ListCount || startPos < 0)
		startPos = 0;

	currentPos = startPos;

	do {
		// String insensitive compare, limited to the size of our search criteria in findInfo->lvfi.psz
		// So this is a "String starts with, ignore case" match
		switch (col) {
		case RB_FileName:
			if(_tcsnicmp(ItemList.List[currentPos].FileName, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_InternalName:
			if(_tcsnicmp(ItemList.List[currentPos].InternalName, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_GoodName:
			if(_tcsnicmp(ItemList.List[currentPos].GoodName, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_Status:
			if(_tcsnicmp(ItemList.List[currentPos].Status, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_CoreNotes:
			if(_tcsnicmp(ItemList.List[currentPos].CoreNotes, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_PluginNotes:
			if(_tcsnicmp(ItemList.List[currentPos].PluginNotes, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_UserNotes:
			if(_tcsnicmp(ItemList.List[currentPos].UserNotes, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_Developer:
			if(_tcsnicmp(ItemList.List[currentPos].Developer, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_ReleaseDate:
			if(_tcsnicmp(ItemList.List[currentPos].ReleaseDate, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		case RB_Genre:
			if(_tcsnicmp(ItemList.List[currentPos].Genre, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
				return currentPos;
			break;
		default:
			return -1;	// Do not support, this was done because there is no uniqueness on these search results
		}

		// Start at the top if we've reached the bottom of the list
		if (currentPos == ItemList.ListCount - 1)
			currentPos = 0;
		else
			currentPos++;
	}
	while (currentPos != startPos);

	// Nothing found
	return -1;
}

DWORD WINAPI RefreshRomBrowserMT (LPVOID lpArgs) {
	char RomDirectory[MAX_PATH+1];

	if (scanning)
		return 0;

	scanning = TRUE;
	cancelled = FALSE;
	CreateThread(NULL, 0, UpdateBrowser, NULL, 0, NULL);

	Settings_GetDirectory(RomDir, RomDirectory, sizeof(RomDirectory));
	FillRomList (RomDirectory);
	scanning = FALSE;

	if (!cancelled) {
		RomList_SortList();
		ListView_SetItemCount(hRomList, ItemList.ListCount);
		RomList_SelectFind(LastRoms[0]);
		SaveRomList();
	}

	return 0;
}

// Thread to update the Rom list as it is being loaded
DWORD WINAPI UpdateBrowser (LPVOID lpArgs) {
	DWORD wait_result;
	int count = 0;
	
	while (scanning) {
		wait_result = WaitForSingleObject(gLVMutex, INFINITE);
		
		// Got control of the mutex, that means nothing else is using the listview control
		if (wait_result == WAIT_OBJECT_0) {
			RomList_SortList();
			count = ItemList.ListCount;
		}
		
		// Abandoned mutex, why???
		if (wait_result == WAIT_ABANDONED) {
			DisplayError(GS(MSG_MEM_ALLOC_ERROR));
			ExitThread(0);
		}

		if (!ReleaseMutex(gLVMutex))
			MessageBox(NULL, "Failed to release a mutex???", "Error!", MB_OK);

		// Be sure the mutex is released, as the displaying of items is also going to use the mutex
		ListView_SetItemCount(hRomList, count);
		Sleep(50);
	}
	return 0;
}

// Not worrying about size of arrays because both szFullFileName and LastRoms are 261 characters long
// Otherwise I would use a safer compare
void RomList_SelectFind(char* match) {
	int counter = 0;

	if (scanning)
		return;

	for (counter = 0; counter < ItemList.ListCount; counter++) {
		if (_stricmp(ItemList.List[counter].szFullFileName, match) == 0) {
			ListView_SetItemState(hRomList, counter, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_EnsureVisible(hRomList, counter, FALSE);
			break;
		}
	}
}

void RomList_UpdateSelectedInfo(void) {
	int selected;

	if (scanning)
		return;

	selected = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);

	if (selected != -1) {
		FillRomExtensionInfo(&ItemList.List[selected]);
		ListView_RedrawItems(hRomList, selected, selected);
	}
}

void RomList_StopScanning() {
	scanning = FALSE;
	cancelled = TRUE;
	WaitForSingleObject(romThread, INFINITE);
}