#pragma once

#include <cgicc/Cgicc.h>
#include <mariadb/conncpp.hpp>
#include <string>
#include <vector>

// array that provides a sample set of service options for selectors  
static const std::vector<std::string> services = {
	"Gardening", "Plumbing", "Landscaping",
	"Cleaning", "Painting", "Carpentry",
	"Electrical Work", "IT Services"
};

class SessionManager {
	private:
		sql::Connection* conn;

		std::string generateSessionId();
		std::string getSessionCookie(cgicc::Cgicc& cgi);
	
	public:
		// constructor, takes the db connection from the CGI page
		SessionManager(sql::Connection* dbConn);

		// defines the format for storing the session data returned by validateSession()
		struct SessionData {
			std::string sessionId;
			std::string username;
			std::string role;
		};

		std::string createSession(cgicc::Cgicc& cgi, const std::string& username, const std::string& role);
		SessionData validateSession(cgicc::Cgicc& cgi);
		void destroySession(cgicc::Cgicc& cgi);
};

// global utility functions
bool posixMatch(const std::string& input, const std::string& pattern);
bool isFormSubmission(cgicc::Cgicc& cgi, const std::string& fieldName);
std::string sanitiseInput(const std::string& input, const std::string& elementName);
void redirectByRole(const std::string& role);
int authenticateAdmin(const int secretKey, const int inputtedCode);