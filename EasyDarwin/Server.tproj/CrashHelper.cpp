#include "CrashHelper.h"
#include "../../LogModule/Logger.h"
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <signal.h>

#ifndef _WIN32

#include <execinfo.h>

//signal 函数用法参考http://www.kernel.org/doc/man-pages/online/pages/man2/signal.2.html
//backtrace ，backtrace_symbols函数用法参考 http://www.kernel.org/doc/man-pages/online/pages/man3/backtrace.3.html

#define STACKCALL __attribute__((regparm(1),noinline))
void ** STACKCALL getEBP(void){
        void **ebp=NULL;
        __asm__ __volatile__("mov %%rbp, %0;\n\t"
                    :"=m"(ebp)      /* 输出 */
                    :      /* 输入 */
                    :"memory");     /* 不受影响的寄存器 */
        return (void **)(*ebp);
}
int my_backtrace(void **buffer,int size){

    int frame=0;
    void ** ebp;
    void **ret=NULL;
    unsigned long long func_frame_distance=0;
    if(buffer!=NULL && size >0)
    {
        ebp=getEBP();
        func_frame_distance=(unsigned long long)(*ebp) - (unsigned long long)ebp;
        while(ebp&& frame<size
            &&(func_frame_distance< (1ULL<<24))//assume function ebp more than 16M
            &&(func_frame_distance>0))
        {
            ret=ebp+1;
            buffer[frame++]=*ret;
            ebp=(void**)(*ebp);
            func_frame_distance=(unsigned long long)(*ebp) - (unsigned long long)ebp;
        }
    }
    return frame;
}

static void WidebrightSegvHandler(int signum) {
    void *array[10];
    size_t size;
    char **strings;
    size_t i, j;

    signal(signum, SIG_DFL);

    size = backtrace (array, 10);
    //size = my_backtrace(array,10);
    strings = (char **)backtrace_symbols (array, size);

    log_error( "widebright received SIGSEGV! Stack trace:\n");
    for (i = 0; i < size; i++) {
        log_error("%d %s \n",i,strings[i]);
    }

    free (strings);
    exit(1);
}
#endif

CrashHelper::CrashHelper()
{
#ifndef _WIN32
    signal(SIGSEGV, WidebrightSegvHandler); // SIGSEGV      11       Core    Invalid memory reference
    signal(SIGABRT, WidebrightSegvHandler); // SIGABRT       6       Core    Abort signal from
#endif
}

