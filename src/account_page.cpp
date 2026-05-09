#include <cgicc/Cgicc.h>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTMLClasses.h>
#include "../include/layout.h"
#include "../include/utils.h"
#include "../include/db_utils.h"

using namespace std;

ContactData extractFormData(cgicc::Cgicc &cgi) {
	/*
	SR6 - assists in the sanitisation process by ensuring inputs that bypassed the HTML
				patterns are detected and flagged (by replacing the input with an empty string)

	DESC.: extracts and sanitises (via sanitiseInput()) contact details from the POST request
	RETURNS: ContactData object containing the processed contact details
	*/

	ContactData proposedContactData;

	proposedContactData.email = sanitiseInput(cgi("email"), "email");
	proposedContactData.phone = sanitiseInput(cgi("phone"), "phone");
	proposedContactData.location = sanitiseInput(cgi("location"), "location");

	return proposedContactData;
}

string getValidationError(const ContactData& proposedContactData) {
	/*
	SR6 - assists in the sanitisation proces by ensuring invalid input is flagged and warned against

	DESC.: validates that input fields are not empty after sanitisation, producing an error
				 message for the first empty input field
	RETURNS: a specific error message string if a field is invalid
					 "none" if no empty input was found
	*/

	if (proposedContactData.email.empty()) {
		return "Invalid email. Please enter a valid email address between 6-100 characters.";
	}

	if (proposedContactData.phone.empty()) {
		return "Invalid phone number. Please enter 10-15 digits with an optional country code e.g. +447350202133.";
	}

	if (proposedContactData.location.empty()) {
		return "Invalid location. Please enter a city name between 2-60 characters. Letters, spaces, hyphens, and apostrophes only.";
	}

	return "none";
}

int main() {
	/*
	FR2

	DESC.: provides a dedicated page where all users can view and update their contact details
	RETURNS: 0 on success
					 1 if a db error occurs
	*/

	try {
		cgicc::Cgicc cgi;

		unique_ptr<sql::Connection> conn(createDbConnection());
		DatabaseManager db(conn.get());
		SessionManager mgr(conn.get());

		// if session is invalid, destroy session (which clears stale session if
		// necessary and redirects to login)
		SessionManager::SessionData sessionData = mgr.validateSession(cgi);
		if (sessionData.username.empty()) {
			mgr.destroySession(cgi);

			return 0;
		}

		// if user requests a logout, destroy session
		if (isFormSubmission(cgi, "logout")) {
			mgr.destroySession(cgi);

			return 0;
		}

		string bannerMsg;
		// get contact information from database
		ContactData currContactData = db.getContactInfo(sessionData.username);

		if (isFormSubmission(cgi, "email")) {
			ContactData proposedContactData = extractFormData(cgi);
			
			// only continue if the proposedContactData actually differs from the stored contact data
			bool isChanged = (proposedContactData.email != currContactData.email ||
												proposedContactData.phone != currContactData.phone ||
												proposedContactData.location != currContactData.location);

			if (isChanged) {
				bannerMsg = getValidationError(proposedContactData);

				if (bannerMsg == "none") {
					// attempt to update user info in database
					if (db.updateContactInfo(sessionData.username, proposedContactData) == 0) {
						bannerMsg = "Updated contact information successfully!";

						// sync current data so the form shows the new values
						currContactData = proposedContactData;
					} else {
							bannerMsg = "db";
					}
				}
			}
		}

		cout << cgicc::HTTPHTMLHeader() << endl;

		cout << cgicc::html().set("lang", "en") << "\n";
			cout << renderHeader("Handyman - Account Details");

			cout << cgicc::body() << "\n";
		
				// check if there is a banner message to display
				if (!bannerMsg.empty() && bannerMsg != "none") {
					cout << renderNotiBanner(bannerMsg);
				}

				// nav bar
				cout << renderNavBar("account_page.cgi", sessionData.role);

				// title for page
				cout << cgicc::div().set("class", "header") << "\n";
					cout << cgicc::h1("Update Contact Information") << "\n";
				cout << cgicc::div() << "\n";

				cout << cgicc::form().set("method", "POST").set("action", "account_page.cgi") << "\n";
					// nested divs must be hard coded as trying to use cgicc syntax to open a 
					// nested div will simply close the parent div
					cout << "<div class=\"container\">" << "\n";
						cout << "<div class=\"card\">" << "\n";
							// SR6 - all fields enforce regex patterns as a first line of defense

							cout << cgicc::p() << "Email: " 
									<< cgicc::input().set("type", "email")
																		.set("name", "email")
																		.set("placeholder", "Enter 6-100 characters")
																		.set("value", currContactData.email)
																		.set("title", "6-100 characters. Must contain an @ and a . to be considered valid")
																		.set("pattern", "^(?=.{6,100}$)[a-zA-Z0-9\\._\\%\\+\\-]+@[a-zA-Z0-9\\.\\-]+\\.[a-zA-Z]{2,}$")
																		.set("required", "")
									<< cgicc::p() << "\n";

							cout << cgicc::p() << "Phone: " 
									<< cgicc::input().set("type", "text")
																		.set("name", "phone")
																		.set("placeholder", "Enter 10-15 digits with country code")
																		.set("value", currContactData.phone)
																		.set("title", "10-15 characters. Digits and plus sign (+) only. e.g. +447350202133")
																		.set("pattern", "^\\+?[0-9]{10,15}$")
																		.set("required", "")
									<< cgicc::p() << "\n";

							cout << cgicc::p() << "Location (City): " 
									<< cgicc::input().set("type", "text")
																		.set("name", "location")
																		.set("placeholder", "Enter your city in 2-60 characters")
																		.set("value", currContactData.location)
																		.set("title", "2-60 characters. Letters, spaces, hyphens, and apostrophes only")
																		.set("pattern", "^(?=.*[a-zA-Z])[a-zA-Z\\s\\-']{2,60}$")
																		.set("required", "") 
										<< cgicc::p() << "\n";

						cout << "</div>"<< "\n";

						// update button
						cout << cgicc::button(cgicc::h2("Update")).set("type", "submit").set("class", "submit-button")
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