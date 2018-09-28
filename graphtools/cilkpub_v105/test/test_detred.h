#ifndef __TEST_DETRED_H_
#define __TEST_DETRED_H_

#include <cilkpub/detred_opadd.h>

using namespace cilkpub;

template <typename T>
inline void tfprint(FILE* f, const DetRedIview<T>& iview)
{
    __test_DetRedIview::tfprint_iview(f, iview);
}


#endif // __TEST_DETRED_H_
