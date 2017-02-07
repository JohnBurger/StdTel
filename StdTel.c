/*
 * StdTel.cpp
 *
 *   Author: John Burger
 *
 *   This simple program merely opens a TCP connection to the specified
 *   address and port, and sends data back and forth from/to stdin/stdout.
 *
 *   This code is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 */

#pragma warning (disable:4668) // Macro not defined
#pragma warning (disable:4710) // Function not inlined
#pragma warning (disable:4711) // Function selected for inlining
#pragma warning (disable:4820) // Padding added

#define STRICT
#define WIN32_LEAN_AND_MEAN
#define WINVER              _WIN32_WINNT_NT4 // Earliest available option to Windows 98
#define _WIN32_WINNT        _WIN32_WINNT_NT4
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Windows.h>
#include <WinSock2.h>

#include <stdio.h>
#include <stdlib.h>

#define ErrorSuffix "\a\r\n"

const u_short TelnetPort = 23;

const char ExitChar = ']' - '@'; // Convert to <Ctrl> character

u_short port = TelnetPort;

void Usage(const char *argv0) {
    const char *ptr = argv0;
    while (*ptr++ != '\0') {
    } // while
    while (ptr > argv0) {
        if (*--ptr == '\\') {
            break;
        } // if
    } // while
    printf("Usage:\r\n");
    printf("      %s <host> [<port>]\r\n", ptr);
    printf("Communicate with <host> on <port> [default=%d] over a network.\r\n", TelnetPort);
    printf("To exit, type <Ctrl><%c><Enter>\r\n", ExitChar+'@');
} // Usage(argv0)

bool __cdecl WSACheck(bool check, const char *format, ...) {
    if (check) {
        return true;
    } // if
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);

    int error = WSAGetLastError();
    char errorString[256];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | 79u, NULL, (DWORD)error, 0, errorString, sizeof errorString, NULL);
    fprintf(stderr, " (Error: %d)" ErrorSuffix "%s\r\n\r\n", error, errorString);
    va_end(args);
    return false;
} // WSACheck(check, format, ...)

bool Connect(SOCKET s, const hostent &host) {
    sockaddr_in addr = { AF_INET, htons(port),
                         { { (UCHAR)host.h_addr[0], (UCHAR)host.h_addr[1],
                             (UCHAR)host.h_addr[2], (UCHAR)host.h_addr[3] } } };

    printf("Connecting to %s", host.h_name);
    if (port != TelnetPort) {
        printf(" on port %d", port);
    } // if
    printf(".\r\n");

    const char * const *alias = host.h_addr_list;
    while (*alias!=NULL) {
        addr.sin_addr.S_un = { (UCHAR)(*alias)[0], (UCHAR)(*alias)[1],
                               (UCHAR)(*alias)[2], (UCHAR)(*alias)[3] };
        printf("Trying %u.%u.%u.%u... ",
            addr.sin_addr.S_un.S_un_b.s_b1, addr.sin_addr.S_un.S_un_b.s_b2,
            addr.sin_addr.S_un.S_un_b.s_b3, addr.sin_addr.S_un.S_un_b.s_b4);
        if (connect(s, (sockaddr *)&addr, sizeof addr) == 0) {
            printf("Success!\r\n");
            return true;
        } // if
        printf("Failed.\r\n");
        ++alias;
    } // while
    return false;
} // Connect(s, host)

DWORD WINAPI Send(void *param) {
    SOCKET s = (SOCKET)param;
    for (;;) {
        int got = getc(stdin);
        if ((got == EOF) || (got == ExitChar)) {
            break;
        } // if
        char c = (char)got;
        if (send(s, &c, 1, 0) == SOCKET_ERROR) {
            break;
        } // if
    } // for
    shutdown(s, SD_SEND);
    return 0;
} // Send(param)

int __cdecl main(int argc, char *argv[], char * /*env*/[]) {
    if (argc<2 || argc>3) {
        Usage(argv[0]);
        return 1;
    } // if

    WSADATA wsaData;
    int wsaStartup = WSAStartup(MAKEWORD(1, 1), &wsaData);
    if (wsaStartup != 0) {
        fprintf(stderr, "Unable to initialise Socket interface!" ErrorSuffix);
        fprintf(stderr, "Error %d\r\n", wsaStartup);
        return 2;
    } // if

    hostent *host = gethostbyname(argv[1]);
    if (!WSACheck(host != NULL, "Host %s not found!", argv[1])) {
        Usage(argv[0]);
        return 3;
    } // if

    if (argc == 3) {
        port = (u_short)atoi(argv[2]);
        if (port == 0) {
            servent *serv = getservbyname(argv[2], "tcp");
            if (serv != NULL) {
                port = ntohs((u_short)serv->s_port);
            } // if
        } // if
    } // if
    if (port == 0) {
        fprintf(stderr, "Invalid port: %s\r\n", argv[2]);
        Usage(argv[0]);
        return 4;
    } // if

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (!WSACheck(s!=INVALID_SOCKET, "Could not create Socket!")) {
        return 5;
    } // if

    BOOL option = TRUE;
    if (!WSACheck(setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&option, sizeof option)!=SOCKET_ERROR,
                  "Socket option error!")) {
        return 6;
    } // if
    if (!Connect(s, *host)) {
        fprintf(stderr, "Could not connect to %s on port %d!" ErrorSuffix, argv[1], port);
        closesocket(s);
        return 7;
    } // if
    if (CreateThread(NULL, 0, &Send, (void *)s, 0, NULL)==NULL) {
        fprintf(stderr, "Out of resources!" ErrorSuffix);
        closesocket(s);
        return 8;
    } // if
    for (;;) {
        char buffer[1024];
        int rxed = recv(s, buffer, sizeof buffer, 0);
        if (rxed < 0) {
            closesocket(s);
            return 9;
        } // if
        if (rxed == 0) {
            break;
        } // if
        for (int i = 0; i < rxed; ++i) {
            char c = buffer[i];
            if (c != '\0') {
                putc(c, stdout);
            } // if
        } // for
    } // for
    closesocket(s);
    return 0;
} // main(argc, argv, env)
