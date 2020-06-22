/**
 * Copyright (C) 2020 Tristan. All Rights Reserved.
 * This file is licensed under the BSD 2-Clause license.
 * See the COPYING file for licensing information.
 */

#include <iostream>

#include <cstdlib>

#include "base/logger.hpp"
#include "http/configuration.hpp"
#include "http/server.hpp"

int
main() {
	HTTP::Configuration config;
	// Change config before passing it to the HTTP::Server

	HTTP::Server server(config);
	server.Start();

	Logger::Info("Main", "Server Started");

	std::string test;
	std::cin >> test;

	Logger::Info("Main", "Stopping...");

	server.SignalShutdown();
	server.Join();

	Logger::Info("Main", "Stopped!");
	return EXIT_SUCCESS;
}
