#include <ctime>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTTPRedirectHeader.h>
#include <random>
#include <regex.h>
#include "../include/utils.h"

using namespace std;

SessionManager::SessionManager(sql::Connection* dbConn) : conn(dbConn) {}

string SessionManager::generateSessionId() {
	/*
	DESC.: generates a 32-character hexadecimal string that serves as a unique session ID
	RETURNS: string containing 32 random characters from a hexadecimal set [0-9A-F] 

	- the writing of this function was informed by https://medium.com/@ryan_forrester_/c-random-string-generation-practical-guide-e7e789b348d4
	- use of static ensures RNG components are setup once, not each time the function is called
	*/

	const string characters = "0123456789ABCDEF";

	// RNG that uses OS's entropy source to provide a non-deterministic seed for mt19937
	static random_device rd;
	
	// PRNG initialised with hardware-based random seed
	static mt19937 generator(rd());

	// maps the output from mt19937 to a valid index within the range of the "characters" string
	static uniform_int_distribution<> dis(0, characters.size() - 1);

	string sessionId;
	for (size_t i = 0; i < 32; i++) {
		sessionId += characters[dis(generator)];
	}

	return sessionId;
}

string SessionManager::getSessionCookie(cgicc::Cgicc& cgi) {
	/*
	DESC.: retrieves session_id cookie from HTTP request headers, validates it, and returns it
	RETURNS: 32-character session ID string if found and valid
					 empty string if cookie is missing or invalid
	*/

	// valid session_id cookie will be 32 hexadecimal characters
	string validCookieRE = "^[0-9A-F]{32}$";
	const cgicc::CgiEnvironment& env = cgi.getEnvironment();

	for (const auto& cookie : env.getCookieList()) {
		if (cookie.getName() == "session_id") {
			string cookieVal = cookie.getValue();

			if (posixMatch(cookieVal, validCookieRE)) {
				// found and validated session_id cookie, return early
				return cookieVal;
			}
		}
	}

	return "";
}

string SessionManager::createSession(cgicc::Cgicc& cgi, const string& username, const string& role) {
	/*
	SR3

	DESC.: establishes a new session by generating a unique session ID, storing it in the db,
				 and setting a browser cookie
	RETURNS: sessionId string if session create successfully
					 an empty string if a database error occurred
	*/

	string sessionId = generateSessionId();

	try {
		unique_ptr<sql::PreparedStatement> sessionStmnt(conn->prepareStatement(
			"INSERT INTO sessions (session_id, user_id, role) "
			"VALUES (?, (SELECT id FROM users WHERE username = ?), ?)"
		));
		sessionStmnt->setString(1, sessionId);
		sessionStmnt->setString(2, username);
		sessionStmnt->setString(3, role);
		sessionStmnt->execute();

		return sessionId;

	} catch (sql::SQLException& e) {
			cerr << "SQL error occured while creating session: " << e.what() << endl;

			return "";
	}
}

SessionManager::SessionData SessionManager::validateSession(cgicc::Cgicc& cgi) {
	/*
	SR2 - destroys session if last activity surpasses interval
	SR3 - assists in maintaining the session by updating the last_active time
	SR4 - used on all pages to ensure user has valid session before rendering content

	DESC.: validates the user's session by checking the browser cookie against the db and 
				 verifying the session is not expired
	RETURNS: SessionData struct with username, role and sessionId if valid
					 empty SessionData struct if missing, expired, or invalid
	*/

	SessionData data;
	string sessionId = getSessionCookie(cgi);

	// if no valid session_id cookie is found, user needs to return to login
	if (sessionId.empty()) {
		return data;
	}

	try {
		// check if session exists and is less than 10 mintues old
		unique_ptr<sql::PreparedStatement> validateStmnt(conn->prepareStatement(
			"SELECT u.username, s.role FROM sessions s "
			"JOIN users u ON s.user_id = u.id "
			"WHERE s.session_id = ? AND s.last_active > NOW() - INTERVAL 10 MINUTE"
		));
		validateStmnt->setString(1, sessionId);

		unique_ptr<sql::ResultSet> validateRes(validateStmnt->executeQuery());

		if (validateRes->next()) {
			data.sessionId = sessionId;
			data.username = validateRes->getString("username");
			data.role = validateRes->getString("role");

			// update the last_active timestamp to extend the session
			unique_ptr<sql::PreparedStatement> updateStmnt(conn->prepareStatement(
				"UPDATE sessions SET last_active = CURRENT_TIMESTAMP WHERE session_id = ?"
			));
			updateStmnt->setString(1, sessionId);
			updateStmnt->execute();

			return data;
		}
	} catch (sql::SQLException& e) {
			cerr << "SQL error occured while validating session: " << e.what() << endl;
	}

	// if cookie exists but db entry is absent or expired, then return empty struct
	return data;
}

void SessionManager::destroySession(cgicc::Cgicc& cgi) {
	/*
	SR2 - efficiently logs out users, clearing session and redirecting to login
	SR3 - assists in maintaining the session 

	DESC.: removes a session from both the db storage and the user's browswer, and returns user to login
	RETURNS: void

	- though it is the official suggestion on the cgicc site, .setMaxAge(0) does not work for expiring the cookie
	  however, the commented-out raw HTML does work, but this disallows me from using the proper HTTPRedirectHeader(), so
		I've left it as is, setting the cookie value to "" and clearing the session from the db
	- the commented-out cookie deletion was informed by https://stackoverflow.com/questions/5285940/correct-way-to-delete-cookies-server-side
	*/

	string sessionId = getSessionCookie(cgi);

	if (!sessionId.empty()) {
		// if sessionId is found, try to clear the session entry
		try {
			unique_ptr<sql::PreparedStatement> delStmnt(conn->prepareStatement(
				"DELETE FROM sessions WHERE session_id = ?"
			));
			delStmnt->setString(1, sessionId);
			delStmnt->execute();

		} catch (sql::SQLException& e) {
				cerr << "SQL error occured while trying to clear session: " << e.what() << endl;
		}
	}

	// attempt to expire the stale session_id cookie and redirect
	cgicc::HTTPCookie staleCookie("session_id", "");
	staleCookie.setMaxAge(0);
	cout << cgicc::HTTPRedirectHeader("login.cgi").setCookie(staleCookie) << endl;

	// cout << "Set-Cookie: session_id=; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n";
	// cout << "Content-Type: text/html\r\n\r\n";
}

// MISCELLANEOUS UTILITY FUNCTIONS

bool posixMatch(const string& input, const string& pattern) {
	/*
	SR6 - assists in validating that user input matches a safe, functional format

	DESC.: uses POSIX regular expression match to validate incoming user input
	RETURNS: true if the string fully matches the regex pattern
					 false if no match was found

	- std::regex compiles but consistently causes an internal server error
	- reference for POSIX regex usage is found here https://man7.org/linux/man-pages/man3/regex.3.html
	*/

	// declares "storage area" for the compiled regex pattern
	regex_t compiled;

	// REG_EXTENDED gives Extended Regular Expression (ERE) syntax
	if (regcomp(&compiled, pattern.c_str(), REG_EXTENDED | REG_NOSUB) != 0) {
		return false;
	}

	// 0 and nullptr ignore substring capturing since we only need a pass/fail result
	int result = regexec(&compiled, input.c_str(), 0, nullptr, 0);

	// free the memory allocated for the compiled pattern
	regfree(&compiled);

	return result == 0;
}

bool isFormSubmission(cgicc::Cgicc &cgi, const std::string& fieldName) {
	/*
	DESC.: determines if the current request on any given page is a form submission
				by checking the HTTP request method
	RETURNS: true if the request is a POST
					 false if the request is a GET
	
	- fundamental for all pages, ensures logic and specific HTMl rendering
		only occurs when necessary
	*/

	// check for specific field element as some pages have multiple forms
	cgicc::form_iterator it = cgi.getElement(fieldName);

	// it == .end() would mean data for name="username" was never sent
	if (it != cgi.getElements().end()) {
		return true;
	}

	return false;
}

string sanitiseInput(const string& input, const string& elementName) {
	/*
	SR6 - deny-by-default policy, only input that matches the specific pattern for its type is allowed

	DESC.: validates incoming using input against element-specific regex patterns
	RETURNS: the original input if valid
					 an empty string if the input has invalid characters or length

	- defense in depth: works alongside HTML enforced regex patterns and parameterised
		SQL queries to ensure no invalid or malicious data reaches the db or DOM
	*/

	if (elementName == "username") {
		// 3-20 characters, excludes all characters except for alphanumeric and '_'
		// must contain at least one letter
		if (!posixMatch(input, "[a-zA-Z]") || !posixMatch(input, "^[a-zA-Z0-9_]{3,20}$")) {
			return "";
		}

		return input;
	}

	if (elementName == "email") {
		// 6-100 characters, (consider a@b.uk as a valid 6 character email)
		// must contain an '@' and a '.', excludes harmful characters like <> ; ' "
		if (!posixMatch(input, "^.{6,100}$") || !posixMatch(input, "^[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}$")) {
			return "";
		}

		return input;
	}

	if (elementName == "phone") {
		// 12-15 characters
		// must contain only numbers and an optional '+'
		if (!posixMatch(input, "^\\+?[0-9]{10,15}$")) {
			return "";
		}

		return input;
	}

	if (elementName == "location") {
		// 2-60 characters, excludes all characters except for alphanumeric, spaces, ', and '-'
		// must contain at least one letter
		if (!posixMatch(input, "[a-zA-Z]") || !posixMatch(input, "^[a-zA-Z[:space:]\\'\\-]{2,60}$")) {
			return "";
		}

		return input;
	}

	if (elementName == "password") {
		// 8-20 characters
		// in the interest of supporting secure passwords, all characters are allowed
		// as input will never be displayed in the DOM and SQL inputs are paramaterised
		if (!posixMatch(input, "^.{8,20}$")) {
			return "";
		}

		return input;
	}

	if (elementName == "code") {
		// exactly 6 digits
		if (!posixMatch(input, "^[0-9]{6}$")) {
			return "";
		}
		
		return input;
	}

 	return "";
}

void redirectByRole(const string& role) {
	/*
	SR4 - assists in redirecting users if they try to access a page their role does not have
				access to or if they are not yet logged in (no valid session)

	DESC.: redirects the user to a specific page based on their role or authentication status
	RETURNS: void
	*/

	string targetPage;

	if (role == "admin") {
		targetPage = "admin_homepage.cgi";
	} else if (role == "client") {
		targetPage = "client_homepage.cgi";
	} else if (role == "provider") {
		targetPage = "connection_page.cgi";
	} else if (role == "admin_pending") {
		targetPage = "admin_auth.cgi";
	} else {
		targetPage = "login.cgi";
	}

	cout << cgicc::HTTPRedirectHeader(targetPage) << endl;
}

int authenticateAdmin(const int secretKey, const int inputtedCode) {
	/*
	SR8

	DESC.: simulates a hardware token ("challenge-response") by mimicking TOTP flow
				- admin's standalone script and server possess a pre-shared secret key (the "something you have")
				- the system clock provides a synchronised "challenge" by calculating a number based on the current 30-second interval
				- this secret key and the time interval number are combined with XOR to create a unique shared seed
				- the shared seed initialises the mt19937 PRNG to ensure the pseudo-random output is identical on both devices
				- the generator produces a 6-digit code via the pre-defined uniform_int_distribution
				- this code is the "response"
	RETURNS: 0 if inputted code and computed code match
					 1 if codes do not match
	*/

	// the "challenge": integer divison ensures the same value for 30 seconds
	const int challenge = time(0) / 30;

	// XOR operator combines secret key with "challenge" (30-second interval number)
	const int sharedSeed = (secretKey ^ challenge);
	
	mt19937 generator(sharedSeed);
	uniform_int_distribution<>dis(100000, 999999);

	// the "response": generate the 6-digit code
	const int correctCode = dis(generator);

	if (correctCode == inputtedCode) {
		return 0;
	}

	return 1;
}