#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include "curl/curl.h"
#include <boost/asio.hpp>
#include <iostream>
#include "Roku.h"
#include <time.h>
#include <codecvt>
#include <regex>

#define SSDP_ADDR "239.255.255.250"
#define SSDP_PORT "1900"
#define SSDP_REQUEST \
    "M-SEARCH * HTTP/1.1\r\n" \
    "Host: 239.255.255.250:1900\r\n" \
    "Man: \"ssdp:discover\"\r\n" \
    "ST: roku:ecp\r\n" \
    "MX: 2\r\n" \
    "\r\n"

using namespace boost::asio;
using ip::udp;

RokuTVController::RokuTVController(std::string ipA) {
    ipAddress = ipA;
}

void RokuTVController::SetInput(int HDMINumber) {
    CURL* curl = curl_easy_init();
    if (curl) {
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.11.122:8060/keypress/InputHDMI2");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);  // Perform a POST request
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}

std::wstring RokuTVController::GetName() {
    std::wstring returnVal(this->ipAddress.begin(), this->ipAddress.end());
    return returnVal;
}

std::wstring RokuTVController::Serialize() {
    std::wstring returnVal(this->ipAddress.begin(), this->ipAddress.end());
    return returnVal;
}

bool RokuTVController::Equals(TVController* other){
    RokuTVController* o = dynamic_cast<RokuTVController*>(other);
    if (o) {
        return (this->ipAddress == o->ipAddress);
    }
    return false;
}

std::list<TVController*> RokuTVController::SearchDevices() {
    std::list<TVController*> returnVal;
    boost::asio::io_context io_context;
    udp::resolver resolver(io_context);
    udp::endpoint sender_endpoint = *resolver.resolve(udp::v4(), SSDP_ADDR, SSDP_PORT).begin();;
    udp::socket socket(io_context);

    socket.open(udp::v4());
    socket.set_option(detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{200});
    socket.send_to(buffer(SSDP_REQUEST), sender_endpoint);

    clock_t startTime = clock();
    double timeElapsed;
    do {
        std::array<char, 512> recv_buf;
        udp::endpoint response_endpoint;

        try {
            size_t len = socket.receive_from(
                boost::asio::buffer(recv_buf), response_endpoint);
            std::string response(recv_buf.data());
            response.resize(len);

            std::regex rgx("LOCATION: http://([\\d\\.]+)");
            std::smatch matches;

            if (std::regex_search(response, matches, rgx)) {
                OutputDebugStringA("Found Roku Device ");
                OutputDebugStringA(matches[1].str().c_str());
                OutputDebugStringA("\r\n");
                returnVal.push_front(new RokuTVController(matches[1].str()));
            }
        }
        catch (boost::system::system_error e) {

        }
        timeElapsed = difftime(clock(), startTime);
    } while (timeElapsed < 3000.0);

    return returnVal;

}