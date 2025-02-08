/*
 * P1 SAMPLE SERVER
 * ---------------
 * Author: Thoshitha Gamage
 * Date: 01/29/2025
 * License: MIT License
 * Description: This is a sample code for CS447 Spring 2025 P1 server code.
 */

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <system_error>
#include <fstream>
#include <algorithm>
#include <array>
#include <optional>
#include <filesystem>
#include <format>
#include <thread>
#include <chrono>
#include "p1_helper.h"

using namespace std;

#define BACKLOG 10
#define MAXDATASIZE 100

// Signal handler for SIGCHLD
void sigchld_handler(int s) {
    (void)s;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// Get sockaddr, IPv4 or IPv6
void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Convert string to camel case
std::string toCamelCase(const std::string& input) {
    std::string output;
    bool capitalize = true;
    for (char c: input) {
        if (std::isalpha(c)) {
            output += capitalize? std::toupper(c): std::tolower(c);
            capitalize =!capitalize;
        } else {
            output += c;
        }
    }
    return output;
}
// modes
enum class Modes {
	START,
	BROWSE,
	RENT,
	MYGAMES
};

//list mode for testing
const char* modeToString(Modes mode) {
    switch (mode) {
        case Modes::START: return "START";
        case Modes::BROWSE: return "BROWSE";
        case Modes::RENT: return "RENT";
        case Modes::MYGAMES: return "MYGAMES";
        default: return "Unknown Mode";  // Default case for safety
    }
}

// browse function


// Log events with timestamp
void logEvent(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S") << "] " << message << std::endl;
}

int main(int argc, char* argv[]) {
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
   // Modes mode = Modes::START;
   // bool initialized = false; // to ensure client uses HELO <hostname>
    string serverHostname;
    std::memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (argc != 2) {
        std::cerr << std::format("Usage: {} <config_file>\n", *argv);
        return 1;
    }

    std::string configFileName = argv[1];
    std::optional<std::string> port;

    std::filesystem::path configFilePath(configFileName);
    if (!std::filesystem::is_regular_file(configFilePath)) {
        std::cerr << std::format("Error opening configuration file: {}\n", configFileName);
        return 1;
    }

    std::ifstream configFile(configFileName);
    std::string line;
    while (std::getline(configFile, line)) {
        std::string_view lineView(line);
	if (lineView.substr(0, 16) == "SERVER_HOSTNAME=") {
		serverHostname = line.substr(16);
	}	
        if (lineView.substr(0, 5) == "PORT=") {
            port = lineView.substr(5);
            //break;
        }
    }
    configFile.close();

    if (!port.has_value()) {
        std::cerr << "Port number not found in configuration file!\n";
        return 1;
    }

    if ((rv = getaddrinfo(nullptr, port->c_str(), &hints, &servinfo))!= 0) {
        std::cerr << std::format("getaddrinfo: {}\n", gai_strerror(rv));
        return 1;
    }

    // Loop through all the results and bind to the first we can
    for (p = servinfo; p!= NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            std::perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            throw std::system_error(errno, std::generic_category(), "setsockopt");
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            std::perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        std::cerr << "server: failed to bind\n";
        return 2;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        throw std::system_error(errno, std::generic_category(), "listen");
    }

    sa.sa_handler = sigchld_handler; // Reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        throw std::system_error(errno, std::generic_category(), "sigaction");
    }

    std::cout << "server: waiting for connections...\n";

    while (true) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
        if (new_fd == -1) {
            std::perror("accept");
            continue;
        }

	// initial connection
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof s);
	logEvent("Connection from: " + std::string(s));
        // Create a new thread to handle the client communication
        std::thread clientThread([new_fd, s, serverHostname]() {
            std::array<char, MAXDATASIZE> buf;
            int numbytes;
	    Modes mode = Modes::START;
    	    bool initialized = false;
            
	    while (true) {
                if ((numbytes = recv(new_fd, buf.data(), MAXDATASIZE - 1, 0)) == -1) {
                    perror("recv");
                    exit(1);
                }  else if (numbytes == 0) {
                    logEvent("Client disconnected: " + std::string(s));
                    break;
                }

                buf[numbytes] = '\0';
                std::string receivedMsg(buf.data());

		// formatting
		receivedMsg.erase(receivedMsg.find_last_not_of(" \n\r\t") + 1);
		std::transform(receivedMsg.begin(), receivedMsg.end(), receivedMsg.begin(), ::toupper);

		std::cout << "Received message: '" << receivedMsg << "'" << std::endl;
		string badRequest = "400 BAD REQUEST";
	        string received = "200 BYE";
		string myGames = "230 Switched to Mygames Mode";
		string browse = "210 Switched to Browse Mode";
		string rent = "220 Switched to Rent Mode";
	        string greeting = "HELO " + string(s) + " (TCP)";
		string helo = "HELO " + serverHostname;
		cout << "server hostName: " << serverHostname << endl;
		
		// bye command
                if (receivedMsg == "BYE") {
                	if (send(new_fd, received.c_str(), received.size(), 0) == -1) {
                        	perror("send");
                        }
                        close(new_fd);
                        break;
                }

		if (initialized == true) { // this code is allowed after the greeting
	     		if (receivedMsg == "BROWSE") {
				if (send(new_fd, browse.c_str(), browse.size(), 0) == -1) 
				{
					perror("send");
				}
				mode = Modes::BROWSE;
			}
		
			else if(receivedMsg == "RENT") {
				if (send(new_fd, rent.c_str(), rent.size(), 0) == -1)
				{
                                       	perror("send");
                               	}
				mode = Modes::RENT;
			}	

			else if (receivedMsg == "MYGAMES") {

				if (send(new_fd, myGames.c_str(), myGames.size(), 0) == -1)
				{
                                     perror("send");
                               	}
				mode = Modes::MYGAMES;
			}
	
			else { //bad request
				if (send(new_fd, badRequest.c_str(), badRequest.size(), 0) == -1) {
                                	perror("send");
                        	}
			}

		}	
		
		if (initialized == false) { // pre-greeting
                        if (receivedMsg == helo) {
                                initialized = true;
                                if (send(new_fd, greeting.c_str(), greeting.size(), 0) == -1) {
                                        perror("send");
                                }
                        }
                        else { // bad request
                                if (send(new_fd, badRequest.c_str(), badRequest.size(), 0) == -1) {
                                perror("send");
                                }
                        }

                }			
		
		if (mode == Modes::BROWSE) {
			cout << "in browse mode\n";
		}
		else if (mode == Modes::RENT) {
			cout << "in rent mode\n";
		}
		else if (mode == Modes::MYGAMES) {
			cout << "in mygames mode";
		}
		
		           	
	    }
            close(new_fd);
        });

        clientThread.detach();
    }

    return 0;
}
