#include <iostream>
#include <mariadb/conncpp.hpp>
#include <string>
#include <vector>

using namespace std;

int main() {
	/*
	DESC.: connects to MariaDB and creates the core tables for user management, 
				 sessions, and connections
	RETURNS: 0 on success
					 1 if a db error occurs
	*/

	try {
		sql::Driver* driver = sql::mariadb::get_driver_instance();
		sql::SQLString url("jdbc:mariadb://localhost:3306/handyman_db");
		sql::Properties properties({
			{"user", "db_user"},
      {"password", "db_user_password"}
		});

		unique_ptr<sql::Connection> conn(driver->connect(url, properties));

		vector<string> schema = {
			/*
			"IF NOT EXISTS" allows the script to be run multiple times safely
			allotted chars for password must be more substantial to allow room for hash
			since email is used for MFA it must be unique

			VARCHAR(15) is used for phone nums to account for international nums
			ON DELETE CASCADE ensures that if an admin deletes a user, their profile 
			is deleted from clients or providers tables
			*/
			"CREATE TABLE IF NOT EXISTS users ("
			"id INT AUTO_INCREMENT PRIMARY KEY,"
			"username VARCHAR(20) UNIQUE NOT NULL,"
			"password_hash VARCHAR(255) NOT NULL,"
			"email VARCHAR(100) UNIQUE NOT NULL,"
			"phone VARCHAR(15) UNIQUE,"
			"location VARCHAR(60),"
			"role ENUM('client', 'provider', 'admin') NOT NULL)",

			// client-role specific table
			"CREATE TABLE IF NOT EXISTS clients ("
			"user_id INT PRIMARY KEY,"
			"FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE)",

			// provider-role specific table
			"CREATE TABLE IF NOT EXISTS providers ("
			"user_id INT PRIMARY KEY,"
			"service_type VARCHAR(50) NOT NULL,"
			"FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE)",

			// admin-role specific table
			"CREATE TABLE IF NOT EXISTS admins ("
			"user_id INT PRIMARY KEY,"
			"secret_key INT NOT NULL,"
			"FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE)",

			// table to track user sessions
			"CREATE TABLE IF NOT EXISTS sessions ("
			"session_id CHAR(32) PRIMARY KEY,"
			"user_id INT NOT NULL,"
			"role ENUM('client', 'provider', 'admin', 'admin_pending') NOT NULL,"
			// ON UPDATE CURRENT_TIMESTAMP automatically updates the last_active time to now time
			"last_active TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
			"FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE)",

			// table to track connection requests and their status
			"CREATE TABLE IF NOT EXISTS connections ("
			"id INT AUTO_INCREMENT PRIMARY KEY,"
			"client_id INT NOT NULL,"
			"provider_id INT NOT NULL,"
			"status ENUM('pending', 'accepted', 'rejected') DEFAULT 'pending',"
			"created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
			"FOREIGN KEY (client_id) REFERENCES users(id) ON DELETE CASCADE,"
			"FOREIGN KEY (provider_id) REFERENCES users(id) ON DELETE CASCADE,"
			// ensures only one connection request can be active between a client and provider
			"UNIQUE(client_id, provider_id))"
		};

		unique_ptr<sql::Statement> stmnt(conn->createStatement());

		for (const auto& query : schema) {
			try {
				stmnt->execute(query);
				cout << "Table skipped or initialised!" << endl;
			} catch (sql::SQLException& e) {
					// have to provide params to substr, or it will print entire table schema
					cerr << "Failed to execute: " << query.substr(0, 35) << endl;
					cerr << "Reason: " << e.what() << endl;
			}
		}

		cout << "Database setup complete!" << endl;

	} catch (sql::SQLException& e) {
			cerr << "Critical SQL error: " << e.what() << endl;

			return 1;
	}

	return 0;
}