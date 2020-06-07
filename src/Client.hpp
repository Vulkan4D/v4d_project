#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	using namespace app::networking;
	using namespace v4d::modular;
	
	class Client : public v4d::networking::OutgoingConnection {
		std::thread* actionsThread = nullptr;
	public:
		using OutgoingConnection::OutgoingConnection;
		
		virtual ~Client() {
			Disconnect();
			LOG_VERBOSE("~Client")
		}
		
		virtual void Disconnect() {
			v4d::networking::OutgoingConnection::Disconnect();
			if (actionsThread) {
				if (actionsThread->joinable()) actionsThread->join();
				delete actionsThread;
				actionsThread = nullptr;
			}
		}
		
		std::string GetAppName() const override {
			return APP_NETWORKING_APPNAME;
		}
		std::string GetVersion() const override {
			return APP_NETWORKING_VERSION;
		}

		virtual void Authenticate(v4d::data::Stream* authStream) override {
			// *authStream << zapdata::Auth{"bob", "12345", {4,16,512}};
		}
		
		#ifdef APP_ENABLE_BURST_STREAMS
			static bool HandleIncomingBurst(v4d::io::SocketPtr socket) {
				BURST_ACTION action;
				try {
					action = socket->Read<BURST_ACTION>();
				} catch (...) {
					LOG_ERROR("Client : burst read error")
					return false;
				}
				switch (action) {
					case BURST_ACTION::QUIT: return false;
					
					case BURST_ACTION::MODULE:{
						auto _vendor = socket->Read<typeof ModuleID::vendor>();
						auto _module = socket->Read<typeof ModuleID::module>();
						ModuleID moduleID(_vendor, _module);
						auto module = V4D_Client::GetModule(moduleID.String());
						DEBUG_ASSERT_ERROR(module, "Client::HandleIncomingBurst : Module '" << moduleID.String() << "' is not loaded")
						if (module && module->ReceiveBurst) {
							module->ReceiveBurst(socket);
						}
					}break;
					
					case BURST_ACTION::INIT:
						LOG("CLIENT Received Server initial burst")
					break;
					
					default: 
						LOG_ERROR("Client ReceiveBurst UNRECOGNIZED ACTION " << std::to_string((int)action))
						return false;
					break;
				}
				return true;
			}
		#endif
		
		static bool HandleIncomingAction(v4d::io::SocketPtr socket) {
			ACTION action;
			try {
				action = socket->Read<ACTION>();
			} catch (...) {
				LOG_ERROR("Client : server disconnected suddenly")
				return false;
			}
			switch (action) {
				case ACTION::QUIT: return false;
				
				case ACTION::MODULE:{
					auto _vendor = socket->Read<typeof ModuleID::vendor>();
					auto _module = socket->Read<typeof ModuleID::module>();
					ModuleID moduleID(_vendor, _module);
					auto module = V4D_Client::GetModule(moduleID.String());
					DEBUG_ASSERT_ERROR(module, "Client::HandleIncomingAction : Module '" << moduleID.String() << "' is not loaded")
					if (module && module->ReceiveAction) {
						module->ReceiveAction(socket);
					}
				}break;
				
				#ifdef APP_ENABLE_BURST_STREAMS
					case ACTION::BURST:
						LOG("Client incoming burst....")
						HandleIncomingBurst(socket);
					break;
				#endif
				
				default: 
					LOG_ERROR("Client ReceiveAction UNRECOGNIZED ACTION " << std::to_string((int)action))
					return false;
				break;
			}
			return true;
		}
		
		virtual void Run(v4d::io::SocketPtr socket) override {
			actionsThread = new std::thread([this, socket](){
				THREAD_BEGIN("Client SendActions", 1) {
					
					while (socket->IsConnected()) {
						THREAD_TICK
						
						V4D_Client::ForEachSortedModule([this, socket](auto* mod){
							if (mod->SendActions) {
								
								socket->Begin = [this, socket, mod](){
									ModuleID moduleID(mod->ModuleName());
									socket->LockWrite();
									*socket << ACTION::MODULE;
									*socket << moduleID.vendor << moduleID.module;
								};
								socket->End = [this, socket, mod](){
									DEBUG_ASSERT_WARN(socket->GetWriteBufferSize() <= APP_NETWORKING_ACTION_BUFFER_SIZE, "V4D_Client::SendActions for module " << mod->ModuleName() << " stream size was " << socket->GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_ACTION_BUFFER_SIZE << " bytes")
									socket->Flush();
									socket->UnlockWrite();
								};
								mod->SendActions(socket);
							}
						});
						
						LIMIT_FRAMERATE(APP_NETWORKING_MAX_ACTION_STREAMS_PER_SECOND)
					}
					
				}THREAD_END(true)
			});
			
			THREAD_BEGIN("Client ReceiveActions", 1) {
				
				while (socket->IsConnected()) {
					THREAD_TICK
					
					int polled = socket->Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
					if (polled == 0) continue; // timeout, no data yet, stay in the loop
					if (polled == -1) break; // Disconnected (or error, either way we must disconnect)
					if (!socket->IsConnected()) break;
					if (!HandleIncomingAction(socket)) break;
				}
				socket->SetConnected(false);
				
			}THREAD_END(true)
			
			if (app::IsRunning()) {
				LOG_ERROR("Disconnected from server")
				app::isRunning = false;
			}
		}

	};
	
	#ifdef APP_ENABLE_BURST_STREAMS
		class BurstClient : public v4d::networking::OutgoingConnection {
			v4d::io::SocketPtr burstSocket;
			std::thread* burstsThread = nullptr;
			
		public:
			using OutgoingConnection::OutgoingConnection;
			
			~BurstClient() {
				Stop();
				LOG_VERBOSE("~BurstClient")
			}
			
			std::string GetAppName() const override {
				return APP_NETWORKING_APPNAME;
			}
			std::string GetVersion() const override {
				return APP_NETWORKING_VERSION;
			}

			void Start(byte clientType) {
				ConnectRunAsync("", 0, clientType);
				burstsThread = new std::thread([this, clientType](){
					THREAD_BEGIN("Client SendBursts", 1) {
						
						if (socket->IsTCP()) {
							*socket << ACTION::BURST;
						}
						*socket << BURST_ACTION::INIT;
						socket->Flush();
						
						while(socket->IsConnected()) {
							THREAD_TICK
							
							V4D_Client::ForEachSortedModule([this, clientType](auto* mod){
								if (mod->SendBursts) {
									socket->Begin = [this, clientType, mod](){
										ModuleID moduleID(mod->ModuleName());
										socket->LockWrite();
										if (socket->IsUDP()) {
											Connect("", 0, clientType);
										} else {
											*socket << ACTION::BURST;
										}
										*socket << BURST_ACTION::MODULE;
										*socket << moduleID.vendor << moduleID.module;
									};
									socket->End = [this, mod](){
										DEBUG_ASSERT_WARN(socket->GetWriteBufferSize() <= APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE, "V4D_Client::SendBursts for module " << mod->ModuleName() << " stream size was " << socket->GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE << " bytes")
										socket->Flush();
										socket->UnlockWrite();
									};
									mod->SendBursts(socket);
								}
							});
							
							LIMIT_FRAMERATE(APP_NETWORKING_MAX_BURST_STREAMS_PER_SECOND)
						}
						
					}THREAD_END(true)
				});
			}
			
			void Stop() {
				Disconnect();
				if (burstSocket) {
					burstSocket->Disconnect();
				}
				if (burstsThread) {
					if (burstsThread->joinable()) burstsThread->join();
					delete burstsThread;
					burstsThread = nullptr;
				}
			}

			virtual void Authenticate(v4d::data::Stream* authStream) override {} // No authentication should be used for bursts
			
			virtual void Run(v4d::io::SocketPtr socket) override {
				THREAD_BEGIN("Client ReceiveBursts", 1) {
				
					static uint64_t serverIncrement = 0;
					socket->Bind();
					while (socket->IsConnected()) {
						THREAD_TICK
						
						int polled = socket->Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
						if (polled == 0) continue; // timeout, no data yet, stay in the loop
						if (polled == -1) break; // Disconnected (or error, either way we must disconnect)
						if (socket->IsUDP()) {
							std::string token = socket->ReadEncrypted<std::string>(&aes);
							uint64_t increment = socket->ReadEncrypted<uint64_t>(&aes);
							if (token != this->token) continue;
							if (increment <= serverIncrement) continue;
							serverIncrement = increment;
						} else {
							try {
								if (socket->Read<app::networking::ACTION>() != app::networking::ACTION::BURST) {
									LOG_ERROR("Client ReceiveBursts on TCP burst socket : did not receive BURST ACTION")
									throw std::runtime_error("");
								}
							} catch(...) {
								break;
							}
						}
						if (!Client::HandleIncomingBurst(socket)) break;
					}
					socket->Unbind();
					socket->SetConnected(false);
					
				}THREAD_END(true)
			}

		};
	#endif
}
