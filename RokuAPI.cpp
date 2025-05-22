#include"curl/curl.h"


void ActivateHDMI2() {
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
