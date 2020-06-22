/**
 * Copyright (C) 2020 Tristan. All Rights Reserved.
 * This file is licensed under the BSD 2-Clause license.
 * See the COPYING file for licensing information.
 */

#include "server_launch_error.hpp"

#include <array>
#include <string>

const std::array<std::string, 4> launchErrorNames = {
	"NO_ERROR",
	"SOCKET_BIND",
	"SOCKET_CREATION",
	"SOCKET_REUSABLE",
};

std::ostream &
operator<<(std::ostream &stream, HTTP::ServerLaunchError error) {
	return stream << launchErrorNames[static_cast<std::size_t>(error)];
}
