#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	using namespace app::networking;
	
	#ifdef APP_ENABLE_BURST_STREAMS
		namespace BurstCache {
			std::mutex burstMutex;
			std::unordered_map<ulong, v4d::io::SOCKET_TYPE> burstClientSocketTypes {};
			std::unordered_map<ulong, uint64_t> burstIncrementsFromServer {};
			std::unordered_map<ulong, v4d::io::SocketPtr> burstSockets {};
			std::unordered_map<ulong, std::thread> burstThreads {};
			std::unordered_map<ulong, std::vector<v4d::data::Stream>> burstStreamsPerClient {};
		};
	#endif
	
	// This must only be instanciated ONCE, Otherwise we need to fix the clearing of BurstCache in the Stop() method.
	class Server : public v4d::networking::ListeningServer {
		std::atomic<ulong> nextClientId = 1;
		
		std::mutex clientsMutex;
		std::unordered_map<ulong, std::thread> actionThreads {};
		// cache
		std::unordered_map<ulong, std::vector<v4d::data::Stream>> actionStreamsPerClient {};

	public:
		using ListeningServer::ListeningServer;
		
		virtual ~Server() {
			Stop();
			LOG_VERBOSE("~Server")
		}
		
		virtual void Stop() {
			v4d::networking::ListeningServer::Stop();
			
			for (auto&[id, t] : actionThreads) {
				if (t.joinable()) t.join();
			}
			actionThreads.clear();
			actionStreamsPerClient.clear();
			
			#ifdef APP_ENABLE_BURST_STREAMS
				for (auto&[id, s] : BurstCache::burstSockets) {
					s->Disconnect();
				}
				for (auto&[id, t] : BurstCache::burstThreads) {
					if (t.joinable()) t.join();
				}
				BurstCache::burstClientSocketTypes.clear();
				BurstCache::burstIncrementsFromServer.clear();
				BurstCache::burstSockets.clear();
				BurstCache::burstThreads.clear();
				BurstCache::burstStreamsPerClient.clear();
			#endif
		}

		std::string GetAppName() const override {
			return APP_NETWORKING_APPNAME;
		}
		std::string GetVersion() const override {
			return APP_NETWORKING_VERSION;
		}

		virtual ulong Authenticate(v4d::data::ReadOnlyStream* authStream) override {
			if (authStream) {
				// auto[username, password, stuff] = zapdata::Auth::ReadFrom(authStream);
				return nextClientId++;
			} else {
				// Anonymous
				return nextClientId++;
			}
		}

		#ifdef APP_ENABLE_BURST_STREAMS
			static bool HandleIncomingBurst(v4d::io::SocketPtr socket, IncomingClientPtr client, byte clientType) {
				{std::lock_guard lock(BurstCache::burstMutex);
					BurstCache::burstClientSocketTypes[client->id] = socket->GetSocketType();
				}
				BURST_ACTION action;
				try {
					action = socket->Read<BURST_ACTION>();
				} catch (...) {
					LOG_ERROR("Server : burst read error")
					return false;
				}
				switch (action) {
					case BURST_ACTION::QUIT: return false;
					
					case BURST_ACTION::MODULE:{
						v4d::modular::ModuleID moduleID(socket->Read<uint64_t>(), socket->Read<uint64_t>());
						auto module = V4D_Server::GetModule(moduleID.String());
						if (module && module->ReceiveBurst) {
							static v4d::data::ReadOnlyStream stream(APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE);
							socket->ReadStream(stream);
							module->ReceiveBurst(stream, client);
						}
					}break;
					
					case BURST_ACTION::INIT:
						LOG("SERVER Received Client initial burst")
					break;
					
					default:break;
				}
				return true;
			}
		#endif
		
		static bool HandleIncomingAction(v4d::io::SocketPtr socket, IncomingClientPtr client, byte clientType) {
			ACTION action;
			try {
				action = socket->Read<ACTION>();
			} catch (...) {
				LOG_ERROR_VERBOSE("Server : client has disconnected suddenly")
				return false;
			}
			switch (action) {
				case ACTION::QUIT: return false;
				
				case ACTION::MODULE:{
					v4d::modular::ModuleID moduleID(socket->Read<uint64_t>(), socket->Read<uint64_t>());
					auto module = V4D_Server::GetModule(moduleID.String());
					if (module && module->ReceiveAction) {
						static v4d::data::ReadOnlyStream stream(APP_NETWORKING_ACTION_BUFFER_SIZE);
						socket->ReadStream(stream);
						module->ReceiveAction(stream, client);
					}
				}break;
				
				#ifdef APP_ENABLE_BURST_STREAMS
					case ACTION::BURST:
						HandleIncomingBurst(socket, client, clientType);
					break;
				#endif
				
				default: break;
			}
			return true;
		}
		
		virtual void RunClient(v4d::io::SocketPtr socket, IncomingClientPtr client, byte clientType) override {
			if (clientType == CLIENT_TYPE::INITIAL) {
				{std::lock_guard lock(clientsMutex);
					actionStreamsPerClient[client->id].clear();
					#ifdef APP_ENABLE_BURST_STREAMS
						BurstCache::burstClientSocketTypes[client->id] = v4d::io::TCP;
						BurstCache::burstStreamsPerClient[client->id].clear();
						BurstCache::burstIncrementsFromServer[client->id] = 0;
						BurstCache::burstSockets[client->id] = nullptr;
					#endif
					
					// Actions
					try {
						actionThreads.at(client->id);
					} catch(...) {
						actionThreads[client->id] = std::thread([this, client, socket](){
							LOG_VERBOSE("Server SendActionThread started")
							while(socket->IsConnected()) {
								V4D_Server::ForEachSortedModule([this, client, socket](auto* mod){
									if (mod->SendActions) {
										actionStreamsPerClient[client->id].clear();
										mod->SendActions(actionStreamsPerClient[client->id], client);
										for (auto& stream : actionStreamsPerClient[client->id]) {
											*socket << ACTION::MODULE;
											*socket << stream;
											socket->Flush();
											if (stream.GetWriteBufferSize() > APP_NETWORKING_ACTION_BUFFER_SIZE) {
												LOG_WARN("V4D_Server::SendActions for module '" << mod->ModuleName() << "' stream size was " << stream.GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_ACTION_BUFFER_SIZE << " bytes")
											}
										}
									}
								});
								
								LIMIT_FRAMERATE(APP_NETWORKING_MAX_ACTION_STREAMS_PER_SECOND)
							}
							LOG_VERBOSE("Server SendActionThread terminated")
						});
					}
					
					#ifdef APP_ENABLE_BURST_STREAMS
						// Bursts
						try {
							BurstCache::burstThreads.at(client->id);
						} catch(...) {
							BurstCache::burstThreads[client->id] = std::thread([this, client, socket](){
								v4d::io::SocketPtr burstSocket = std::make_shared<v4d::io::Socket>(v4d::io::UDP, socket->GetProtocol());
								burstSocket->SetRemoteAddr(socket->GetRemoteAddr());
								burstSocket->Connect();
								BurstCache::burstSockets[client->id] = burstSocket;
								if (BurstCache::burstClientSocketTypes[client->id] == v4d::io::UDP) {
									*burstSocket << BURST_ACTION::INIT;
									burstSocket->Flush();
								}
								
								LOG_VERBOSE("Server SendBurstThread started")
								while(socket->IsConnected()) {
									v4d::io::SocketPtr currentBurstSocket = (BurstCache::burstClientSocketTypes[client->id] == v4d::io::UDP ? burstSocket : socket);
									V4D_Server::ForEachSortedModule([this, client, currentBurstSocket](auto* mod){
										if (mod->SendBursts) {
											BurstCache::burstStreamsPerClient[client->id].clear();
											mod->SendBursts(BurstCache::burstStreamsPerClient[client->id], client);
											for (auto& stream : BurstCache::burstStreamsPerClient[client->id]) {
												if (currentBurstSocket->GetSocketType() == v4d::io::UDP) {
													currentBurstSocket->WriteEncrypted<std::string>(&client->aes, client->token);
													currentBurstSocket->WriteEncrypted<uint64_t>(&client->aes, ++BurstCache::burstIncrementsFromServer[client->id]);
												} else {
													*currentBurstSocket << ACTION::BURST;
												}
												*currentBurstSocket << BURST_ACTION::MODULE;
												*currentBurstSocket << stream;
												currentBurstSocket->Flush();
												if (stream.GetWriteBufferSize() > APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE) {
													LOG_WARN("V4D_Server::SendBursts for module " << mod->ModuleName() << " stream size was " << stream.GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE << " bytes")
												}
											}
										}
									});
									
									LIMIT_FRAMERATE(APP_NETWORKING_MAX_BURST_STREAMS_PER_SECOND)
								}
								LOG_VERBOSE("Server SendBurstThread terminated")
							});
						}
					#endif
				}
				
				LOG_VERBOSE("Server RunClient started")
				while (socket->IsConnected()) {
					int polled = socket->Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
					if (polled == 0) continue; // timeout, keep going
					if (polled == -1) { // error, stop here
						LOG_ERROR("Server ClientRUN Socket Poll error, disconnecting...")
						break;
					}
					if (!socket->IsConnected()) break;
					if (!HandleIncomingAction(socket, client, clientType)) break;
				}
				socket->SetConnected(false);
				LOG_VERBOSE("Server RunClient terminated")
			}
		}
		
	};
	
	#ifdef APP_ENABLE_BURST_STREAMS
		class BurstServer : public v4d::networking::ListeningServer {
		public:
			using ListeningServer::ListeningServer;
			
			~BurstServer() {
				LOG_VERBOSE("~BurstServer")
			}
			
			std::string GetAppName() const override {
				return APP_NETWORKING_APPNAME;
			}
			std::string GetVersion() const override {
				return APP_NETWORKING_VERSION;
			}

			virtual ulong Authenticate(v4d::data::ReadOnlyStream* authStream) override {return 0;}
			
			virtual void RunClient(v4d::io::SocketPtr socket, IncomingClientPtr client, byte clientType) override {
				if (!socket->IsConnected()) return;
				Server::HandleIncomingBurst(socket, client, clientType);
			}
			
		};
	#endif
}
