#include <cgicc/Cgicc.h>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTTPRedirectHeader.h>
#include <cgicc/HTMLClasses.h>
#include "../include/layout.h"
#include "../include/utils.h"
#include "../include/db_utils.h"

using namespace std;

LoginData extractFormData(cgicc::Cgicc &cgi) {
	/*
	SR6 - assists in the sanitisation process by ensuring inputs that bypassed the HTML
				patterns are detected and flagged (by replacing the input with an empty string)

	DESC.: extracts and sanitises (via sanitiseInput()) login credentials from the POST request
	RETURNS: LoginData object containing the processed username and password
	*/

	LoginData data;

	data.username = sanitiseInput(cgi("username"), "username");
	data.password = sanitiseInput(cgi("password"), "password");

	return data;
}

string getValidationError(const LoginData& data) {
	/*
	SR6 - assists in the sanitisation proces by ensuring invalid input is flagged and warned against

	DESC.: validates that input fields are not empty after sanitisation, producing an error
				 message for the first empty input field
	RETURNS: a specific error message string if a field is invalid
					 "none" if no empty input was found
	*/

	if (data.username.empty()) {
		return "Invalid username. Please enter 3-20 characters. Letters, numbers, and underscores only.";
	}

	if (data.password.empty()) {
		return "Invalid password. Please enter 8-20 characters.";
	}

	return "none";
}

int main() {
	/*
	SR1

	DESC.: orchestrates the login workflow, including credential verification, 
				 2FA authentication, session creation, and incorrect/invalid login attempts
	RETURNS: 0 on successful execution
					 1 if a db error occurs
	*/

	try {
		cgicc::Cgicc cgi;

		unique_ptr<sql::Connection> conn(createDbConnection());
		DatabaseManager db(conn.get());
		SessionManager mgr(conn.get());

		// if user already has a valid session, redirect to homepage for their role
		SessionManager::SessionData sessionData = mgr.validateSession(cgi);
		if (!sessionData.username.empty()) {
			redirectByRole(sessionData.role);
			return 0;
		}

		// must process any login attempt before outputting headers
		bool loginAttempt = isFormSubmission(cgi, "username");
		string bannerMsg;

		if (loginAttempt) {
			LoginData loginData = extractFormData(cgi);
			bannerMsg = getValidationError(loginData);

			// if validation passes, proceed to db check
			if (bannerMsg == "none") {
				// log in user and get user's role
				string loginResult = db.loginUser(loginData);

				if (loginResult == "client" || loginResult == "provider" || loginResult == "admin") {
					// if user is an admin, create pending session and redirect to challenge-response
					string role = (loginResult != "admin") ? loginResult : "admin_pending";
					
					string sessionId = mgr.createSession(cgi, loginData.username, role);
					if (sessionId != "") {
						// if user logs in successfully and session is created, set cookie and redirect by role
						cgicc::HTTPCookie sessionCookie("session_id", sessionId);
						string targetPage;

						if (role == "admin_pending") {
							targetPage = "admin_auth.cgi";
						} else if (role == "client") {
							targetPage = "client_homepage.cgi";
						} else if (role == "provider") {
							targetPage = "connection_page.cgi";
						}
						
						cout << cgicc::HTTPRedirectHeader(targetPage).setCookie(sessionCookie) << endl;
						
						return 0;
					}

					// session creation failed
					bannerMsg = "db";
				} else {
						// if error occurs in loginUser
						bannerMsg = loginResult != "error" ? loginResult : "db";
				}
			}
		}

		cout << cgicc::HTTPHTMLHeader() << endl;
		
		cout << cgicc::html().set("lang", "en") << "\n";
			cout << renderHeader("Handyman - Account Login");

			cout << cgicc::body() << "\n";
				// check if error message needs to be displayed
				if (!bannerMsg.empty() && bannerMsg != "none") {
					cout << renderNotiBanner(bannerMsg);
				}

				// title for page
				cout << cgicc::div().set("class", "header") << "\n";
					cout << cgicc::h1("Log In to Your Account") << "\n";
				cout << cgicc::div() << "\n";

				// divs must be hard coded as trying to use cgicc
				// syntax to open a nested div will simply close the parent div
				cout << "<div class=\"container\">" << "\n";
					// login fields
					cout << cgicc::form().set("method", "POST").set("action", "login.cgi") << "\n";
						cout << "<div class=\"card\">" << "\n";
								// SR6 - all fields enforce regex patterns as a first line of defense

								cout << cgicc::p() << "Username: " 
										<< cgicc::input().set("type", "text")
																			.set("name", "username").set("placeholder", "Enter 3-20 characters")
																			.set("title", "3-20 characters. Letters, numbers, and underscores only")
																			.set("pattern", "^(?=.*[a-zA-Z])[a-zA-Z0-9_]{3,20}$")
																			.set("required", "") 
										<< cgicc::p() << "\n";

								cout << cgicc::p() << "Password: " 
										<< cgicc::input().set("type", "password")
																			.set("name", "password")
																			.set("placeholder", "Enter 8-20 characters")
																			.set("title", "8-20 characters")
																			.set("pattern", "^.{8,20}$")
																			.set("required", "") 
										<< cgicc::p() << "\n";

						cout << "</div>"<< "\n";

						cout << cgicc::button(cgicc::h2("Login")).set("type", "submit").set("class", "submit-button")
								<< "\n";

					cout << cgicc::form() << "\n";
				cout << "</div>" << "\n";
			cout << cgicc::body() << "\n";
		cout << cgicc::html() << endl;

		return 0;	

	} catch (exception& e) {
			cerr << "Exception occurred: " << e.what() << endl;

			return 1;
  }
}