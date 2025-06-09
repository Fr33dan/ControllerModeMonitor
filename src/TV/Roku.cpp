#include <regex>

#include <boost/asio.hpp>
#include "curl/curl.h"
#include "pugixml.hpp"

#include "../framework.h"
#include "Roku.h"

#define SSDP_ADDR "239.255.255.250"
#define SSDP_PORT "1900"
#define SSDP_REQUEST \
    "M-SEARCH * HTTP/1.1\r\n" \
    "Host: 239.255.255.250:1900\r\n" \
    "Man: \"ssdp:discover\"\r\n" \
    "ST: roku:ecp\r\n" \
    "MX: 2\r\n" \
    "\r\n"
#define ROKU_NETWORK_WAIT 200

using namespace boost::asio;
using ip::udp;

RokuTV::RokuTV(std::string ipA) {
    ipAddress = ipA;
    this->deviceInfo = new pugi::xml_document();
    this->mediaPlayer = new pugi::xml_document();
}

size_t WriteCallback(char* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

BOOL RokuTV::IsAtHDMI() {
    this->UpdateDocument("media-player", this->mediaPlayer);
    pugi::xml_node playerNode = this->mediaPlayer->child("player");
    pugi::xml_node pluginNode = playerNode.child("plugin");
    return pluginNode == NULL;
}

void RokuTV::SetInput(int HDMINumber) {
    this->UpdateDocument("device-info", this->deviceInfo);
    pugi::xml_node rootNode = this->deviceInfo->child("device-info");
    pugi::xml_node powerStatusNode = rootNode.child("power-mode");
    if (!strncmp(powerStatusNode.child_value(), "Ready", 5)) {
        this->SendCommand("keypress/PowerOn");
        Sleep(ROKU_NETWORK_WAIT);
        OutputDebugString(L"Roku: Powered on TV.");
    }

    this->SendCommand("keypress/InputHDMI" + std::to_string(HDMINumber + 1));
    while (!this->IsAtHDMI()) {
        Sleep(ROKU_NETWORK_WAIT);
        this->SendCommand("keypress/InputHDMI" + std::to_string(HDMINumber + 1));
    }
}

void RokuTV::UpdateDocument(std::string queryCommand, pugi::xml_document * document){
    CURL* curl = curl_easy_init();
    std::string url = "http://" + this->ipAddress + ":8060/query/" + queryCommand;
    if (curl) {
        CURLcode res;
        std::string readBuffer;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, ROKU_NETWORK_WAIT);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        document->load_string(readBuffer.c_str());
    }
}

void RokuTV::SendCommand(std::string command) {
    CURL* curl = curl_easy_init();
    std::string url = "http://" + this->ipAddress + ":8060/" + command;
    if (curl) {
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);  // Perform a POST request
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, ROKU_NETWORK_WAIT);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

    }
}

std::wstring RokuTV::GetName() {
    pugi::xml_node rootNode = this->deviceInfo->child("device-info");
    if (!rootNode) {
        this->UpdateDocument("device-info", this->deviceInfo);
        rootNode = this->deviceInfo->child("device-info");
    }
    pugi::xml_node devNameNode = rootNode.child("user-device-name");
    std::wstring deviceName = pugi::as_wide(devNameNode.child_value());
    pugi::xml_node devLocNode = rootNode.child("user-device-location");
    std::wstring deviceLoc = pugi::as_wide(devLocNode.child_value());
    return deviceName + L" - " + deviceLoc;
}

std::string RokuTV::Serialize() {
    std::string returnVal = this->ipAddress;
    return returnVal;
}

int RokuTV::HDMICount() {
    return 4;
}

bool RokuTV::Equals(TV* other){
    RokuTV* o = dynamic_cast<RokuTV*>(other);
    if (o) {
        return (this->ipAddress == o->ipAddress);
    }
    return false;
}

bool RokuTV::Validate(std::string &s) {
    size_t n = s.size();

    if (n < 7)
        return false;

    // Using string stream to separate all the string from
    // '.' and push back into vector like for ex -
    std::vector<std::string> v;
    std::stringstream ss(s);
    while (ss.good())
    {
        std::string substr;
        getline(ss, substr, '.');
        v.push_back(substr);
    }

    if (v.size() != 4)
        return false;

    // Iterating over the generated vector of strings
    for (int i = 0; i < v.size(); i++)
    {
        std::string temp = v[i];

        if (temp.size() > 1)
        {
            if (temp[0] == '0')
                return false;
        }

        for (int j = 0; j < temp.size(); j++)
        {
            if (isalpha(temp[j]))
                return false;
        }

        // And lastly we are checking if the number is
        // greater than 255 or not
        if (stoi(temp) > 255)
            return false;
    }
    return true;
}

std::vector<TV*> RokuTV::SearchDevices() {
    std::vector<TV*> returnVal;
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
                returnVal.push_back(new RokuTV(matches[1].str()));
            }
        }
        catch (boost::system::system_error e) {

        }
        timeElapsed = difftime(clock(), startTime);
    } while (timeElapsed < 3000.0);

    return returnVal;

}