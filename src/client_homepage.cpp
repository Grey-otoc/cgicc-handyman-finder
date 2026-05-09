#include <algorithm>
#include <cgicc/Cgicc.h>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTMLClasses.h>
#include <iostream>
#include "../include/layout.h"
#include "../include/utils.h"
#include "../include/db_utils.h"

using namespace std;

SearchData extractFormData(cgicc::Cgicc &cgi, const vector<string>& availableLocations) {
	/*
	SR6 - assists in the sanitisation process by ensuring even if the dropdown selectors are tampered
				with, invalid services and locations are detected and flagged (by replacing the input with an empty string)

	DESC.: extracts search input from the POST request and validates it against the valid list of services and locations
	RETURNS: SearchData object containing the processed service_type and location
	*/

	SearchData data;

	data.location = cgi("location");
	data.service_type = cgi("service_type");

	// if either input does not match a potential option from availableLocations or services, then replace with ""
	if (find(availableLocations.begin(), availableLocations.end(), data.location) == availableLocations.end() ||
			find(services.begin(), services.end(), data.service_type) == services.end()) {
				data.location = "";
				data.service_type = "";
	}

	return data;
}

string getValidationError(const SearchData& data) {
	/*
	SR6 - assists in the sanitisation proces by ensuring invalid input is flagged and warned against

	DESC.: validates that input fields are not empty after sanitisation, producing an error
				 message for the first empty input field
	RETURNS: a specific error message string if a field is invalid
					 "none" if no empty input was found
	*/

	if (data.location == "" || data.service_type == "") {
		return "Invalid input for Service and/or Location fields. Please try again.";

	} else {
			// if no empty field found, return "none" to represent no errors
			return "success";
	}
}

int main() {
	/*
	FR1

	DESC.: orchestrates the provider search workflow, allowing clients to search for providers
				 by service type and location, and send a connection request once they've found a suitable provider
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

		// check user's role to ensure only client's have access to this page
		if (sessionData.role != "client") {
			redirectByRole(sessionData.role);

			return 0;
		}

		// check for logout request, destroySession() handles redirect
		if (isFormSubmission(cgi, "logout")) {
			mgr.destroySession(cgi);

			return 0;
		}

		// get array of valid provider locations to populate selector
		static const vector<string> availableLocations = db.getProviderLocations();
		string bannerMsg;
		string searchResult;
		SearchData searchData;

		// process search request
		if (isFormSubmission(cgi, "service_type")) {
			searchData = extractFormData(cgi, availableLocations);
			searchResult = getValidationError(searchData);

			if (searchResult != "success") {
				bannerMsg = searchResult;
			}
		}

		// process connection send request
		if (isFormSubmission(cgi, "connect_with")) {
			string providerUsername = cgi("connect_with");
			int clientId = db.getUserId(sessionData.username);
			int providerId = db.getUserId(providerUsername);

			if (clientId != -1 && providerId != -1) {
				string connectionResult = db.sendConnectionRequest(clientId, providerId);
				
				if (connectionResult == "success") bannerMsg = "Connection request sent!";
				else if (connectionResult == "exists") bannerMsg = "You have already sent a connection request to this provider.";
				else bannerMsg = "db";
				
			} else {
					bannerMsg = "db";
			}
		}

		cout << cgicc::HTTPHTMLHeader() << endl;

		cout << cgicc::html().set("lang", "en") << "\n";
			cout << renderHeader("Handyman - Provider Search");

			cout << cgicc::body() << "\n";
				// check if there is a banner message to display
				if (!bannerMsg.empty() && bannerMsg != "none") {
					cout << renderNotiBanner(bannerMsg);
				}

				// nav bar
				cout << renderNavBar("client_homepage.cgi", sessionData.role);

				// title for page
				cout << cgicc::div().set("class", "header") << "\n";
					cout << cgicc::h1("Find Your Handyman!") << "\n";
				cout << cgicc::div() << "\n";

				// nested divs must be hard coded as trying to use cgicc
				// syntax to open a nested div will simply close the parent div
				cout << "<div class=\"container\">" << "\n";
					cout << cgicc::form().set("method", "POST").set("action", "client_homepage.cgi") << "\n";
						// provider search menu
						cout << "<div class=\"card\">" << "\n";
							cout << cgicc::h2("Find contractors near you") << "\n";

							// service type selector
							cout << cgicc::select().set("name", "service_type") << "\n";
								cout << cgicc::option("Select a service...").set("disabled", "true").set("selected", "true").set("value", "")  << "\n";
								for (const auto& service : services) {
									cout << cgicc::option(service).set("value", service) << "\n";
								}
							cout << cgicc::select() << "\n";

							cout<< cgicc::br() << cgicc::br() << "\n";

							// location selector
							cout << cgicc::select().set("name", "location") << "\n";
								cout << cgicc::option("Select an available location...").set("disabled", "true").set("selected", "true") << "\n";

								for (auto& location : availableLocations) {
									cout << cgicc::option(location).set("value", location) << "\n";
								}
							cout << cgicc::select() << "\n";

							// search button
							cout << cgicc::button("Search").set("type", "submit") << "\n";

						cout << "</div>" << "\n";
					cout << cgicc::form() << "\n";
					
					if (searchResult == "success") {
						vector<string> matchingProviders = db.getMatchingProviders(searchData.service_type, searchData.location);

						cout << "<div class=\"card\">" << "\n";
							cout << cgicc::h2("Matching providers: ") << "\n";

							if (matchingProviders.empty()) {
								cout << cgicc::h2("No providers found.") << "\n";
							} else {
									for (const auto& providerUsername: matchingProviders) {
										cout << "<div class=\"provider-row\">" << "\n";
											cout << cgicc::h3("Username: " + providerUsername) << "\n";
											cout << cgicc::h3("Service: " + searchData.service_type) << "\n";
											cout << cgicc::h3("Location: " + searchData.location) << "\n";

											cout << cgicc::form().set("method", "POST").set("action", "client_homepage.cgi") << "\n";
												cout << cgicc::button("Connect").set("type", "submit")
																												.set("name", "connect_with")
																												.set("value", providerUsername) << "\n";
											cout << cgicc::form() << "\n";
										cout << "</div>" << "\n";
								}
							}
						
						cout << "</div>" << "\n";
					}
				cout << "</div>" << "\n";
			cout << cgicc::body() << "\n";
		cout << cgicc::html() << "\n";					

		return 0;

	} catch (exception& e) {
			cerr << "Exception occurred: " << e.what() << endl;

      return 1;
	}
}