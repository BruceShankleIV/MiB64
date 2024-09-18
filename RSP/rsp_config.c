/*
 * MiB64 - A Nintendo 64 emulator.
 *
 * Project64 (c) Copyright 2001 Zilmar, Jabo, Smiff, Gent, Witten
 * Projectg64 Legacy (c) Copyright 2010 PJ64LegacyTeam
 * MiB64 (c) Copyright 2024 MiB64Team
 *
 * MiB64 Homepage: www.mib64.net
 *
 * Permission to use, copy, modify and distribute MiB64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * MiB64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for MiB64 or software derived from MiB64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */

#include "../main.h"
#include "../Plugin.h"
#include "../resource.h"
#include "../Settings Common Defines.h"
#include "../X86.h"
#include "rsp_Cpu.h"
#include "RSP_breakpoint.h"
#include "RSP Command.h"
#include "RSP Recompiler CPU.h"
#include "rsp_registers.h"
#include "rsp_memory.h"

#include <windows.h>
#include <windowsx.h>

BOOL AudioHle = FALSE;
BOOL GraphicsHle = FALSE;
DWORD RspCPUCore = 0;
BOOL RspProfiling = FALSE;
BOOL IndividualRspBlock = FALSE;
BOOL RspShowErrors = FALSE;
HANDLE hRspConfigMutex = NULL;
HMENU hRSPMenu = NULL;

RSP_COMPILER RspCompiler;

BOOL GetBooleanCheck(HWND hDlg, DWORD DialogID) {
	return (IsDlgButtonChecked(hDlg, DialogID) == BST_CHECKED) ? TRUE : FALSE;
}

static BOOL CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HWND hWndItem;
	DWORD value;

	UNREFERENCED_PARAMETER(lParam);

	switch (uMsg) {
	case WM_INITDIALOG:
		if (AudioHle == TRUE) {
			CheckDlgButton(hDlg, IDC_AUDIOHLE, BST_CHECKED);
		}

		if (GraphicsHle == TRUE) {
			CheckDlgButton(hDlg, IDC_GRAPHICSHLE, BST_CHECKED);
		}

		hWndItem = GetDlgItem(hDlg, IDC_COMPILER_SELECT);
		ComboBox_AddString(hWndItem, "Interpreter");
		ComboBox_AddString(hWndItem, "Recompiler");
		ComboBox_SetCurSel(hWndItem, RspCPUCore);
		break;

	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, lParam)) {
		case IDOK:
			hWndItem = GetDlgItem(hDlg, IDC_COMPILER_SELECT);
			value = ComboBox_GetCurSel(hWndItem);
			SetRspCPU(value);

			AudioHle = GetBooleanCheck(hDlg, IDC_AUDIOHLE);
			GraphicsHle = GetBooleanCheck(hDlg, IDC_GRAPHICSHLE);

			Settings_Write(APPS_NAME, STR_RSP_SETTINGS, STR_AUDIO_HLE, AudioHle ? "True" : "False");
			Settings_Write(APPS_NAME, STR_RSP_SETTINGS, STR_GRAPHICS_HLE, GraphicsHle ? "True" : "False");

			char tmpBuf[3];
			sprintf(tmpBuf, "%2d", value);
			Settings_Write(APPS_NAME, STR_RSP_SETTINGS, STR_RSP_CORE, tmpBuf);

			EndDialog(hDlg, TRUE);
			break;
		case IDCANCEL:
			EndDialog(hDlg, TRUE);
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

static void LoadRspSettings() {
	AudioHle = Settings_ReadBool(APPS_NAME, STR_RSP_SETTINGS, STR_AUDIO_HLE, Default_AudioHLE);
	GraphicsHle = Settings_ReadBool(APPS_NAME, STR_RSP_SETTINGS, STR_GRAPHICS_HLE, Default_GraphicsHLE);
	RspCPUCore = Settings_ReadInt(APPS_NAME, STR_RSP_SETTINGS, STR_RSP_CORE, Default_RspCore);
}

void __cdecl rspConfig(HWND hWnd) {
	LoadRspSettings();

	DialogBox(hInst, "RSPCONFIG", hWnd, ConfigDlgProc);
}

void InitiateInternalRSP() {
	memset(&RspCompiler, 0, sizeof(RspCompiler));

	RspCompiler.bAlignGPR = FALSE;
	/*Compiler.bAlignVector = TRUE;
	Compiler.bFlags = TRUE;*/
	RspCompiler.bReOrdering = TRUE;
	RspCompiler.bSections = TRUE;
	RspCompiler.bDest = TRUE;
	RspCompiler.bAccum = TRUE;
	RspCompiler.bGPRConstants = TRUE;

	DetectCpuSpecs();
	RspCompiler.mmx = IsMMXSupported();
	RspCompiler.mmx2 = IsMMX2Supported();
	RspCompiler.sse = IsSSESupported();
	RspCompiler.sse2 = IsSSE2Supported();
	RspCompiler.sse41 = IsSSE41Supported();
	RspCompiler.avx = IsAVXSupported();
	RspCompiler.avx2 = IsAVX2Supported();
	hRspConfigMutex = CreateMutex(NULL, FALSE, NULL);

	LoadRspSettings();

	AllocateRspMemory();
	InitilizeRSPRegisters();
	Build_RSP();
#ifdef GenerateLog
	Start_Log();
#endif
}

static BOOL CALLBACK CompilerDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	extern BYTE* pLastRspSecondary;
	char Buffer[256];

	UNREFERENCED_PARAMETER(lParam);

	switch (uMsg) {
	case WM_INITDIALOG:
		if (RspCompiler.mmx == TRUE)
			CheckDlgButton(hDlg, IDC_CHECK_MMX, BST_CHECKED);
		if (RspCompiler.mmx2 == TRUE)
			CheckDlgButton(hDlg, IDC_CHECK_MMX2, BST_CHECKED);
		if (RspCompiler.sse == TRUE)
			CheckDlgButton(hDlg, IDC_CHECK_SSE, BST_CHECKED);
		if (RspCompiler.sse2 == TRUE)
			CheckDlgButton(hDlg, IDC_CHECK_SSE2, BST_CHECKED);
		if (RspCompiler.sse41 == TRUE)
			CheckDlgButton(hDlg, IDC_CHECK_SSE41, BST_CHECKED);
		if (RspCompiler.avx == TRUE)
			CheckDlgButton(hDlg, IDC_CHECK_AVX, BST_CHECKED);
		if (RspCompiler.avx2 == TRUE)
			CheckDlgButton(hDlg, IDC_CHECK_AVX2, BST_CHECKED);

		if (RspCompiler.bAlignGPR == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_ALIGNGPR, BST_CHECKED);
		/*if (Compiler.bAlignVector == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_ALIGNVEC, BST_CHECKED);*/

		if (RspCompiler.bSections == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_SECTIONS, BST_CHECKED);
		if (RspCompiler.bGPRConstants == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_GPRCONSTANTS, BST_CHECKED);
		if (RspCompiler.bReOrdering == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_REORDER, BST_CHECKED);
		/*if (Compiler.bFlags == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_FLAGS, BST_CHECKED);*/
		if (RspCompiler.bAccum == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_ACCUM, BST_CHECKED);
		if (RspCompiler.bDest == TRUE)
			CheckDlgButton(hDlg, IDC_COMPILER_DEST, BST_CHECKED);

		SetTimer(hDlg, 1, 250, NULL);
		break;

	case WM_TIMER:
		sprintf(Buffer, "x86: %2.2f KB / %2.2f KB", (float)(RspRecompPos - RspRecompCode) / 1024.0F,
			pLastRspSecondary ? (float)((pLastRspSecondary - RspRecompCodeSecondary) / 1024.0F) : 0.0F);

		SetDlgItemText(hDlg, IDC_COMPILER_BUFFERS, Buffer);
		break;

	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, lParam)) {
		case IDOK:
			RspCompiler.mmx = GetBooleanCheck(hDlg, IDC_CHECK_MMX);
			RspCompiler.mmx2 = GetBooleanCheck(hDlg, IDC_CHECK_MMX2);
			RspCompiler.sse = GetBooleanCheck(hDlg, IDC_CHECK_SSE);
			RspCompiler.sse2 = GetBooleanCheck(hDlg, IDC_CHECK_SSE2);
			RspCompiler.sse41 = GetBooleanCheck(hDlg, IDC_CHECK_SSE41);
			RspCompiler.avx = GetBooleanCheck(hDlg, IDC_CHECK_AVX);
			RspCompiler.avx2 = GetBooleanCheck(hDlg, IDC_CHECK_AVX2);
			RspCompiler.bSections = GetBooleanCheck(hDlg, IDC_COMPILER_SECTIONS);
			RspCompiler.bReOrdering = GetBooleanCheck(hDlg, IDC_COMPILER_REORDER);
			RspCompiler.bGPRConstants = GetBooleanCheck(hDlg, IDC_COMPILER_GPRCONSTANTS);
			/*Compiler.bFlags = GetBooleanCheck(hDlg, IDC_COMPILER_FLAGS);*/
			RspCompiler.bAccum = GetBooleanCheck(hDlg, IDC_COMPILER_ACCUM);
			RspCompiler.bDest = GetBooleanCheck(hDlg, IDC_COMPILER_DEST);
			RspCompiler.bAlignGPR = GetBooleanCheck(hDlg, IDC_COMPILER_ALIGNGPR);
			/*Compiler.bAlignVector = GetBooleanCheck(hDlg, IDC_COMPILER_ALIGNVEC);*/
			KillTimer(hDlg, 1);
			EndDialog(hDlg, TRUE);
			break;

		case IDCANCEL:
			KillTimer(hDlg, 1);
			EndDialog(hDlg, TRUE);
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

void __cdecl ProcessMenuItem(int ID) {
	/*DWORD Disposition;
	HKEY hKeyResults;
	char String[200];
	long lResult;*/
	UINT uState;

	switch (ID) {
	case ID_RSPCOMMANDS: Enter_RSP_Commands_Window(); break;
	case ID_RSPREGISTERS: Enter_RSP_Register_Window(); break;
	case ID_DUMP_RSPCODE: DumpRSPCode(); break;
	case ID_DUMP_DMEM: DumpRSPData(); break;
	/*case ID_PROFILING_ON:
	case ID_PROFILING_OFF:
		uState = GetMenuState(hRSPMenu, ID_PROFILING_ON, MF_BYCOMMAND);
		hKeyResults = 0;
		Disposition = 0;

		if (uState & MFS_CHECKED) {
			CheckMenuItem(hRSPMenu, ID_PROFILING_ON, MF_BYCOMMAND | MFS_UNCHECKED);
			CheckMenuItem(hRSPMenu, ID_PROFILING_OFF, MF_BYCOMMAND | MFS_CHECKED);
			GenerateTimerResults();
			Profiling = FALSE;
		}
		else {
			CheckMenuItem(hRSPMenu, ID_PROFILING_ON, MF_BYCOMMAND | MFS_CHECKED);
			CheckMenuItem(hRSPMenu, ID_PROFILING_OFF, MF_BYCOMMAND | MFS_UNCHECKED);
			ResetTimerList();
			Profiling = TRUE;
		}

		sprintf(String, "Software\\N64 Emulation\\DLL\\%s", AppName);
		lResult = RegCreateKeyEx(HKEY_CURRENT_USER, String, 0, "",
			REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyResults, &Disposition);

		if (lResult == ERROR_SUCCESS) {
			RegSetValueEx(hKeyResults, "Profiling On", 0, REG_DWORD, (BYTE*)&Profiling, sizeof(DWORD));
		}
		RegCloseKey(hKeyResults);
		break;
	case ID_PROFILING_RESETSTATS: ResetTimerList(); break;
	case ID_PROFILING_GENERATELOG: GenerateTimerResults(); break;
	case ID_PROFILING_LOGINDIVIDUALBLOCKS:
		uState = GetMenuState(hRSPMenu, ID_PROFILING_LOGINDIVIDUALBLOCKS, MF_BYCOMMAND);
		hKeyResults = 0;
		Disposition = 0;

		ResetTimerList();
		if (uState & MFS_CHECKED) {
			CheckMenuItem(hRSPMenu, ID_PROFILING_LOGINDIVIDUALBLOCKS, MF_BYCOMMAND | MFS_UNCHECKED);
			IndvidualBlock = FALSE;
		}
		else {
			CheckMenuItem(hRSPMenu, ID_PROFILING_LOGINDIVIDUALBLOCKS, MF_BYCOMMAND | MFS_CHECKED);
			IndvidualBlock = TRUE;
		}

		sprintf(String, "Software\\N64 Emulation\\DLL\\%s", AppName);
		lResult = RegCreateKeyEx(HKEY_CURRENT_USER, String, 0, "",
			REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyResults, &Disposition);

		if (lResult == ERROR_SUCCESS) {
			RegSetValueEx(hKeyResults, "Log Indvidual Blocks", 0, REG_DWORD,
				(BYTE*)&IndvidualBlock, sizeof(DWORD));
		}
		RegCloseKey(hKeyResults);
		break;*/
	case ID_SHOWCOMPILERERRORS:
		uState = GetMenuState(hRSPMenu, ID_SHOWCOMPILERERRORS, MF_BYCOMMAND);

		if (uState & MFS_CHECKED) {
			CheckMenuItem(hRSPMenu, ID_SHOWCOMPILERERRORS, MF_BYCOMMAND | MFS_UNCHECKED);
			RspShowErrors = FALSE;
		}
		else {
			CheckMenuItem(hRSPMenu, ID_SHOWCOMPILERERRORS, MF_BYCOMMAND | MFS_CHECKED);
			RspShowErrors = TRUE;
		}

		Settings_Write(APPS_NAME, STR_RSP_SETTINGS, STR_RSP_SHOW_ERRORS, RspShowErrors ? "True" : "False");
		break;
	case ID_COMPILER:
		DialogBox(hInst, "RSPCOMPILER", HWND_DESKTOP, CompilerDlgProc);
		break;
	}
}

void GetInternalRspDebugInfo(RSPDEBUG_INFO* DebugInfo) {
	hRSPMenu = LoadMenu(hInst, "RspMenu");
	DebugInfo->hRSPMenu = hRSPMenu;
	DebugInfo->ProcessMenuItem = ProcessMenuItem;

	DebugInfo->UseBPoints = TRUE;
	sprintf(DebugInfo->BPPanelName, " RSP ");
	DebugInfo->Add_BPoint = Add_TextFieldRspBPoint;
	DebugInfo->CreateBPPanel = CreateRspBPPanel;
	DebugInfo->HideBPPanel = HideRspBPPanel;
	DebugInfo->PaintBPPanel = PaintRspBPPanel;
	DebugInfo->RefreshBpoints = RefreshRspBpoints;
	DebugInfo->RemoveAllBpoint = RemoveAllRspBpoint;
	DebugInfo->RemoveBpoint = RemoveRspBpoint;
	DebugInfo->ShowBPPanel = ShowRspBPPanel;

	DebugInfo->Enter_RSP_Commands_Window = Enter_RSP_Commands_Window;

	RspProfiling = Settings_ReadBool(APPS_NAME, STR_RSP_SETTINGS, STR_RSP_PROFILING, Default_RspProfilingOn);
	IndividualRspBlock = Settings_ReadBool(APPS_NAME, STR_RSP_SETTINGS, STR_INDIVIDUAL_RSP_BLOCK, Default_RspIndividualBlock);
	RspShowErrors = Settings_ReadBool(APPS_NAME, STR_RSP_SETTINGS, STR_RSP_SHOW_ERRORS, Default_RspShowErrors);

	if (RspProfiling) {
		CheckMenuItem(hRSPMenu, ID_PROFILING_ON, MF_BYCOMMAND | MFS_CHECKED);
	}
	else {
		CheckMenuItem(hRSPMenu, ID_PROFILING_OFF, MF_BYCOMMAND | MFS_CHECKED);
	}
	if (IndividualRspBlock) {
		CheckMenuItem(hRSPMenu, ID_PROFILING_LOGINDIVIDUALBLOCKS, MF_BYCOMMAND | MFS_CHECKED);
	}
	if (RspShowErrors) {
		CheckMenuItem(hRSPMenu, ID_SHOWCOMPILERERRORS, MF_BYCOMMAND | MFS_CHECKED);
	}
}
