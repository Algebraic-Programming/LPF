
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdexcept>
#include <vector>
#include <cerrno>
#include <numeric>
#include <functional>
#include <cstring>

#include <mpi.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <setjmp.h>

#include "log.hpp"
#include "dynamichook.hpp"
#include "time.hpp"
#include "assert.hpp"

#define USE_SIGALRM 1

namespace lpf { namespace mpi {

struct _LPFLIB_LOCAL HookException : public std::runtime_error { 
    HookException( const std::string & reason )
        : std::runtime_error("Could not hook, because " + reason )
    {}
};

#ifdef MPI_HAS_OPEN_PORT
namespace {

    // Compares two sockaddr objects. 
    bool isSocketEqual( const struct sockaddr * a, const struct sockaddr * b )
    {
        if ( a->sa_family != AF_INET && a->sa_family != AF_INET6 )
            throw HookException("Unsupported address family");

        return a->sa_family == b->sa_family
            && (! (a->sa_family == AF_INET) 
                    || std::memcmp( &reinterpret_cast<const struct sockaddr_in*>(a)->sin_addr,
                        &reinterpret_cast<const struct sockaddr_in*>(b)->sin_addr,
                        sizeof(struct in_addr)) == 0
               )
            && (! (a->sa_family == AF_INET6) 
                    || std::memcmp( &reinterpret_cast<const struct sockaddr_in6*>(a)->sin6_addr,
                        &reinterpret_cast<const struct sockaddr_in6*>(b)->sin6_addr,
                        sizeof(struct in6_addr)) == 0
               );
    }

    bool amIServer( const std::string & serverNode, const std::string & serverPort )
    {
        int rc = 0; 
        struct addrinfo * addrinfoHints = NULL;
        struct addrinfo * serverAddresses = NULL;

        rc = getaddrinfo( serverNode.c_str(), serverPort.c_str(), addrinfoHints, &serverAddresses);

        if ( 0 != rc )
            throw HookException( gai_strerror(rc) );

        struct ifaddrs * myAddresses = NULL;
        rc = getifaddrs( &myAddresses );

        if ( -1 == rc )
        {
            freeaddrinfo( serverAddresses );
            throw HookException( std::strerror(rc) );
        }

        bool match = false;
        for ( struct ifaddrs * myAddr = myAddresses; 
                myAddr != NULL; 
                myAddr = myAddr->ifa_next )
        {
            if ( myAddr->ifa_addr->sa_family != AF_INET && myAddr->ifa_addr->sa_family != AF_INET6 )
                continue;

            for ( struct addrinfo * targetAddr = serverAddresses;
                    targetAddr != NULL;
                    targetAddr = targetAddr->ai_next)
            {
                if ( targetAddr->ai_family != AF_INET && targetAddr->ai_family != AF_INET6 )
                    continue;

                if ( isSocketEqual( myAddr->ifa_addr , targetAddr->ai_addr ) )
                {
                    match = true;
                }
            }
        }

        freeifaddrs( myAddresses );
        freeaddrinfo( serverAddresses);

        return match;
    }

    size_t readAll( int fd, void * b, size_t size)
    {
        size_t x=0;
        ssize_t y = 0;
        char * buf = static_cast<char *>(b);

        while (x < size)
        {
            y = read(fd, buf + x, size-x);
            if (y == 0 || y < 0)
            {
                break;
            }
            x = x + y;
        }
        return x;
    }

    void closeSock( int s ) {
        std::string errnoMsg;
        if( shutdown(s, SHUT_RDWR ) ) {
            errnoMsg = std::strerror(errno);
            LOG (2, "Could not shut down socket, because " << errnoMsg ); 
        }
        else
            LOG( 3, "Socket was successfully shut down");

        
        if (close(s)) {
            errnoMsg = std::strerror(errno);
            LOG (2, "Could not close socket, because " << errnoMsg ); 
        }
        else
            LOG( 3, "Socket closed");
    }

    // magic value to check when establishing connection
    const char magicClient[] = "LPF_v0_client";
    const char magicServer[] = "LPF_v0_server";

    std::vector<int> serverAcceptsClients(
            const std::string & serverPort, int nprocs, Time timeout)
    {
        // lookup my interfaces
        int rc = 0; 

        Time t0 = Time::now();
        Time t1 = t0;

        struct addrinfo addrinfoHints;
        std::memset( &addrinfoHints, 0, sizeof(addrinfoHints) );
        addrinfoHints.ai_family = AF_UNSPEC;
        addrinfoHints.ai_socktype = static_cast<int>(SOCK_STREAM);
        addrinfoHints.ai_protocol = 0;
        addrinfoHints.ai_flags = AI_PASSIVE;

        struct addrinfo * serverAddresses = NULL;
        rc = getaddrinfo( NULL, serverPort.c_str(), &addrinfoHints, &serverAddresses);

        if ( 0 != rc )
            throw HookException( gai_strerror(rc) );

        std::vector< bool > success(nprocs, false); success[0] = true;
        std::vector< int > connections( nprocs, -1 );

        // for each interface set-up a listing server socket
        errno = 0; const char * errnoMsg = NULL;
        std::vector< struct pollfd > serverSockets;
        do {
            for ( const struct addrinfo * addr = serverAddresses; 
                    addr != NULL; 
                    addr = addr->ai_next
                ) 
            {
                int s = socket( addr->ai_family, addr->ai_socktype, addr->ai_protocol);
                if ( s == -1) { 
                    errnoMsg = std::strerror(errno);
                    LOG( 2, "Server could not open socket, because " << errnoMsg
                            << ". Skipping to next interface" ); 
                    continue;
                }
                int yes = 1;
                rc = setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes) );
                if ( rc == -1) { 
                    errnoMsg = std::strerror(errno); 
                    LOG( 2, "Server could not configure a server socket, because " << errnoMsg 
                             << ". Skipping to next interface." ); 
                    closeSock(s);
                    LOG( 3, "Server-connection socket closed");
                    continue;
                }
                rc = bind( s, addr->ai_addr, addr->ai_addrlen);
                if ( rc == -1) { 
                    errnoMsg = std::strerror(errno); 
                    LOG( 2, "Server could not bind server socket, because " << errnoMsg
                            << ". Skipping to next interface." ); 
                    closeSock(s);
                    LOG( 3, "Server-connection socket closed");
                    continue;
                }
                rc = listen( s, nprocs );
                if ( rc == -1) { 
                    errnoMsg = std::strerror(errno); 
                    LOG( 2, "Server could not make socket to listen, because " << errnoMsg
                            << ". Skipping to next interface." ); 
                    closeSock(s);
                    LOG( 3, "Server-connection socket closed");
                    continue;
                }
                rc = fcntl( s, F_SETFL, O_NONBLOCK);
                if ( rc == -1) { 
                    errnoMsg = std::strerror(errno); 
                    LOG( 2, "Server could not configure socket to be non-blocking, because " << errnoMsg
                            << ". Skipping to next interface." ); 
                    closeSock(s);
                    LOG( 3, "Server-connection socket closed");
                    continue;
                }

                struct pollfd pfd;
                pfd.fd = s;
                pfd.events = POLLIN ;
                pfd.revents = 0;
                serverSockets.push_back( pfd );
            }
            t1 = Time::now();
            if (serverSockets.empty()) { 
                LOG( 2, "Server was not able to open any server sockets during the first "
                        << (t1-t0).toSeconds() << " seconds.... " );
                (void) usleep(1000);
            }
        } while (serverSockets.empty() && t1 - t0 < timeout );

        LOG( 3, "Opened " << serverSockets.size() << " server sockets");

        if (!serverSockets.empty()) 
        {
            int p = 1;
            while ( p < nprocs )
            {
                if (  success[p] ) continue;
                 
                t1 = Time::now();
                int pollTimeout = 
                    static_cast<int>((timeout - (t1 - t0)).toSeconds() * 1000);

                // wait for an incoming connection for at most timeout ms.
                LOG( 3, "BEGIN server polling for at most " << pollTimeout << " milliseconds..." );
                rc = poll( &serverSockets[0], serverSockets.size(), pollTimeout );
                LOG( 3, "END server polling");

                if (rc > 0)
                {
                   for ( unsigned i = 0; i < serverSockets.size(); ++i)
                   {
                       if (serverSockets[i].revents & POLLIN)
                       {
                           int s = serverSockets[i].fd;
                            // accept the connection
                           int fd = accept( s, NULL, 0  );
                         
                           if (fd != -1) 
                           {
                               char buf[ sizeof(magicClient) ]; std::memset(buf, 0, sizeof(buf));
                               rc = write(fd, magicServer, sizeof(magicServer));
                               size_t rc2 = readAll(fd, buf, sizeof(buf));
                               if (rc != -1 && rc2 > 0 )
                               {
                                   if ( std::memcmp(buf, magicClient, sizeof(buf)) == 0)
                                   {
                                       // join the server and the client
                                       connections[p] = fd; 
                                       success[p] = true;
                                       ++p;
                                       LOG( 3, "Server-client connection established");
                                   }
                                   else
                                   {
                                       closeSock(fd);
                                       LOG( 3, "Server-connection socket closed");
                                       errnoMsg = "initial handshake failed due to garbled magic cooky";
                                       LOG( 2, "Server dropped connection with client, because " << errnoMsg);
                                   }
                               }
                               else
                               {
                                   closeSock(fd);
                                   LOG( 3, "Server-connection socket closed");
                                   errnoMsg = std::strerror(errno);
                                   LOG( 2, "Server dropped connection with client, because " << errnoMsg);
                               }
                           }
                           else
                           {
                                errnoMsg = std::strerror(errno);
                                LOG( 2, "Server was unable to accept client connection, because " << errnoMsg);
                           }
                       }
                   }
                }
                else if (rc == 0)
                {
                    errnoMsg = "Server has had no incoming connections within the set timeout";
                    LOG( 2, errnoMsg << " of " << timeout.toSeconds() << " seconds." );
                }
                else 
                {
                    errnoMsg = std::strerror(errno);
                    LOG( 2, "Server experienced an error during polling: " << errnoMsg);
                }
            } // end for each process

            // close server sockets
            for (unsigned i = 0 ; i < serverSockets.size(); ++i)
            {
                closeSock( serverSockets[i].fd );
            }
        }
        else
        {
            ASSERT( serverSockets.empty() );
            errnoMsg = "Server could not open any network ports within timeout";
            LOG( 2, errnoMsg << ". Giving up." ); 
            throw HookException( errnoMsg );
        }
         
        freeaddrinfo( serverAddresses );
                
        if (!std::accumulate( success.begin(), success.end(), true, std::logical_and<bool>()))
        {
            for (unsigned i = 0; i < success.size(); ++i)
            {
                if (success[i])
                {
                    int OK = 0;
                    errno = 0;
                    ssize_t bytes =  write( connections[i], &OK, sizeof(OK));
                    if (bytes < 0 || size_t(bytes) != sizeof(OK) ) {
                        LOG(2, "Unable to inform client of disconnection, because "
                                << std::strerror(errno); );
                    }
                    closeSock( connections[i] );
                }
            }

            std::string error = "Could not establish connection with all processes; ";
            if (errnoMsg)
                error += errnoMsg; 
            throw HookException( error );
        }
        else
        {
            for (unsigned i = 0; i < success.size(); ++i)
            {
                int OK = 1;
                if (write( connections[i], &OK, sizeof(OK)) != sizeof(OK))
                {
                    errnoMsg = std::strerror(errno);
                    LOG(1, "Unable to confirm connection with process " << i << 
                            ", because " << errnoMsg << 
                            ". I have a bad feeling about this. I might deadlock" );
                }
            }
        }

        return connections;
    }

    int clientConnectsToServer( const std::string & serverNode,
            const std::string & serverPort, Time timeout )
    {
        int rc = 0; 
        struct addrinfo addrinfoHints;
        std::memset( &addrinfoHints, 0, sizeof(addrinfoHints) );
        addrinfoHints.ai_family = AF_UNSPEC;
        addrinfoHints.ai_socktype = static_cast<int>(SOCK_STREAM);
        addrinfoHints.ai_protocol = 0;
        addrinfoHints.ai_flags = 0;

        struct addrinfo * serverAddresses = NULL;
        rc = getaddrinfo( serverNode.c_str(), serverPort.c_str(), &addrinfoHints, &serverAddresses);

        if ( 0 != rc )
            throw HookException( gai_strerror(rc) );

        // by the way: ignore SIGPIPE signals from now on, since they might
        // occur when trying to establish connections.
#ifdef LPF_ON_MACOS
        sig_t        oldSigPipeHandler
#else
        sighandler_t oldSigPipeHandler 
#endif
            = signal( SIGPIPE, SIG_IGN );
       
        Time t0 = Time::now();
        int connection = -1;
        errno = 0; const char * errnoMsg = NULL;
        while ( Time::now() - t0 < timeout )
        {
            for ( const struct addrinfo * addr = serverAddresses; 
                    addr != NULL; 
                    addr = addr->ai_next
                ) 
            {
                int s = socket( addr->ai_family, addr->ai_socktype, addr->ai_protocol);
                if ( s == -1) { 
                    errnoMsg = std::strerror(errno);
                    LOG( 2, "Client could not open socket, because " << errnoMsg
                            << ". Skipping to next interface" ); 
                    continue;
                }

                rc = connect( s, addr->ai_addr, addr->ai_addrlen );
                if (rc == -1) 
                { 
                    errnoMsg = std::strerror(errno); 
                    LOG( 2, "Client could not connect to server, because " << errnoMsg);
                    closeSock(s); 
                    LOG( 3, "Client socket closed");
                    continue; 
                }

                char buf[ sizeof(magicServer) ]; std::memset( buf, 0, sizeof(magicServer));
                size_t rc2 = readAll(s, buf, sizeof(magicServer));
                rc = write(s, magicClient, sizeof(magicClient));
                if (rc != -1 && rc2 > 0 )
                {
                    if ( std::memcmp(buf, magicServer, sizeof(magicServer)) == 0)
                    {
                        int OK = 0;
                        rc2 = readAll(s, &OK, sizeof(OK));
                        if ( rc2 > 0 && OK )
                        {
                            connection = s;
                            LOG( 3, "MPI connection on client established");
                            goto loopexit;
                        }
                        else
                        {
                            errnoMsg = "Server abandoned connection set-up";
                            LOG(2, errnoMsg );
                        }
                    }
                    else
                    {
                        errnoMsg = "initial handshake failed because of garbled magic cooky";
                        LOG( 2, "Client dropped connection, because " << errnoMsg);
                    }
                }
                else
                {
                    errnoMsg = std::strerror(errno);
                    LOG( 2, "Client could not send magic cooky to servery, because " << errnoMsg);
                }
                closeSock(s);
                LOG( 3, "Client socket closed");
            }
            LOG( 3, "BEGIN client sleep...");
            (void) usleep(1000);
            LOG( 3, "END client cleep");
        }
loopexit:
         
        freeaddrinfo( serverAddresses );

        // restore SIGPIPE signal handler        
        (void) signal( SIGPIPE, oldSigPipeHandler );
        
        if ( connection == -1 )
        {
            std::string error = "Could not establish connection with all processes; ";
            if (errnoMsg)
                error += errnoMsg; 
            throw HookException( error );
        }

        return connection;
    }

#ifdef USE_SIGALRM
    // crude functionality to prevent deadlocks
    sigjmp_buf timeoutEscape;

    void timeoutPanic(int signal)
    {
        // switch off timer
        (void) alarm(0);
        ASSERT( signal == SIGALRM );
        siglongjmp( timeoutEscape, 1); // jump to a safe place
    }
#endif
}
#endif


//
// \param timeout The connection timeout per process in number of milliseconds 
MPI_Comm dynamicHook( const std::string & serverNode,
        const std::string & serverPort, int pid, int nprocs, Time timeout )
{
    ASSERT( nprocs > 0 );
    ASSERT( pid < nprocs );

#ifndef MPI_HAS_OPEN_PORT
    (void) serverNode;
    (void) serverPort;
    (void) timeout; (void) pid; (void) nprocs;
    throw HookException("MPI does not support MPI_Open_port");
#else

#ifdef USE_SIGALRM
    HookException timeoutException("operation timed out");
    struct sigaction oldact;
    // save old alarm handler
    if (sigaction( SIGALRM, NULL, &oldact)) {
        throw HookException("Cannot get SIGALRM handler");
    }
    // save old alarm timeout
    int oldtimeout = alarm(0);

    if ( 0 != sigsetjmp( timeoutEscape, 1 ) )
    {   
        // restore old alarm handler & timeout
        if (sigaction( SIGALRM, &oldact, NULL)) {
            LOG(1, "Cannot restore old SIGALRM handler");
        }
        (void) alarm( oldtimeout );
        throw timeoutException; 
    }

    // set new alarm handler
    struct sigaction newact;
    std::memset( &newact, 0, sizeof(newact));
    newact.sa_handler = & timeoutPanic;
    newact.sa_flags = 0;
    if (sigaction( SIGALRM, &newact, NULL ))
        throw HookException("Cannot set alarm");

    // enable alarm
    int alarmTimeoutSeconds = static_cast<int>(timeout.toSeconds() + 1.0);
    LOG(3, "Setting alarm time out to" << alarmTimeoutSeconds << " seconds");
    (void) alarm( alarmTimeoutSeconds );
#endif

    if ( pid == 0 && !amIServer( serverNode, serverPort ) )
        throw HookException("PID 0 is not launched on the server node");

    int rc = 0;
    MPI_Comm comm = MPI_COMM_SELF;
    rc = MPI_Comm_dup( MPI_COMM_SELF, &comm);
    ASSERT( MPI_SUCCESS == rc );

    int p0 = 1;
    char portName[ MPI_MAX_PORT_NAME ];
    std::memset( portName, 0, MPI_MAX_PORT_NAME);

    std::vector<int> clients;
    if ( pid == 0 )
    {
        clients = serverAcceptsClients(serverPort, nprocs, timeout);

        ASSERT( clients.size() == unsigned(nprocs) );
        LOG(4, "Opening MPI server port on server using MPI_Open_port" );

        rc = MPI_Open_port( MPI_INFO_NULL, portName );
        ASSERT( MPI_SUCCESS == rc );

        LOG(4, "MPI Port '" << portName << "' has been opened");
    }
    else // or else, we're the client
    {   
        int server = clientConnectsToServer(serverNode, serverPort, timeout);

        LOG(3, "Reading port name" );
        size_t bytes = readAll(server, portName, MPI_MAX_PORT_NAME);
        LOG(3, "Port name is '" << portName << "'" );
        bytes += readAll( server, &p0, sizeof(p0));
        closeSock(server);
        if ( bytes < MPI_MAX_PORT_NAME + sizeof(p0))
        {
            throw HookException("connection to server terminated unexpectedly");
        }

        p0 = p0 + 1;

        LOG(3, "Connecting to MPI server");

        MPI_Comm newcomm = MPI_COMM_NULL;
        rc = MPI_Comm_connect( portName, MPI_INFO_NULL, 0, comm, &newcomm );
        ASSERT( MPI_SUCCESS == rc );
        rc = MPI_Comm_free( & comm );
        ASSERT( MPI_SUCCESS == rc );
        rc = MPI_Intercomm_merge( newcomm, 1, &comm);
        ASSERT( MPI_SUCCESS == rc );
        rc = MPI_Comm_free( & newcomm );
        ASSERT( MPI_SUCCESS == rc );

        LOG(3, "This client was added to the MPI computation");
    }


    for ( int p = p0; p < nprocs; ++p)
    {
#ifndef NDEBUG
        { int np;
          MPI_Comm_size( comm, &np);
          ASSERT( np == p );
        }
#endif

        int success = 1;
        if (pid == 0)
        {
            ssize_t bytes;
            ASSERT( unsigned(p) < clients.size() );
            LOG(3, "Sending port name '" << portName << "' to client " << p  );
            
            errno = 0;
            bytes = write( clients[p], portName, MPI_MAX_PORT_NAME );

            std::string errnoMsg;
            if ( bytes < 0 || size_t(bytes) != MPI_MAX_PORT_NAME ) {
                errnoMsg = std::strerror(errno);
                success = 0;
            }

            errno = 0;
            bytes = write( clients[p], &p, sizeof(p) );
            if ( bytes < 0 || size_t(bytes) != sizeof(p) ) {
                errnoMsg = errnoMsg + ", " + std::strerror(errno);
                success = 0;
            }
            
            errno = 0;
            if (shutdown(clients[p], SHUT_RDWR )) {
                errnoMsg = errnoMsg + ", " + std::strerror(errno);
                success = 0;
            }

            errno = 0;
            if (close(clients[p])) {
                errnoMsg = errnoMsg + ", " + std::strerror(errno);
                success = 0;
            }

            if (!success)
                LOG(1, "Connection to client closed unexpectedly: " << errnoMsg );
        }
        int allSuccess = 0;
        rc = MPI_Allreduce( &success, &allSuccess, 1, MPI_INT, MPI_LAND, comm);
        ASSERT( MPI_SUCCESS == rc );
        if (!allSuccess)
        {
            LOG(1, "Connection to client was lost during setup");
            continue;
        }

        LOG(3, "Starting MPI server");
        MPI_Comm newcomm = MPI_COMM_NULL;
        rc  = MPI_Comm_accept( portName, MPI_INFO_NULL, 0, comm, &newcomm );
        ASSERT( MPI_SUCCESS == rc );
        rc = MPI_Comm_free( & comm );
        ASSERT( MPI_SUCCESS == rc );
        rc = MPI_Intercomm_merge( newcomm, 0, &comm);
        ASSERT( MPI_SUCCESS == rc );
        rc = MPI_Comm_free( & newcomm );
        ASSERT( MPI_SUCCESS == rc );
        LOG(3, "New client was added to MPI computation");
        

#ifndef NDEBUG
        { int np;
          MPI_Comm_size( comm, &np);
          ASSERT( np == p + 1);
        }
#endif

        LOG( (pid == 0?2:3), "Hooked " << (p+1) << " processes together");
    }

    if (pid == 0)
    {
        rc = MPI_Close_port( portName );
        ASSERT( MPI_SUCCESS == rc );
    }

    // Renumber the processes to reflect the choice in parameters
    MPI_Comm result = MPI_COMM_NULL;
    rc = MPI_Comm_split( comm, 0, pid, &result);
    ASSERT( MPI_SUCCESS == rc );
    rc = MPI_Comm_free(&comm);
    ASSERT( MPI_SUCCESS == rc );


#ifdef USE_SIGALRM
    // restore old alarm handler & timeout
    (void) sigaction( SIGALRM, &oldact, NULL);
    (void) alarm( oldtimeout );
#endif

    return result;
#endif
}



} }
