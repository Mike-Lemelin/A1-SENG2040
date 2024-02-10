/*
	Reliability and Flow Control Example
	From "Networking for Game Programmers" - http://www.gaffer.org/networking-for-game-programmers
	Author: Glenn Fiedler <gaffer@gaffer.org>
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>

#include "Net.h"
#include "CRC.h"

#pragma warning(disable : 4996)

#define VOID "void"
#define CLIENT "Client"
#define SERVER "Server"
#define TRANSFER_COMPLETE "complete"

//#define SHOW_ACKS

using namespace std;
using namespace net;

const int ServerPort = 30000;
const int ClientPort = 30001;
const int ProtocolId = 0x11223344;
const float DeltaTime = 1.0f / 30.0f;
const float SendRate = 1.0f / 30.0f;
const float TimeOut = 10.0f;
const int PacketSize = 256;

class FlowControl
{
public:

	FlowControl()
	{
		printf("flow control initialized\n");
		Reset();
	}

	void Reset()
	{
		mode = Bad;
		penalty_time = 4.0f;
		good_conditions_time = 0.0f;
		penalty_reduction_accumulator = 0.0f;
	}

	void Update(float deltaTime, float rtt)
	{
		const float RTT_Threshold = 250.0f;

		if (mode == Good)
		{
			if (rtt > RTT_Threshold)
			{
				printf("*** dropping to bad mode ***\n");
				mode = Bad;
				if (good_conditions_time < 10.0f && penalty_time < 60.0f)
				{
					penalty_time *= 2.0f;
					if (penalty_time > 60.0f)
						penalty_time = 60.0f;
					printf("penalty time increased to %.1f\n", penalty_time);
				}
				good_conditions_time = 0.0f;
				penalty_reduction_accumulator = 0.0f;
				return;
			}

			good_conditions_time += deltaTime;
			penalty_reduction_accumulator += deltaTime;

			if (penalty_reduction_accumulator > 10.0f && penalty_time > 1.0f)
			{
				penalty_time /= 2.0f;
				if (penalty_time < 1.0f)
					penalty_time = 1.0f;
				printf("penalty time reduced to %.1f\n", penalty_time);
				penalty_reduction_accumulator = 0.0f;
			}
		}

		if (mode == Bad)
		{
			if (rtt <= RTT_Threshold)
				good_conditions_time += deltaTime;
			else
				good_conditions_time = 0.0f;

			if (good_conditions_time > penalty_time)
			{
				printf("*** upgrading to good mode ***\n");
				good_conditions_time = 0.0f;
				penalty_reduction_accumulator = 0.0f;
				mode = Good;
				return;
			}
		}
	}

	float GetSendRate()
	{
		return mode == Good ? 30.0f : 10.0f;
	}

private:

	enum Mode
	{
		Good,
		Bad
	};

	Mode mode;
	float penalty_time;
	float good_conditions_time;
	float penalty_reduction_accumulator;
};

// ----------------------------------------------------
// creating a struct for parsing command line arguments 
struct CommandLineArg
{
	string mode = "Server"; // Default mode
	string filePath;
	string checksumMethod = "CRC32";
	string address = "127.0.0.1"; // Default address
	int port = 30000; // Default port

	// or initialize a constructor here with the default values 
	CommandLineArg(int argc, char* argv[])
	{
		parseArgs(argc, argv);
	}

private: 

	void parseArgs(int argc, char* argv[])
	{
		// for loop for each argument on the command line 
			// add handlers for mode argument 
			// add file handler 
			// add crc handler 
			// add default ip address handler 
			// add default port handler 
		for (int i = 1; i < argc; ++i)
		{
			string arg = argv[i];

			if (arg == "-m")
			{
				mode = getNextArg(argc, argv, i);
			}
			else if (arg == "-f" && mode != SERVER) // Skip parsing file path for server mode
			{
				filePath = getNextArg(argc, argv, i);
			}
			else if (arg == "-a")
			{
				address = getNextArg(argc, argv, i);
			}
			else if (arg == "-p")
			{
				string portStr = getNextArg(argc, argv, i);
				port = stoi(portStr);
			}
			else if(arg == "-h")
			{
				printf("Usage: SENG2040-A1 -m <mode> -f <file_path> -a <address> -p <port>\n");
				printf("Arguments:\n");
				printf("  -m <mode>: Specify the mode of operation (server or client).\n");
				printf("  -f <file_path>: Specify the path to the file (required for client mode).\n");
				printf("  -a <address>: Specify the IP address of the destination.\n");
				printf("  -p <port>: Specify the port number.\n");
				printf("  -h Display usage.\n");

				mode = VOID; // End program if user chooses to display usage
			}
		}
	}

	string getNextArg(int argc, char* argv[], int& index)
	{
		if (index + 1 < argc)
		{
			return argv[++index];
		}
		else
		{
			cerr << "Missing argument for option: " << argv[index] << endl;
			mode = VOID; // End program by setting mode to VOID
			return VOID; // Return void for whatever switch is missing an argument
		}
	}
};

// ------------------------------------------------------
// creating struct for FileMetadata 
struct FileMetadata 
{
	char fileName[256];
	uint32_t fileSize;
	uint32_t CRC;

	FileMetadata(const string& filePath)
	{
		getMetadata(filePath);
		calculateCRC(filePath);
	}

private:

	void getMetadata(const string& filePath)
	{
		// Open the file
		ifstream file(filePath, ios::binary);

		if (!file.is_open()) 
		{
			cerr << "Error opening file: " << filePath << endl;
			exit(1);
			// Set default values for metadata
			strcpy(fileName, "");
			fileSize = 0;
		}

		// Get the file name
		const char* lastSlash = strrchr(filePath.c_str(), '\\');

		if (lastSlash == nullptr) 
		{
			lastSlash = fileName;
		}
		else 
		{
			lastSlash++;
		}
		strcpy(fileName, lastSlash);

		// Get the file size
		file.seekg(0, ios::end);
		fileSize = file.tellg();
		file.seekg(0, ios::beg);

		// Close the file
		file.close();
	}

public:
	// serialization method by taking metadata and inserting into byte vector 
	vector<unsigned char> serializeMetadata(const FileMetadata& metadata)
	{
		vector<unsigned char> buffer;

		// need to store 32-bit ints into 4 bytes and store into byte vector 
		for (int i = 0; i <  sizeof(uint32_t); ++i)
		{
			// use shift operator to shift counter i * 8 bits to right 
			// use 0xFF as a mask to isolate right side byte to append to buffer
			unsigned char byte = (metadata.fileSize >> (i * 8)) & 0xFF;
			buffer.push_back(byte);
		}
	}

	void calculateCRC(const string& filePath)
	{
		// Open the file
		ifstream file(filePath, ios::binary);

		if (!file.is_open())
		{
			cerr << "Error opening file: " << filePath << endl;
			exit(1);
		}

		// Read file contents into a buffer
		vector<char> buffer(std::istreambuf_iterator<char>(file), {});

		// Calculate CRC
		uint32_t crc = CRC::Calculate(buffer.data(), buffer.size(), CRC::CRC_32());

		CRC = crc;
	}

	// methods such as getting the file metadata from file by using the file path 
	// reading file size and compute the CRC 
	// convert between FileMetadata and byte array for manual byte array manipulation ? 

};




int main(int argc, char* argv[])
{
	// parse command line
	// create instance of CommanLineArg constructor 
	CommandLineArg arguments(argc, argv);

	cout << "Mode: " << arguments.mode << endl;
	cout << "File Path: " << arguments.filePath << endl;
	cout << "Address: " << arguments.address << endl;
	cout << "Port: " << arguments.port << endl;

	enum Mode
	{
		Client,
		Server
	};

	Mode mode = Server;
	Address address;

	/*
	*
	* HERE WE HAVE TO RETRIEVE ADDITIONAL COMMAND LINE ARGUMENTS
	* If Condition for mode == client 
	*	{ after serialization of metadata send file data }
	* else if mode == server 
	*	{ receive file metadata and associated data }
	* return 0;
	* 
	*/

	if (arguments.mode == VOID)
	{
		return 0;
	}
	else if (arguments.mode == CLIENT)
	{
		mode = Client;
		int a, b, c, d;
		if (sscanf(arguments.address.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d))
		{
			address = Address(a, b, c, d, arguments.port);
		}

		FileMetadata metadata(arguments.filePath);
		cout << "File name " << metadata.fileName << endl;
		cout << "File size " << metadata.fileSize << endl;
		cout << "CRC: " << metadata.CRC << endl;
	}
	else if (arguments.mode == SERVER)
	{
		mode = Server;
		int a, b, c, d;
		if (sscanf(arguments.address.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d))
		{
			address = Address(a, b, c, d, arguments.port);
		}

		// Receive metadata and file data
	}

	// initialize

	if (!InitializeSockets())
	{
		printf("failed to initialize sockets\n");
		return 1;
	}

	ReliableConnection connection(ProtocolId, TimeOut);

	const int port = mode == Server ? ServerPort : ClientPort;

	if (!connection.Start(port))
	{
		printf("could not start connection on port %d\n", port);
		return 1;
	}
		
	if (mode == Client)
	{
		connection.Connect(address);
	}
	else
	{
		connection.Listen();
	}

	bool connected = false;
	float sendAccumulator = 0.0f;
	float statsAccumulator = 0.0f;

	FlowControl flowControl;

	bool loopFlag = true;

	char filename[256];
	int filesize;
	long long int crc;

	while (loopFlag)
	{
		// update flow control

		if (connection.IsConnected())
			flowControl.Update(DeltaTime, connection.GetReliabilitySystem().GetRoundTripTime() * 1000.0f);

		const float sendRate = flowControl.GetSendRate();

		// detect changes in connection state

		if (mode == Server && connected && !connection.IsConnected())
		{
			flowControl.Reset();
			printf("reset flow control\n");
			connected = false;
		}

		if (!connected && connection.IsConnected())
		{
			printf("client connected to server\n");
			connected = true;
		}

		if (!connected && connection.ConnectFailed())
		{
			printf("connection failed\n");
			break;
		}

		
		if (mode == Client)
		{
			FileMetadata metadata(arguments.filePath);
			// Read file from disk
			ifstream file(arguments.filePath, ios::binary);
			if (!file.is_open())
			{
				printf("Error: Unable to open file\n");
				break;
			}

			// Extract file metadata
			string fileName = metadata.fileName;
			int fileSize = file.tellg();
			metadata.calculateCRC(arguments.filePath); 

			// starting transmission timer 
			clock_t startTimer = clock();

			// Send file metadata
			string MetaData = fileName + "|" + to_string(fileSize) + "|" + to_string(metadata.CRC);
			connection.SendPacket(reinterpret_cast<const unsigned char*>(MetaData.c_str()), MetaData.length());

			// Break file into pieces and send each piece
			const int chunkSize = 256; 
			char buffer[chunkSize];
			bool deliberateError = false; // Introduce an error to test Whole-File Error Detection Capabilities

			while (file)
			{
				file.read(buffer, chunkSize);
				int bytesRead = file.gcount();
				if (bytesRead > 0)
				{
					///////////////////////////////////////////////////////////////
					/////////////////////// MODIFY THIS ///////////////////////////
					///////////////////////////////////////////////////////////////
					// for the first byte change value that creates an error 
					if (!deliberateError)
					{
						buffer[0] ^= 0xff;
						deliberateError = true;
					}
					// sending the pieces 
					connection.SendPacket(reinterpret_cast<const unsigned char*>(buffer), bytesRead);
				}
			}

			loopFlag = false; // End top loop once file transfer is complete

			// Send message indicating file transfer completion
			string transferCompleteMessage = TRANSFER_COMPLETE;
			connection.SendPacket(reinterpret_cast<const unsigned char*>(transferCompleteMessage.c_str()), transferCompleteMessage.length());

			// ending transmission timer 
			clock_t endTimer = clock();

			// calculation to get transmission time in sec 
			double transmissionTime = double(endTimer - startTimer) / CLOCKS_PER_SEC;

			// calculation to get transfer speed 
			double transferSpeed = (metadata.fileSize * 8) / (transmissionTime * 1000000);

			printf("Transmission Time: %.2f secs\n", transmissionTime);
			printf("Transfer Speed: %.2f megabits/secs\n", transferSpeed);
		}

		while (true)
		{
			unsigned char packet[256];
			int bytes_read = connection.ReceivePacket(packet, sizeof(packet));
			if (bytes_read == 0)
				break;

			string receivedData(reinterpret_cast<char*>(packet), bytes_read);
			printf("Received data: %s\n", receivedData.c_str());

			// Use sscanf to parse the incoming metadata
			if (sscanf(receivedData.c_str(), "%255[^|]|%d|%lld", filename, &filesize, &crc) == 3) 
			{
				// Null-terminate the filename string
				filename[sizeof(filename) - 1] = '\0';

				// The string is formatted as metadata
				printf("Filename: %s\n", filename);
				printf("Filesize: %d\n", filesize);
				printf("CRC: %lld\n", crc);
			}
			else if (receivedData.compare(TRANSFER_COMPLETE) == 0)
			{
				break;
			}
			else
			{
				// Write the received data to an output file
				ofstream outputFile(filename, ios::app); // Open the file in append mode
				if (outputFile.is_open()) 
				{
					outputFile.write(reinterpret_cast<const char*>(packet), bytes_read);
					outputFile.close();
					printf("Received data written to %s\n", filename);
				}
				else 
				{
					printf("Error: Failed to open file: %s\n", filename);
				}
			}
		}

		//uint32_t calculatedCRC = CRC::Calculate(&fileDataAccumulated[0], fileDataAccumulateda.size(), CRC::CRC_32());

		//if (calculatedCRC == expectedCRC) 
		//{
		//	printf("CRC check for File Integrity passed.\n");
		//}
		//else 
		//{
		//	printf("CRC check for File Integrity failed.\n");
		//}

		// show packets that were acked this frame

#ifdef SHOW_ACKS
		unsigned int* acks = NULL;
		int ack_count = 0;
		connection.GetReliabilitySystem().GetAcks(&acks, ack_count);
		if (ack_count > 0)
		{
			printf("acks: %d", acks[0]);
			for (int i = 1; i < ack_count; ++i)
				printf(",%d", acks[i]);
			printf("\n");
		}
#endif

		// update connection

		connection.Update(DeltaTime);

		// show connection stats

		statsAccumulator += DeltaTime;

		while (statsAccumulator >= 0.25f && connection.IsConnected())
		{
			float rtt = connection.GetReliabilitySystem().GetRoundTripTime();

			unsigned int sent_packets = connection.GetReliabilitySystem().GetSentPackets();
			unsigned int acked_packets = connection.GetReliabilitySystem().GetAckedPackets();
			unsigned int lost_packets = connection.GetReliabilitySystem().GetLostPackets();

			float sent_bandwidth = connection.GetReliabilitySystem().GetSentBandwidth();
			float acked_bandwidth = connection.GetReliabilitySystem().GetAckedBandwidth();

			printf("rtt %.1fms, sent %d, acked %d, lost %d (%.1f%%), sent bandwidth = %.1fkbps, acked bandwidth = %.1fkbps\n",
				rtt * 1000.0f, sent_packets, acked_packets, lost_packets,
				sent_packets > 0.0f ? (float)lost_packets / (float)sent_packets * 100.0f : 0.0f,
				sent_bandwidth, acked_bandwidth);

			statsAccumulator -= 0.25f;
		}

		net::wait(DeltaTime);
	}

	ShutdownSockets();

	return 0;
}

