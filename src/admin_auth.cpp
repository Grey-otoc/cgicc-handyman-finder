#include <cgicc/Cgicc.h>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTMLClasses.h>
#include "../include/layout.h"
#include "../include/utils.h"
#include "../include/db_utils.h"

using namespace std;

int main() {
	/*
	SR8 - requires a 6-digit TOTP code computed using the pre-shared secret key

	DESC.: orchestrates the admin challenge-response workflow by implementing the
				 TOTP logic found in authenticateAdmin() in utils.cpp and promoting the user's role
				 in the database upon successful validation
	RETURNS: 0 on success
					 1 if a db error occurs
	*/

	try {
		cgicc::Cgicc cgi;

		unique_ptr<sql::Connection> conn(createDbConnection());
		DatabaseManager db(conn.get());
		SessionManager mgr(conn.get());

		// if session is invalid, redirect to login
		SessionManager::SessionData sessionData = mgr.validateSession(cgi);
		if (sessionData.role != "admin_pending") {
			redirectByRole("");

			return 0;
		}

		// check for code submission attempt
		string bannerMsg;
		bool authSuccess = false;

		if (isFormSubmission(cgi, "code")) {
			string inputtedCode = sanitiseInput(cgi("code"), "code");
			int secretKey = db.getAdminSecretKey(sessionData.username);

			// verify code format
			if (inputtedCode.empty()) {
				bannerMsg = "Code must be exactly 6 digits.";
			}

			// verify secret key was retrieved
			else if (secretKey == -1) {
				bannerMsg = "db";
			}

			// validate inputted code matches computed code
			else if (authenticateAdmin(secretKey, stoi(inputtedCode)) != 0) {
				bannerMsg = "Code incorrect. Please try again.";
			}

			// update admin's role in session entry to no longer be pending
			else if (db.promoteAdminRole(sessionData.sessionId) != 0) {
				bannerMsg = "db";
			} else {
				authSuccess = true;
			}
		}

		// challenge-response passed, redirect to admin_homepage
		if (authSuccess) {
			redirectByRole("admin");

			return 0;
		}

		cout << cgicc::HTTPHTMLHeader() << endl;

		cout << cgicc::html().set("lang", "en") << "\n";
			cout << renderHeader("Handyman - Admin Authentication");

			cout << cgicc::body() << "\n";
				// check if there is a banner message to display
				if (!bannerMsg.empty()) {
					cout << renderNotiBanner(bannerMsg);
				}

				// title for page
				cout << cgicc::div().set("class", "header") << "\n";
					cout << cgicc::h1("Generate and Enter Your Authenticator Code") << "\n";
				cout << cgicc::div() << "\n";

				cout << cgicc::form().set("method", "POST").set("action", "admin_auth.cgi") << "\n";
					// nested divs must be hard coded as trying to use cgicc syntax to open a 
					// nested div will simply close the parent div
					cout << "<div class=\"container\">" << "\n";
						cout << "<div class=\"card\">" << "\n";
							// SR6 - enforces 6-digit pattern

							cout << cgicc::p() << "Code: " 
									<< cgicc::input().set("type", "text")
																		.set("name", "code")
																		.set("placeholder", "Enter exactly 6 digits")
																		.set("title", "6 characters. Numbers only.")
																		.set("pattern", "[0-9]{6}")
																		.set("required", "")
									<< cgicc::p() << "\n";

						cout << "</div>"<< "\n";

						// update button
						cout << cgicc::button(cgicc::h2("Validate")).set("type", "submit").set("class", "submit-button")
								 << "\n";
					cout << "</div>" << "\n";
				cout << cgicc::form() << "\n";
			cout << cgicc::body() << "\n";
		cout << cgicc::html() << "\n";

		return 0;

	} catch (exception& e) {
			cerr << "Exception occurred: " << e.what() << endl;

      return 1;
	}
}