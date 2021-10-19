#ifndef __SASOCKET_CXX_HPP__
#define __SASOCKET_CXX_HPP__

#include <string>
#include <cstring>
#include "common.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include "salog.hpp"

namespace libsa {
class SASocket {
    enum class Error { kDnsSolveFailure, kSocketCreateFailure, kConnectFailure, kBindFailure, kListenFailure };
    public:
        explicit SASocket( bool bServer ) noexcept : m_iSocket(-1), m_bServer(bServer) { }
        SASocket(SASocket &&socket) noexcept = default;
        SASocket& operator=(SASocket && socket) noexcept = default;
        ~SASocket() {
            Close();
        }
        
        bool Open( const std::string &strHost, int port ) {
            struct addrinfo hints, *pAddrList = nullptr;
            char strPort[16] = { 0 };
            std::memset( &hints, 0, sizeof( hints ) );
            hints.ai_family   = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            try {
                // 处理dns
                snprintf(strPort, sizeof(strPort), "%d", port);
                if ( 0 != getaddrinfo( strHost.c_str(), strPort, &hints, &pAddrList ) ) {
                    std::printf("error : %s", strerror(errno));
                    throw Error::kDnsSolveFailure;
                }
                // 创建socket
                m_iSocket = socket(pAddrList->ai_family, pAddrList->ai_socktype, pAddrList->ai_protocol);
                if( m_iSocket < 0 ) {
                    std::printf("error : %s", strerror(errno));
                    throw Error::kSocketCreateFailure;
                }

                if( m_bServer ) {
                    if( 0 != bind( m_iSocket, pAddrList->ai_addr, (int)pAddrList->ai_addrlen) ) {
                        std::printf("error : %s", strerror(errno));
                        throw Error::kBindFailure;
                    }

                    if( 0 != listen( m_iSocket, 32) ) {
                        std::printf("error : %s", strerror(errno));
                        throw Error::kListenFailure;
                    }
                }
                else {
                    // 尝试建立链接
                    if ( 0 != connect( m_iSocket, pAddrList->ai_addr, (int)pAddrList->ai_addrlen ) ) {
                        std::printf("error : %s", strerror(errno));
                        throw Error::kConnectFailure;
                    }
                }
            }
            catch(Error error) {
                switch(error) {
                    case Error::kDnsSolveFailure: 
                        SALog::logW("dns resolve failure", __func__, __LINE__);
                    break;
                    case Error::kSocketCreateFailure: 
                        SALog::logW("socket create failure", __func__, __LINE__);
                    break;
                    case Error::kConnectFailure: 
                        SALog::logW("connect failure", __func__, __LINE__);
                    break;
                    case Error::kBindFailure: 
                        SALog::logW("bind server failure", __func__, __LINE__);
                    break;
                    case Error::kListenFailure:
                        SALog::logW("listen server failure", __func__, __LINE__);          
                    break;
                }
                return false;
            }
            catch(...) {
                SALog::logW("Not catch", __func__, __LINE__);
                std::terminate();
            }

            if( nullptr != pAddrList )
                freeaddrinfo( pAddrList );
            return true;
        }

        bool Close() {
            if( m_iSocket >= 0 )
                close(m_iSocket);
            m_iSocket = -1;
            return true;
        }

        bool Send(const std::string &data ) {
            struct timeval tv;
            tv.tv_sec  = 3;
            tv.tv_usec = 0;
            setsockopt(m_iSocket, SOL_SOCKET, SO_SNDTIMEO, (struct timeval*)&tv, sizeof(tv));
            std::cout << "Data: " <<data << std::endl;
            send(m_iSocket, data.c_str(), data.size(), MSG_WAITALL);
            return true;
        }

        std::unique_ptr<std::string> Recv(size_t nLen) {
            struct timeval tv;
            tv.tv_sec  = 3;
            tv.tv_usec = 0;
            setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&tv, sizeof(tv));

            std::shared_ptr<char> l_strRecv(new char[nLen + 1], std::default_delete<char[]>());
            ssize_t ret = recv(m_iSocket, l_strRecv.get(), nLen, MSG_WAITALL);
            if( !ret ) {
                SALog::logD("recv over", __func__, __LINE__);
                return nullptr;
            }
            else if( ret < 0 ) {
                SALog::logD(strerror(errno), __func__, __LINE__);
                ret = 0;
            }
            return libsa::make_unique<std::string>(l_strRecv.get(), ret);
        }
    private:
        SASocket(const SASocket & ) = delete;
        SASocket& operator=(const SASocket &) = delete;
    private:
        int m_iSocket;
        bool m_bServer;
};

};


#endif /* __SASOCKET_CXX_HPP__ */