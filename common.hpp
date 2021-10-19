#ifndef __COMMON_CXXXXX_H__
#define __COMMON_CXXXXX_H__

#include <memory>
#include <type_traits>

namespace libsa {

#if __cplusplus > 201103L && __cplusplus < 201703L
template<typename T, typename... Args>
inline std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#elif __cplusplus >= 201703L
using std::make_unique;
#endif

};

#endif /* __COMMON_CXXXXX_H__ */