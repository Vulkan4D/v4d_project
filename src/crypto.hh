#pragma once
#include "app.hh"
#include "networking.hh"
#include "Server.hpp"
#include "Client.hpp"

namespace app::crypto {

	// To be executed server-side
	std::shared_ptr<v4d::crypto::RSA> LoadOrCreateServerPrivateKey() {
		std::shared_ptr<v4d::crypto::RSA> serverRsaKey;
		if (app::isServer && !app::isClient) {
			v4d::io::TextFile rsaPrivateKeyFile("server/private/key");
			rsaPrivateKeyFile.Load();
			if (rsaPrivateKeyFile.text.str() != "") {
				serverRsaKey = std::make_shared<v4d::crypto::RSA>(rsaPrivateKeyFile.text.str(), true);
			}
			if (!serverRsaKey) {
				serverRsaKey = std::make_shared<v4d::crypto::RSA>(4096, 3);
				rsaPrivateKeyFile.text = std::stringstream(serverRsaKey->GetPrivateKeyPEM());
				rsaPrivateKeyFile.WriteToFile();
			}
		}
		return serverRsaKey;
	}

	// To be executed client-side
	v4d::crypto::RSA GetServerPublicKey(const std::string& hostName, int port) {
		std::string rsaPEM = "";
		if (!app::isServer) {
			v4d::io::TextFile serverKeyFile(std::string("servers/") + hostName + "/rsa.pub");
			rsaPEM = serverKeyFile.Load();
			if (rsaPEM == "") {
				rsaPEM = app::Client{v4d::io::TCP}.GetServerPublicKey(hostName, port);
				if (rsaPEM != "") {
					serverKeyFile.text = std::stringstream(rsaPEM);
					serverKeyFile.WriteToFile();
				}
			}
		}
		return v4d::crypto::RSA{rsaPEM, false};
	}

}
