#ifndef __SALOG_CXX_HPP__
#define __SALOG_CXX_HPP__

#include <cstdio>

#ifndef SA_ASSERT
#define SA_ASSERT( param, notice )                                           \
    do{                                                                      \
        if( !(param) ) {                                                     \
            fputs("SA_ASSERT failure : " #param " -> " notice "\n", stderr); \
            fflush(NULL);                                                    \
            abort();                                                         \
        }                                                                    \
    }while(0)
#endif

#define PRINT_ARGS __func__, __FILE__, __LINE__
#define logD(fmt, args...) do{printf("[SA][D] " fmt "  [%s] in [%s:%d]\n", ##args, PRINT_ARGS);fflush(NULL);}while(0)
#define logW(fmt, args...) do{printf("[SA][W] " fmt "  [%s] in [%s:%d]\n", ##args, PRINT_ARGS);fflush(NULL);}while(0)
#define logE(fmt, args...) do{printf("[SA][E] " fmt "  [%s] in [%s:%d]\n", ##args, PRINT_ARGS);fflush(NULL);}while(0)

#endif /* __SALOG_CXX_HPP__ */