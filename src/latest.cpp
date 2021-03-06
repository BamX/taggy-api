#include <fastcgi++/fcgistream.hpp>
#include <fastcgi++/request.hpp>
#include <fastcgi++/manager.hpp>

#include "utility/error.hpp"
#include "utility/types.hpp"
#include "boost-fix/ptree-fix.hpp"
#include "boost-fix/json_parser.hpp"

#include "storage/currency_storage.hpp"

using boost::property_tree::ptree;

class MainRequest : public Fastcgipp::Request<tchar_t>
{
    void httpHeader(bool isJson = true) 
    {
        if (isJson) {
            out << "Content-Type: application/json; charset=utf-8\r\n\r\n";
        } else {
            out << "Content-Type: text/html; charset=utf-8\r\n\r\n";
        }
    }

    void http404()
    {
        debug_log("Called 404");
        out << "Status: 404 Not found\r\n";
        out << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    }

    void http500()
    {
        debug_log("Called 500");
        out << "Status: 500 Internal Server Error\r\n";
        out << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    }

    bool jsonGetCurrency()
    {
        httpHeader();
        out << staticStorage.getJson();
        return true;
    }

    bool jsonUpdateCurrency()
    {
        httpHeader();

        ptree root = environment().jsonRoot;
        std::map<std::string, float> rates;
        for (auto &kvp : root.get_child("currency")) {
            auto rate = kvp.second;
            auto name = rate.get<std::string>("name");
            auto value = rate.get<float>("value");
            rates[name] = value;
        }
        staticStorage.updateCurrency(rates);

        return true;
    }

    bool response()
    {
        debug_log("===response start");
        switch (environment().requestMethod) {
            case Fastcgipp::Http::HTTP_METHOD_GET:
                if (environment().scriptName == "/api/v1/latest") {
                    debug_log("Get");
                    return jsonGetCurrency();
                }
                break; 

            case Fastcgipp::Http::HTTP_METHOD_POST:
                if (environment().scriptName == "/api/v1/latest") {
                    debug_log("Update");
                    return jsonUpdateCurrency();
                }
                break;

            default:
                break;
        }

        http404();
        out << "404 Not found";
        return true;
    }
};

int main()
{
    try {
        Fastcgipp::Manager<MainRequest> fcgi;
        fcgi.handler();
    } catch(std::exception& e) {
        error_log(e.what());
    }
}