#ifndef __SAHTTP_CXX_HPP__
#define __SAHTTP_CXX_HPP__

#include "sasocket.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <future>
#include "common.hpp"
#include <boost/regex.hpp>
#include <unordered_map>

#define GET_FORMAT "GET %s HTTP/1.1\r\nUser-Agent: libsa\r\nAccept: */*\r\nConnection: Close\r\nHost: %s\r\n\r\n"
#define POST_FORMAT "POST %s HTTP/1.1\r\nUser-Agent: libsa\r\nAccept: */*\r\nConnection: Close\r\nHost: %s\r\nContent-Length: %lu\r\nContent-Type: application/x-www-form-urlencoded;charset=UTF-8\r\n\r\n%s\r\n"
#define POST_EMPTY "POST %s HTTP/1.1\r\nUser-Agent: libsa\r\nAccept: */*\r\nConnection: Close\r\nHost: %s\r\nContent-Length: 0\r\nContent-Type: application/x-www-form-urlencoded;charset=UTF-8\r\n\r\n"
#define GET_TEST "GET %s HTTP/1.1\r\nUser-Agent: libsa\r\nAccept: */*\r\nConnection: Close\r\nHost: %s\r\nRange: bytes=%lu-%lu\r\n\r\n"

namespace libsa {

    using Progress = std::function<bool(uint64_t curr, uint64_t total)>;
    using RecvProc = std::function<bool(const char* strData, size_t nLen, bool bMore)>;

    inline constexpr auto status_message(int status) {
        switch (status) {
            case 100: return "Continue";
            case 101: return "Switching Protocol";
            case 102: return "Processing";
            case 103: return "Early Hints";
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 203: return "Non-Authoritative Information";
            case 204: return "No Content";
            case 205: return "Reset Content";
            case 206: return "Partial Content";
            case 207: return "Multi-Status";
            case 208: return "Already Reported";
            case 226: return "IM Used";
            case 300: return "Multiple Choice";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 305: return "Use Proxy";
            case 306: return "unused";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 402: return "Payment Required";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 406: return "Not Acceptable";
            case 407: return "Proxy Authentication Required";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 410: return "Gone";
            case 411: return "Length Required";
            case 412: return "Precondition Failed";
            case 413: return "Payload Too Large";
            case 414: return "URI Too Long";
            case 415: return "Unsupported Media Type";
            case 416: return "Range Not Satisfiable";
            case 417: return "Expectation Failed";
            case 418: return "I'm a teapot";
            case 421: return "Misdirected Request";
            case 422: return "Unprocessable Entity";
            case 423: return "Locked";
            case 424: return "Failed Dependency";
            case 425: return "Too Early";
            case 426: return "Upgrade Required";
            case 428: return "Precondition Required";
            case 429: return "Too Many Requests";
            case 431: return "Request Header Fields Too Large";
            case 451: return "Unavailable For Legal Reasons";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            case 505: return "HTTP Version Not Supported";
            case 506: return "Variant Also Negotiates";
            case 507: return "Insufficient Storage";
            case 508: return "Loop Detected";
            case 510: return "Not Extended";
            case 511: return "Network Authentication Required";
        default:
            case 500: return "Internal Server Error";
        }
    }

template<typename T>
struct List {
    struct Node {
        std::unique_ptr<Node> m_pNext;
        std::shared_ptr<T> m_pData;
    };
    public:
        explicit List(): m_pHead(std::unique_ptr<Node>()) {}
        ~List() {}
        void push(const T data) {
            std::unique_ptr<Node> New(new Node());
            New->m_pNext = nullptr;
            New->m_pData = std::make_shared<T>(std::move(data));
            std::lock_guard<std::mutex> guard(m_pMtx);
            m_pHead->m_pNext = std::move(New);
        }

        std::shared_ptr<T> pop() {
            std::lock_guard<std::mutex> guard(m_pMtx);
            if( !m_pHead->m_pNext )
                return std::shared_ptr<T>();
            
            std::unique_ptr<Node> oldHead = std::move(m_pHead->m_pNext);
            std::shared_ptr<T> res = oldHead->m_pData;
            m_pHead->m_pNext = std::move(oldHead->m_pNext);
            return res;
        }
    private:
        std::mutex m_pMtx;
        std::unique_ptr<Node> m_pHead;
};

struct UrlParser {
    explicit UrlParser(const std::string &strHost) {
        boost::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^*]*)");
        boost::cmatch what;
        if( !boost::regex_match(strHost.c_str(), what, ex) ) {
            logW("error %s", strHost.c_str());
            throw "Not except";
        }
        
        m_strScheme = std::string(what[1].first, what[1].second);
        m_strHost   = std::string(what[2].first, what[2].second);
        m_strPort   = std::string(what[3].first, what[3].second);
        m_strPath   = std::string(what[4].first, what[4].second);
    }
    ~UrlParser() {}

    std::string m_strHost;
    std::string m_strPort;
    std::string m_strScheme;
    std::string m_strPath;
};

struct Header {
    enum class Method { kGetMethod, kPostMethod, kInvaildMethod };
    enum class Error { kInVaildMethod, kSetPostDataInGetMethod, kNotFoundHost, kUndefineError };
    using TagValue = std::pair<std::string, std::string>;
    using MapValue = std::unordered_map<std::string, TagValue>;
    public:
        explicit Header() : m_pContainer(new MapValue()), m_eMethod(Method::kInvaildMethod) {}
        ~Header() {}
        void setPath(const std::string &strPath) {
            std::string strKey("path", 4);
            m_pContainer->emplace(std::pair<std::string, TagValue>(strKey, TagValue(strKey, strPath)));
        }

        void setHost(const std::string &strHost) {
            std::string strKey("host", 4);
            m_pContainer->emplace(std::pair<std::string, TagValue>(strKey, TagValue(strKey, strHost)));
        }

        void setMethod(Method method) {
            using MethodVal = std::pair<Method, std::string>;
            using MethodMap = std::unordered_map<Method, MethodVal>;
            static thread_local const MethodMap l_kVal = { 
                { Method::kGetMethod, MethodVal(Method::kGetMethod, std::string("GET")) },
                { Method::kPostMethod, MethodVal(Method::kPostMethod, std::string("POST")) }
            };

            std::string strKey("method", 4);
            const auto l_res = l_kVal.find(method);
            if( l_res == l_kVal.end())
                throw Error::kInVaildMethod;
            m_eMethod = method;
            m_pContainer->emplace(std::pair<std::string, TagValue>(strKey, TagValue(strKey, l_res->second.second)));
        }

        void setRange(std::size_t begin, std::size_t end) {
            if( begin == end ) {
                logW("Range begin : %lu is euqal to end : %lu", begin, end);
                return;
            }
            
            std::string strKey("range", 5);
            std::shared_ptr<char> strTmp(new char[64]{0});
            int ret = snprintf(strTmp.get(), 64, "Range: bytes=%lu-%lu", begin, end);
            if( ret > 0 )
                m_pContainer->emplace(std::pair<std::string, TagValue>(strKey, TagValue(strKey, std::string(strTmp.get(), ret))));
        }

        void setPostData(const char* strPostData, std::size_t nLen) {
            if(m_eMethod != Method::kPostMethod )
                throw Error::kSetPostDataInGetMethod;

            if( !strPostData || !nLen ) {
                logW("strPostData:%p, nLen:%lu", strPostData, nLen);
                return;
            }
            std::string strKey("postData", 8);
            std::string strPost(strPostData, nLen);
            m_pContainer->emplace(std::pair<std::string, TagValue>(strKey, TagValue(strKey, std::move(strPost))));
        }

        void setPostData(const std::string& strPostData) {
            if(m_eMethod != Method::kPostMethod )
                throw Error::kSetPostDataInGetMethod;

            if( strPostData.empty() ) {
                logW("strPostData is empty");
                return;
            }
            std::string strKey("postData", 8);
            m_pContainer->emplace(std::pair<std::string, TagValue>(strKey, TagValue(strKey, strPostData)));
        }

        std::string generate() const {
            if( m_pContainer->empty() )
                return std::string();

            std::stringstream Header("");
            if( m_eMethod == Method::kGetMethod )
                Header << "GET ";
            else if(m_eMethod == Method::kPostMethod)
                Header << "POST ";
            else
                throw Error::kInVaildMethod;
            //
            auto l_vHost = m_pContainer->find(std::string("host"));
            if(l_vHost == m_pContainer->end())
                throw Error::kNotFoundHost;

            auto l_vPath = m_pContainer->find(std::string("path"));
            if(l_vPath != m_pContainer->end())
                Header << l_vPath->second.second;
            else
                Header << "/";

            Header << " HTTP/1.1\r\n";
            Header << "User-Agent: libsa\r\n";
            Header << "Accept: */*\r\n";
            Header << "Connection: Close\r\n";
            Header << "Host: " << l_vHost->second.second;
            // 
            auto l_vRange = m_pContainer->find(std::string("range"));
            if(l_vRange != m_pContainer->end()) {
                Header << "\r\n";
                Header << l_vRange->second.second;
            }
            auto l_vPostData = m_pContainer->find(std::string("postData"));
            if( l_vPostData != m_pContainer->end() ) {
                std::string strPostData = l_vPostData->second.second;
                Header << "\r\nContent-Length: ";
                Header << strPostData.capacity();
                Header << "\r\nContent-Type: application/x-www-form-urlencoded;charset=UTF-8\r\n\r\n";
                Header << strPostData;
                Header << "\r\n";
            }
            else
                Header << "\r\n\r\n";
            return Header.str();
        }
    private:
        std::unique_ptr<MapValue> m_pContainer;
        Method m_eMethod;
};

struct Result {
    std::string m_strVersion;
    int m_iStatus = -1;
    std::string m_strHeaders;
    std::string m_strLocation; // Redirect location
    size_t m_nContentTotal = 0;
    size_t m_nContentCurr = 0;
    bool m_bChunked = false;
};

class SAHttp {
    public:
        explicit SAHttp(const std::string &strUrl) : m_pSocket(false) {
            UrlParser parse(strUrl);
            int port = 80;
            if( !parse.m_strPort.empty() )
                port = std::stoi(parse.m_strPort);
            m_pSocket.Open(parse.m_strHost, port);
            m_pHeader.setHost(parse.m_strHost);
        }

        explicit SAHttp(const std::string &strHost, int port) : m_pSocket(false) {
            m_pSocket.Open(strHost, port);
            m_pHeader.setHost(strHost);
        }

        ~SAHttp() {}

        Result Get(const std::string &strPath) {
            return Get(strPath, 0, 0, [](uint64_t curr, uint64_t total) -> bool {
                logW("curr:%lu,total:%lu", curr, total);
                return true;
            }, [](const char *strData, size_t nLen, bool bMore) -> bool {
                logW("recv data: %s,len:%lu", strData, nLen);
                return bMore;
            });
        }

        Result Get(const std::string &strPath, Progress progress, RecvProc recvFunc) {
            return Get(strPath, 0, 0, progress, recvFunc);
        }

        Result Get(const std::string &strPath, std::size_t rangeBegin, std::size_t rangeEnd, Progress progress, RecvProc recvFunc) {
            m_pHeader.setMethod(Header::Method::kGetMethod);
            m_pHeader.setPath(strPath);
            m_pHeader.setRange(rangeBegin, rangeEnd);
            if( !m_pSocket.Send(std::move(m_pHeader.generate())) )
                return Result();
            Result res;
            parseResponse(res, progress, recvFunc);
            return res;
        }

        Result Post(const std::string &strPath) {
            return Post(strPath, nullptr, 0, [](uint64_t curr, uint64_t total) -> bool {
                logW("curr:%lu,total:%lu", curr, total);
                return true;
            }, [](const char *strData, size_t nLen, bool bMore) -> bool {
                logW("recv data: %s,len:%lu", strData, nLen);
                return bMore;
            });
        }

        Result Post(const std::string &strPath, Progress progress, RecvProc recvFunc) {
            return Post(strPath, nullptr, 0, progress, recvFunc);
        }

        Result Post(const std::string &strPath, const std::string &strPostData) {
           return Post(strPath, strPostData, [](uint64_t curr, uint64_t total) -> bool {
                logW("curr:%lu,total:%lu", curr, total);
                return true;
            }, [](const char *strData, size_t nLen, bool bMore) -> bool {
                logW("recv data: %s,len:%lu", strData, nLen);
                return bMore;
            });
        }

        Result Post(const std::string &strPath, const char* strPostData, std::size_t nPostData) {
            return Post(strPath, nullptr, 0, [](uint64_t curr, uint64_t total) -> bool {
                logW("curr:%lu,total:%lu", curr, total);
                return true;
            }, [](const char *strData, size_t nLen, bool bMore) -> bool {
                logW("recv data: %s,len:%lu", strData, nLen);
                return bMore;
            });
        }

        Result Post(const std::string &strPath, const char* strPostData, std::size_t nPostData, Progress progress, RecvProc recvFunc) {
            m_pHeader.setMethod(Header::Method::kPostMethod);
            m_pHeader.setPath(strPath);
            m_pHeader.setPostData(strPostData, nPostData);
            if( !m_pSocket.Send(std::move(m_pHeader.generate())) ) 
                return Result();
            
            Result res;
            parseResponse(res, progress, recvFunc);
            return res;
        }

        Result Post(const std::string &strPath, const std::string &strPostData, Progress progress, RecvProc recvFunc) {
            m_pHeader.setMethod(Header::Method::kPostMethod);
            m_pHeader.setPath(strPath);
            m_pHeader.setPostData(strPostData);
            if( !m_pSocket.Send(std::move(m_pHeader.generate())) ) 
                return Result();
            
            Result res;
            parseResponse(res, progress, recvFunc);
            return res;
        }
    protected:
        void parseResponse(Result &res, Progress progress, RecvProc recvFunc) {
            std::async([&]() mutable -> bool {
                while( 1 ) {
                    auto pBuffer = m_pSocket.Recv(4096);
                    if( !pBuffer ) {
                        recvFunc(nullptr, 0, false);
                        break;
                    }
                    // 1. 检查是不是接收头
                    const char* strWorkBuf = pBuffer->c_str();
                    if( 1 == sscanf(strWorkBuf, "HTTP/1.1 %d", &res.m_iStatus) ) {
                        if( res.m_iStatus >= 200 && res.m_iStatus < 300 ) {
                            // 文件长度
                            char *strTmp = strstr(strWorkBuf, "Content-Length: ");
                            if( strTmp )
                                sscanf(strTmp, "Content-Length: %lu\n", &res.m_nContentTotal);
                            // 判断chunked模式
                            else if( strstr(strWorkBuf, "Transfer-Encoding: chunked") ) 
                                res.m_bChunked = true;
                            else
                                logW("undefined error");
                            // 当前Header加body的总长度
                            std::size_t nTotal = pBuffer->capacity();
                            // 寻找body的位置
                            char* strBody = strstr(strWorkBuf, "\r\n\r\n");
                            if( !strBody ) {
                                logW("recv buffer is too small, do not found body");
                                break;
                            }
                            strBody += 4;
                            std::size_t nDiff = (strBody - strWorkBuf);
                            res.m_strHeaders.clear();
                            res.m_strHeaders.append(strWorkBuf, nDiff);
                            // 
                            std::size_t nBody = nTotal - nDiff;
                            res.m_nContentCurr += nBody;
                            // 推送Body
                            if( !res.m_bChunked )
                                progress(res.m_nContentCurr, res.m_nContentTotal);
                            recvFunc(strBody, nBody, true);
                        }
                        else if( res.m_iStatus >= 300 && res.m_iStatus < 400 ) {
                            logW("URL REDIRECT into");
                            res.m_strHeaders.clear();
                            res.m_strHeaders.append(strWorkBuf, pBuffer->length());
                            char* strNewurl = strstr(strWorkBuf, "Location: ");
                            if( strNewurl ) {
                                strNewurl += 10;
                                char *strEnd = strchr(strNewurl, '\n');
                                res.m_strLocation.clear();
                                res.m_strLocation.append(strNewurl, (std::size_t)(strEnd - strNewurl));
                            }
                            break;
                        }
                        else {
                            logW("error : %s", status_message(res.m_iStatus));
                            break;
                        }
                    }
                    // body不带Header
                    else {
                        std::size_t nBody = pBuffer->capacity();
                        res.m_nContentCurr += nBody;
                        // 推送Body
                        if( !res.m_bChunked )
                            progress(res.m_nContentCurr, res.m_nContentTotal);
                        recvFunc(strWorkBuf, nBody, true);
                    }
                }
                return true;
            }).get();
        }
    
    private:
        SASocket m_pSocket;
        Header m_pHeader;
};

};

#endif /* __SAHTTP_CXX_HPP__ */