#pragma once

#include <mariadb/conncpp.hpp>
#include <string>
#include <vector>

// struct used to store input from registration attempts
struct RegistrationData {
	std::string username;
	std::string email;
	std::string phone;
	std::string location;
	std::string password;
	std::string service_type;
};

// struct used to store input from login attempts
struct LoginData {
	std::string username;
	std::string password;
};

// struct used to store input from provider search attempts
struct SearchData {
	std::string location;
	std::string service_type;
};

// struct used to store contact information read from database
// when displaying account page
struct ContactData {
	std::string email;
	std::string phone;
	std::string location;
};

// struct used to information about the provider or client in a connection
struct ConnectionData {
	int id;
	std::string username;
	std::string email;
	std::string location;
	std::string service_type;
	std::string status;
};

class DatabaseManager {
	private:
		sql::Connection* conn;

		std::string checkUserExists(const RegistrationData& data);
		int simulate2FA(const std::string& email);
	
	public:
		DatabaseManager(sql::Connection* connection) : conn(connection) {}

		// authorisation and account management
		std::string insertNewUser(const RegistrationData& data);
		std::string loginUser(const LoginData& data);
		ContactData getContactInfo(const std::string& username);
		int updateContactInfo(const std::string& username, const ContactData& contactData);
		int getUserId(const std::string& username);
		int getAdminSecretKey(const std::string& username);
		int promoteAdminRole(const std::string& sessionId);

		// provider search and connections
		std::vector<std::string> getProviderLocations();
		std::vector<std::string> getMatchingProviders(const std::string& service, const std::string& location);
		std::string sendConnectionRequest(int clientId, int providerId);
		std::vector<ConnectionData> getConnections(int userId, const std::string& role);
		int updateConnectionStatus(int connectionId, const std::string& newStatus);
};

sql::Connection* createDbConnection();