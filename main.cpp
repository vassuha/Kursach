#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <vector>
#include <cassert>
#include <perturb/perturb.hpp>
#include <cmath>
#define lmax 100

using namespace perturb;

struct TLE {
    std::string satelliteName;
    std::string line1;
    std::string line2;
};

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

void writeInFile(std::string url, std::string outputFile){
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        // Укажите URL вашего веб-сайта
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Отключите проверку SSL-сертификата
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        // Укажите функцию обратного вызова для записи данных в переменную
        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);


        res = curl_easy_perform(curl);


        // Проверка на ошибки
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        else {
            // Сохраните данные в текстовый файл
            std::ofstream outFile(outputFile.c_str());
            outFile << response;
            outFile.close();
        }

        // Освободите ресурсы
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}


void ISS_test(std::string ISS_TLE_1, std::string ISS_TLE_2 ){
    // Let try simulating the orbit of the International Space Station
    // Got TLE from Celestrak sometime around 2022-03-12";

    // Create and initialize a satellite object from the TLE
    auto sat = Satellite::from_tle(ISS_TLE_1, ISS_TLE_2);
    assert(sat.last_error() == Sgp4Error::NONE);

    // Prompt the user to enter date and time
    int year, month, day, hour, minute;
    double second;
    std::cout << "Enter date and time (year month day hour minute second): ";
    std::cin >> year >> month >> day >> hour >> minute >> second;

    // Let's see what the ISS is doing at the specified date and time
    const auto t = JulianDate(DateTime { year, month, day, hour, minute, second });
    const double delta_days = t - sat.epoch();

    // Calculate the position and velocity at the chosen time
    StateVector sv;
    const auto err = sat.propagate(t, sv);
    const auto &pos = sv.position, &vel = sv.velocity;

    // Вывод: МКС движется довольно быстро (~ 8 км / с)
    int distant;
    distant = sqrt( pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
    std::cout << "Position [km]: { " << pos[0] << ", " << pos[1] << ", " << pos[2] << " }\n";
    std::cout << "Speed [km/s]: { " << vel[0] << ", " << vel[1] << ", " << vel[2] << " }\n";
    std::cout << "distance to the ground - " << distant - 6378 << "\n";
}


void readFromFile(std::string fileName, std::vector<TLE>& data){
    std::ifstream inputFile(fileName);
    if (inputFile.is_open()) {
        std::string name, line1, line2;
        while (std::getline(inputFile, name) &&
               std::getline(inputFile, line1) &&
               std::getline(inputFile, line2)) {
            TLE newTLE;
            newTLE.satelliteName = name;
            newTLE.line1 = line1;
            newTLE.line2 = line2;
            data.push_back(newTLE);

            // Вывод данных после считывания
            std::cout << std::endl;
            std::cout << "Read TLE:" << std::endl;
            std::cout << "Satellite Name: " << newTLE.satelliteName << std::endl;
            std::cout << "Line 1: " << newTLE.line1 << std::endl;
            std::cout << "Line 2: " << newTLE.line2 << std::endl;


            // Вызов функции ISS_test с TLE-данными
            ISS_test(newTLE.line1, newTLE.line2);
        }
        inputFile.close();
    } else {
        std::cerr << "Unable to open the file." << std::endl;
    }
}

int main() {
    std::vector<TLE> data;
    std::string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=last-30-days&FORMAT=tle";

    //std::string url = "https://www.google.com";
    //char url[] = "http://r4uab.ru/satonline.txt";
    writeInFile(url, "output.txt");
    readFromFile("output.txt", data);

    return 0;
}
