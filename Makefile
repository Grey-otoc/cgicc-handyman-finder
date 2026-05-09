# DESC.: automates the compilation of CGI binaries to the server's execution directory
# PROCESS:
# 1. compiles each page in $(PAGES) with shared dependency logic (utils, layout, db_utils) 
# 2. links required libraries (cgicc, mariadbcpp, bcrypt)
# 3. moves the .cgi binary to the cgi-bin directory and updates permissions to ensure
#    "www-data" can execute the scripts

# informed by https://www.geeksforgeeks.org/cpp/makefile-in-c-and-its-applications/
# chmod 755 allows server read and execution

CXX = g++
LIBS = -lcgicc -lmariadbcpp -lbcrypt
TARGET = /usr/lib/cgi-bin/

INCLUDE_FILES = src/layout.cpp src/utils.cpp src/db_utils.cpp

PAGES = client_homepage login account_page admin_homepage connection_page admin_auth

all: $(PAGES)

%: src/%.cpp $(INCLUDE_FILES)
	$(CXX) -o $@.cgi $^ $(LIBS)
	sudo cp $@.cgi $(TARGET)
	sudo chmod 755 $(TARGET)$@.cgi
#sudo chown www-data $(TARGET)$@.cgi