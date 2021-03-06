#ifndef ERROR_HPP
#define ERROR_HPP

#include <fstream>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <cstdlib>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace logging 
{
    struct Message
    {
        std::string text;
        bool isError;
        boost::posix_time::ptime time;

        Message(const char *msg, bool error) : text(msg), isError(error) 
        {
            time = boost::posix_time::second_clock::local_time();
        }
    };

    std::queue<Message> messages;
    std::condition_variable loggerCondition;
    std::mutex loggerMutex;
    bool loggerClose = false;

    void syncLog(const Message &msg)
    {
        using namespace std;
        using namespace boost;
#ifndef XCODE
        static ofstream debug;

        if (!debug.is_open()) {
            debug.open(std::getenv("FCGI_DEBUG_LOG"), ios_base::out | ios_base::app);
            debug.imbue(locale(debug.getloc(), new posix_time::time_facet()));
        }
#else
        auto &debug = std::cerr;
#endif

        debug << (msg.isError ? "E " : "D ") << '[' << msg.time << "] " << msg.text << endl;
    }

    void asyncLogger() 
    {
        while (!loggerClose) {
            std::unique_lock<std::mutex> locker(loggerMutex);
            loggerCondition.wait(locker, []{ return loggerClose || !messages.empty(); });

            while (!messages.empty()) {
                syncLog(messages.front());
                messages.pop();
            }
        }
    }

    void logSync(const char *msg, bool isError)
    {
        std::unique_lock<std::mutex> locker(loggerMutex);
        syncLog(Message(msg, isError));
    }

    void logAsync(const char *msg, bool isError)
    {
        std::unique_lock<std::mutex> locker(loggerMutex);
        messages.push(Message(msg, isError));
        loggerCondition.notify_one();
    }

    void closeAsync()
    {
        std::unique_lock<std::mutex> locker(loggerMutex);
        loggerClose = true;
        loggerCondition.notify_one();
    }

    class Logger
    {
        std::thread loggerThread;

    public:
        Logger() : loggerThread(asyncLogger) { }

        ~Logger()
        {
            closeAsync();
            loggerThread.join();
        }
    };

    static Logger logger;
}

void error_log(const char* msg)
{
    logging::logSync(msg, true);
}

void debug_log(const char* msg)
{
    #ifdef DEBUG
    logging::logAsync(msg, false);
    #endif
}

#endif
