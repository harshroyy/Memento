#include <iostream>
#include <sys/socket.h> // for socket functions - socket(), bind(), listen(), accept(), recv(), send(), setsockopt()
#include <netinet/in.h> // for socket structures/constants related to IPv4 - sockaddr_in
#include <unistd.h> // for close() and fork() - UNIX system calls
#include <cstring> // for memset() and memcpy()
#include <string>
#include <sstream>
#include <netdb.h>

#define PORT 8080

using namespace std;

string extract_host(const string& http_request) { // Input : Raw HTTP Request (String) from Phase-1 -> Output : Clean Hostname (String)

    // 1. Convert the HTTP request into a stream of strings
    istringstream stream(http_request); // stream helps read the request line by line 
    
    string line; 

    while(getline(stream, line)) { // getline - reads one line from stream , and puts it into line
        if(line.find("Host: ") == 0) { // 0th idx pe Host hai toh isko extract krke return krde 
            string host = line.substr(6); // 6 characters aage se read krega

            if(!host.empty() && host.back() == '\r') { // HTTP/1.x lines normally end with: \r\n
                host.pop_back(); // toh last mein \r reh jaata hai, wo hatane ke liye 
            }
            return host; // finally returns cleaned hostname
        }
    }

    // agar koi host nahi mila toh 
    return "";
}

void handle_client(int client_socket) {
    
}

int main() {
    // 1. Creating a socket (IPv4, TCP)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); // aisa socket bnega jisse IPv4 par TCP connection accept kr sake , (AF_INET - IPv4 , SOCK_STREAM - TCP , 0 - default protocol)

    if(server_fd == -1) { // Most UNIX system returns -ve value when fails
        cerr << "Failed to create socket. \n";
        return 1;
    }

    // Allow port reuse , Baar baar server ko start/close/start karenge toh , OS will say address already in use (SO_REUSEADDR helps with certain address-reuse situations during restart)
    int opt = 1;
    
    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt, 
        sizeof(opt)
    );

    // 2. Bind the socket to an IP and Port
    sockaddr_in address; // stores info about our server's network address

    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections arriving on any local IPv4 interface
    address.sin_port = htons(PORT); // Host to Network Short - converts the number from the computer's host representation into network byte order.

    // Abb humne socket upar bnaa liya , address bnaa liya - bss socket isn't associated  with PORT 8080 yet 
    if(::bind( // bind() wohi krta hai
        server_fd, 
        (struct sockaddr*)&address, // Address jo humne upar bnaaya hai
        sizeof(address)
    ) < 0) {
        cerr << "Bind failed.\n";
        close(server_fd);
        return 1;
    }

    // 3. Listen for incoming connections
    if(listen(server_fd, 10) < 0) { // 10 - how many connections can wait in queue, if busy server
        cerr << "Listen failed.\n";
        close(server_fd);
        return 1;
    }

    cout << "Server is listening on port " << PORT << "\n";

    // 4. Accept clients continously
    while(true) { // coz , server shouldn't accept just one client and then die , it should keep running
        sockaddr_in client_addr; // store client details here
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept( // here client actually connects
            server_fd,
            (struct sockaddr*)&client_addr,
            &client_len
        );

        if(client_socket < 0) {
            cerr << "Accept failed.\n";
            continue;
        }

        cout << "\n --- New Client connected! ---\n";
        
        // 5. Read the HTTP request from client
        char buffer[4096]; // temporary memory area to store request

        int bytes_read = recv( // It reads bytes that have arrived from this client socket and put them into buffer.
            client_socket, // → where the data comes from.
            buffer, // → where the data goes.
            sizeof(buffer)-1, // → maximum amount to read.
            0
        );

        if(bytes_read > 0) {
            buffer[bytes_read] = '\0'; // bytes_read - tells us how many bytes we actually recieve (nhi toh buffer me extra garbage values bhi aa skti hai)
             
            cout << "Recieved Raw HTTP Request:\n"; 
            cout << buffer << "\n";

            // 6. Convert the request into a C++ string
            string request_str(buffer);

            // 7. Extract the target website 
            string target_website = extract_host(request_str);

            cout << "\n -- Target Website Extracted: "
                 << target_website
                 << " ---\n";

            // 8. DNS Lookup - Convert hostname into IP address
            struct addrinfo hints = {}; // addrinfo is a struct (defined in netdb.h) that holds address information , hints tells getaddrinfo() what kind of result we want

            hints.ai_family = AF_INET; // we want IPV4
            hints.ai_socktype = SOCK_STREAM; // we want TCP

            struct addrinfo* server_info; // This pointer will store DNS results

            int status = getaddrinfo( // Ask OS to resolve the hostname into an IP
                target_website.c_str(), // Who 
                "80", // Which Port
                &hints, // What kind
                &server_info // Where to store DNS result
            );

            if(status != 0) { // 0 -> Success , non-zero -> Error
                cerr << "DNS Lookup failed for: "
                     << target_website
                     << "\n";

                close(client_socket);
                continue;
            }

            cout << "DNS Lookup Successful!\n";

            // 9. Create a socket to communicate with real website
            int remote_socket = socket( // remote_socket -> real website ka socket
                server_info -> ai_family,
                server_info -> ai_socktype,
                server_info -> ai_protocol
            );

            if(remote_socket < 0) {
                cerr << "Failed to create remote socket.\n";
            } else {
                
                // 10. Connect to the real website
                if(connect(remote_socket, // Take this socket and establish a TCP connection to this address
                    server_info -> ai_addr,
                    server_info -> ai_addrlen
                ) == 0) {
                    cout << "Successfully Connected to " << target_website << ". Forwarding Traffic . . . \n";

                    // 11. Forward the original request
                    send( // Take the request we got from client and send it to the real website.
                        remote_socket,
                        buffer, 
                        bytes_read,
                        0
                    );

                    // 12. Receive the website's response
                    char remote_buffer[4096]; // create a buffer to recieve data from website

                    int remote_bytes_read;

                    while( // while coz TCP data streams me bhejega na , 5MB to 4096 , 4096 ko chunks me bhejega , ek ek karke 
                        (remote_bytes_read = recv( // recv > 0 -> data recieved , recv == 0 -> connection closed by server , recv < 0 -> error
                            remote_socket,
                            remote_buffer,
                            sizeof(remote_buffer),
                            0
                        )) > 0
                    ) {

                        send( // Forward the response to browser --> real website se recvd data ko browser ko bhejega chunks me also called relaying or proxying
                            client_socket,
                            remote_buffer,
                            remote_bytes_read,
                            0
                        );
                    }

                    cout << "Finished relaying data from "
                         << target_website
                         << "!\n";
                } else {
                    cerr << "Failed to connect to website.\n";
                }

                // 13. Close the remote socket
                close(remote_socket);
            }
            
            // 14. Free Memory allocated by getaddrinfo()
            freeaddrinfo(server_info); 


            // 15. Send a fake HTTP response 
            string body = "Hello from phase 1 : I heard your request.";

            string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + to_string(body.length()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" +
                body;
            
                send( // send these bytes through connected to client
                    client_socket, 
                    response.c_str(),
                    response.length(),
                    0
                );
        }

        // 16. Close the client connection
        close(client_socket);

        cout << "Client disconnected. \n";
    }

    // 17. Close the server socket
    close(server_fd);

    return 0;
}




