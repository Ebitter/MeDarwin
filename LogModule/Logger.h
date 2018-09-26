#ifndef CWLOGGER_H
#define CWLOGGER_H
#include <stdarg.h>

#define VER_STR "1.0.39.180912"

#ifdef WIN32
#define LOG_API _declspec(dllexport)
#else
#define LOG_API

#endif

/*
#logger config
#loggger level "trace < debug < information < notice < warnning < error < fatal"


const char* config =
                "logging.loggers.root.channel = c3\n"
                "logging.loggers.root.level = information\n"
                "logging.channels.c1.class = ColorConsoleChannel\n"
                "logging.channels.c1.formatter = f1\n"
                "logging.channels.c2.class = FileChannel\n"
                "logging.channels.c2.path = ./log/run.log\n"
                "logging.channels.c2.formatter = f1\n"
                "logging.channels.c2.rotation=1 days\n"
                "logging.channels.c2.archive=number\n"
                "logging.channels.c2.purgeCount=90\n"
                "logging.channels.c2.compress=true\n"
                "logging.channels.c2.flush=false\n"
                "logging.channels.c3.class = SplitterChannel\n"
                "logging.channels.c3.channel1=c1\n"
                "logging.channels.c3.channel2=c2\n"
                "logging.formatters.f1.class = PatternFormatter\n"
                "logging.formatters.f1.pattern = [%p] %L%Y-%n-%d %H:%M:%S %t  %U %u\n"
                "logging.formatters.f1.times = UTC\n";
*/
#define MAX_BUF_SIZE 8192
enum LOG_ERR_CODE
{
    LOG_ERR_OK=0,
    LOG_ERR_PARAM, //ÅäÖÃ²ÎÊý´íÎó
    LOG_ERR_BUFF_NULL, //´íÎóbufferÎªNULL
    LOG_ERR_BUFF_TOO_SMALL, //´íÎóbufferÌ«Ð¡.
    LOG_ERR_UNKOWN_ERR, //Î´Öª´íÎó.
};
LOG_API void    log_print(char* buf,int size,const char* fmt,...);
LOG_API void    log_write(int level,const char* msg, const char* file, int line);
LOG_API int     log_init(const char* config,char* err_buffer, int err_buffer_size);



#define log_printf(level,...) {\
    char __buf__[MAX_BUF_SIZE]; \
    log_print(__buf__,MAX_BUF_SIZE,__VA_ARGS__); \
    log_write(level,__buf__,__FUNCTION__,__LINE__);}
#define CW_DEBUG_ENABLE 1
#ifdef CW_DEBUG_ENABLE
#define   log_trace(...)  log_printf(0, __VA_ARGS__)
#define   log_debug(...)  log_printf(1,  __VA_ARGS__)
#define   log_info(...)   log_printf(2,  __VA_ARGS__)
#define   log_notice(...) log_printf(3,  __VA_ARGS__)
#define   log_warn(...)   log_printf(4,  __VA_ARGS__)
#define   log_error(...)  log_printf(5,  __VA_ARGS__)
#define   log_fatal(...)  log_printf(6,  __VA_ARGS__)
#else
#define   cw_trace(...)
#define   cw_debug(...)
#define   cw_info(...)
#define   cw_notice(...)
#define   cw_warn(...)
#define   cw_error(...)
#define   cw_fatal(...)
#endif

#endif

