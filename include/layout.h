#pragma once

#include <string>

std::string renderHeader(const std::string& title, const std::string& cssPath = "https://ribbonvolcano-noiseelectra-80.codio-box.uk/css/styles.css?v=1.3");
std::string renderNavBar(const std::string& currPage, const std::string& role);
std::string renderNotiBanner(const std::string& msg);