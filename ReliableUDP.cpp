/*
	Reliability and Flow Control Example
	From "Networking for Game Programmers" - http://www.gaffer.org/networking-for-game-programmers
	Author: Glenn Fiedler <gaffer@gaffer.org>
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "Net.h"
#include "CRC.h"

#pragma warning(disable:4996)

using namespace std;
using namespace net;

//#define SHOW_ACKS

const int ServerPort = 30000;
const int ClientPort = 30001;
const int ProtocolId = 0x11223344;
const float DeltaTime = 1.0f / 30.0f;
const float SendRate = 1.0f / 30.0f;
const float TimeOut = 10.0f;
const int PacketSize = 256;

// Structure to hold file metadata
struct FileMetadata 
{
	string filename;
	streamsize fileSize;
};

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

// ----------------------------------------------
// creating a struct for parsing command line arguments 
struct CommandLineArg
{
	string mode;
	string filePath;
	string checksumMethod = "CRC32";
	string defaultAddress = "127.0.0.1";
	int defaultPort = 30000;

	// or initialize a constructor here with the default values 
	CommandLineArg(int argc, char* argv[])
	{ 
		// Function to parseArgs(argc, argv);
	
	}

private: 
	void parseArgs() 
	{
		// for loop for each argument on the command line 
			// add handlers for mode argument 
			// add file handler 
			// add crc handler 
			// add default ip address handler 
			// add default port handler 
	}

	void handleMode() {}
	void handleFile() {} 
	void handleChecksum() {}
	void handleAddress() {}
	void handlePort() {}
	
};

// ------------------------------------------------------
// creating struct for FileMetadata 
struct FileMetadata 
{
	char fileName[256];
	uint32_t fileSize;
	uint32_t crc;

	// methods such as getting the file metadata from file by using the file path 
	// reading file size and compute the CRC 
	// convert between FileMetadata and byte array for manual byte array manipulation ? 

};

int main(int argc, char* argv[])
{
	// parse command line
	// creat instance of CommanLineArg constructor 
	CommandLineArg arguments(argc, argv);

	enum Mode
	{
		Client,
		Server
	};

	Mode mode = Server;
	Address address;

	string filename;

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

	if (argc == 3)
	{
		int a, b, c, d;

		if (sscanf(argv[1], "%d.%d.%d.%d", &a, &b, &c, &d))
		{
			mode = Client;
			address = Address(a, b, c, d, ServerPort);
		}

		filename = argv[2];
	}
	else if (argc == 1)
	{
		mode = Server;
	}
	else
	{
		printf("Usage: program_name IP_address filename\n");
		printf("Description: This program sends a file to the specified IP address over the network.\n");
		printf("Arguments:\n");
		printf("  - IP_address: The IP address of the destination.\n");
		printf("  - filename: The name of the file to send.\n");
		return 0;
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

		// Open file
		ifstream file(filename.c_str());

		if (file.is_open())
		{
			cout << "File opened successfully" << endl;

			// Get file size
			file.seekg(0, ios::end);
			streamsize fileSize = file.tellg();
			file.seekg(0, ios::beg);

			// Chunk size
			const int chunkSize = 256;

			// Calculate number of chunks
			int numOfChunks = (fileSize + chunkSize - 1) / chunkSize;

			// Create buffer for chunk
			vector<char> chunk(chunkSize);


		}
		else
		{
			// Failed to open file
			cerr << "Failed to open file: " << strerror(errno) << " " << filename << endl;
			return 1;
		}
	}
	else
	{
		connection.Listen();
	}

	bool connected = false;
	float sendAccumulator = 0.0f;
	float statsAccumulator = 0.0f;

	FlowControl flowControl;

	while (true)
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

		// send and receive packets

		sendAccumulator += DeltaTime;

		while (sendAccumulator > 1.0f / sendRate)
		{
			unsigned char packet[PacketSize];
			memset(packet, 0, sizeof(packet));
			
			connection.SendPacket(packet, sizeof(packet));
			sendAccumulator -= 1.0f / sendRate;
		}

		while (true)
		{
			unsigned char packet[256];
			int bytes_read = connection.ReceivePacket(packet, sizeof(packet));
			if (bytes_read == 0)
				break;

			printf("Packet: %s\n", packet);
		}

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