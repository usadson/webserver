/**
 * Copyright (C) 2020 Tristan. All Rights Reserved.
 * This file is licensed under the BSD 2-Clause license.
 * See the COPYING file for licensing information.
 */

#include <array>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "base/logger.hpp"
#include "base/media_type.hpp"
#include "cgi/manager.hpp"
#include "http/configuration.hpp"
#include "http/server.hpp"
#include "security/policies.hpp"
#include "security/process.hpp"
#include "security/tls_configuration.hpp"

bool
LoadTLSConfiguration(Security::TLSConfiguration &config);

bool
LoadHostName(HTTP::Configuration &config);

[[nodiscard]] bool
DropPrivileges(gid_t, uid_t) noexcept;

int
main() {
	CGI::Manager manager{};
	MediaTypeFinder mediaTypeFinder{};
	Security::Policies securityPolicies{};
	Security::TLSConfiguration tlsConfiguration{};

	if (!LoadTLSConfiguration(tlsConfiguration)) {
		Logger::Error("Main", "Failed to load TLS configuration");
		return EXIT_FAILURE;
	}

	HTTP::Configuration httpConfig1(mediaTypeFinder, securityPolicies, tlsConfiguration);
	HTTP::Configuration httpConfig2(mediaTypeFinder, securityPolicies, tlsConfiguration);

	if ((httpConfig1.hostname.empty() && !LoadHostName(httpConfig1)) ||
		(httpConfig1.hostname.empty() && !LoadHostName(httpConfig1))) {
		Logger::Error("Main", "Failed to retrieve hostname");
		return EXIT_FAILURE;
	}

	httpConfig1.rootDirectory = "/dev/null";
	httpConfig1.port = 80;
	httpConfig1.upgradeToHTTPS = true;
	httpConfig2.rootDirectory = "/var/www/html";
	httpConfig2.port = 443;
	httpConfig2.useTransportSecurity = true;

	HTTP::Server httpServer1(httpConfig1, manager);
	HTTP::Server httpServer2(httpConfig2, manager);

	if (!httpServer1.Initialize() ||
		!httpServer2.Initialize()) {
		Logger::Error("Main", "Failed to initialize servers");
		return EXIT_FAILURE;
	}

	if (!DropPrivileges(securityPolicies.privileges.groupID, securityPolicies.privileges.userID)) {
		return EXIT_FAILURE;
	}

	httpServer1.Start();
	httpServer2.Start();

	Logger::Log("Main", "Server Started");

	std::string test;
	std::cin >> test;

	Logger::Log("Main", "Stopping...");

	httpServer1.SignalShutdown();
	httpServer2.SignalShutdown();
	httpServer1.Join();
	httpServer2.Join();

	Logger::Log("Main", "Stopped!");

	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	return EXIT_SUCCESS;
}

bool
LoadTLSConfiguration(Security::TLSConfiguration &config) {
	auto *certificateFile = std::getenv("WS_TLS_CERT");

	if (!certificateFile) {
		Logger::Error("TLS Configuration", "WS_TLS_CERT not found in environment");
		return false;
	}

	auto *chainFile = std::getenv("WS_TLS_CHAIN");

	if (chainFile == nullptr) {
		Logger::Error("TLS Configuration", "WS_TLS_CHAIN not found in environment");
		return false;
	}

	auto *privateKeyFile = std::getenv("WS_TLS_PRIVATE_KEY");

	if (privateKeyFile == nullptr) {
		Logger::Error("TLS Configuration", "WS_TLS_PRIVATE_KEY not found in environment");
		return false;
	}

	config.certificateFile = certificateFile;
	config.chainFile = chainFile;
	config.privateKeyFile = privateKeyFile;

	config.cipherList =
		"ECDHE-ECDSA-AES128-GCM-SHA256:"
		"ECDHE-RSA-AES128-GCM-SHA256:"
		"ECDHE-ECDSA-AES256-GCM-SHA384:"
		"ECDHE-RSA-AES256-GCM-SHA384:"
		"ECDHE-ECDSA-CHACHA20-POLY1305:"
		"ECDHE-RSA-CHACHA20-POLY1305:"
		"DHE-RSA-AES128-GCM-SHA256:"
		"DHE-RSA-AES256-GCM-SHA384";

	return config.CreateContext() && config.context != nullptr;
}

bool
LoadHostName(HTTP::Configuration &config) {
	auto *envHostName = std::getenv("WS_HOSTNAME");

	if (envHostName) {
		config.hostname = std::string(envHostName);
		return false;
	}


	std::array<char, 256> buffer;

	if (gethostname(buffer.data(), buffer.size()) != 0) {
		perror("LoadHostName gethostname(2)");
		return false;
	}

	config.hostname = std::string(buffer.data());
	return true;
}

bool
DropPrivileges(gid_t group, uid_t user) noexcept {
	auto privilegeStatus = Security::Process::DropPrivileges(group, user);

	if (privilegeStatus != Security::PrivilegesStatus::OK) {
		std::stringstream stream;
		stream << "Failed to drop privileges: ";
		switch (privilegeStatus) {
			case Security::PrivilegesStatus::SWITCHABLE_TO_SUPERUSER:
				stream << "switchable to superuser's user";
				break;
			case Security::PrivilegesStatus::SWITCHABLE_TO_SUPERUSER_GROUP:
				stream << "switchable to superuser's group";
				break;
			case Security::PrivilegesStatus::UNABLE_DROP_GROUP:
				stream << "unable to drop group";
				break;
			case Security::PrivilegesStatus::UNABLE_DROP_USER:
				stream << "unable to drop user";
				break;
		}
		Logger::Error("Main", stream.str());
		return false;
	}

	return true;
}
