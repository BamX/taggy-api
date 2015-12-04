#include <fastcgi++/fcgistream.hpp>
#include <fastcgi++/request.hpp>
#include <fastcgi++/manager.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <locale>
#include "ptree-fix.hpp"
#include "json_parser.hpp"
#include <map>
#include <sstream>

#include "error.hpp"
#include "currency_storage.hpp"

typedef char tchar_t;
typedef std::basic_string<tchar_t> tstring;
typedef std::basic_stringstream<tchar_t, std::char_traits<tchar_t>, std::allocator<tchar_t> > tstream;
typedef Fastcgipp::Fcgistream<tchar_t> fcgistream;

using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

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
        out << "Status: 404 Not found\r\n";
        out << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    }

    void http500()
    {
        out << "Status: 500 Internal Server Error\r\n";
        out << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    }

    std::string utcDate(boost::posix_time::ptime time)
    {
        using namespace boost::posix_time;
        static std::locale loc(std::cout.getloc(), new time_facet("%Y%m%dT%H%M%S"));

        std::ostringstream buffer;
        buffer.imbue(loc);
        buffer << time;
        return buffer.str();
    }

    bool jsonGetCurrency()
    {
        boost::posix_time::ptime updateTime;
        auto currency = staticStorage.getCurrency(&updateTime);

        if (currency.size() == 0) {
            http500();
            return true;
        }
        ptree result;

        ptree currencyRates;
        for (auto &kvp : currency) {
            ptree element;
            put_str(element, "name", kvp.first);
            element.put<float>("value", kvp.second);
            currencyRates.push_back(std::make_pair("", element));
        }
        result.add_child("currency", currencyRates);

        put_str(result, "time", utcDate(updateTime));

        httpHeader();
        write_json(out, result);
        return true;
    }

    bool jsonUpdateCurrency()
    {
        httpHeader();

        ptree root = environment().jsonRoot;
        std::map<std::string, float> currency;
        for (auto &kvp : root.get_child("currency")) {
            auto rate = kvp.second;
            auto name = rate.get<std::string>("name");
            auto value = rate.get<float>("value");
            currency[name] = value;
        }
        staticStorage.updateCurrency(currency);

        return true;
    }

    bool response()
    {
        switch (environment().requestMethod) {
            case Fastcgipp::Http::HTTP_METHOD_GET:
                if (environment().requestUri == "/api/v1/currency/") {
                    return jsonGetCurrency();
                }
                break; 

            case Fastcgipp::Http::HTTP_METHOD_POST:
                if (environment().requestUri == "/api/v1/currency/") {
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