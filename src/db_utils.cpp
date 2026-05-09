#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include "bcrypt/BCrypt.hpp"
#include "../include/db_utils.h"

using namespace std;

sql::Connection* createDbConnection() {
	/*
	DESC.: establishes a conection to the handyman_db database
	RETURNS: pointer to active sql::Connection if successful
					 nullptr if there was an SQLException
	
	- allows the db connection to be initialised once per page and then passed
		to SessionManager and DatabaseManager instances
	*/

	try {
		sql::Driver* driver = sql::mariadb::get_driver_instance();
		sql::SQLString url("jdbc:mariadb://localhost:3306/handyman_db");
		sql::Properties properties({
			{"user", "db_user"},
			{"password", "db_user_password"}
		});

		return driver->connect(url, properties);	

	} catch (sql::SQLException& e) {
			std::cerr << "Error connnecting to handyman_db: " << e.what() << std::endl;

			return nullptr;
	}
}

string DatabaseManager::checkUserExists(const RegistrationData& data) {
	/*
	DESC.: validates whether the user inputted username, email, or phone is already
				 associated with an existing account
	RETURNS: first invalid field name if a conflict is detected
					 "no" if all fields are unused in the db
					 "error" if a database exception occurs
	*/

	try {
		unique_ptr<sql::PreparedStatement> userStmnt(conn->prepareStatement(
			"SELECT username, email, phone FROM users "
			"WHERE username = ? OR email = ? OR phone = ?"
		));
		userStmnt->setString(1, data.username);
		userStmnt->setString(2, data.email);
		userStmnt->setString(3, data.phone);

		unique_ptr<sql::ResultSet> userRes(userStmnt->executeQuery());

		// if a result is found, return the first field that is already in use
		if (userRes->next()) {
			string existingUsername = string(userRes->getString("username"));
			string existingEmail = string(userRes->getString("email"));
			string existingPhone = string(userRes->getString("phone"));

			if (existingUsername == data.username) return "username";
			if (existingEmail == data.email) return "email";
			if (existingPhone == data.phone) return "phone";
		}

		// if all inputs are unused in db, return "no" for no existing user found
		return "no";

	} catch (sql::SQLException& e) {
			cerr << "SQL error occured in checkUserExists: " << e.what() << endl;

			return "error";
	}
}

string DatabaseManager::insertNewUser(const RegistrationData& data) {
	/*
	FR3 - provides the logic behind admin_homepage allowing admins to create new user accounts
	SR5 - utilises BCrypt for password encryption, which is an irreversible hashing algorithm that 
				automatically generates and embeds a unique salt, hindering rainbow-table attacks
				it is also intentionally slow, making brute-force attacks computationally expensive

	DESC.: orchestrates a multi-table insertion to register a new user with their 
				 role-specific details
	RETURNS: "success" if registration completes successfully
					 "error" if a db error occurs
					 field name string if checkUserExists() finds a conflicting field
	
	- utilises BCrypt C++ wrapper from https://github.com/trusch/libbcrypt
	*/

	try {
	// must use .get() to pass raw ptr to checkUserExists 
		string userExists = checkUserExists(data);

		// if username, email, and phone are unused, insert new user
		if (userExists == "no") {
			// if service_type is empty, user is a client, else, a provider
			string role = data.service_type.empty() ? "client" : "provider";

			unique_ptr<sql::PreparedStatement> userStmnt(conn->prepareStatement(
				"INSERT INTO users(username, password_hash, email, phone, location, role) "
				"VALUES (?, ?, ?, ?, ?, ?)"
			));
			userStmnt->setString(1, data.username);
			userStmnt->setString(2, BCrypt::generateHash(data.password));
			userStmnt->setString(3, data.email);
			userStmnt->setString(4, data.phone);
			userStmnt->setString(5, data.location);
			userStmnt->setString(6, role);
			userStmnt->executeUpdate();

			// get id of user we just inserted
			unique_ptr<sql::Statement> idStmnt(conn->createStatement());
			unique_ptr<sql::ResultSet> idRes(idStmnt->executeQuery("SELECT LAST_INSERT_ID()"));
			idRes->next();
			int userId = idRes->getInt(1);

			if (role == "client") {
				unique_ptr<sql::PreparedStatement> clientStmnt(conn->prepareStatement(
					"INSERT INTO clients (user_id) VALUES (?)"
				));
				clientStmnt->setInt(1, userId);
				clientStmnt->executeUpdate();
			} else {
				unique_ptr<sql::PreparedStatement> providerStmnt(conn->prepareStatement(
					"INSERT INTO providers (user_id, service_type) VALUES (?, ?)"
				));
				providerStmnt->setInt(1, userId);
				providerStmnt->setString(2, data.service_type);
				providerStmnt->executeUpdate();
			}

			return "success";

		} else {
				return userExists;
		}

	} catch (sql::SQLException& e) {
			cerr << "SQL error occured in insertNewUser: " << e.what() << endl;

			return "error";
	}
}

int DatabaseManager::simulate2FA(const string& email) {
	/*
	SR7

	DESC.: generates a random 6-digit authentication code and simulates an Email-based
				 one-time passcode (OTP) by appending email and code to a local mail spool file
	RETURNS: 0 if code is generated and email and code are appended to the file
					 1 if there was an error opening the file
	*/

	// open file in append mode
	ofstream mailSpool("/home/codio/workspace/mail_spool.txt", ios::app);

	if (mailSpool.is_open()) {
		mailSpool << "To: " << email << "\n";

		// random number generator that uses randomness from OS to generate seed for mt19937
		random_device rd;
		// "mersenne twister 19937" is a random number generator that takes a seed
		mt19937 generator(rd());
		// defines the range for a 6-digit code 
		uniform_int_distribution<> dis(100000, 999999);
		// generates random 6-digit code
		string code = to_string(dis(generator));

		mailSpool << "Body: Your authentication code is: " << code << "\n";
		mailSpool << "---------------------------------------------" << "\n";

		mailSpool.close();

		return 0;
	}

	return 1;
}

string DatabaseManager::loginUser(const LoginData& data) {
	/*
	SR1
	SR5 - utilises BCrypt's built-in validatePassword() to hash and compare the inputted
				password against the stored salted and hashed password

	DESC.: validates pre-sanitised user credentials and initiates the email 2FA process
	RETURNS: user's role (client, provider, or admin) if username, password, and 2FA checks are successful
					 a specific error message string if credentials or 2FA fail
	*/

	try {
		// ensure username exists before attempting login
		unique_ptr<sql::PreparedStatement> userStmnt(conn->prepareStatement(
			"SELECT email, password_hash, role FROM users WHERE username = ?"
		));

		// binds data.username to placeholder in query
		userStmnt->setString(1, data.username);
		unique_ptr<sql::ResultSet> userRes(userStmnt->executeQuery());

		// reads result of COUNT(*)
		if (!userRes->next()) {
			return "Invalid username provided.";
		}

		string storedHash = string(userRes->getString("password_hash"));

		if (!BCrypt::validatePassword(data.password, storedHash)) {
			return "Invalid password provided.";
		}

		// simulate 2 factor authentication
		string email = string(userRes->getString("email"));
		if (simulate2FA(email) != 0) {
			return "Unable to authenticate email.";
		}

		return string(userRes->getString("role"));
		
	} catch (sql::SQLException& e) {
			cerr << "SQL error occured while logging in user: " << e.what() << endl;

			return "error";
	}
}

vector<string> DatabaseManager::getProviderLocations() {
	/*
	FR1 - assists in provider search function by providing a list of locations to choose from

	DESC.: retrieves a unique, sorted list of locations where service providers are currently registered
	RETURNS: vector of sorted, unique locations if successful
					 empty vector if a db error occurs
	*/

	try {
		// query for all locations in users table where user's role is "provider"
		unique_ptr<sql::PreparedStatement> locStmnt(conn->prepareStatement(
			"SELECT DISTINCT location FROM users WHERE role = ? ORDER BY location ASC"
		));
		locStmnt->setString(1, "provider");

		unique_ptr<sql::ResultSet> locRes(locStmnt->executeQuery());
		vector<string> availableLocations;

		// while there are remaining rows in the result set, add each location to the vector
		while (locRes->next()) {
			availableLocations.push_back(string(locRes->getString(1)));
		}

		return availableLocations;

	} catch (sql::SQLException& e) {
			cerr << "SQL error occured in getProviderLocations: " << e.what() << endl;

			return {};
	}
}

vector<string> DatabaseManager::getMatchingProviders(const string& service, const string& location) {
	/*
	FR1

	DESC.: retrieves a list of provider usernames who match the pre-sanitised selected location and service type
	RETURNS: vector of strings containing matching provider's usernames if successful
					 an empty vector if no matches are found or a db error occurs
	*/

	try {
		// query links users table to providers
		unique_ptr<sql::PreparedStatement> providerStmnt(conn->prepareStatement(
			"SELECT username "
			"FROM users "
			"INNER JOIN providers ON users.id = providers.user_id "
			"WHERE providers.service_type = ? AND users.location = ?"
		));
		providerStmnt->setString(1, service);
		providerStmnt->setString(2, location);

		unique_ptr<sql::ResultSet> providerRes(providerStmnt->executeQuery());
		vector<string> matchingProviders;

		// while there are remaining rows in the result set, add each providers username,
		// email, and phone number
		while (providerRes->next()) {
			matchingProviders.push_back(string(providerRes->getString("username")));
		}

		return matchingProviders;
	} catch (sql::SQLException& e) {
			cerr << "SQL error occured in getMatchingProviders: " << e.what() << endl;

			return {};
	}
}

ContactData DatabaseManager::getContactInfo(const string& username) {
	/*
	FR2 - assists in allowing users to update contact info by retrieving current contact info

	DESC.: retrieves user's contact information for display on their account page
	RETURNS: ContactData struct populated with email, phone, and location if successful
					 empty struct if a db error occurs
	*/

	try {
		unique_ptr<sql::PreparedStatement> contactStmnt(conn->prepareStatement(
			"SELECT email, phone, location FROM users WHERE username = ?"
		));
		contactStmnt->setString(1, username);		

		unique_ptr<sql::ResultSet> contactRes(contactStmnt->executeQuery());
		ContactData contactData;

		if (contactRes->next()) {
			contactData.email = string(contactRes->getString("email"));
			contactData.phone = string(contactRes->getString("phone"));
			contactData.location = string(contactRes->getString("location"));
		}

		return contactData;

	} catch (sql::SQLException& e) {
			cerr << "SQL error occured in getContactInfo: " << e.what() << endl;

			return {};
	}
}

int DatabaseManager::updateContactInfo(const string& username, const ContactData& contactData) {
	/*
	FR2

	DESC.: updates a user's contact details in db with pre-sanitised input
	RETURNS: 0 on success
					 1 if a database error occurs
	*/

	try {
		unique_ptr<sql::PreparedStatement> updateStmnt(conn->prepareStatement(
			"UPDATE users SET email = ?, phone = ?, location = ? WHERE username = ?"
		));
		updateStmnt->setString(1, contactData.email);
		updateStmnt->setString(2, contactData.phone);
		updateStmnt->setString(3, contactData.location);
		updateStmnt->setString(4, username);

		updateStmnt->executeUpdate();

		return 0;

	} catch (sql::SQLException& e) {
			cerr << "SQL error occured in updateContactInfo: " << e.what() << endl;

			return 1;
	}
}

int DatabaseManager::getUserId(const string& username) {
	/*
	DESC.: retrieves primary key (id) from users table based on the provided username
	RETURNS: integer ID if found
					 -1 if no entry is found for the provided username, or a db error occurs 
	*/

	try {
		unique_ptr<sql::PreparedStatement> idStmnt(conn->prepareStatement(
			"SELECT id FROM users WHERE username = ?"
		));
		idStmnt->setString(1, username);

		unique_ptr<sql::ResultSet> idRes(idStmnt->executeQuery());

		if (idRes->next()) {
			return idRes->getInt("id");
		}

		return -1;

	} catch (sql::SQLException& e) {
			cerr << "Error occured while retrieving user's id: " << e.what() << endl;

			return -1;
	}
}

string DatabaseManager::sendConnectionRequest(int clientId, int providerId) {
	/*
	FR1
	FR2 - by default all new connection requests are recorded as "pending", ensuring
				contact details are not shared until the provider accepts the request

	DESC.: establishes a new connection between a client and provider after verifying 
				 the connection does not already exist
	RETURNS: "success" if the new connection entry was recorded successfully
					 "exists" if a request already exist between the client and provider
					 "error" if a db error occurs
	*/

	try {
		unique_ptr<sql::PreparedStatement> checkStmnt(conn->prepareStatement(
			"SELECT 1 FROM connections WHERE client_id = ? AND provider_id = ?"
		));
		checkStmnt->setInt(1, clientId);
		checkStmnt->setInt(2, providerId);

		unique_ptr<sql::ResultSet> checkRes(checkStmnt->executeQuery());

		if (checkRes->next()) {
			return "exists";
		}

		unique_ptr<sql::PreparedStatement> insertStmnt(conn->prepareStatement(
			"INSERT INTO connections (client_id, provider_id) VALUES (?, ?)"
		));
		insertStmnt->setInt(1, clientId);
		insertStmnt->setInt(2, providerId);
		insertStmnt->executeUpdate();

		return "success";

	} catch (sql::SQLException& e) {
			cerr << "Error occurred while sending connection request: " << e.what() << endl;

			return "error";
	}
}

vector<ConnectionData> DatabaseManager::getConnections(int userId, const string& role) {
	/*
	FR2 - retrieves connection request details (whether accepted or pending) that user's can view on their connections page

	DESC.: retrieves all connection requests (and their details) associated with a specific user
	RETURNS: a vector of ConnectionData objects tailored to the user's role if successful
					 empty vector if no connections exist or a db error occurs
	*/

	vector<ConnectionData> connections;
	string query;

	// clients and providers use the same connection page, so the information returned must be tailored to the user's role
	if (role == "client") {
		query = "SELECT users.username, users.email, users.location, providers.service_type, connections.status, connections.id "
						"FROM connections "
						"JOIN users ON connections.provider_id = users.id "
						"JOIN providers ON users.id = providers.user_id "
						"WHERE connections.client_id = ? "
						"ORDER BY connections.created_at DESC";
	} else {
		query = "SELECT users.username, users.email, users.location, '' AS service_type, connections.status, connections.id "
						"FROM connections "
						"JOIN users ON connections.client_id = users.id "
						"WHERE connections.provider_id = ? "
						"ORDER BY connections.created_at DESC";
	}

	try {
		unique_ptr<sql::PreparedStatement> connectStmnt(conn->prepareStatement(query));
		connectStmnt->setInt(1, userId);

		unique_ptr<sql::ResultSet> connectRes(connectStmnt->executeQuery());

		while (connectRes->next()) {
			ConnectionData connection;
			connection.id = connectRes->getInt("id");
			connection.username = connectRes->getString("username");
			connection.email = connectRes->getString("email");
			connection.location = connectRes->getString("location");
			connection.status = connectRes->getString("status");
			// for providers' connection requests, this will be an unused empty string
			connection.service_type = connectRes->getString("service_type");

			connections.push_back(connection);
		}

		return connections;

	} catch (sql::SQLException& e) {
			cerr << "Error occured while gathering connection requests: " << e.what() << endl;

			return {};
	}
}

int DatabaseManager::updateConnectionStatus(int connectionId, const string& newStatus) {
	/*
	FR1 - handles logic of allowing providers to accept a connection request
	FR2 - by changing the status to "accepted", the connections page now knows to display contact details 
				to clients and providers

	DESC.: updates the state of a connection from "pending" to "accepted"
	RETURNS: 0 if the update was successful
					 1 if a db error occurs
	*/

	try {
		unique_ptr<sql::PreparedStatement> connectionStmnt(conn->prepareStatement(
			"UPDATE connections SET status = ? WHERE id = ?"
		));
		connectionStmnt->setString(1, newStatus);
		connectionStmnt->setInt(2, connectionId);

		connectionStmnt->executeUpdate();

 		return 0;

	} catch (sql::SQLException& e) {
			cerr << "Error occured while updating connection request: " << e.what() << endl;

			return 1;
	}
}

int DatabaseManager::getAdminSecretKey(const string& username) {
	/*
	SR8 - assists in challenge-response authentication process

	DESC.: retrieves the unique secret key for an admin to facilitate challenge-response authentication
	RETURNS: integer secret key if found
					 -1 if the entry is not found or a db error occurs
	*/

	try {
		unique_ptr<sql::PreparedStatement> keyStmnt(conn->prepareStatement(
			"SELECT secret_key FROM admins "
			"JOIN users ON users.id = admins.user_id "
			"WHERE users.username = ?"
		));
		keyStmnt->setString(1, username);

		unique_ptr<sql::ResultSet> keyRes(keyStmnt->executeQuery());
		
		if (keyRes->next()) {
			return keyRes->getInt("secret_key");
		}

		return -1;

	} catch (sql::SQLException& e) {
			cerr << "Error occured while updating connection request: " << e.what() << endl;

			return -1;
	}
}

int DatabaseManager::promoteAdminRole(const string& sessionId) {
	/*
	SR8 - assists in challenge-response authentication process

	DESC.: finalises the admin authentication by upgrading the user's session role
				 from "admin_pending" to "admin"
	RETURNS: 0 on success
					 1 if a db error occurs

	- see authenticateAdmin() in utils.cpp for logic implementation and explanation
	*/

	try {
		unique_ptr<sql::PreparedStatement> promoteStmnt(conn->prepareStatement(
			"UPDATE sessions SET role = ? WHERE session_id = ? AND role = 'admin_pending'"
		));
		promoteStmnt->setString(1, "admin");
		promoteStmnt->setString(2, sessionId);

		promoteStmnt->executeUpdate();

		return 0;

	} catch (sql::SQLException& e) {
			cerr << "Error occured while promoting admin role: " << e.what() << endl;

			return 1;
	}
}