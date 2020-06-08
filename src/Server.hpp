#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	using namespace app::networking;
	using namespace v4d::modular;
	
	#ifdef APP_ENABLE_BURST_STREAMS
		namespace BurstCache {
			std::mutex burstMutex;
			std::unordered_map<uint64_t /* clientID */, v4d::io::SOCKET_TYPE> burstClientSocketTypes {};
			std::unordered_map<uint64_t /* clientID */, uint64_t> burstIncrementsFromServer {};
			std::unordered_map<uint64_t /* clientID */, v4d::io::SocketPtr> burstSockets {};
			std::unordered_map<uint64_t /* clientID */, std::thread> burstThreads {};
		};
	#endif
	
	// This must only be instanciated ONCE, Otherwise we need to fix the clearing of BurstCache in the Stop() method.
	class Server : public v4d::networking::ListeningServer {
		std::atomic<uint64_t> nextClientId = 1;
		
		std::mutex clientsMutex;
		std::unordered_map<uint64_t /* clientID */, std::thread> actionThreads {};
	public:
		using ListeningServer::ListeningServer;
		
		virtual ~Server() {
			Stop();
			LOG_VERBOSE("~Server")
		}
		
		virtual void Start(uint16_t port) override {
			ListeningServer::Start(port);
		}
		
		virtual void Stop() {
			v4d::networking::ListeningServer::Stop();
			
			for (auto&[id, t] : actionThreads) {
				if (t.joinable()) t.join();
			}
			actionThreads.clear();
			
			#ifdef APP_ENABLE_BURST_STREAMS
				std::lock_guard lock(BurstCache::burstMutex);
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
						auto _vendor = socket->Read<typeof ModuleID::vendor>();
						auto _module = socket->Read<typeof ModuleID::module>();
						ModuleID moduleID(_vendor, _module);
						auto module = V4D_Server::GetModule(moduleID.String());
						DEBUG_ASSERT_ERROR(module, "Server::HandleIncomingBurst : Module '" << moduleID.String() << "' is not loaded")
						if (module && module->ReceiveBurst) {
							module->ReceiveBurst(socket, client);
						}
					}break;
					
					case BURST_ACTION::INIT:
						LOG("SERVER Received Client initial burst")
					break;
					
					default: 
						LOG_ERROR("Server ReceiveBurst UNRECOGNIZED ACTION " << std::to_string((int)action))
						return false;
					break;
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
					auto _vendor = socket->Read<typeof ModuleID::vendor>();
					auto _module = socket->Read<typeof ModuleID::module>();
					ModuleID moduleID(_vendor, _module);
					auto module = V4D_Server::GetModule(moduleID.String());
					DEBUG_ASSERT_ERROR(module, "Server::HandleIncomingAction : Module '" << moduleID.String() << "' is not loaded")
					if (module && module->ReceiveAction) {
						module->ReceiveAction(socket, client);
					}
				}break;
				
				#ifdef APP_ENABLE_BURST_STREAMS
					case ACTION::BURST:
						HandleIncomingBurst(socket, client, clientType);
					break;
				#endif
				
				default: 
					LOG_ERROR("Server ReceiveAction UNRECOGNIZED ACTION " << std::to_string((int)action))
					return false;
				break;
			}
			return true;
		}
		
		virtual void RunClient(v4d::io::SocketPtr socket, IncomingClientPtr client, byte clientType) override {
			if (clientType == CLIENT_TYPE::INITIAL) {
				
				// IncomingClient
				V4D_Server::ForEachSortedModule([this, client, socket](auto* mod){
					if (mod->IncomingClient) {
						mod->IncomingClient(client);
					}
				});
				
				{std::lock_guard lock(clientsMutex);
					#ifdef APP_ENABLE_BURST_STREAMS
						{std::lock_guard lock(BurstCache::burstMutex);
							BurstCache::burstClientSocketTypes[client->id] = v4d::io::TCP;
							BurstCache::burstIncrementsFromServer[client->id] = 0;
							BurstCache::burstSockets[client->id] = nullptr;
						}
					#endif
					
					// Actions
					try {
						actionThreads.at(client->id);
					} catch(...) {
						actionThreads[client->id] = std::thread([this, client, socket](){
							THREAD_BEGIN("Server SendActions " + std::to_string(client->id), 1) {
								
								while(socket->IsConnected()) {
									THREAD_TICK
									
									V4D_Server::ForEachSortedModule([this, client, socket](auto* mod){
										if (mod->SendActions) {
											socket->LockWrite();
											socket->Begin = [this, socket, mod](){
												ModuleID moduleID(mod->ModuleName());
												*socket << ACTION::MODULE;
												*socket << moduleID.vendor;
												*socket << moduleID.module;
											};
											socket->End = [this, socket, mod](){
												DEBUG_ASSERT_WARN(socket->GetWriteBufferSize() <= APP_NETWORKING_ACTION_BUFFER_SIZE, "V4D_Server::SendActions for module '" << mod->ModuleName() << "' stream size was " << socket->GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_ACTION_BUFFER_SIZE << " bytes")
												socket->Flush();
											};
											mod->SendActions(socket, client);
											socket->UnlockWrite();
										}
									});
									
									LIMIT_FRAMERATE(APP_NETWORKING_MAX_ACTION_STREAMS_PER_SECOND)
								}
								
							}THREAD_END(app::isClient)
						});
					}
					
					#ifdef APP_ENABLE_BURST_STREAMS
						// Bursts
						std::lock_guard lockBurst(BurstCache::burstMutex);
						try {
							BurstCache::burstThreads.at(client->id);
						} catch(...) {
							BurstCache::burstThreads[client->id] = std::thread([this, client, socket](){
								THREAD_BEGIN("Server SendBursts " + std::to_string(client->id), 1) {
								
									v4d::io::SocketPtr burstSocket = std::make_shared<v4d::io::Socket>(v4d::io::UDP, socket->GetProtocol());
									burstSocket->SetRemoteAddr(socket->GetRemoteAddr());
									burstSocket->Connect();
									
									{std::lock_guard lock(BurstCache::burstMutex);
										BurstCache::burstSockets[client->id] = burstSocket;
										if (BurstCache::burstClientSocketTypes[client->id] == v4d::io::UDP) {
											*burstSocket << BURST_ACTION::INIT;
											burstSocket->Flush();
										}
									}
									
									LOG_VERBOSE("Server SendBurstThread started")
									while(socket->IsConnected()) {
										THREAD_TICK
										
										v4d::io::SocketPtr currentBurstSocket;
										{std::lock_guard lock(BurstCache::burstMutex);
											currentBurstSocket = (BurstCache::burstClientSocketTypes[client->id] == v4d::io::UDP ? burstSocket : socket);
										}
										V4D_Server::ForEachSortedModule([this, client, currentBurstSocket](auto* mod){
											ModuleID moduleID(mod->ModuleName());
											if (mod->SendBursts) {
												currentBurstSocket->LockWrite();
												currentBurstSocket->Begin = [this, currentBurstSocket, mod, client](){
													ModuleID moduleID(mod->ModuleName());
													if (currentBurstSocket->GetSocketType() == v4d::io::UDP) {
														currentBurstSocket->WriteEncrypted<std::string>(&client->aes, client->token);
														currentBurstSocket->WriteEncrypted<uint64_t>(&client->aes, ++BurstCache::burstIncrementsFromServer[client->id]);
													} else {
														*currentBurstSocket << ACTION::BURST;
													}
													*currentBurstSocket << BURST_ACTION::MODULE;
													*currentBurstSocket << moduleID.vendor;
													*currentBurstSocket << moduleID.module;
												};
												currentBurstSocket->End = [this, currentBurstSocket, mod](){
													DEBUG_ASSERT_WARN(currentBurstSocket->GetWriteBufferSize() <= APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE, "V4D_Server::SendBursts for module " << mod->ModuleName() << " stream size was " << currentBurstSocket->GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE << " bytes")
													currentBurstSocket->Flush();
												};
												mod->SendBursts(currentBurstSocket, client);
												currentBurstSocket->UnlockWrite();
											}
										});
										
										LIMIT_FRAMERATE(APP_NETWORKING_MAX_BURST_STREAMS_PER_SECOND)
									}
									
								}THREAD_END(app::isClient)
							});
						}
					#endif
				}
				
				THREAD_BEGIN("Server ReceiveActions " + std::to_string(client->id), 1) {
				
					while (socket->IsConnected()) {
						THREAD_TICK
						
						int polled = socket->Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
						if (polled == 0) continue; // timeout, no data yet, stay in the loop
						if (polled == -1) break; // Disconnected (or error, either way we must disconnect)
						if (!HandleIncomingAction(socket, client, clientType)) break;
					}
					socket->SetConnected(false);
					
				}THREAD_END(app::isClient)
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
