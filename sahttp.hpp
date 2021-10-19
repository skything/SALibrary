#ifndef __SAHTTP_CXX_HPP__
#define __SAHTTP_CXX_HPP__

#include "sasocket.hpp"
#include <functional>
#include "common.hpp"
#include <boost/regex.hpp>

namespace libsa {
template<typename T>
class SAHttp {
    struct ErrCatcher {
        enum class Error { kRedirectError, kRedirectSwitch, kBadRequest };
        explicit ErrCatcher(Error err, const std::string& data) noexcept : m_strData(data), m_kErr(err) {}
        ~ErrCatcher() {}
        std::string m_strData;
        Error m_kErr;
    };
    using RecvFunc = std::function<int(std::shared_ptr<T> context, const std::unique_ptr<std::string> &output, bool bMore)>;
    using Progress = std::function<int(uint64_t curr, uint64_t total)>;
    struct Parser {
        public:
            explicit Parser(const std::unique_ptr<std::string>& res) {
                const char *strBuf = res->data();
                char *strBody = nullptr;
                // 检查是否有状态码
                int Code = 0;
                if( 1 == sscanf(strBuf, "HTTP/1.1 %d", &Code) ) {
                    if( Code < 200 || Code > 299 ) {
                        // 重定向
                        char *strNewUrl = nullptr;
                        if( Code == 302 && (strNewUrl = (char*)strstr(strBuf, "Location: ")) ) {
                            strNewUrl += 10;
                            char *strEnd = strchr(strNewUrl, '\n');
                            // 重定向的新连接
                            if( strEnd ) {
                                // std::cout << "Recv : " << strBuf << std::endl;
                                std::size_t nL = (size_t)(strEnd - strNewUrl);
                                throw std::unique_ptr<ErrCatcher>(new ErrCatcher(ErrCatcher::Error::kRedirectSwitch, std::string(strNewUrl, nL)));
                            }
                            else {
                                libsa::SALog::logW("redirect failure do not had new url", __func__, __LINE__);
                                throw std::unique_ptr<ErrCatcher>(new ErrCatcher(ErrCatcher::Error::kRedirectError, "is null"));
                            }
                        }
                        else if( Code == 400 )
                            throw std::unique_ptr<ErrCatcher>(new ErrCatcher(ErrCatcher::Error::kBadRequest, "400 Bad Request"));
                    }

                    strBody = (char*)strstr(strBuf, "\r\n\r\n");
                    // 排除响应头数据
                    if( strBody ) {
                        while( *strBody == '\r' || *strBody == '\n' )
                            ++strBody;
                    }
                }

                std::size_t ret = res->capacity();
                if( strBody ) {
                    std::size_t  nDiff = (strBody - strBuf + 1);
                    strBuf += nDiff;
                    ret -= nDiff;
                    m_pResult = libsa::make_unique<std::string>(strBuf, ret);
                }
                else
                    m_pResult = libsa::make_unique<std::string>(strBuf, ret);
            }
            ~Parser() {}
            std::unique_ptr<std::string>& getResult() { return m_pResult; }
        private:
            Parser(const Parser&) = delete;
            Parser(Parser&&) = delete;
            Parser& operator=(const Parser&) = delete;
            Parser& operator=(Parser&&) = delete;
        private:
            std::unique_ptr<std::string> m_pResult;
    };
    public:
        explicit SAHttp( const std::string &url ) {
            boost::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
            boost::cmatch what;
            if( !boost::regex_match(url.c_str(), what, ex) )
                throw "Not except";

            m_strProtocol = libsa::make_unique<std::string>(what[1].first, what[1].second);
            m_strDomain   = libsa::make_unique<std::string>(what[2].first, what[2].second);
            m_strPort     = libsa::make_unique<std::string>(what[3].first, what[3].second);
            m_strPath     = libsa::make_unique<std::string>(what[4].first, what[4].second);
            m_strQuery    = libsa::make_unique<std::string>(what[5].first, what[5].second);
            m_pSocket     = libsa::make_unique<SASocket>(false);
            m_pSocket->Open(*(m_strDomain.get()), 80);
        }
        ~SAHttp() { }
        bool setContext(std::shared_ptr<T> context) { m_pCtx = context; return true; }
        bool Get() {
            auto l_strPath = std::make_shared<std::string>();
            l_strPath->reserve(10240);
            if( m_strPath && m_strPath->empty() )
                l_strPath->append(m_strPath->c_str());
            
            if( m_strQuery && m_strQuery )
                l_strPath->append(m_strQuery->c_str());
        
            if( l_strPath->empty() )
                return false;
            return Get(*(l_strPath.get()));
        }

        bool Get(Progress progress, RecvFunc recv) {
            auto l_strPath = std::make_shared<std::string>();
            l_strPath->reserve(10240);
            if( m_strPath && m_strPath->empty() )
                l_strPath->append(m_strPath->c_str());

            if( m_strQuery && m_strQuery )
                l_strPath->append(m_strQuery->c_str());

            if( l_strPath->empty() )
                return false;
            return Get(*(l_strPath.get()), progress, recv);
        }

        bool Get(const std::string &l_strPath) {
            return Get(l_strPath, [](uint64_t curr, uint64_t total) -> int {
                std::cout << "curr : " << curr << "total : " << total << std::endl;
                return 0;
            }, 
            [](std::shared_ptr<T> context, const std::unique_ptr<std::string> &output, bool bMore)-> int {
                std::cout << "recv:" << output->c_str() << std::endl;
                return 0;
            });
        }

        bool Get(const std::string &l_strPath, Progress progress, RecvFunc recv) {
            std::cout << l_strPath << std::endl;
            SendRequest(*(m_strDomain.get()), l_strPath);
            return ProcessRecv(progress, recv);
        }

        bool Post(Progress progress, RecvFunc recv) {
            if( !m_strPath )
                return false;

            return Post(*(m_strPath.get()), *(m_strQuery.get()), progress, recv);
        }
        bool Post() {
            if( !m_strPath )
                return false;
            return Post(*(m_strPath.get()), *(m_strQuery.get()));
        }

        bool Post(const std::string &path, const std::string &postData ) {
            return Post(path, postData, [](uint64_t curr, uint64_t total) -> int {
                std::cout << "curr : " << curr << "total : " << total << std::endl;
                return 0;
            }, 
            [](std::shared_ptr<T> context, const std::unique_ptr<std::string> &output, bool bMore)-> int {
                std::cout << "recv:" << output->c_str() << std::endl;
                return 0;
            });
        }

        bool Post(const std::string &path, const std::string &postData, Progress progress, RecvFunc recv ) {
            SendRequest(*(m_strDomain.get()), path, postData);
            return ProcessRecv(progress, recv);
        }

    protected:
        void SendRequest( const std::string &strHost, const std::string &strParams ) {
            std::stringstream Header("");
            Header << "GET " << strParams << " HTTP/1.1\r\n";
            Header << "User-Agent: libsa\r\n";
            Header << "Accept: */*\r\n";
            Header << "Connection: Close\r\n";
            Header << "Host: " << strHost << "\r\n\r\n";
            m_pSocket->Send(Header.str());
        }
        
        void SendRequest( const std::string &strHost, const std::string &strParams, const std::string &strPostData ) {
            std::stringstream Header("");
            Header << "POST " << strParams << " HTTP/1.1\r\n";
            Header << "User-Agent: libsa\r\n";
            Header << "Accept: */*\r\n";
            Header << "Connection: Close\r\n";
            Header << "Host: " << strHost << "\r\n";
            Header << "Content-Length: " << strPostData.capacity() << "\r\n";
            Header << "Content-Type: application/x-www-form-urlencoded;charset=UTF-8\r\n\r\n";
            Header << strPostData << "\r\n";
            m_pSocket->Send(Header.str());
        }

        bool ProcessRecv(Progress progress, RecvFunc recv) {
            while( 1 ) {
                auto RecvBuf = m_pSocket->Recv(4096);
                if( !RecvBuf ) {
                    recv(m_pCtx, nullptr, false);
                    std::cout << "Recv is over" << std::endl;
                    break;
                }

                try {
                    std::unique_ptr<Parser> Res = libsa::make_unique<Parser>(RecvBuf);
                    auto Rev = std::move(Res->getResult());
                    // std::cout <<"Recv : " <<*(Rev.get()) << std::endl;
                    recv(m_pCtx, Rev, true);
                }
                catch(const std::unique_ptr<ErrCatcher> &pCatch) {
                    switch(pCatch->m_kErr) {
                        case ErrCatcher::Error::kRedirectSwitch: {
                            auto pH = std::make_shared<libsa::SAHttp<T>>(std::move(pCatch->m_strData));
                            pH->setContext(m_pCtx);
                            return pH->Get(progress, recv);
                            // boost::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
                            // boost::cmatch what;
                            // if( !boost::regex_match(pCatch->m_strData.c_str(), what, ex) ) {
                            //     std::cout << "Not match this url" << std::endl;
                            //     throw "Not except";
                            // }

                            // auto oldDomain = std::move(std::unique_ptr<std::string>(m_strDomain.release()));
                            // m_strDomain    = libsa::make_unique<std::string>(what[2].first, what[2].second);

                            // auto oldPath = std::move(std::unique_ptr<std::string>(m_strPath.release()));
                            // m_strPath    = libsa::make_unique<std::string>(what[4].first, what[4].second);
                            // //
                            // auto pOld = std::unique_ptr<SASocket>(m_pSocket.release());
                            // m_pSocket = std::move(libsa::make_unique<SASocket>(false));
                            // m_pSocket->Open(*(m_strDomain.get()), 80);
                            // Get(*(m_strPath.get()), progress, recv);

                            // // std::string pDomain(what[2].first, what[2].second);
                            // // std::string pPath(what[4].first, what[4].second);
                            // // SendRequest(pDomain.c_str(), pPath.c_str());
                            // break;
                        }
                        case ErrCatcher::Error::kRedirectError: {
                            std::terminate();
                            break;
                        }
                        case ErrCatcher::Error::kBadRequest: {
                            libsa::SALog::logW(pCatch->m_strData.c_str(), __func__, __LINE__);
                            break;
                        }
                    }
                }
                catch(...) {
                    std::cout << "not found this error" << std::endl;
                }
            }
            return true;
        }
    private:
        std::shared_ptr<T> m_pCtx;
        std::unique_ptr<SASocket> m_pSocket;
        std::unique_ptr<std::string> m_strProtocol;
        std::unique_ptr<std::string> m_strDomain;
        std::unique_ptr<std::string> m_strPort;
        std::unique_ptr<std::string> m_strPath;
        std::unique_ptr<std::string> m_strQuery;
};

};


#endif /* __SAHTTP_CXX_HPP__ */