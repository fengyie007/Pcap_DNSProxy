﻿// This code is part of Pcap_DNSProxy
// Pcap_DNSProxy, a local DNS server based on WinPcap and LibPcap
// Copyright (C) 2012-2017 Chengr28
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


#include "Monitor.h"

//Monitor launcher process
void MonitorLauncher(
	void)
{
//Network monitor(Mark Local DNS address to PTR records)
	ParameterModificating.SetToMonitorItem();
	std::thread NetworkInformationMonitorThread(std::bind(NetworkInformationMonitor));
	NetworkInformationMonitorThread.detach();

//DNSCurve initialization(Encryption mode)
#if defined(ENABLE_LIBSODIUM)
	if (Parameter.IsDNSCurve)
	{
		DNSCurveParameterModificating.SetToMonitorItem();
		if (DNSCurveParameter.IsEncryption)
			DNSCurveInit();
	}
#endif

//Read parameter(Monitor mode)
	if (!GlobalRunningStatus.FileList_IPFilter->empty())
	{
		std::thread ReadParameterThread(std::bind(ReadParameter, false));
		ReadParameterThread.detach();
	}

//Read Hosts monitor
	if (!GlobalRunningStatus.FileList_Hosts->empty())
	{
		std::thread ReadHostsThread(std::bind(ReadHosts));
		ReadHostsThread.detach();
	}

//Read IPFilter monitor
	if (Parameter.OperationMode == LISTEN_MODE::CUSTOM || Parameter.DataCheck_Blacklist || Parameter.IsLocalRouting)
	{
		std::thread ReadIPFilterThread(std::bind(ReadIPFilter));
		ReadIPFilterThread.detach();
	}

//Capture monitor
#if defined(ENABLE_PCAP)
	if (Parameter.IsPcapCapture && 
	//Direct Request mode
		!(Parameter.DirectRequest == REQUEST_MODE_DIRECT::BOTH || 
		(Parameter.DirectRequest == REQUEST_MODE_DIRECT::IPV6 && Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0 && 
		Parameter.DirectRequest == REQUEST_MODE_DIRECT::IPV4 && Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0)) && 
	//SOCKS request only mode
		!(Parameter.SOCKS_Proxy && Parameter.SOCKS_Only) && 
	//HTTP CONNECT request only mode
		!(Parameter.HTTP_CONNECT_Proxy && Parameter.HTTP_CONNECT_Only)
	//DNSCurve request only mode
	#if defined(ENABLE_LIBSODIUM)
		&& !(Parameter.IsDNSCurve && DNSCurveParameter.IsEncryptionOnly)
	#endif
		)
	{
	#if defined(ENABLE_PCAP)
		std::thread CaptureInitializationThread(std::bind(CaptureInit));
		CaptureInitializationThread.detach();
	#endif

	//Get Hop Limits with normal DNS request(IPv6).
		if (Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV6 || //IPv6
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV4 && Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0))) //Non-IPv4
		{
			std::thread IPv6TestDoaminThread(std::bind(DomainTestRequest, AF_INET6));
			IPv6TestDoaminThread.detach();
		}

	//Get TTL with normal DNS request(IPv4).
		if (Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV4 || //IPv4
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV6 && Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0))) //Non-IPv6
		{
			std::thread IPv4TestDoaminThread(std::bind(DomainTestRequest, AF_INET));
			IPv4TestDoaminThread.detach();
		}

	//Get Hop Limits with ICMPv6 echo(IPv6).
		if (Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV6 || //IPv6
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV4 && Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0))) //Non-IPv4
		{
			std::thread ICMPv6Thread(std::bind(ICMP_TestRequest, AF_INET6));
			ICMPv6Thread.detach();
		}

	//Get TTL with ICMP echo(IPv4).
		if (Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV4 || //IPv4
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV6 && Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0))) //Non-IPv6
		{
			std::thread ICMP_Thread(std::bind(ICMP_TestRequest, AF_INET));
			ICMP_Thread.detach();
		}
	}
#endif

//Alternate server monitor(Set Preferred DNS servers switcher)
	if ((!Parameter.AlternateMultipleRequest && 
		(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 || 
			Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0
	#if defined(ENABLE_LIBSODIUM)
		|| DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0
		|| DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0
	#endif
		)) || Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family != 0 || 
		Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family != 0)
	{
		std::thread AlternateServerMonitorThread(std::bind(AlternateServerMonitor));
		AlternateServerMonitorThread.detach();
	}

//MailSlot and FIFO monitor
#if defined(PLATFORM_WIN)
	std::thread Flush_DNS_MailSlotMonitorThread(std::bind(Flush_DNS_MailSlotMonitor));
	Flush_DNS_MailSlotMonitorThread.detach();
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	std::thread Flush_DNS_FIFO_MonitorThread(std::bind(Flush_DNS_FIFO_Monitor));
	Flush_DNS_FIFO_MonitorThread.detach();
#endif

	return;
}

//Local DNS server initialization
bool MonitorInit(
	void)
{
//Initialization
	std::vector<std::thread> MonitorThread((Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM);
	SOCKET_DATA LocalSocketData;
	memset(&LocalSocketData, 0, sizeof(LocalSocketData));
	size_t MonitorThreadIndex = 0;
	auto ReturnValue = true, * const Result = &ReturnValue;

//Set local machine Monitor sockets(IPv6/UDP).
	if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV6)
	{
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::UDP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET6;
				LocalSocketData.AddrLen = sizeof(sockaddr_in6);

			//Listen Address available(IPv6)
				if (Parameter.ListenAddress_IPv6 != nullptr)
				{
					for (const auto &ListenAddressIter:*Parameter.ListenAddress_IPv6)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_addr = (reinterpret_cast<const sockaddr_in6 *>(&ListenAddressIter))->sin6_addr;
						(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_port = (reinterpret_cast<const sockaddr_in6 *>(&ListenAddressIter))->sin6_port;

					//Add to global list.
						if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
						{
							std::thread MonitorThreadTemp(std::bind(UDP_Monitor, LocalSocketData, Result));
							std::swap(MonitorThread.at(MonitorThreadIndex), MonitorThreadTemp);
							++MonitorThreadIndex;
							LocalSocketData.Socket = 0;
						}
						else {
							MonitorThread.clear();
							return false;
						}
					}
				}
				else {
				//Normal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_addr = in6addr_loopback;
					//Server Mode, Priavte Mode and Custom Mode
						else 
							(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_addr = in6addr_any;

					//Set monitor port.
						for (const auto &ListenPortIter:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}
							}

							(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_port = ListenPortIter;

						//Add to global list.
							if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
							{
								std::thread MonitorThreadTemp(std::bind(UDP_Monitor, LocalSocketData, Result));
								std::swap(MonitorThread.at(MonitorThreadIndex), MonitorThreadTemp);
								++MonitorThreadIndex;
								LocalSocketData.Socket = 0;
							}
							else {
								MonitorThread.clear();
								return false;
							}
						}
					}
				}
			}
		}

	//Set local machine Monitor sockets(IPv6/TCP).
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::TCP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET6;
				LocalSocketData.AddrLen = sizeof(sockaddr_in6);

			//Listen Address available(IPv6)
				if (Parameter.ListenAddress_IPv6 != nullptr)
				{
					for (const auto &ListenAddressIter:*Parameter.ListenAddress_IPv6)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_addr = (reinterpret_cast<const sockaddr_in6 *>(&ListenAddressIter))->sin6_addr;
						(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_port = (reinterpret_cast<const sockaddr_in6 *>(&ListenAddressIter))->sin6_port;

					//Add to global list.
						if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
						{
							std::thread MonitorThreadTemp(std::bind(TCP_Monitor, LocalSocketData, Result));
							std::swap(MonitorThread.at(MonitorThreadIndex), MonitorThreadTemp);
							++MonitorThreadIndex;
							LocalSocketData.Socket = 0;
						}
						else {
							MonitorThread.clear();
							return false;
						}
					}
				}
				else {
				//Mormal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_addr = in6addr_loopback;
					//Server Mode, Priavte Mode and Custom Mode
						else 
							(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_addr = in6addr_any;

					//Set monitor port.
						for (const auto &ListenPortIter:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}
							}

							(reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr))->sin6_port = ListenPortIter;

						//Add to global list.
							if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
							{
								std::thread MonitorThreadTemp(std::bind(TCP_Monitor, LocalSocketData, Result));
								std::swap(MonitorThread.at(MonitorThreadIndex), MonitorThreadTemp);
								++MonitorThreadIndex;
								LocalSocketData.Socket = 0;
							}
							else {
								MonitorThread.clear();
								return false;
							}
						}
					}
				}
			}
		}
	}

//Set local machine Monitor sockets(IPv4/UDP).
	if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV4)
	{
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::UDP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET;
				LocalSocketData.AddrLen = sizeof(sockaddr_in);

			//Listen Address available(IPv4)
				if (Parameter.ListenAddress_IPv4 != nullptr)
				{
					for (const auto &ListenAddressIter:*Parameter.ListenAddress_IPv4)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_addr = (reinterpret_cast<const sockaddr_in *>(&ListenAddressIter))->sin_addr;
						(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_port = (reinterpret_cast<const sockaddr_in *>(&ListenAddressIter))->sin_port;

					//Add to global list.
						if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
						{
							std::thread MonitorThreadTemp(std::bind(UDP_Monitor, LocalSocketData, Result));
							std::swap(MonitorThread.at(MonitorThreadIndex), MonitorThreadTemp);
							++MonitorThreadIndex;
							LocalSocketData.Socket = 0;
						}
						else {
							MonitorThread.clear();
							return false;
						}
					}
				}
				else {
				//Normal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
					//Server Mode, Priavte Mode and Custom Mode
						else 
							(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_addr.s_addr = INADDR_ANY;

					//Set monitor port.
						for (const auto &ListenPortIter:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}
							}

							(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_port = ListenPortIter;

						//Add to global list.
							if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
							{
								std::thread MonitorThreadTemp(std::bind(UDP_Monitor, LocalSocketData, Result));
								std::swap(MonitorThread.at(MonitorThreadIndex), MonitorThreadTemp);
								++MonitorThreadIndex;
								LocalSocketData.Socket = 0;
							}
							else {
								MonitorThread.clear();
								return false;
							}
						}
					}
				}
			}
		}

	//Set local machine Monitor sockets(IPv4/TCP).
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::TCP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET;
				LocalSocketData.AddrLen = sizeof(sockaddr_in);

			//Listen Address available(IPv4)
				if (Parameter.ListenAddress_IPv4 != nullptr)
				{
					for (const auto &ListenAddressIter:*Parameter.ListenAddress_IPv4)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_addr = (reinterpret_cast<const sockaddr_in *>(&ListenAddressIter))->sin_addr;
						(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_port = (reinterpret_cast<const sockaddr_in *>(&ListenAddressIter))->sin_port;

					//Add to global list.
						if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
						{
							std::thread MonitorThreadTemp(std::bind(TCP_Monitor, LocalSocketData, Result));
							std::swap(MonitorThread.at(MonitorThreadIndex), MonitorThreadTemp);
							++MonitorThreadIndex;
							LocalSocketData.Socket = 0;
						}
						else {
							MonitorThread.clear();
							return false;
						}
					}
				}
				else {
				//Normal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
					//Server Mode, Priavte Mode and Custom Mode
						else 
							(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_addr.s_addr = INADDR_ANY;

					//Set monitor port.
						for (const auto &ListenPortIter:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, true, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}

								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}

							(reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr))->sin_port = ListenPortIter;

						//Add to global list.
							if (MonitorThreadIndex < (Parameter.ListenPort->size() + 1U) * TRANSPORT_LAYER_PARTNUM)
							{
								std::thread InnerMonitorThreadTemp(std::bind(TCP_Monitor, LocalSocketData, Result));
								std::swap(MonitorThread.at(MonitorThreadIndex), InnerMonitorThreadTemp);
								++MonitorThreadIndex;
								LocalSocketData.Socket = 0;
							}
							else {
								MonitorThread.clear();
								return false;
							}
						}
					}
				}
			}
		}
	}

	memset(&LocalSocketData, 0, sizeof(LocalSocketData));

//Start monitor request consumer threads.
	if (Parameter.ThreadPoolBaseNum > 0)
	{
		for (MonitorThreadIndex = 0;MonitorThreadIndex < Parameter.ThreadPoolBaseNum;++MonitorThreadIndex)
		{
		//Start monitor consumer thread.
			std::thread MonitorConsumerThread(std::bind(MonitorRequestConsumer));
			MonitorConsumerThread.detach();
		}

		*GlobalRunningStatus.ThreadRunningNum += Parameter.ThreadPoolBaseNum;
		*GlobalRunningStatus.ThreadRunningFreeNum += Parameter.ThreadPoolBaseNum;
		MonitorThreadIndex = 0;
	}

//Join threads.
	for (auto &ThreadIter:MonitorThread)
	{
		if (ThreadIter.joinable())
			ThreadIter.join();
	}

//Wait a moment to close all thread handles.
#if defined(PLATFORM_WIN)
	if (!*Result)
		Sleep(SHORTEST_FILEREFRESH_TIME * SECOND_TO_MILLISECOND);
#endif

	return true;
}

//Local DNS server with UDP protocol
bool UDP_Monitor(
	const SOCKET_DATA LocalSocketData, 
	bool * const Result)
{
//Socket attribute settings
	if (
	#if defined(PLATFORM_WIN)
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::UDP_BLOCK_RESET, true, nullptr) || 
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr) || 
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		(LocalSocketData.SockAddr.ss_family == AF_INET6 && !SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr)) || 
	#endif
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr))
	{
		*Result = false;
		return false;
	}

//Bind socket to port.
	if (bind(LocalSocketData.Socket, reinterpret_cast<const sockaddr *>(&LocalSocketData.SockAddr), LocalSocketData.AddrLen) == SOCKET_ERROR)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"Bind UDP Monitor socket error", WSAGetLastError(), nullptr, 0);
		SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		*Result = false;

		return false;
	}

//Initialization
	std::unique_ptr<uint8_t[]> RecvBuffer(new uint8_t[PACKET_MAXSIZE * Parameter.ThreadPoolMaxNum]());
	std::unique_ptr<uint8_t[]> SendBuffer(new uint8_t[PACKET_MAXSIZE]());
	memset(RecvBuffer.get(), 0, PACKET_MAXSIZE * Parameter.ThreadPoolMaxNum);
	memset(SendBuffer.get(), 0, PACKET_MAXSIZE);
	MONITOR_QUEUE_DATA MonitorQueryData;
	fd_set ReadFDS;
	memset(&ReadFDS, 0, sizeof(ReadFDS));
	MonitorQueryData.first.BufferSize = PACKET_MAXSIZE;
	MonitorQueryData.first.Protocol = IPPROTO_UDP;
	const auto SocketDataPointer = &MonitorQueryData.second;
	uint64_t LastMarkTime = 0, NowTime = 0;
	if (Parameter.QueueResetTime > 0)
		LastMarkTime = GetCurrentSystemTime();
	ssize_t RecvLen = 0;
	size_t Index = 0;

//Listening module
	for (;;)
	{
	//Interval time between receive
		if (Parameter.QueueResetTime > 0 && Index + 1U == Parameter.ThreadPoolMaxNum)
		{
			NowTime = GetCurrentSystemTime();
			if (LastMarkTime + Parameter.QueueResetTime > NowTime)
				Sleep(LastMarkTime + Parameter.QueueResetTime - NowTime);

			LastMarkTime = GetCurrentSystemTime();
		}

	//Reset parameters.
		memset(RecvBuffer.get() + PACKET_MAXSIZE * Index, 0, PACKET_MAXSIZE);
		memset(SendBuffer.get(), 0, PACKET_MAXSIZE);
		MonitorQueryData.second = LocalSocketData;
		FD_ZERO(&ReadFDS);
		FD_SET(MonitorQueryData.second.Socket, &ReadFDS);

	//Wait for system calling.
	#if defined(PLATFORM_WIN)
		ssize_t SelectResult = select(0, &ReadFDS, nullptr, nullptr, nullptr);
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		ssize_t SelectResult = select(MonitorQueryData.second.Socket + 1U, &ReadFDS, nullptr, nullptr, nullptr);
	#endif
		if (SelectResult > 0)
		{
			if (FD_ISSET(MonitorQueryData.second.Socket, &ReadFDS))
			{
			//Receive response and check DNS query data.
				RecvLen = recvfrom(MonitorQueryData.second.Socket, reinterpret_cast<char *>(RecvBuffer.get() + PACKET_MAXSIZE * Index), PACKET_MAXSIZE, 0, reinterpret_cast<sockaddr *>(&SocketDataPointer->SockAddr), reinterpret_cast<socklen_t *>(&SocketDataPointer->AddrLen));
				if (RecvLen < static_cast<ssize_t>(DNS_PACKET_MINSIZE))
				{
					continue;
				}
				else {
					MonitorQueryData.first.Buffer = RecvBuffer.get() + PACKET_MAXSIZE * Index;
					MonitorQueryData.first.Length = RecvLen;
					MonitorQueryData.first.IsLocalRequest = false;
					MonitorQueryData.first.IsLocalForce = false;
					memset(&MonitorQueryData.first.LocalTarget, 0, sizeof(MonitorQueryData.first.LocalTarget));

				//Check DNS query data.
					if (!CheckQueryData(&MonitorQueryData.first, SendBuffer.get(), PACKET_MAXSIZE, MonitorQueryData.second))
						continue;
				}

			//Request process
				if (Parameter.ThreadPoolBaseNum > 0) //Thread pool mode
				{
					MonitorRequestProvider(MonitorQueryData);
				}
				else { //New thread mode
					std::thread RequestProcessThread(std::bind(EnterRequestProcess, MonitorQueryData, nullptr, 0));
					RequestProcessThread.detach();
				}

				Index = (Index + 1U) % Parameter.ThreadPoolMaxNum;
			}
		}
	//Timeout
		else if (SelectResult == 0)
		{
			continue;
		}
	//SOCKET_ERROR
		else {
		//Block error messages when monitor is terminated.
		#if defined(PLATFORM_WIN)
			if (WSAGetLastError() != WSAENOTSOCK)
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			if (errno != WSAENOTSOCK && errno != EBADF)
		#endif
				PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"UDP Monitor socket initialization error", WSAGetLastError(), nullptr, 0);

		//Refresh interval
			Sleep(Parameter.FileRefreshTime);
		}
	}

//Monitor terminated
	SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"UDP listening module Monitor terminated", 0, nullptr, 0);
	return true;
}

//Local DNS server with TCP protocol
bool TCP_Monitor(
	const SOCKET_DATA LocalSocketData, 
	bool * const Result)
{
//Socket attribute settings
	if (
	#if defined(PLATFORM_WIN)
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr) || 
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		(LocalSocketData.SockAddr.ss_family == AF_INET6 && !SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr)) || 
	#endif
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::TCP_FAST_OPEN, true, nullptr) || 
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr))
	{
		*Result = false;
		return false;
	}

//Bind socket to port.
	if (bind(LocalSocketData.Socket, reinterpret_cast<const sockaddr *>(&LocalSocketData.SockAddr), LocalSocketData.AddrLen) == SOCKET_ERROR)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"Bind TCP Monitor socket error", WSAGetLastError(), nullptr, 0);
		SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		*Result = false;

		return false;
	}

//Listen request from socket.
	if (listen(LocalSocketData.Socket, SOMAXCONN) == SOCKET_ERROR)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"TCP Monitor socket listening initialization error", WSAGetLastError(), nullptr, 0);
		SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		*Result = false;

		return false;
	}

//Initialization
	MONITOR_QUEUE_DATA MonitorQueryData;
	fd_set ReadFDS;
	memset(&ReadFDS, 0, sizeof(ReadFDS));
	MonitorQueryData.first.BufferSize = Parameter.LargeBufferSize;
	MonitorQueryData.first.Protocol = IPPROTO_TCP;
	const auto SocketDataPointer = &MonitorQueryData.second;
	uint64_t LastMarkTime = 0, NowTime = 0;
	if (Parameter.QueueResetTime > 0)
		LastMarkTime = GetCurrentSystemTime();
	size_t Index = 0;

//Start Monitor.
	for (;;)
	{
	//Interval time between receive
		if (Parameter.QueueResetTime > 0 && Index + 1U == Parameter.ThreadPoolMaxNum)
		{
			NowTime = GetCurrentSystemTime();
			if (LastMarkTime + Parameter.QueueResetTime > NowTime)
				Sleep(LastMarkTime + Parameter.QueueResetTime - NowTime);

			LastMarkTime = GetCurrentSystemTime();
		}

	//Reset parameters.
		memset(&MonitorQueryData.second.SockAddr, 0, sizeof(MonitorQueryData.second.SockAddr));
		MonitorQueryData.second.AddrLen = LocalSocketData.AddrLen;
		MonitorQueryData.second.SockAddr.ss_family = LocalSocketData.SockAddr.ss_family;
		FD_ZERO(&ReadFDS);
		FD_SET(LocalSocketData.Socket, &ReadFDS);

	//Wait for system calling.
	#if defined(PLATFORM_WIN)
		ssize_t SelectResult = select(0, &ReadFDS, nullptr, nullptr, nullptr);
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		ssize_t SelectResult = select(LocalSocketData.Socket + 1U, &ReadFDS, nullptr, nullptr, nullptr);
	#endif
		if (SelectResult > 0)
		{
			if (FD_ISSET(LocalSocketData.Socket, &ReadFDS))
			{
			//Accept connection.
				MonitorQueryData.second.Socket = accept(LocalSocketData.Socket, reinterpret_cast<sockaddr *>(&SocketDataPointer->SockAddr), &SocketDataPointer->AddrLen);
				if (!SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
				{
					continue;
				}
			//Check request address.
				else if (!CheckQueryData(nullptr, nullptr, 0, MonitorQueryData.second))
				{
					SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
					MonitorQueryData.second.Socket = 0;

					continue;
				}

			//Accept process.
				if (Parameter.ThreadPoolBaseNum > 0) //Thread pool mode
				{
					MonitorRequestProvider(MonitorQueryData);
				}
				else { //New thread mode
					std::thread TCP_ReceiveThread(std::bind(TCP_ReceiveProcess, MonitorQueryData, nullptr, 0));
					TCP_ReceiveThread.detach();
				}

				Index = (Index + 1U) % Parameter.ThreadPoolMaxNum;
			}
		}
	//Timeout
		else if (SelectResult == 0)
		{
			continue;
		}
	//SOCKET_ERROR
		else {
		//Block error messages when monitor is terminated.
		#if defined(PLATFORM_WIN)
			if (WSAGetLastError() != WSAENOTSOCK)
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			if (errno != WSAENOTSOCK && errno != EBADF)
		#endif
				PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP Monitor socket initialization error", WSAGetLastError(), nullptr, 0);

		//Refresh interval
			Sleep(Parameter.FileRefreshTime);
		}
	}

//Monitor terminated
	SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"TCP listening module Monitor terminated", 0, nullptr, 0);
	return true;
}

//TCP Monitor receive process
bool TCP_ReceiveProcess(
	MONITOR_QUEUE_DATA MonitorQueryData, 
	uint8_t * const OriginalRecv, 
	size_t RecvSize)
{
//Initialization(Part 1)
	std::unique_ptr<uint8_t[]> RecvBuffer(new uint8_t[Parameter.LargeBufferSize]());
	memset(RecvBuffer.get(), 0, Parameter.LargeBufferSize);
	fd_set ReadFDS;
	timeval Timeout;
	memset(&ReadFDS, 0, sizeof(ReadFDS));
	memset(&Timeout, 0, sizeof(Timeout));
	ssize_t RecvLen = 0;

//Receive process
#if defined(PLATFORM_WIN)
	Timeout.tv_sec = Parameter.SocketTimeout_Reliable_Once / SECOND_TO_MILLISECOND;
	Timeout.tv_usec = Parameter.SocketTimeout_Reliable_Once % SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND;
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	Timeout = Parameter.SocketTimeout_Reliable_Once;
#endif
	FD_ZERO(&ReadFDS);
	FD_SET(MonitorQueryData.second.Socket, &ReadFDS);

#if defined(PLATFORM_WIN)
	RecvLen = select(0, &ReadFDS, nullptr, nullptr, &Timeout);
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	RecvLen = select(MonitorQueryData.second.Socket + 1U, &ReadFDS, nullptr, nullptr, &Timeout);
#endif
	if (RecvLen > 0 && FD_ISSET(MonitorQueryData.second.Socket, &ReadFDS))
	{
		RecvLen = recv(MonitorQueryData.second.Socket, reinterpret_cast<char *>(RecvBuffer.get()), static_cast<int>(Parameter.LargeBufferSize), 0);
	}
//Timeout or SOCKET_ERROR
	else {
		SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}

//Connection closed or SOCKET_ERROR
	size_t Length = 0;
	if (RecvLen <= 0)
	{
		SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}
	else if (RecvLen < static_cast<ssize_t>(DNS_PACKET_MINSIZE))
	{
		Length = RecvLen;

	//Socket selecting structure initialization
		memset(&ReadFDS, 0, sizeof(ReadFDS));
		memset(&Timeout, 0, sizeof(Timeout));
	#if defined(PLATFORM_WIN)
		Timeout.tv_sec = Parameter.SocketTimeout_Reliable_Once / SECOND_TO_MILLISECOND;
		Timeout.tv_usec = Parameter.SocketTimeout_Reliable_Once % SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND;
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		Timeout = Parameter.SocketTimeout_Reliable_Once;
	#endif
		FD_ZERO(&ReadFDS);
		FD_SET(MonitorQueryData.second.Socket, &ReadFDS);

	//Wait for system calling.
	#if defined(PLATFORM_WIN)
		RecvLen = select(0, &ReadFDS, nullptr, nullptr, &Timeout);
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		RecvLen = select(MonitorQueryData.second.Socket + 1U, &ReadFDS, nullptr, nullptr, &Timeout);
	#endif
		if (RecvLen > 0 && FD_ISSET(MonitorQueryData.second.Socket, &ReadFDS))
		{
			RecvLen = recv(MonitorQueryData.second.Socket, reinterpret_cast<char *>(RecvBuffer.get() + Length), static_cast<int>(Parameter.LargeBufferSize - Length), 0);

		//Receive length check
			if (RecvLen > 0)
			{
				RecvLen += Length;
			}
		//Connection closed or SOCKET_ERROR
			else {
				SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				return false;
			}
		}
	//Timeout or SOCKET_ERROR
		else {
			SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}
	}

//Length check
	Length = ntohs(reinterpret_cast<uint16_t *>(RecvBuffer.get())[0]);
	if (RecvLen >= static_cast<ssize_t>(Length) && Length >= DNS_PACKET_MINSIZE)
	{
		MonitorQueryData.first.Buffer = RecvBuffer.get() + sizeof(uint16_t);
		MonitorQueryData.first.Length = Length;
		MonitorQueryData.first.IsLocalRequest = false;
		MonitorQueryData.first.IsLocalForce = false;
		memset(&MonitorQueryData.first.LocalTarget, 0, sizeof(MonitorQueryData.first.LocalTarget));

	//Check DNS query data.
		std::unique_ptr<uint8_t[]> SendBuffer(new uint8_t[Parameter.LargeBufferSize]());
		memset(SendBuffer.get(), 0, Parameter.LargeBufferSize);
		if (!CheckQueryData(&MonitorQueryData.first, SendBuffer.get(), Parameter.LargeBufferSize, MonitorQueryData.second))
		{
			SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}

	//Main request process
		EnterRequestProcess(MonitorQueryData, OriginalRecv, RecvSize);
	}
	else {
		SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}

//Block Port Unreachable messages of system.
	shutdown(MonitorQueryData.second.Socket, SD_SEND);
#if defined(PLATFORM_WIN)
	Sleep(Parameter.SocketTimeout_Reliable_Once);
#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	usleep(Parameter.SocketTimeout_Reliable_Once.tv_sec * SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND + Parameter.SocketTimeout_Reliable_Once.tv_usec);
#endif
	SocketSetting(MonitorQueryData.second.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

	return true;
}

//Alternate DNS servers switcher
void AlternateServerMonitor(
	void)
{
//Initialization
	size_t Index = 0;
	uint64_t RangeTimer[ALTERNATE_SERVERNUM]{0}, SwapTimer[ALTERNATE_SERVERNUM]{0};

//Switcher
	for (;;)
	{
	//Complete request process check
		for (Index = 0;Index < ALTERNATE_SERVERNUM;++Index)
		{
		//Reset TimeoutTimes out of alternate time range.
			if (RangeTimer[Index] <= GetCurrentSystemTime())
			{
				RangeTimer[Index] = GetCurrentSystemTime() + Parameter.AlternateTimeRange;
				AlternateSwapList.TimeoutTimes[Index] = 0;

				continue;
			}

		//Reset alternate switching.
			if (AlternateSwapList.IsSwap[Index])
			{
				if (SwapTimer[Index] <= GetCurrentSystemTime())
				{
					AlternateSwapList.IsSwap[Index] = false;
					AlternateSwapList.TimeoutTimes[Index] = 0;
					SwapTimer[Index] = 0;
				}
			}
			else {
			//Mark alternate switching.
				if (AlternateSwapList.TimeoutTimes[Index] >= Parameter.AlternateTimes)
				{
					AlternateSwapList.IsSwap[Index] = true;
					AlternateSwapList.TimeoutTimes[Index] = 0;
					SwapTimer[Index] = GetCurrentSystemTime() + Parameter.AlternateResetTime;
				}
			}
		}

		Sleep(Parameter.FileRefreshTime);
	}

//Monitor terminated
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Alternate Server module Monitor terminated", 0, nullptr, 0);
	return;
}

//Get local address list
#if defined(PLATFORM_WIN)
addrinfo *GetLocalAddressList(
	const uint16_t Protocol, 
	uint8_t * const HostName)
{
	memset(HostName, 0, DOMAIN_MAXSIZE);

//Get local machine name.
	if (gethostname(reinterpret_cast<char *>(HostName), DOMAIN_MAXSIZE) == SOCKET_ERROR || CheckEmptyBuffer(HostName, DOMAIN_MAXSIZE))
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine name error", WSAGetLastError(), nullptr, 0);
		return nullptr;
	}

//Initialization
	addrinfo Hints;
	memset(&Hints, 0, sizeof(Hints));
	addrinfo *Result = nullptr;
	Hints.ai_family = Protocol;
	Hints.ai_socktype = SOCK_DGRAM;
	Hints.ai_protocol = IPPROTO_UDP;

//Get local machine name data.
	const auto UnsignedResult = getaddrinfo(reinterpret_cast<char *>(HostName), nullptr, &Hints, &Result);
	if (UnsignedResult != 0 || Result == nullptr)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine address error", UnsignedResult, nullptr, 0);
		return nullptr;
	}

	return Result;
}

#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
//Get address from best network interface
bool GetBestInterfaceAddress(
	const uint16_t Protocol, 
	const sockaddr_storage * const OriginalSockAddr)
{
//Initialization
	sockaddr_storage SockAddr;
	memset(&SockAddr, 0, sizeof(SockAddr));
	SockAddr.ss_family = Protocol;
	SYSTEM_SOCKET InterfaceSocket = socket(Protocol, SOCK_DGRAM, IPPROTO_UDP);
	socklen_t AddrLen = 0;

//Socket check
	if (!SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
	{
		SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		if (Protocol == AF_INET6)
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
		else if (Protocol == AF_INET)
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;

		return false;
	}

//Check parameter.
	if (Protocol == AF_INET6)
	{
		(reinterpret_cast<sockaddr_in6 *>(&SockAddr))->sin6_addr = (reinterpret_cast<const sockaddr_in6 *>(OriginalSockAddr))->sin6_addr;
		(reinterpret_cast<sockaddr_in6 *>(&SockAddr))->sin6_port = (reinterpret_cast<const sockaddr_in6 *>(OriginalSockAddr))->sin6_port;
		AddrLen = sizeof(sockaddr_in6);

	//UDP connecting
		if (connect(InterfaceSocket, reinterpret_cast<sockaddr *>(&SockAddr), sizeof(sockaddr_in6)) == SOCKET_ERROR || 
			getsockname(InterfaceSocket, reinterpret_cast<sockaddr *>(&SockAddr), &AddrLen) == SOCKET_ERROR || 
			SockAddr.ss_family != AF_INET6 || AddrLen != sizeof(sockaddr_in6) || 
			CheckEmptyBuffer(&(reinterpret_cast<sockaddr_in6 *>(&SockAddr))->sin6_addr, sizeof((reinterpret_cast<sockaddr_in6 *>(&SockAddr))->sin6_addr)))
		{
			SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;

			return false;
		}
	}
	else if (Protocol == AF_INET)
	{
		(reinterpret_cast<sockaddr_in *>(&SockAddr))->sin_addr = (reinterpret_cast<const sockaddr_in *>(OriginalSockAddr))->sin_addr;
		(reinterpret_cast<sockaddr_in *>(&SockAddr))->sin_port = (reinterpret_cast<const sockaddr_in *>(OriginalSockAddr))->sin_port;
		AddrLen = sizeof(sockaddr_in);

	//UDP connecting
		if (connect(InterfaceSocket, reinterpret_cast<sockaddr *>(&SockAddr), sizeof(sockaddr_in)) == SOCKET_ERROR || 
			getsockname(InterfaceSocket, reinterpret_cast<sockaddr *>(&SockAddr), &AddrLen) == SOCKET_ERROR || 
			SockAddr.ss_family != AF_INET || AddrLen != sizeof(sockaddr_in) || 
			CheckEmptyBuffer(&(reinterpret_cast<sockaddr_in *>(&SockAddr))->sin_addr, sizeof((reinterpret_cast<sockaddr_in *>(&SockAddr))->sin_addr)))
		{
			SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;

			return false;
		}
	}
	else {
		SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}

	SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	return true;
}
#endif

//Get gateway information
void GetGatewayInformation(
	const uint16_t Protocol)
{
	if (Protocol == AF_INET6)
	{
		if (Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Main_IPv6.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family == 0
		#if defined(ENABLE_LIBSODIUM)
			&& DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0
			&& DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family == 0
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
			return;
		}
	#if defined(PLATFORM_WIN)
		DWORD AdaptersIndex = 0;
		if ((Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Main_IPv6.AddressData.IPv6), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Alternate_IPv6.AddressData.IPv6), &
				AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Main_IPv6.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Main_IPv6.IPv6), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Alternate_IPv6.IPv6), 
				&AdaptersIndex) != NO_ERROR)
		#if defined(ENABLE_LIBSODIUM)
			|| (DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.IPv6), 
				&AdaptersIndex) != NO_ERROR) || 
			(DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.IPv6), 
				&AdaptersIndex) != NO_ERROR)
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
			return;
		}

	//Multiple list(IPv6)
		if (Parameter.Target_Server_IPv6_Multiple != nullptr)
		{
			for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv6_Multiple)
			{
				if (GetBestInterfaceEx(
						reinterpret_cast<sockaddr *>(const_cast<sockaddr_in6 *>(&DNSServerDataIter.AddressData.IPv6)), 
						&AdaptersIndex) != NO_ERROR)
				{
					GlobalRunningStatus.GatewayAvailable_IPv6 = false;
					return;
				}
			}
		}
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		if ((Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Main_IPv6.AddressData.Storage)) || 
			(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Alternate_IPv6.AddressData.Storage)) || 
			(Parameter.Target_Server_Local_Main_IPv6.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Local_Main_IPv6.Storage)) || 
			(Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Local_Alternate_IPv6.Storage))
		#if defined(ENABLE_LIBSODIUM)
			|| (DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage)) || 
			(DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage))
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
			return;
		}

	//Multiple list(IPv6)
		if (Parameter.Target_Server_IPv6_Multiple != nullptr)
		{
			for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv6_Multiple)
			{
				if (!GetBestInterfaceAddress(AF_INET6, &DNSServerDataIter.AddressData.Storage))
				{
					GlobalRunningStatus.GatewayAvailable_IPv6 = false;
					return;
				}
			}
		}
	#endif

		GlobalRunningStatus.GatewayAvailable_IPv6 = true;
	}
	else if (Protocol == AF_INET)
	{
		if (Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Main_IPv4.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family == 0
		#if defined(ENABLE_LIBSODIUM)
			&& DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0
			&& DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family == 0
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;
			return;
		}
	#if defined(PLATFORM_WIN)
		DWORD AdaptersIndex = 0;
		if ((Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Main_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Alternate_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Main_IPv4.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Main_IPv4.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Alternate_IPv4.IPv4), 
				&AdaptersIndex) != NO_ERROR)
		#if defined(ENABLE_LIBSODIUM)
			|| DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			(GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			(GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR)
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;
			return;
		}

	//Multiple list(IPv4)
		if (Parameter.Target_Server_IPv4_Multiple != nullptr)
		{
			for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv4_Multiple)
			{
				if (GetBestInterfaceEx(
						reinterpret_cast<sockaddr *>(const_cast<sockaddr_in *>(&DNSServerDataIter.AddressData.IPv4)), 
						&AdaptersIndex) != NO_ERROR)
				{
					GlobalRunningStatus.GatewayAvailable_IPv4 = false;
					return;
				}
			}
		}
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		if ((Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Main_IPv4.AddressData.Storage)) || 
			(Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Alternate_IPv4.AddressData.Storage)) || 
			(Parameter.Target_Server_Local_Main_IPv4.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Local_Main_IPv4.Storage)) || 
			(Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Local_Alternate_IPv4.Storage))
		#if defined(ENABLE_LIBSODIUM)
			|| (DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage)) || 
			(DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage))
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;
			return;
		}

	//Multiple list(IPv4)
		if (Parameter.Target_Server_IPv4_Multiple != nullptr)
		{
			for (const auto &DNSServerDataIter:*Parameter.Target_Server_IPv4_Multiple)
			{
				if (!GetBestInterfaceAddress(AF_INET, &DNSServerDataIter.AddressData.Storage))
				{
					GlobalRunningStatus.GatewayAvailable_IPv4 = false;
					return;
				}
			}
		}
	#endif

		GlobalRunningStatus.GatewayAvailable_IPv4 = true;
	}

	return;
}

//Local network information monitor
void NetworkInformationMonitor(
	void)
{
//Initialization
#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
	uint8_t AddrBuffer[ADDRESS_STRING_MAXSIZE]{0};
	std::string DomainString;
	#if defined(PLATFORM_WIN)
		uint8_t HostName[DOMAIN_MAXSIZE]{0};
		addrinfo *LocalAddressList = nullptr, *LocalAddressTableIter = nullptr;
	#elif defined(PLATFORM_LINUX)
		ifaddrs *InterfaceAddressList = nullptr, *InterfaceAddressIter = nullptr;
		auto IsErrorFirstPrint = true;
	#endif
#elif defined(PLATFORM_MACOS)
	ifaddrs *InterfaceAddressList = nullptr, *InterfaceAddressIter = nullptr;
	auto IsErrorFirstPrint = true;
#endif
	dns_hdr *DNS_Header = nullptr;
	dns_qry *DNS_Query = nullptr;
	void *DNS_Record = nullptr;
	std::unique_lock<std::mutex> LocalAddressMutexIPv6(LocalAddressLock[NETWORK_LAYER_TYPE_IPV6], std::defer_lock), LocalAddressMutexIPv4(LocalAddressLock[NETWORK_LAYER_TYPE_IPV4], std::defer_lock), SocketMarkingMutex(SocketMarkingLock, std::defer_lock);

//Monitor
	for (;;)
	{
	//Get local machine addresses(IPv6).
		if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV6)
		{
		#if defined(PLATFORM_WIN)
			memset(HostName, 0, DOMAIN_MAXSIZE);
			LocalAddressList = GetLocalAddressList(AF_INET6, HostName);
			if (LocalAddressList == nullptr)
			{
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			errno = 0;
			if (getifaddrs(&InterfaceAddressList) != 0 || InterfaceAddressList == nullptr)
			{
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine address error", errno, nullptr, 0);
				if (InterfaceAddressList != nullptr)
					freeifaddrs(InterfaceAddressList);
				InterfaceAddressList = nullptr;
		#endif

				goto JumpToRestart;
			}
			else {
				LocalAddressMutexIPv6.lock();
				memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6], 0, PACKET_MAXSIZE);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] = 0;
			#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV6]->clear();
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV6]->shrink_to_fit();
			#endif

			//Mark local addresses(A part).
				DNS_Header = reinterpret_cast<dns_hdr *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6]);
				DNS_Header->Flags = htons(DNS_SQR_NEA);
				DNS_Header->Question = htons(U16_NUM_ONE);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += sizeof(dns_hdr);
				memcpy_s(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6], PACKET_MAXSIZE - GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6], Parameter.Local_FQDN_Response, Parameter.Local_FQDN_Length);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += Parameter.Local_FQDN_Length;
				DNS_Query = reinterpret_cast<dns_qry *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6]);
				DNS_Query->Type = htons(DNS_TYPE_AAAA);
				DNS_Query->Classes = htons(DNS_CLASS_INTERNET);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += sizeof(dns_qry);

			//Read addresses list and convert to Fully Qualified Domain Name/FQDN PTR record.
			#if defined(PLATFORM_WIN)
				for (LocalAddressTableIter = LocalAddressList;LocalAddressTableIter != nullptr;LocalAddressTableIter = LocalAddressTableIter->ai_next)
				{
					if (LocalAddressTableIter->ai_family == AF_INET6 && LocalAddressTableIter->ai_addrlen == sizeof(sockaddr_in6) && 
						LocalAddressTableIter->ai_addr->sa_family == AF_INET6)
					{
			#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				for (InterfaceAddressIter = InterfaceAddressList;InterfaceAddressIter != nullptr;InterfaceAddressIter = InterfaceAddressIter->ifa_next)
				{
					if (InterfaceAddressIter->ifa_addr != nullptr && InterfaceAddressIter->ifa_addr->sa_family == AF_INET6)
					{
			#endif
					//Mark local addresses(B part).
						if (GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] <= PACKET_MAXSIZE - sizeof(dns_record_aaaa))
						{
							DNS_Record = reinterpret_cast<dns_record_aaaa *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6]);
							(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->Name = htons(DNS_POINTER_QUERY);
							(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->Classes = htons(DNS_CLASS_INTERNET);
							if (Parameter.HostsDefaultTTL > 0)
								(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->TTL = htonl(Parameter.HostsDefaultTTL);
							else 
								(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->TTL = htonl(DEFAULT_HOSTS_TTL);
							(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->Type = htons(DNS_TYPE_AAAA);
							(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->Length = htons(sizeof(in6_addr));
						#if defined(PLATFORM_WIN)
							(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->Address = (reinterpret_cast<sockaddr_in6 *>(LocalAddressTableIter->ai_addr))->sin6_addr;
						#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
							(reinterpret_cast<dns_record_aaaa *>(DNS_Record))->Address = (reinterpret_cast<sockaddr_in6 *>(InterfaceAddressIter->ifa_addr))->sin6_addr;
						#endif
							GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += sizeof(dns_record_aaaa);
							++DNS_Header->Answer;
						}

					#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
					//Convert from binary to string.
						DomainString.clear();
						for (ssize_t Index = sizeof(in6_addr) / sizeof(uint8_t) - 1U;Index >= 0;--Index)
						{
							memset(AddrBuffer, 0, ADDRESS_STRING_MAXSIZE);

						#if defined(PLATFORM_WIN)
							if ((reinterpret_cast<sockaddr_in6 *>(LocalAddressTableIter->ai_addr))->sin6_addr.s6_addr[Index] == 0)
						#elif defined(PLATFORM_LINUX)
							if ((reinterpret_cast<sockaddr_in6 *>(InterfaceAddressIter->ifa_addr))->sin6_addr.s6_addr[Index] == 0)
						#endif
							{
								DomainString.append("0.0.");
							}
						#if defined(PLATFORM_WIN)
							else if ((reinterpret_cast<sockaddr_in6 *>(LocalAddressTableIter->ai_addr))->sin6_addr.s6_addr[Index] < 0x10)
						#elif defined(PLATFORM_LINUX)
							else if ((reinterpret_cast<sockaddr_in6 *>(InterfaceAddressIter->ifa_addr))->sin6_addr.s6_addr[Index] < 0x10)
						#endif
							{
							#if defined(PLATFORM_WIN)
								if (sprintf_s(reinterpret_cast<char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE, "%x", (reinterpret_cast<sockaddr_in6 *>(LocalAddressTableIter->ai_addr))->sin6_addr.s6_addr[Index]) < 0 || 
							#elif defined(PLATFORM_LINUX)
								if (sprintf(reinterpret_cast<char *>(AddrBuffer), "%x", (reinterpret_cast<sockaddr_in6 *>(InterfaceAddressIter->ifa_addr))->sin6_addr.s6_addr[Index]) < 0 || 
							#endif
									strnlen_s(reinterpret_cast<const char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE) != 1U)
								{
									goto StopLoop;
								}
								else {
									DomainString.append(reinterpret_cast<const char *>(AddrBuffer));
									DomainString.append(".0.");
								}
							}
							else {
							#if defined(PLATFORM_WIN)
								if (sprintf_s(reinterpret_cast<char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE, "%x", (reinterpret_cast<sockaddr_in6 *>(LocalAddressTableIter->ai_addr))->sin6_addr.s6_addr[Index]) < 0 || 
							#elif defined(PLATFORM_LINUX)
								if (sprintf(reinterpret_cast<char *>(AddrBuffer), "%x", (reinterpret_cast<sockaddr_in6 *>(InterfaceAddressIter->ifa_addr))->sin6_addr.s6_addr[Index]) < 0 || 
							#endif
									strnlen_s(reinterpret_cast<const char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE) != 2U)
								{
									goto StopLoop;
								}
								else {
									DomainString.append(reinterpret_cast<const char *>(AddrBuffer + 1U), 1U);
									DomainString.append(".");
									DomainString.append(reinterpret_cast<const char *>(AddrBuffer), 1U);
									DomainString.append(".");
								}
							}
						}

					//Add to global list.
						DomainString.append("ip6.arpa");
						GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV6]->push_back(DomainString);
					#endif
					}
				}

			//Jump here to stop loop.
			StopLoop:

			//Mark local addresses(C part).
				if (DNS_Header->Answer == 0)
				{
					memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6], 0, PACKET_MAXSIZE);
					GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] = 0;
				}
				else {
					DNS_Header->Answer = htons(DNS_Header->Answer);
				}

			//Free all lists.
				LocalAddressMutexIPv6.unlock();
			#if defined(PLATFORM_WIN)
				freeaddrinfo(LocalAddressList);
				LocalAddressList = nullptr;
			#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				freeifaddrs(InterfaceAddressList);
				InterfaceAddressList = nullptr;
			#endif
			}
		}

	//Get local machine addresses(IPv4).
		if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV4)
		{
		#if defined(PLATFORM_WIN)
			memset(HostName, 0, DOMAIN_MAXSIZE);
			LocalAddressList = GetLocalAddressList(AF_INET, HostName);
			if (LocalAddressList == nullptr)
			{
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			errno = 0;
			if (getifaddrs(&InterfaceAddressList) != 0 || InterfaceAddressList == nullptr)
			{
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine address error", errno, nullptr, 0);
				if (InterfaceAddressList != nullptr)
					freeifaddrs(InterfaceAddressList);
				InterfaceAddressList = nullptr;
		#endif

				goto JumpToRestart;
			}
			else {
				LocalAddressMutexIPv4.lock();
				memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4], 0, PACKET_MAXSIZE);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] = 0;
			#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV4]->clear();
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV4]->shrink_to_fit();
			#endif

			//Mark local addresses(A part).
				DNS_Header = reinterpret_cast<dns_hdr *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4]);
				DNS_Header->Flags = htons(DNS_SQR_NEA);
				DNS_Header->Question = htons(U16_NUM_ONE);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += sizeof(dns_hdr);
				memcpy_s(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4], PACKET_MAXSIZE - GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4], Parameter.Local_FQDN_Response, Parameter.Local_FQDN_Length);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += Parameter.Local_FQDN_Length;
				DNS_Query = reinterpret_cast<dns_qry *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4]);
				DNS_Query->Type = htons(DNS_TYPE_AAAA);
				DNS_Query->Classes = htons(DNS_CLASS_INTERNET);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += sizeof(dns_qry);

			//Read addresses list and convert to Fully Qualified Domain Name/FQDN PTR record.
			#if defined(PLATFORM_WIN)
				for (LocalAddressTableIter = LocalAddressList;LocalAddressTableIter != nullptr;LocalAddressTableIter = LocalAddressTableIter->ai_next)
				{
					if (LocalAddressTableIter->ai_family == AF_INET && LocalAddressTableIter->ai_addrlen == sizeof(sockaddr_in) && 
						LocalAddressTableIter->ai_addr->sa_family == AF_INET)
					{
			#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				for (InterfaceAddressIter = InterfaceAddressList;InterfaceAddressIter != nullptr;InterfaceAddressIter = InterfaceAddressIter->ifa_next)
				{
					if (InterfaceAddressIter->ifa_addr != nullptr && InterfaceAddressIter->ifa_addr->sa_family == AF_INET)
					{
			#endif
					//Mark local addresses(B part).
						if (GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] <= PACKET_MAXSIZE - sizeof(dns_record_a))
						{
							DNS_Record = reinterpret_cast<dns_record_a *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4]);
							(reinterpret_cast<dns_record_a *>(DNS_Record))->Name = htons(DNS_POINTER_QUERY);
							(reinterpret_cast<dns_record_a *>(DNS_Record))->Classes = htons(DNS_CLASS_INTERNET);
							if (Parameter.HostsDefaultTTL > 0)
								(reinterpret_cast<dns_record_a *>(DNS_Record))->TTL = htonl(Parameter.HostsDefaultTTL);
							else 
								(reinterpret_cast<dns_record_a *>(DNS_Record))->TTL = htonl(DEFAULT_HOSTS_TTL);
							(reinterpret_cast<dns_record_a *>(DNS_Record))->Type = htons(DNS_TYPE_A);
							(reinterpret_cast<dns_record_a *>(DNS_Record))->Length = htons(sizeof(in_addr));
						#if defined(PLATFORM_WIN)
							(reinterpret_cast<dns_record_a *>(DNS_Record))->Address = (reinterpret_cast<sockaddr_in *>(LocalAddressTableIter->ai_addr))->sin_addr;
						#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
							(reinterpret_cast<dns_record_a *>(DNS_Record))->Address = (reinterpret_cast<sockaddr_in *>(InterfaceAddressIter->ifa_addr))->sin_addr;
						#endif
							GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += sizeof(dns_record_a);
							++DNS_Header->Answer;
						}

					#if (defined(PLATFORM_WIN) || defined(PLATFORM_LINUX))
					//Convert from binary to DNS PTR response.
						memset(AddrBuffer, 0, ADDRESS_STRING_MAXSIZE);
						DomainString.clear();
					#if defined(PLATFORM_WIN)
						sprintf_s(reinterpret_cast<char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(LocalAddressTableIter->ai_addr))->sin_addr)) + sizeof(uint8_t) * 3U));
					#elif defined(PLATFORM_LINUX)
						sprintf(reinterpret_cast<char *>(AddrBuffer), "%u", *((reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(InterfaceAddressIter->ifa_addr))->sin_addr)) + sizeof(uint8_t) * 3U));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer));
						memset(AddrBuffer, 0, ADDRESS_STRING_MAXSIZE);
						DomainString.append(".");
					#if defined(PLATFORM_WIN)
						sprintf_s(reinterpret_cast<char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(LocalAddressTableIter->ai_addr))->sin_addr)) + sizeof(uint8_t) * 2U));
					#elif defined(PLATFORM_LINUX)
						sprintf(reinterpret_cast<char *>(AddrBuffer), "%u", *((reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(InterfaceAddressIter->ifa_addr))->sin_addr)) + sizeof(uint8_t) * 2U));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer));
						memset(AddrBuffer, 0, ADDRESS_STRING_MAXSIZE);
						DomainString.append(".");
					#if defined(PLATFORM_WIN)
						sprintf_s(reinterpret_cast<char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(LocalAddressTableIter->ai_addr))->sin_addr)) + sizeof(uint8_t)));
					#elif defined(PLATFORM_LINUX)
						sprintf(reinterpret_cast<char *>(AddrBuffer), "%u", *((reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(InterfaceAddressIter->ifa_addr))->sin_addr)) + sizeof(uint8_t)));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer));
						memset(AddrBuffer, 0, ADDRESS_STRING_MAXSIZE);
						DomainString.append(".");
					#if defined(PLATFORM_WIN)
						sprintf_s(reinterpret_cast<char *>(AddrBuffer), ADDRESS_STRING_MAXSIZE, "%u", *(reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(LocalAddressTableIter->ai_addr))->sin_addr)));
					#elif defined(PLATFORM_LINUX)
						sprintf(reinterpret_cast<char *>(AddrBuffer), "%u", *(reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(InterfaceAddressIter->ifa_addr))->sin_addr)));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer));
						memset(AddrBuffer, 0, ADDRESS_STRING_MAXSIZE);

					//Add to global list.
						DomainString.append(".in-addr.arpa");
						GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV4]->push_back(DomainString);
					#endif
					}
				}

			//Mark local addresses(C part).
				if (DNS_Header->Answer == 0)
				{
					memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4], 0, PACKET_MAXSIZE);
					GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] = 0;
				}
				else {
					DNS_Header->Answer = htons(DNS_Header->Answer);
				}

			//Free all lists.
				LocalAddressMutexIPv4.unlock();
			#if defined(PLATFORM_WIN)
				freeaddrinfo(LocalAddressList);
				LocalAddressList = nullptr;
			#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				freeifaddrs(InterfaceAddressList);
				InterfaceAddressList = nullptr;
			#endif
			}
		}

	//Free all lists.
	#if defined(PLATFORM_WIN)
		if (LocalAddressList != nullptr)
			freeaddrinfo(LocalAddressList);
		LocalAddressList = nullptr;
	#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		if (InterfaceAddressList != nullptr)
			freeifaddrs(InterfaceAddressList);
		InterfaceAddressList = nullptr;
	#endif

	//Get gateway information and check.
		GetGatewayInformation(AF_INET6);
		GetGatewayInformation(AF_INET);
		if (!GlobalRunningStatus.GatewayAvailable_IPv4)
		{
		#if defined(PLATFORM_WIN)
			if (!GlobalRunningStatus.GatewayAvailable_IPv6)
		#elif (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			if (!(IsErrorFirstPrint || GlobalRunningStatus.GatewayAvailable_IPv6))
		#endif
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Not any available gateways to public network", 0, nullptr, 0);

		#if (defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			IsErrorFirstPrint = false;
		#endif
		}

	//Jump here to restart.
	JumpToRestart:

	//Close all marking sockets.
		SocketMarkingMutex.lock();
		while (!SocketMarkingList.empty() && SocketMarkingList.front().second <= GetCurrentSystemTime())
		{
			SocketSetting(SocketMarkingList.front().first, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			SocketMarkingList.pop_front();
		}

		SocketMarkingMutex.unlock();

	//Wait for interval time.
		Sleep(Parameter.FileRefreshTime);
	}

//Monitor terminated
	PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Get Local Address Information module Monitor terminated", 0, nullptr, 0);
	return;
}
