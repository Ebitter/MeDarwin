#include "Logger.h"

#include "Poco/Logger.h"
#include "Poco/AutoPtr.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/Util/LoggingConfigurator.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/String.h"
#include "Poco/LoggingFactory.h"
#include "Poco/Instantiator.h"
#include <sstream>
#include <fstream>
LOG_API Poco::Logger&  GetDefaultChannel();



Poco::Logger&  GetDefaultChannel()
{
	return Poco::Logger::get("");
}
void log_write(int level,const char* msg, const char* file, int line)
{
    Poco::Logger& logger = GetDefaultChannel();
    int test = logger.getLevel();
    printf("level=%d\n", test);

    switch(level)
    {
        case 0:
            logger.trace(msg,file,line);
            break;
        case 1:
            logger.debug(msg,file,line);
            break;
        case 2:
            logger.information(msg,file,line);
            break;   
        case 3:
            logger.notice(msg,file,line);
            break;
        case 4:
            logger.warning(msg,file,line);
            break;
        case 5:
            logger.error(msg,file,line);
            break;
        case 6:
            logger.fatal(msg,file,line);
            break;
        default:
            logger.debug(msg,file,line);
    }
}
void  log_print(char* buf,int size,const char* fmt,...)
{


      va_list ap;
      int n;

      memset(buf,0,size);
      va_start(ap, fmt);

      n = vsnprintf(buf, size, fmt, ap);

      if (n < 0) {
            n = 0;
      }
      else if (n >= size) {
            n = size-1;
      }
      buf[n] = '\0';

      va_end(ap);

}

static const char* defaultconfig =
                "logging.loggers.root.channel = c3\n"
                "logging.loggers.root.level = trace\n"
                "logging.channels.c1.class = ColorConsoleChannel\n"
                "logging.channels.c1.formatter = f1\n"
                "logging.channels.c2.class = FileChannel\n"
                "logging.channels.c2.path = Logs/run.log\n"
                "logging.channels.c2.formatter = f1\n"
                "logging.channels.c2.rotation=1 days\n"
                "logging.channels.c2.archive=number\n"
                "logging.channels.c2.purgeCount=90\n"
                "logging.channels.c2.flush=false\n"
                "logging.channels.c2.compress=true\n"
                "logging.channels.c3.class = SplitterChannel\n"
                "logging.channels.c3.channel1=c1\n"
                "logging.channels.c3.channel2=c2\n"
                "logging.formatters.f1.class = PatternFormatter\n"
                "logging.formatters.f1.pattern = [%p] %L%Y-%n-%d %H:%M:%S %t  %U %u\n"
                "logging.formatters.f1.times = UTC\n";

void mkLogPath()
{
    std::string strPath = Poco::Path::current()+"/Logs/";
    Poco::Path path(strPath);

    Poco::File file(path);
    if(!file.exists())
    {
        file.createDirectory();
    }


}
static bool init = false;

#include "Poco/LocalDateTime.h"
void log_get_error(char *buffer, int size)
{

}

static int log_check(Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pconfig)
{
    try
    {
        std::string root = pconfig->getString("logging.loggers.root.channel");
        Poco::Util::AbstractConfiguration::Keys keys;
        pconfig->keys("logging.channels",keys);

        for(size_t i = 0; i < keys.size(); i++)
        {

            //printf("key[%d]=%s\n",i,keys[i].c_str());
            std::string key_log_class = "logging.channels."+keys[i]+".class";
            std::string log_class = Poco::trim(pconfig->getString(key_log_class));
            if(log_class == "FileChannel")
            {
                std::string key_log_path = "logging.channels."+keys[i]+".path";
                std::string str_log_path = pconfig->getString(key_log_path,"./");
                //printf("log path=%s\n",str_log_path.c_str());
                Poco::Path path(str_log_path);
                if(path.isDirectory())
                {
                    //printf("is directory\n");
                    Poco::File log_path_file(str_log_path);
                    if(!log_path_file.exists())
                    {
                        log_path_file.createDirectory();
                    }
                    Poco::LocalDateTime ldt;
                    std::string log_file = Poco::format("%04d%02d%02d%02d%02d%02d%03d.log",ldt.year(),ldt.month(),ldt.day(),ldt.hour(),ldt.minute(),ldt.second(),ldt.millisecond());
                    str_log_path+=log_file;
                    pconfig->setString(key_log_path,str_log_path);
                }
                else if(path.isFile())
                {

                    Poco::File log_path_file(str_log_path);
                    if(log_path_file.exists() && log_path_file.isDirectory())
                    {
                        Poco::LocalDateTime ldt;
                        std::string log_file = Poco::format("%04d%02d%02d%02d%02d%02d%03d.log",ldt.year(),ldt.month(),ldt.day(),ldt.hour(),ldt.minute(),ldt.second(),ldt.millisecond());
                        str_log_path+=log_file;
                        pconfig->setString(key_log_path,str_log_path);
                    }
                    std::string parent = path.parent().toString();
                    Poco::File log_parent(path.parent());

                    //printf("parent=%s\n",parent.c_str());
                    if(!log_parent.exists())
                    {
                        log_parent.createDirectories();
                    }
                    //printf("is file\n");
                }
            }


        }
    }
    catch(Poco::Exception& e)
    {

    }
    return 0;
}

static char* log_readconfig(const char* config)
{
    if(config==NULL)
    {
        printf("%s\n","config NULL");
        return NULL;
    }
    std::ifstream fBlock;
    fBlock.open(config, std::ios::in | std::ios::out | std::ios::app);
    if (!fBlock)
    {
        printf("open file error\n");
    }


    char *pBuff = NULL;
    int buffsize =2000;
    pBuff = new char[buffsize];
    memset(pBuff, 0, buffsize);
    fBlock.read(pBuff,buffsize);
    fBlock.close();
    return pBuff;
}

int log_init(const char *config, char *err_buffer, int err_buffer_size)
{
    if(init) return 0;

    if(config==NULL)
    {
        config = defaultconfig;
        mkLogPath();
    }

    try {
        FILE* fd = fopen(config, "rb");
        if(fd)
        {
            fclose(fd);
            char *pConfigBuff =log_readconfig(config);
             std::istringstream istr(pConfigBuff);
            Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pConfig = new Poco::Util::PropertyFileConfiguration(istr);
            log_check(pConfig);
            Poco::Util::LoggingConfigurator configurator;
            configurator.configure(pConfig);
           delete pConfigBuff;
        }
        else
        {
             std::istringstream istr(config);
            Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pConfig = new Poco::Util::PropertyFileConfiguration(istr);
            log_check(pConfig);
            Poco::Util::LoggingConfigurator configurator;
            configurator.configure(pConfig);
        }

    } catch (Poco::Exception& e) {
        printf("error:%s", e.displayText().c_str());
        if(err_buffer == NULL) return   LOG_ERR_BUFF_NULL;
        if(err_buffer_size < 64) return LOG_ERR_BUFF_TOO_SMALL;
        int n = (e.displayText().length() >= err_buffer_size)?err_buffer_size-1:e.displayText().length();

        int size = e.displayText().copy(err_buffer,n);
        err_buffer[size] = 0;

        return LOG_ERR_PARAM;
    }catch(...)
    {

        printf("%s\n","unkown error");
        return LOG_ERR_UNKOWN_ERR;
    }

    init = true;
    return 0;

}
