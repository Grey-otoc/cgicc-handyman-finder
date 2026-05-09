#include <iostream>
#include <vector>
#include <mariadb/conncpp.hpp>
#include <cgicc/HTTPHTMLHeader.h>
#include <cgicc/HTMLClasses.h>

/*
* The code for the Hello Maria practical has been given to you in order to help you start.
The purpose of the software is to connect clients of a service to providers of that service.
Clients can search for providers, send requests to one or more providers, and once the requests are accepted, 
they can view the contact details of the provider and the provider can view their contact details too.
*/
using namespace cgicc;
using namespace std;

void printMessage(shared_ptr<sql::Statement> &stmnt) {
    try{
        unique_ptr<sql::ResultSet> res(stmnt->executeQuery("SELECT 'Hello, World!' AS Message"));
        while (res->next()) {
            cout <<  res->getString("Message") << endl;
        }
    }  catch (sql::SQLException & e) {
        cerr <<"Something went wrong when running the Hello World SQL statement" << e.what() << endl;
    }
}

int main(int argc, char **argv) {
    try {
        
        sql::Driver* driver = sql::mariadb::get_driver_instance();
        sql::SQLString url("jdbc:mariadb://localhost:3306/comp7025"); 
        sql::Properties  properties({
            {"user", "db_user"},
            {"password", "db_user_password"}
        });

        unique_ptr<sql::Connection> conn(driver->connect(url, properties));
        shared_ptr<sql::Statement> stmnt(conn->createStatement());
        
        cout << HTTPHTMLHeader() << endl;
        cout << html() << endl;
        
        printMessage(stmnt);
       
        cout << html() << endl;
        
        conn->close();
    } catch (sql::SQLException& e) {
        cerr << "Error connecting to the database: " << e.what() << endl;
        return 1;
    } catch(exception& e) {
        cerr<<"Exception occurred: " << e.what() << endl;
        return 1;
    }

    return 0;
}