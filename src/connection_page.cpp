#include <cgicc/Cgicc.h>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTMLClasses.h>
#include <iostream>
#include "../include/layout.h"
#include "../include/utils.h"
#include "../include/db_utils.h"

using namespace std;

int main() {
	/*
	FR1 - provides providers with an "accept" button next to each pending request from a client
	FR2 - ensures clients and providers can see base details while connection are pending
				and contact details (email) once connection requests are accepted

	DESC.: orchestrates the connection request workflow, allowing all clients and providers to view 
	existing connections (with contact details) and providers to accept pending requests
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

		// check for logout request, end session and redirect if logout requested
		if (isFormSubmission(cgi, "logout")) {
			mgr.destroySession(cgi);

			return 0;
		}

		// check user's role to ensure only clients and providers have access to this page
		if (sessionData.role != "client" && sessionData.role != "provider") {
			redirectByRole(sessionData.role);

			return 0;
		}

		string bannerMsg;
		// check connection status update request
		if (isFormSubmission(cgi, "new_status")) {
			// .set("value", ...) expects a string so we must convert back to int
			int connectionId = stoi(cgi("connection_id"));
			string newStatus = cgi("new_status");

			if (db.updateConnectionStatus(connectionId, newStatus) == 0) {
				bannerMsg = "Connection accepted!";
			} else {
					bannerMsg = "An error occured while updating the connection. Please try again.";
			}
		}

		cout << cgicc::HTTPHTMLHeader() << endl;

		cout << cgicc::html().set("lang", "en") << "\n";
			cout << renderHeader("Handyman - Connection Requests");

			cout << cgicc::body() << "\n";

				if (!bannerMsg.empty()) {
					cout << renderNotiBanner(bannerMsg);
				}

				// nav bar
				cout << renderNavBar("connection_page.cgi", sessionData.role);

				// title for page
				cout << cgicc::div().set("class", "header") << "\n";
					cout << cgicc::h1("Manage Your Connections") << "\n";
				cout << cgicc::div() << "\n";

				// nested divs must be hard coded as trying to use cgicc
				// syntax to open a nested div will simply close the parent div
				cout << "<div class=\"container\">" << "\n";
					
					int userId = db.getUserId(sessionData.username);
					vector<ConnectionData> connections = db.getConnections(userId, sessionData.role);

					cout << "<div class=\"card\">" << "\n";
						cout << cgicc::h2(sessionData.role == "client" ? "Requests sent:" : "Incoming connection requests:") << "\n";

						// user has no connections or SQL error
						if (connections.empty()) {
							cout << cgicc::h2("No connections found.") << "\n";

						} else {
								for (const auto& connection : connections) {
									cout << "<div class=\"provider-row\">" << "\n";				
										cout << cgicc::h3("Username: " + connection.username) << "\n";
										if (sessionData.role == "client") {
											cout << cgicc::h3("Service: " + connection.service_type) << "\n";
										}
										cout << cgicc::h3("Location: " + connection.location) << "\n";

										if (connection.status == "pending") {
											if (sessionData.role == "client") {
												cout << cgicc::h3("Status: " + connection.status) << "\n";
											} else {
												cout << cgicc::form().set("method", "POST").set("action", "connection_page.cgi") << "\n";
												// need hidden input field to pass connection_id so we know which connection to update	
												cout << cgicc::input().set("type", "hidden")
																							.set("name", "connection_id")
																							.set("value", to_string(connection.id)) << "\n";

												cout << cgicc::button("Accept").set("type", "submit")
																											.set("name", "new_status")
																											.set("value", "accepted") << "\n";																							 
												cout << cgicc::form() << "\n";
											}
										} else if (connection.status == "accepted") {
												cout << cgicc::h3("Email: " + connection.email) << "\n";
										}
									cout << "</div>" << "\n";
								}
						}
					cout << "</div>" << "\n";
				cout << "</div>" << "\n";
			cout << cgicc::body() << "\n";
		cout << cgicc::html() << "\n";

		return 0;

	} catch (exception& e) {
			cerr << "Exception occurred: " << e.what() << endl;
			
      return 1;
	}
}