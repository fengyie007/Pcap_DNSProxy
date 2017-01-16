﻿// This code is part of Pcap_DNSProxy
// A local DNS server based on WinPcap and LibPcap
// Copyright (C) 2012-2016 Chengr28
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Main.h"

//The Main function of program
#if defined(PLATFORM_WIN)
int wmain(
	int argc, 
	wchar_t* argv[])
{
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
int main(
	int argc, 
	char *argv[])
{
#endif
//Get commands.
	if (argc < 1)
	{
		return EXIT_FAILURE;
	}
	else {
	//Read commands and configuration file, also launch all monitors.
		if (!ReadCommand(argc, argv))
			return EXIT_SUCCESS;
		else if (!ReadParameter(true))
			return EXIT_FAILURE;
		else 
			MonitorLauncher();

	//Wait for multiple threads to work.
		Sleep(STANDARD_TIMEOUT);
	}

//Main process initialization
#if defined(PLATFORM_WIN)
	const SERVICE_TABLE_ENTRYW ServiceTable[]{{SYSTEM_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain}, {nullptr, nullptr}}; //Service beginning
	if (StartServiceCtrlDispatcherW(ServiceTable) == 0)
	{
	//Print to screen.
		
		if (GetLastError() == 0)
		{
			std::wstring Message(L"[System Error] Service start error.\n");
			std::lock_guard<std::mutex> ScreenMutex(ScreenLock);
			PrintToScreen(false, Message.c_str());
			PrintToScreen(false, L"[Notice] Program will continue to run in console mode.\n");
			PrintToScreen(false, L"[Notice] Please ignore these error messages if you want to run in console mode.\n\n");
		}
		else {
			std::wstring Message(L"[System Error] Service start error");
			ErrorCodeToMessage(LOG_ERROR_TYPE::SYSTEM, GetLastError(), Message);
			Message.append(L".\n");
			std::lock_guard<std::mutex> ScreenMutex(ScreenLock);
			PrintToScreen(false, Message.c_str(), GetLastError());
			PrintToScreen(false, L"[Notice] Program will continue to run in console mode.\n");
			PrintToScreen(false, L"[Notice] Please ignore these error messages if you want to run in console mode.\n\n");
		}

	//Handle the system signal.
		if (SetConsoleCtrlHandler(
				(PHANDLER_ROUTINE)CtrlHandler, 
				TRUE) == 0)
		{
			PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::SYSTEM, L"Set console control handler error", GetLastError(), nullptr, 0);
			return EXIT_FAILURE;
		}

	//Main process
		if (!MonitorInit())
			return EXIT_FAILURE;
	}
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
//Set system signal handler to ignore EPIPE signal when transport with socket.
	errno = 0;
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::SYSTEM, L"Ignore system signal error", errno, nullptr, 0);
		return EXIT_FAILURE;
	}

//Main process initialization
	if (!MonitorInit())
		return EXIT_FAILURE;
#endif

	return EXIT_SUCCESS;
}
