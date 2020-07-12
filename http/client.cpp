/**
 * Copyright (C) 2020 Tristan. All Rights Reserved.
 * This file is licensed under the BSD 2-Clause license.
 * See the COPYING file for licensing information.
 */

#include "client.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <cctype>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <strings.h>

#include "base/error_reporter.hpp"
#include "base/logger.hpp"
#include "base/media_type.hpp"
#include "base/strings.hpp"
#include "http/configuration.hpp"
#include "http/server.hpp"
#include "http/utils.hpp"
#include "io/file.hpp"
#include "io/file_resolver.hpp"
#include "security/policies.hpp"

[[nodiscard]] inline bool
StringStartsWith(const std::string &string, const std::string &prefix) {
#ifdef __cpp_lib_starts_ends_with
	return str.starts_with(str);
#else
	return std::mismatch(std::begin(prefix), std::end(prefix), std::begin(string), std::end(string)).first == std::end(prefix);
#endif
}

namespace HTTP {

Client::Client(Server *server, int sock) noexcept :
	connection(std::make_unique<Connection>(sock, server->config().useTransportSecurity)),
	server(server), thread(&Client::Entrypoint, this) {
}

ClientError
Client::CheckFileLocation(const std::string &path) const noexcept {
	std::array<char, PATH_MAX> dest{};
	if (realpath(path.c_str(), dest.data()) == nullptr) {
		return ClientError::CHECK_FILE_LOCATION_VERIFICATION_FAILURE;
	}

	if (!StringStartsWith(dest.data(), server->config().rootDirectory)) {
		return ClientError::CHECK_FILE_LOCATION_OUTSIDE_ROOT_DIRECTORY;
	}

	return ClientError::NO_ERROR;
}

void
Client::Clean() noexcept {
	connection = nullptr;

	server->SignalClientDeath(thread);
}

ClientError
Client::ConsumeCRLF() noexcept {
	char cr;
	char lf;
	if (!connection->ReadChar(&cr) || !connection->ReadChar(&lf)) {
		return ClientError::FAILED_READ_CRLF;
	}

	if (cr != '\r' || lf != '\n') {
		return ClientError::INCORRECT_CRLF;
	}

	return ClientError::NO_ERROR;
}

ClientError
Client::ConsumeHeaderField(char firstCharacter) noexcept {
	std::vector<char> fieldName;
	std::vector<char> fieldValue;
	ClientError subroutineError;

	/* Consume field-name */
	fieldName.push_back(firstCharacter);
	subroutineError = ConsumeHeaderFieldName(&fieldName);
	if (subroutineError != ClientError::NO_ERROR) {
		return subroutineError;
	}

	/* Consume OWS (Optional Whitespaces) */
	while (true) {
		char character;

		if (!connection->ReadChar(&character)) {
			return ClientError::FAILED_READ_HEADER_FIELD_GENERIC;
		}

		if (character != ' ' && character != '\t') {
			fieldValue.push_back(character);
			break;
		}
	}

	/* Consume header-value */
	subroutineError = ConsumeHeaderFieldValue(&fieldValue);
	if (subroutineError != ClientError::NO_ERROR) {
		return subroutineError;
	}

	/* Store in strings */
	fieldName.push_back('\0');
	fieldValue.push_back('\0');

	/* Trim end of OWS's. */
	auto spaceIterator = std::find(std::begin(fieldValue), std::end(fieldValue), ' ');
	auto tabIterator = std::find(std::begin(fieldValue), std::end(fieldValue), '\t');

	auto endIterator = spaceIterator < tabIterator ? spaceIterator : tabIterator;

	currentRequest.headers.insert({ std::string(fieldName.data()), std::string(std::begin(fieldValue), endIterator - 1) });
	return ClientError::NO_ERROR;
}

ClientError
Client::ConsumeHeaderFieldValue(std::vector<char> *dest) noexcept {
	/* obs-fold (optional line folding) isn't supported. */
	while (true) {
		/* Set next character */
		char character;

		if (!connection->ReadChar(&character)) {
			return ClientError::FAILED_READ_HEADER_FIELD_VALUE;
		}

		if (character == '\r') {
			if (!connection->ReadChar(&character)) {
				return ClientError::FAILED_READ_HEADER_NEWLINE;
			}
			if (character != '\n') {
				return ClientError::INCORRECT_HEADER_FIELD_NEWLINE;
			}
			return ClientError::NO_ERROR;
		}

		auto uc = static_cast<unsigned char>(character);
		if ((uc >= 0x21 && uc <= 0x7E) || // VCHAR
			(uc >= 0x80 && uc <= 0xFF) || // obs-text
			character == ' ' || character == '\t') {	// SP / HTAB
			dest->push_back(character);
		} else {
			return ClientError::INCORRECT_HEADER_FIELD_VALUE;
		}
	}
}

ClientError
Client::ConsumeHeaderFieldName(std::vector<char> *dest) noexcept {
	static const std::vector<char> unreservedCharacters
		= { '!', '#', '$', '%', '&', '\'', '*', '+', '-', '.', '^', '_', '`', '|', '~' };

	while (true) {
		char character;

		if (!connection->ReadChar(&character)) {
			return ClientError::FAILED_READ_HEADER_FIELD_NAME;
		}

		if (character == ':') {
			return ClientError::NO_ERROR;
		}

		if (std::find(std::begin(unreservedCharacters),
					  std::end(unreservedCharacters), character)
			!= std::end(unreservedCharacters) ||
			(character >= '0' && character <= '9') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= 'a' && character <= 'z')) {
			dest->push_back(std::tolower(character));
		} else {
			return ClientError::INCORRECT_HEADER_FIELD_NAME;
		}
	}
}

ClientError
Client::ConsumeHeaders() noexcept {
	do {
		char character;
		if (!connection->ReadChar(&character)) {
			return ClientError::FAILED_READ_HEADER_FIELD_NAME;
		}

		if (character == '\r') {
			if (!connection->ReadChar(&character)) {
				return ClientError::FAILED_READ_HEADER_NEWLINE;
			}
			if (character != '\n') {
				return ClientError::UNEXPECTED_CR_IN_FIELD_NAME;
			}
			break;
		}

		auto error = ConsumeHeaderField(character);
		if (error != ClientError::NO_ERROR) {
			return error;
		}
	} while (true);

	return ClientError::NO_ERROR;
}

ClientError
Client::ConsumeMethod() noexcept {
	std::vector<char> buffer;

	// Reserve 4 octets because GET & POST fit in 4 octets, so no reallocation
	// is needed.
	buffer.reserve(4);

	while (true) {
		char character = 0;

		if (!connection->ReadChar(&character)) {
			return ClientError::FAILED_READ_METHOD;
		}

		if (character == ' ') {
			if (buffer.empty()) {
				return ClientError::EMPTY_METHOD;
			}
			this->currentRequest.method = std::string(std::begin(buffer), std::end(buffer));
			return ClientError::NO_ERROR;
		}

		// Character validation
		if (!Utils::IsTokenCharacter(character)) {
			return ClientError::INCORRECT_METHOD;
		}

		buffer.push_back(character);
	}
}

ClientError
Client::ConsumePath() noexcept {
	std::vector<char> buffer;

	while (true) {
		char character = 0;

		if (!connection->ReadChar(&character)) {
			return ClientError::FAILED_READ_PATH;
		}

		if (character == ' ') {
			this->currentRequest.path = std::string(std::begin(buffer), std::end(buffer));
			return ClientError::NO_ERROR;
		}

		// Character validation
		if (!Utils::IsPathCharacter(character)) {
			return ClientError::INCORRECT_PATH;
		}

		buffer.push_back(character);
	}
}

ClientError
Client::ConsumeVersion() noexcept {
	std::array<char, 8> buffer{};

	std::array<char, 7> expectedChars = {
		'H', 'T', 'T', 'P', '/', '1', '.'
	};

	for (std::size_t i = 0; i < 8; i++) {
		if (!connection->ReadChar(&buffer[i])) {
			return ClientError::FAILED_READ_VERSION;
		}

		if (i == 7 ? !Utils::IsNumericCharacter(buffer[i]) : (buffer[i] != expectedChars[i])) {
			return ClientError::INCORRECT_VERSION;
		}
	}

	// Not storing the version atm.

	return ClientError::NO_ERROR;
}

void
Client::Entrypoint() {
	// Ignore SIGPIPE ~= accessing closed connection
	std::signal(SIGPIPE, SIG_IGN);

	if (!connection->Setup(server->config())) {
		Logger::Error("Client::Entrypoint", "Failed to setup connection!");
		Clean();
		return;
	}

	bool previousRequestSuccess;

	do {
		previousRequestSuccess = RunMessageExchange();
		ResetExchangeState();
	} while (previousRequestSuccess && persistentConnection);

	Clean();
}

ClientError
Client::ExtractComponentsFromPath() noexcept {
	std::string path(currentRequest.path);
	auto questionMark = path.find('?');

	if (questionMark == std::string::npos) {
		return ClientError::NO_ERROR;
	}

	if (questionMark != path.rfind('?')) {
		// Multiple ?'s in path
		return ClientError::INVALID_PATH_MULTIPLE_QUESTION_MARKS;
	}

	currentRequest.path = path.substr(0, questionMark);
	currentRequest.query = path.substr(questionMark + 1, path.length() - 1 - questionMark);

	return ClientError::NO_ERROR;
}

ClientError
Client::HandleRequest() noexcept {
	const auto maxRequests = server->config().securityPolicies.maxRequestsPerConnection;
	if (!server->config().securityPolicies.maxRequestsCloseImmediately &&
		maxRequests != 0 && ++requestCount > maxRequests) {
		return ClientError::TOO_MANY_REQUESTS_PER_THIS_CONNECTION;
	}

	auto file = server->fileResolver.Resolve(currentRequest);

	if (!file) {
		return ClientError::FILE_NOT_FOUND;
	}

	auto error = CheckFileLocation(file->Path());
	if (error != ClientError::NO_ERROR) {
		return error;
	}

	if (!SendMetadata(Strings::StatusLines::OK, file->Size(), server->config().mediaTypeFinder.DetectMediaType(file))) {
		return ClientError::FAILED_WRITE_RESPONSE_METADATA;
	}

	if (currentRequest.method != "HEAD" &&
		!connection->SendFile(file->Handle(), file->Size())) {
		perror("HandleRequest");
		return ClientError::FAILED_WRITE_RESPONSE_BODY;
	}

	return ClientError::NO_ERROR;
}

void
Client::InterpretConnectionHeaders() noexcept {
	if (persistentConnection) {
		auto header = currentRequest.headers.find("connection");
		if (header != std::end(currentRequest.headers) &&
			strcasecmp(header->second.c_str(), "close") == 0) {
			MarkConnectionClosing();
		}
	}
}

void
Client::MarkConnectionClosing() noexcept {
	persistentConnection = false;
}

bool
Client::RecoverError(ClientError error) noexcept {
	static const std::string indexPathTarget("/index.html");

	switch (error) {
		case ClientError::FILE_NOT_FOUND:
			if (StringStartsWith(indexPathTarget, currentRequest.path)) {
				return ServeDefaultPage();
			}
			ErrorReporter::ReportError(ErrorReporter::Error::FILE_NOT_FOUND, "Path='" + currentRequest.path + '\'');
			return RecoverErrorFileNotFound();
		case ClientError::EMPTY_METHOD:
			return RecoverErrorBadRequest(Strings::BadRequests::EmptyMethod);
		case ClientError::INCORRECT_HEADER_FIELD_NAME:
			return RecoverErrorBadRequest("invalid header field-name");
		case ClientError::INCORRECT_HEADER_FIELD_NEWLINE:
			return RecoverErrorBadRequest("expected newline (CRLF) after header field");
		case ClientError::INCORRECT_HEADER_FIELD_VALUE:
			return RecoverErrorBadRequest("invalid header field-value");
		case ClientError::INCORRECT_METHOD:
			return RecoverErrorBadRequest("invalid method: not a token as per RFC 7230 section 3.2.6");
		case ClientError::INCORRECT_PATH:
			return RecoverErrorBadRequest("incorrect request-target");
		case ClientError::INCORRECT_CRLF:
			return RecoverErrorBadRequest("request-line should end with a newline (CRLF)");
		case ClientError::INCORRECT_VERSION:
			return RecoverErrorBadRequest("invalid HTTP version as per RFC 7230 section 2.6");
		case ClientError::INVALID_PATH_EMPTY:
			return RecoverErrorBadRequest("request-target was empty");
		case ClientError::INVALID_PATH_NOT_ABSOLUTE:
			return RecoverErrorBadRequest("only absolute-path request-target supported");
		case ClientError::TOO_MANY_REQUESTS_PER_THIS_CONNECTION:
			return RecoverErrorTooManyRequestsPerThisConnection();
		default:
			break;
	}

	std::stringstream test;
	test << "Error Occurred: " << error << '\n';
	Logger::Info("HTTPClient::RecoverError", test.str());
	return false;
}

bool
Client::RecoverErrorBadRequest(const std::string &message) noexcept {
	const std::string body = "Malformed request: " + message;

	// Because the request parsing has abruptly failed, the connection is
	// useless.
	MarkConnectionClosing();

	return ServeStringRequest(Strings::StatusLines::BadRequest, MediaTypes::TEXT, body);
}

bool
Client::RecoverErrorFileNotFound() noexcept {
	return ServeStringRequest(Strings::StatusLines::NotFound, MediaTypes::HTML, Strings::NotFoundPage);
}

bool
Client::RecoverErrorTooManyRequestsPerThisConnection() noexcept {
	return ServeStringRequest(Strings::StatusLines::TooManyRequests, MediaTypes::HTML, Strings::TooManyRequestsPage);
}

void
Client::ResetExchangeState() noexcept {
	this->currentRequest = Request();

	const auto maxRequests = server->config().securityPolicies.maxRequestsPerConnection;
	if (server->config().securityPolicies.maxRequestsCloseImmediately && maxRequests != 0 && ++requestCount >= maxRequests) {
		// Close the connection.
		MarkConnectionClosing();
	}
}

bool
Client::RunMessageExchange() noexcept {
	auto error = ConsumeMethod();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	error = ConsumePath();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	error = ValidateCurrentRequestPath();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	error = ExtractComponentsFromPath();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	error = ConsumeVersion();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	error = ConsumeCRLF();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	error = ConsumeHeaders();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	InterpretConnectionHeaders();

	error = HandleRequest();
	if (error != ClientError::NO_ERROR) {
		return RecoverError(error);
	}

	return true;
}

bool
Client::SendMetadata(const std::string &response, std::size_t contentLength, const MediaType &mediaType) noexcept {
	std::stringstream metadata;
	metadata << response;
	metadata << "\r\nContent-Length: " << contentLength;
	metadata << "\r\nServer: " << server->config().serverProductName;
	metadata << "\r\nConnection: " << (persistentConnection ? "keep-alive" : "close");
	metadata << "\r\nContent-Type: " << mediaType.completeType;

	if (mediaType.includeCharset) {
		metadata << ";charset=utf-8\r\n\r\n";
	} else {
		metadata << "\r\n\r\n";
	}

	return connection->WriteString(metadata.str());
}

bool
Client::ServeDefaultPage() noexcept {
	return ServeStringRequest(Strings::StatusLines::OK, MediaTypes::HTML, Strings::DefaultWebPage);
}

bool
Client::ServeStringRequest(const std::string &responseLine,
							const MediaType &type,
							const std::string &body) noexcept {
	if (!SendMetadata(responseLine, body.length(), type)) {
		return false;
	}

	if (currentRequest.method == "HEAD") {
		return true;
	}

	return connection->WriteString(body);
}

ClientError
Client::ValidateCurrentRequestPath() const noexcept {
	if (currentRequest.path.empty()) {
		return ClientError::INVALID_PATH_EMPTY;
	}

	// We should preferably support:
	//   - * for the OPTIONS method.
	//   - the 'absolute-form' request-target type
	//   -
	if (currentRequest.path[0] != '/') {
		return ClientError::INVALID_PATH_NOT_ABSOLUTE;
	}

	return ClientError::NO_ERROR;
}

} // namespace HTTP
