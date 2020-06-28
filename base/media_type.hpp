#pragma once

/**
 * Copyright (C) 2020 Tristan. All Rights Reserved.
 * This file is licensed under the BSD 2-Clause license.
 * See the COPYING file for licensing information.
 */

#include <map>
#include <memory>
#include <string>

#include "io/file.hpp"

struct MediaType {
	const std::string type;
	const std::string subtype;
	const std::string completeType;
	const bool includeCharset;

	inline MediaType(const std::string &type, const std::string &subtype,
		bool includeCharset)
		: type(type), subtype(subtype), completeType(type + '/' + subtype),
		  includeCharset(includeCharset) {
	}

	inline MediaType(const std::string &type, const std::string &subtype)
		: MediaType(type, subtype, type == "text") {
	}
};

class MediaTypeFinder {
public:
	MediaTypeFinder() noexcept;

	[[nodiscard]] const MediaType &
	DetectMediaType(const std::unique_ptr<IO::File> &) const noexcept;

private:
	std::map<std::string, MediaType> mediaTypes;
};