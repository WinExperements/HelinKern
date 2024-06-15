// Pthreads for HelinOS

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>

__BEGIN_DECLS

extern "C" {
	int pthread_equal(pthread_t t1, pthread_t t2);
	int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg);
	void pthread_exit(void* retval);
	int pthread_join(pthread_t thread, void** retval);
}
__END_DECLS

#endif
