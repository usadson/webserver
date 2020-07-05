/**
 * Copyright (C) 2020 Tristan. All Rights Reserved.
 * This file is licensed under the BSD 2-Clause license.
 * See the COPYING file for licensing information.
 */

#include "strings.hpp"

namespace Strings {

const std::string DefaultWebPage = "<!doctype html><html lang=\"en\"><head><meta name=\"author\" content=\"Tristan\"><meta charset=\"utf-8\"><meta name=\"description\" content=\"It works! This is a default webpage.\"><title>Wizard Web Server - Default Webpage</title><style>*{font-family:\"Noto Sans\",\"Calibri\",\"Roboto\",sans-serif}html{background-color:#ddd;margin:0;height:100%}body{height:100%;margin:0}main{margin:0 10%;height:100%;width:80%;background-color:#fff}h1{padding-top:10%;margin:0;text-align:center}div{margin:0 6%}</style></head><body><main><h1>Wizard Web Server</h1><div><h3>It works!</h3><p>The fact that you can read this means that the web server has been configured correctly!</p><p>Now go ahead and change this page to your personal homepage :)</p></div><div><h3>About</h3><p>This is a default page, served to you by the <a href=\"https://github.com/usadson/WebServer\">Wizard Web Server</a>, a free BSD-3-Clause licensed webserver.</p></div></main></body></html>";
const std::string NotFoundPage = "<!doctype html>"
								 "<html>"
								 "<head><title>File Not Found</title></head>"
								 "<body><h1>File Not Found</h1></body>"
								 "</html>";
const std::string TooManyRequestsPage = "<!doctype html>"
								 "<html>"
								 "<head><title>Too Many Requests</title></head>"
								 "<body><h1>Too Many Requests!</h1></body>"
								 "</html>";


namespace Response {
const std::string BadRequest = "HTTP/1.1 400 Bad Request";
const std::string NotFound = "HTTP/1.1 404 Not Found";
const std::string OK = "HTTP/1.1 200 OK";
const std::string TooManyRequests = "HTTP/1.1 429 Too Many Requests";
}

} // namespace Strings
