#include <cgicc/Cgicc.h>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTMLClasses.h>
#include "../include/layout.h"
#include "../include/utils.h"
#include "../include/db_utils.h"

using namespace std;

RegistrationData extractFormData(cgicc::Cgicc &cgi) {
	/*
	SR6 - assists in the sanitisation process by ensuring inputs that bypassed the HTML
				patterns are detected and flagged (by replacing the input with an empty string)

	DESC.: extracts and sanitises (via sanitiseInput()) contact details from the POST request
	RETURNS: RegistrationData object containing the processed account information
	*/

	RegistrationData data;

	data.username = sanitiseInput(cgi("username"), "username");
	data.email = sanitiseInput(cgi("email"), "email");
	data.phone = sanitiseInput(cgi("phone"), "phone");
	data.location = sanitiseInput(cgi("location"), "location");
	data.password = sanitiseInput(cgi("password"), "password");
	data.service_type = cgi("service_type");

	return data;
}

string getValidationError(const RegistrationData& data) {
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

	if (data.email.empty()) {
		return "Invalid email. Please enter a valid email address between 6-100 characters.";
	}

	if (data.phone.empty()) {
		return "Invalid phone number. Please enter 10-15 digits with an optional country code e.g. +447350202133.";
	}

	if (data.location.empty()) {
		return "Invalid location. Please enter a city name between 2-60 characters. Letters, spaces, hyphens, and apostrophes only.";
	}

	if (data.password.empty()) {
		return "Invalid password. Please enter 8-20 characters.";
	}

	return "none";
}

int main() {
	/*
	FR3

	DESC.: orchestrates the new user registration workflow, allowing admins to create new user
				 accounts with all the necessary account details
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

		// if user attempts to access restricted admin page then redirect them to the homepage 
		// for their respective role
		if (sessionData.role != "admin") {
			redirectByRole(sessionData.role);

			return 0;
		}

		// if user requests a logout then log out user and redirect
		if (isFormSubmission(cgi, "logout")) {
			mgr.destroySession(cgi);

			return 0;
		}

		// check for registration attempt
		string bannerMsg;
		if (isFormSubmission(cgi, "username")) {
			RegistrationData data = extractFormData(cgi);
			bannerMsg = getValidationError(data);

			if (bannerMsg == "none") {
				// attempt to insert new user into db
				string insertResult = db.insertNewUser(data);

				if (insertResult == "success") bannerMsg = "New user created successfully!";
				else if (insertResult == "error") bannerMsg = "db";
				else bannerMsg = "Input provided for " + insertResult + " already exists.";

			}
		}

		cout << cgicc::HTTPHTMLHeader() << endl;

		cout << cgicc::html().set("lang", "en") << "\n";
			cout << renderHeader("Handyman - Admin Account Register");

			cout << cgicc::body() << "\n";
				// check if there is a banner message to display
				if (!bannerMsg.empty()) {
					cout << renderNotiBanner(bannerMsg);
				}

				cout << renderNavBar("admin_homepage.cgi", sessionData.role);

				// title for page
				cout << cgicc::div().set("class", "header") << "\n";
					cout << cgicc::h1("User Account Registration") << "\n";
				cout << cgicc::div() << "\n";

				cout << cgicc::form().set("method", "POST").set("action", "admin_homepage.cgi") << "\n";
					// nested divs must be hard coded as trying to use cgicc
					// syntax to open a nested div will simply close the parent div
					cout << "<div class=\"container\">" << "\n";
						// registration fields
						cout << "<div class=\"card\">" << "\n";
							// SR6 - all fields enforce regex patterns as a first line of defense

							cout << cgicc::p() << "Username: " 
									<< cgicc::input().set("type", "text")
																		.set("name", "username").set("placeholder", "Enter 3-20 characters")
																		.set("title", "3-20 characters. Letters, numbers, and underscores only")
																		.set("pattern", "^(?=.*[a-zA-Z])[a-zA-Z0-9_]{3,20}$")
																		.set("required", "") 
									<< cgicc::p() << "\n";

							cout << cgicc::p() << "Email: " 
									<< cgicc::input().set("type", "email")
																		.set("name", "email")
																		.set("placeholder", "Enter 6-100 characters")
																		.set("title", "6-100 characters. Must contain an @ and a . to be considered valid")
																		.set("pattern", "^(?=.{6,100}$)[a-zA-Z0-9\\._\\%\\+\\-]+@[a-zA-Z0-9\\.\\-]+\\.[a-zA-Z]{2,}$")
																		.set("required", "")
									<< cgicc::p() << "\n";

							cout << cgicc::p() << "Phone: " 
									<< cgicc::input().set("type", "text")
																		.set("name", "phone")
																		.set("placeholder", "Enter 10-15 digits with country code")
																		.set("title", "10-15 characters. Digits and plus sign (+) only. e.g. +447350202133")
																		.set("pattern", "^\\+?[0-9]{10,15}$")
																		.set("required", "")
									<< cgicc::p() << "\n";

							cout << cgicc::p() << "Location (City): " 
									<< cgicc::input().set("type", "text")
																		.set("name", "location")
																		.set("placeholder", "Enter your city in 2-60 characters")
																		.set("title", "2-60 characters. Letters, spaces, hyphens, and apostrophes only")
																		.set("pattern", "^(?=.*[a-zA-Z])[a-zA-Z\\s\\-']{2,60}$")
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

						cout << "<div class=\"card\">" << "\n";
							// provider registration
							cout << cgicc::h2("Is the user a provider? Select their service!") << "\n";

							cout << cgicc::select().set("name", "service_type") << "\n";
								cout << cgicc::option("No, they're a client").set("selected", "true").set("value", "") << "\n";
								// populate options with string from services array found in utils.h
								for (const auto& service : services) {
									cout << cgicc::option(service).set("value", service) << "\n";
								}
							cout << cgicc::select() << "\n";

						cout << "</div>"<< "\n";

						cout << cgicc::button(cgicc::h2("Register User")).set("type", "submit").set("class", "submit-button")
								<< "\n";

					cout << "</div>" << "\n";
				cout << cgicc::form() << "\n";
			cout << cgicc::body() << "\n";
		cout << cgicc::html() << "\n";

		return 0;	

	} catch(exception& e) {
			cerr << "Exception occurred: " << e.what() << endl;

			return 1;
  }
}