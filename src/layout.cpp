#include <cgicc/Cgicc.h>
#include <cgicc/HTMLClasses.h>
#include <sstream>
#include "../include/layout.h"

using namespace std;

// ostringstream is used to buffer HTML in memory before returning it as a single stream

string renderHeader(const string& title, const string& cssPath) {
	/*
	DESC.: creates a reusable HTML <head> section for all pages of the app
	RETURNS: string containing the buffered HTML content
	*/

  ostringstream oss;

  oss << cgicc::head() << "\n";
  	oss << cgicc::title(title) << "\n";
		oss << cgicc::link().set("rel", "stylesheet").set("type", "text/css").set("href", cssPath) << "\n";
  oss << cgicc::head() << "\n";

  return oss.str();
}

string renderNavBar(const string& currPage, const string& role) {
	/*
	SR2 - assists in logout process by providing a logout button to all users

	DESC.: creates a reusable navigation bar element with role-based routing and conditional button display
	RETURNS: string containing the buffered HTML content for the navigation bar
	*/

	ostringstream oss;

	// establish where home button should take the user based on their role
	string homeLink;
	if (role == "admin") homeLink = "admin_homepage.cgi";
	else if (role == "provider") homeLink = "connection_page.cgi";
	else if (role == "client") homeLink = "client_homepage.cgi";

	oss << "<div class=\"nav-bar\">" << "\n";
		// logout button
		oss << cgicc::form().set("method", "GET").set("action", currPage) << "\n";
			oss << cgicc::button(cgicc::h3("Log Out")).set("type", "submit").set("name", "logout") << "\n";
		oss << cgicc::form() << "\n";

		// client-specific "Connections" button
		if (role == "client" && currPage != "connection_page.cgi") {
			oss << cgicc::form().set("method", "GET").set("action", "connection_page.cgi") << "\n";
				oss << cgicc::button(cgicc::h3("Connections")).set("type", "submit") << "\n";
			oss << cgicc::form() << "\n";
		}

		// home button
		if (currPage != homeLink) {
			oss << cgicc::form().set("method", "GET").set("action", homeLink) << "\n";
				oss << cgicc::button(cgicc::h3("Home")).set("type", "submit") << "\n";
			oss << cgicc::form() << "\n";
		}

		// account button
		if (currPage != "account_page.cgi") {
			oss << cgicc::form().set("method", "GET").set("action", "account_page.cgi") << "\n";
				oss << cgicc::button(cgicc::h3("Account")).set("type", "submit") << "\n";
			oss << cgicc::form() << "\n";
		}

	oss << "</div>" << "\n";

	return oss.str();
}

string renderNotiBanner(const string& msg) {
	/*
	DESC.: 	creates a reusable notification banner used to display error or success messages
	RETURNS: string containing the buffered HTML content for the notification banner
	*/

	ostringstream oss;

	cout << "<div class=\"banner\">" << "\n";
		if (msg == "db") {
		// removes the need to pass the same msg each time the func is called for a db error
			cout << cgicc::h2("Database error occured. Please try again later.") << "\n";
		} else {
			cout << cgicc::h2(msg) << "\n";
		}
	cout << "</div>" << endl;

	return oss.str();
}