#pragma once
#include "app.hh"
#include "networking.hh"

namespace app {
	using namespace zapdata;
	using namespace app::networking;
	
	class Client : public v4d::networking::OutgoingConnection {
		std::thread* actionsThread = nullptr;
		// cache
		std::vector<v4d::data::Stream> actionStreams {};
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
						v4d::modular::ModuleID moduleID(socket->Read<uint64_t>(), socket->Read<uint64_t>());
						auto module = V4D_Client::GetModule(moduleID.String());
						if (module && module->ReceiveBurst) {
							static v4d::data::ReadOnlyStream stream(APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE);
							socket->ReadStream(stream);
							module->ReceiveBurst(stream);
						}
					}break;
					
					case BURST_ACTION::INIT:
						LOG("CLIENT Received Server initial burst")
					break;
					
					default:break;
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
					v4d::modular::ModuleID moduleID(socket->Read<uint64_t>(), socket->Read<uint64_t>());
					auto module = V4D_Client::GetModule(moduleID.String());
					if (module && module->ReceiveAction) {
						static v4d::data::ReadOnlyStream stream(APP_NETWORKING_ACTION_BUFFER_SIZE);
						socket->ReadStream(stream);
						module->ReceiveAction(stream);
					}
				}break;
				
				default: break;
			}
			return true;
		}
		
		virtual void Run(v4d::io::SocketPtr socket) override {
			actionsThread = new std::thread([this, socket](){
				LOG_VERBOSE("Client SendActionThread started")
				while(socket->IsConnected()) {
					V4D_Client::ForEachSortedModule([this, socket](auto* mod){
						if (mod->SendActions) {
							actionStreams.clear();
							mod->SendActions(actionStreams);
							for (auto& stream : actionStreams) {
								*socket << ACTION::MODULE;
								*socket << stream;
								socket->Flush();
								if (stream.GetWriteBufferSize() > APP_NETWORKING_ACTION_BUFFER_SIZE) {
									LOG_WARN("V4D_Client::SendActions for module " << mod->ModuleName() << " stream size was " << stream.GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_ACTION_BUFFER_SIZE << " bytes")
								}
							}
						}
					});
					
					LIMIT_FRAMERATE(APP_NETWORKING_MAX_ACTION_STREAMS_PER_SECOND)
				}
				LOG_VERBOSE("Client SendActionThread terminated")
			});
			
			LOG_VERBOSE("Client Run started")
			while (socket->IsConnected()) {
				int polled = socket->Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
				if (polled == 0) continue; // timeout, keep going
				if (polled == -1) { // error, stop here
					LOG_ERROR("Client RUN Socket Poll error, disconnecting...")
					break;
				}
				if (!socket->IsConnected()) break;
				if (!HandleIncomingAction(socket)) break;
			}
			socket->SetConnected(false);
			LOG_VERBOSE("Client Run terminated")
		}

	};
	
	#ifdef APP_ENABLE_BURST_STREAMS
		class BurstClient : public v4d::networking::OutgoingConnection {
			v4d::io::SocketPtr burstSocket;
			std::thread* burstsThread = nullptr;
			// cache
			std::vector<v4d::data::Stream> burstStreams {};
			
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
					LOG_VERBOSE("Client SendBurstThread started")
					if (socket->IsTCP()) {
						*socket << ACTION::BURST;
					}
					*socket << BURST_ACTION::INIT;
					socket->Flush();
					
					while(socket->IsConnected()) {
						V4D_Client::ForEachSortedModule([this, clientType](auto* mod){
							if (mod->SendBursts) {
								burstStreams.clear();
								mod->SendBursts(burstStreams);
								for (auto& stream : burstStreams) {
									if (socket->IsUDP()) {
										if (!Connect("", 0, clientType)) {
											LOG_WARN("Client burst socket could not connect via UDP for sending data")
											continue;
										}
									} else {
										*socket << ACTION::BURST;
									}
									*socket << BURST_ACTION::MODULE;
									*socket << stream;
									socket->Flush();
									if (stream.GetWriteBufferSize() > APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE) {
										LOG_WARN("V4D_Client::SendBursts for module " << mod->ModuleName() << " stream size was " << stream.GetWriteBufferSize() << " bytes, but should be at most " << APP_NETWORKING_BURST_BUFFER_SIZE_PER_MODULE << " bytes")
									}
								}
							}
						});
						
						LIMIT_FRAMERATE(APP_NETWORKING_MAX_BURST_STREAMS_PER_SECOND)
					}
					LOG_VERBOSE("Client SendBurstThread terminated")
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
				burstStreams.clear();
			}

			virtual void Authenticate(v4d::data::Stream* authStream) override {} // No authentication should be used for bursts
			
			virtual void Run(v4d::io::SocketPtr socket) override {
				static uint64_t serverIncrement = 0;
				socket->Bind();
				LOG_VERBOSE("BurstClient Run started")
				while (socket->IsConnected()) {
					int polled = socket->Poll(APP_NETWORKING_POLL_TIMEOUT_MS);
					if (polled <= 0) continue; // timeout, keep going
					if (!socket->IsConnected()) break;
					if (socket->IsUDP()) {
						std::string token = socket->ReadEncrypted<std::string>(&aes);
						uint64_t increment = socket->ReadEncrypted<uint64_t>(&aes);
						if (token != this->token) continue;
						if (increment <= serverIncrement) continue;
						serverIncrement = increment;
					}
					if (!Client::HandleIncomingBurst(socket)) break;
				}
				socket->Unbind();
				socket->SetConnected(false);
				LOG_VERBOSE("BurstClient Run terminated")
			}

		};
	#endif
}
