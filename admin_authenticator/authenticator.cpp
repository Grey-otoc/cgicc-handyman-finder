#include <ctime>
#include <iostream>
#include <random>

using namespace std;

int main(int argc, char* argv[]) {
	/*
	SR8

	DESC.: simulates a hardware token ("challenge-response")by mimicking TOTP flow
				 - admin's standalone script and server possess a pre-shared secret key (the "something you have")
				 - the system clock provides a synchronised "challenge" by calculating a number based on the current 30-second interval
				 - this secret key and the time interval number are combined with XOR to create a unique shared seed
				 - the shared seed initialises the mt19937 PRNG to ensure the pseudo-random output is identical on both devices
				 - the generator produces a 6-digit code via the pre-defined uniform_int_distribution
				 - this code is the "response"
	RETURNS: 0 on success
					 1 if arguments passed at command line are invalid

	- think of this script as the authenticator app on an admin's phone, and
		the pre-shared key represents the unique key stored directly on the device
	*/

	if (argc < 2) {
		cerr << "Usage: " << argv[0] << " <secret_key>" << endl;

		return 1;
	}

	// secret key shared by admin and server (different for each admin)
	const int secretKey = stoi(argv[1]);

	// the "challenge" - integer divison ensures the same value for 30 seconds
	int challenge = time(0) / 30;

	// XOR operator combines secret key with "challenge" (30-second interval number)
	int sharedSeed = (secretKey ^ challenge);
	
	mt19937 generator(sharedSeed);
	uniform_int_distribution<>dis(100000, 999999);

	// the "response": generate the 6-digit code
	int code = dis(generator);

	cout << "Your admin token code: " << code << endl;

	return 0;
}