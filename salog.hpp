#ifndef __SALOG_CXX_HPP__
#define __SALOG_CXX_HPP__

#include <iostream>
#include <string>

namespace libsa {
class SALog {
    public:
        SALog()  = delete;
        ~SALog() = delete;

        static inline void logD(const char *strInfo, const char* strFunc = __func__, int line = __LINE__) {
            std::cout << "[SA][D] " << strInfo << " in [" << strFunc << "][" << line <<"]" << std::endl;
        }

        static inline void logW(const char *strInfo, const char* strFunc = __func__, int line = __LINE__) {
            std::cout << "[SA][W] " << strInfo << " in [" << strFunc << "][" << line <<"]" << std::endl;
        }

        static inline void logE(const char *strInfo, const char* strFunc = __func__, int line = __LINE__) {
            std::cout << "[SA][E] " << strInfo << " in [" << strFunc << "][" << line <<"]" << std::endl;
        }
};
};


#endif /* __SALOG_CXX_HPP__ */