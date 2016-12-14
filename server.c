/**
 * TCP-Server-Programm
 *
 * startet einen TCP Server und akzeptiert auf dem eingestellten Port (default 4030)
 * auf der eingestellten Adresse (standardmaessig 0.0.0.0) TCP-Verbindungen.
 * Von einem Client eingehende Daten werden an alle anderen weiterverteilt.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>

#define SERVER_PORT 4030

// maximale Anzahl gleichzeitig erlaubter TCP-Clients
#define MAX_CLIENTS 1000

// Groesse des Puffers fuer eingehende Daten
#define BUFSIZE 100

// Anzahl Sekunden, die select() auf Daten warten soll
#define TIME_WAIT_FOR_DATA 1

int main()
{
    // ein TCP-Socket oeffnen
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0)
    {
        printf("Fatal: Hab' kein Socket bekommen\n");
        exit(1);
    }

    int iSetOption = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption, sizeof(iSetOption));

    struct sockaddr_in server_address;
    // struct leeren
    bzero(&server_address, sizeof(struct sockaddr_in));
    // struct mit Inhalten fuellen
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY; //inet_addr("127.0.0.1");
    server_address.sin_port = htons(SERVER_PORT);

    // binden
    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(struct sockaddr_in)) < 0)
    {
        printf("Fatal: Socket zu binden hat nicht geklappt\n");
        exit(1);
    }

    // als lauschendes Socket markieren
    listen(server_socket, 42);

    fcntl(server_socket, F_SETFL, O_NONBLOCK);

    // Variable fuer die Adresse des sich gerade verbindenden Clients
    struct sockaddr_in remote_address;
    socklen_t fromlen = sizeof(struct sockaddr_in);

    struct timeval timeout;

    // Puffer fuer eingehende Daten
    char buffer[BUFSIZE];

    // der Satz aller File-Deskriptoren, die von select() ueberwacht werden
    fd_set file_descriptors;
    // Array der Deskriptoren der verbundenen Clients
    int client_socket[MAX_CLIENTS];
    // Anzahl der aktuell verbundenen Clients
    uint16_t num_clients = 0;
    // der jeweils aktuell hoechste File-Deskriptor
    int maxfd = server_socket;

    printf("Der Server wartet auf Port %d...\n", SERVER_PORT);

    // unendlich, bis jemand Strg+C drueckt...
    while(1)
    {
        // muss man bei jedem Schleifendurchlauf neu setzen, da select() das auf Null herunterzaehlt
        timeout.tv_sec  = TIME_WAIT_FOR_DATA;
        timeout.tv_usec = 0;

        // muss man bei jedem Schleifendurchlauf neu setzen, da select() das leert
        // zur Sicherheit Satz nochmal leeren
        FD_ZERO(&file_descriptors);
        // Server-Socket hinzufuegen: select() soll auf neue Verbindungen reagieren
        FD_SET(server_socket, &file_descriptors);

        // auch die Clients zum Satz hinzufuegen
        for (uint16_t i=0; i<num_clients; i++)
        {
            // ist das Socket noch offen, ist der Client noch verbunden?
            if (client_socket[i] > 0)
            {
                FD_SET(client_socket[i], &file_descriptors);
            }
        }

        // gibt es Aktivitaet in irgendeinem File-Deskriptor?
        if (select(maxfd+1, &file_descriptors, NULL, NULL, &timeout) == 1)
        {
            printf("Es wurde was gefunden!\n");

            // gibt es Aktivitaet auf dem Server-Socket?
            if (FD_ISSET(server_socket, &file_descriptors))
            {
                // das muss dan eine Verbindungsanfrage sein
                if (num_clients < MAX_CLIENTS)
                {
            		int newfd = accept(server_socket, (struct sockaddr*) &remote_address, (socklen_t*) &fromlen);

            		// neuen Deskriptor zum Satz der von select() ueberwachten Deskriptoren hinzufuegen
            		FD_SET(newfd, &file_descriptors);

                    // maximal ueberwachten Deskriptor ggf. erhoehen
            		if (newfd > maxfd)
            		{
            		    maxfd = newfd;
            		}

                    // Deskriptor merken, damit wir ihn mit select() abfragen koennen
                    client_socket[num_clients] = newfd;
                    num_clients++;

        	    	      printf("Der %d. Client hat sich verbunden: Socket-Deskriptor %d, IP: %s, Port %d \n" , num_clients, newfd, inet_ntoa(remote_address.sin_addr) , ntohs(remote_address.sin_port));
                }
                else
                {
                    printf("Verbindungsanfrage nicht angenommen: Maximale Client-Anzahl erreicht\n");
                }
            }
            else
            {
                // das muss dann ein Daten-Eingang von einem verbundenen Client sein
                printf("Yay! Ich hab' was bekommen!\n");

                // Empfangs-Puffer vor dem Empfangen mit Nullen fuellen
                memset(buffer, 0, BUFSIZE);

                // herausfinden, welcher Client gesendet hat
                for (uint16_t i=0; i<num_clients; i++)
                {
                    // es war dieser hier
                    if (FD_ISSET(client_socket[i], &file_descriptors))
                    {
                        int n = recvfrom(client_socket[i], buffer, BUFSIZE, 0, (struct sockaddr*) &remote_address, &fromlen);
                        if (n < 0)
                        {
                            printf("Fehler: recvfrom() hat nicht geklappt\n");
                        }

                        printf("Client #%d hat geschrieben: %s\n", client_socket[i], buffer);

                        // Empfangene Daten an alle anderen Clients weiterverteilen
                        for (uint16_t j=0; j<num_clients; j++)
                        {
                            // kein Echo
                            if (i != j)
                            {
                                printf("Weiterverteilen an Client %d...\n", client_socket[j]);
                                sendto(client_socket[j], buffer, n, 0, (struct sockaddr *) &remote_address, sizeof(struct sockaddr_in));
                            }
                        }
                    }
                }
            }
        }
        else
        {
            printf("Hab' nichts bekommen...\n");
        }
    }
}
