#include <string.h>

#include "socket_client_tcp.h"

///////////////////////////////
/////////// WINDOWS ///////////
//////////////////////////////
#ifdef _WIN32

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

SocketClientTCP::SocketClientTCP (const char *ip_addr, int port, bool recv_all_or_nothing)
{
    strcpy (this->ip_addr, ip_addr);
    this->port = port;
    this->recv_all_or_nothing = recv_all_or_nothing;
    connect_socket = INVALID_SOCKET;
    memset (&socket_addr, 0, sizeof (socket_addr));
}

int SocketClientTCP::connect ()
{
    WSADATA wsadata;
    int res = WSAStartup (MAKEWORD (2, 2), &wsadata);
    if (res != 0)
    {
        return (int)SocketClientTCPReturnCodes::WSA_STARTUP_ERROR;
    }
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons (port);
    if (inet_pton (AF_INET, ip_addr, &socket_addr.sin_addr) == 0)
    {
        return (int)SocketClientTCPReturnCodes::PTON_ERROR;
    }
    connect_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect_socket == INVALID_SOCKET)
    {
        return (int)SocketClientTCPReturnCodes::CREATE_SOCKET_ERROR;
    }

    // ensure that library will not hang in blocking recv/send call
    DWORD timeout = 5000;
    setsockopt (connect_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof (timeout));
    setsockopt (connect_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof (timeout));

    if (::connect (connect_socket, (struct sockaddr *)&socket_addr, sizeof (socket_addr)) ==
        SOCKET_ERROR)
    {
        return (int)SocketClientTCPReturnCodes::CONNECT_ERROR;
    }

    return (int)SocketClientTCPReturnCodes::STATUS_OK;
}

int SocketClientTCP::set_timeout (int num_seconds)
{
    if ((num_seconds < 1) || (num_seconds > 100))
    {
        return (int)SocketClientTCPReturnCodes::INVALID_ARGUMENT_ERROR;
    }
    if (connect_socket == INVALID_SOCKET)
    {
        return (int)SocketClientTCPReturnCodes::CREATE_SOCKET_ERROR;
    }

    DWORD timeout = 1000 * num_seconds;
    setsockopt (connect_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof (timeout));
    setsockopt (connect_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof (timeout));

    return (int)SocketClientTCPReturnCodes::STATUS_OK;
}

int SocketClientTCP::send (const char *data, int size)
{
    int len = sizeof (socket_addr);
    int res = ::send (connect_socket, data, size, 0);
    if (res == SOCKET_ERROR)
    {
        return -1;
    }
    return res;
}

int SocketClientTCP::recv (void *data, int size)
{
    if (connect_socket == INVALID_SOCKET)
    {
        return -1;
    }
    int res = ::recv (connect_socket, (char *)data, size, 0);
    if (res == SOCKET_ERROR)
    {
        return -1;
    }
    for (int i = 0; i < res; i++)
    {
        temp_buffer.push (((char *)data)[i]);
    }
    // before we used SO_RCVLOWAT but it didnt work well
    // and we were not sure that it works correctly with timeout
    if (recv_all_or_nothing)
    {
        if (temp_buffer.size () < size)
        {
            return 0;
        }
        for (int i = 0; i < size; i++)
        {
            ((char *)data)[i] = temp_buffer.front ();
            temp_buffer.pop ();
        }
        return size;
    }
    else
    {
        for (int i = 0; i < res; i++)
        {
            ((char *)data)[i] = temp_buffer.front ();
            temp_buffer.pop ();
        }
        return res;
    }
}

void SocketClientTCP::close ()
{
    closesocket (connect_socket);
    connect_socket = INVALID_SOCKET;
    WSACleanup ();
}

///////////////////////////////
//////////// UNIX /////////////
///////////////////////////////
#else

#include <netinet/in.h>
#include <netinet/tcp.h>

SocketClientTCP::SocketClientTCP (const char *ip_addr, int port, bool recv_all_or_nothing)
{
    strcpy (this->ip_addr, ip_addr);
    this->port = port;
    this->recv_all_or_nothing = recv_all_or_nothing;
    connect_socket = -1;
    memset (&socket_addr, 0, sizeof (socket_addr));
}

int SocketClientTCP::connect ()
{
    connect_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect_socket < 0)
    {
        return (int)SocketClientTCPReturnCodes::CREATE_SOCKET_ERROR;
    }

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons (port);
    if (inet_pton (AF_INET, ip_addr, &socket_addr.sin_addr) == 0)
    {
        return (int)SocketClientTCPReturnCodes::PTON_ERROR;
    }

    // ensure that library will not hang in blocking recv/send call
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt (connect_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof (tv));
    setsockopt (connect_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof (tv));

    if (::connect (connect_socket, (struct sockaddr *)&socket_addr, sizeof (socket_addr)) < 0)
    {
        return (int)SocketClientTCPReturnCodes::CONNECT_ERROR;
    }

    return (int)SocketClientTCPReturnCodes::STATUS_OK;
}

int SocketClientTCP::set_timeout (int num_seconds)
{
    if ((num_seconds < 1) || (num_seconds > 100))
    {
        return (int)SocketClientTCPReturnCodes::INVALID_ARGUMENT_ERROR;
    }
    if (connect_socket < 0)
    {
        return (int)SocketClientTCPReturnCodes::CREATE_SOCKET_ERROR;
    }

    struct timeval tv;
    tv.tv_sec = num_seconds;
    tv.tv_usec = 0;
    setsockopt (connect_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof (tv));
    setsockopt (connect_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof (tv));

    return (int)SocketClientTCPReturnCodes::STATUS_OK;
}

int SocketClientTCP::send (const char *data, int size)
{
    int res = ::send (connect_socket, data, size, 0);
    return res;
}

int SocketClientTCP::recv (void *data, int size)
{
    if (connect_socket <= 0)
    {
        return -1;
    }
    int res = ::recv (connect_socket, (char *)data, size, 0);
    if (res < 0)
    {
        return res;
    }
    for (int i = 0; i < res; i++)
    {
        temp_buffer.push (((char *)data)[i]);
    }
    // before we used SO_RCVLOWAT but it didnt work well
    // and we were not sure that it works correctly with timeout
    if (recv_all_or_nothing)
    {
        if (temp_buffer.size () < size)
        {
            return 0;
        }
        for (int i = 0; i < size; i++)
        {
            ((char *)data)[i] = temp_buffer.front ();
            temp_buffer.pop ();
        }
        return size;
    }
    else
    {
        for (int i = 0; i < res; i++)
        {
            ((char *)data)[i] = temp_buffer.front ();
            temp_buffer.pop ();
        }
        return res;
    }
}

void SocketClientTCP::close ()
{
    ::close (connect_socket);
    connect_socket = -1;
}
#endif
