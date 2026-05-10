# cgicc-Handyman-Finder

## Description
Have you ever wanted to create a web application and implement full session management and multi-step authentication with a library from 1998? Me neither, but I did. Anyways, this project is a CGI-based web application written entirely in C++ using the cgicc library. At its core, the application allows clients to search for and connect with service providers. Quite simple, but the functionality was merely a vehicle for practicing the low-level foundations of website security: encrypted password storage, 2-way input sanitisation, session management via cookies and database, two-factor authentication, and challenge-response authentication for administrator accounts.

## Features
- Supports three distinct user roles: client, provider, and administrator
- Clients can search for providers by service type and location
- Clients can send connection requests which providers can review and accept
- Clients and providers can view each other's contact information once a connection request is accepted
- Clients, providers, and admins can view and update their own account information
- Administrator dashboard for creating user accounts and managing credentials
- Session-based authentication system implemented using cookies and sessions SQL table
- Automatic session deletion and logout after inactivity
- Protected route handling that redirects unauthenticated (or incorrect role) users to the login page
- Passwords in database encrypted with BCrypt
- User input sanitisation implemented throughout the application, both in the backend and frontend
- Two-factor authentication workflow using email-based verification codes
- Additional challenge-response authentication layer for administrator accounts simulating TOTP hardware token verification

## Tech Stack
### Frontend
- **CGICC** — Poorly supported C++ library for building web applications using the Common Gateway Interface (CGI)

### Backend
- **C++** — Core application logic and orchestration
- **MariaDB** — Tables for users, role-specific information, sessions, and connections

## Contributing
This is a project made for personal interest and learning, but feel free to fork and experiment! Pull requests are welcome.

## Links
**Repository:** [cgicc-handyman-finder](https://github.com/grey-otoc/cgicc-handyman-finder)
