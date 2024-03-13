#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <vector>
#include <cassert>
#include <perturb/perturb.hpp>
#include <cmath>
#include <chrono>

#define PI 3.141592653589793

using namespace std;
using namespace perturb;

struct TLE {
    string satelliteName;
    string line1;
    string line2;
};

void now(int &year, int &month, int &day, int &hour, int &min, int &sec) {
    auto currentTimePoint = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(currentTimePoint);

    // Преобразуем время в структуру tm
    std::tm *localTime = std::localtime(&currentTime);

    // Извлекаем численные значения года, месяца, дня, и т.д.
    year = localTime->tm_year + 1900;
    month = localTime->tm_mon + 1;
    day = localTime->tm_mday;
    hour = localTime->tm_hour;
    min = localTime->tm_min;
    sec = localTime->tm_sec;
}

double GMST(void){
    int year, month, day, hour, min, sec;
    now(year, month, day, hour, min, sec);

    double dwhole = 367.0*year-int(7*(year+int((month+9)/12))/4)+int(275*month/9)+day-730531.5;
    double dfrac = (hour+min/60.0+sec/3600.0)/24.0;
    double d = dwhole+dfrac;

    double GMST = 281.46061837+360.98564736629*d;
    GMST = GMST-360*int(GMST/360);
    if(GMST < 0)   GMST = 360.0 + GMST;

    double t = GMST*PI/180.0;
    return t;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

void ECI2ECEF(double x, double y, double z,double t, double &X, double &Y, double &Z, bool CorV){
    double W = 0.000072921151467;

    double s_zv  = t;
    double cos_s = cos(s_zv);
    double sin_s = sin(s_zv);

    X =  x * cos_s  + y * sin_s;
    Y = -x * sin_s  + y * cos_s;
    Z = z;


    if(CorV == true)
    {
        X = X + W * Y;
        Y = Y - W * X;
    }
}

void writeInFile(string url, string outputFile){
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Отключение проверки SSL-сертификата
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        else {
            ofstream outFile(outputFile.c_str());
            outFile << response;
            outFile.close();
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

void TLE_decoding(string ISS_TLE_1, string ISS_TLE_2 ){
    auto sat = Satellite::from_tle(ISS_TLE_1, ISS_TLE_2);
    assert(sat.last_error() == Sgp4Error::NONE);

    int year, month, day, hour, minute, second;

    now(year, month, day, hour, minute, second);
    /*
    cout << "Enter date and time (year month day hour minute second): ";
    cin >> year >> month >> day >> hour >> minute >> second;
    */

    const auto t = JulianDate(DateTime { year, month, day, hour, minute, second });
    const double delta_days = t - sat.epoch();

    StateVector sv;
    const auto err = sat.propagate(t, sv);
    const auto &pos = sv.position, &vel = sv.velocity;

    double distant;
    distant = sqrt( pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
    cout << "Position in  ECI [km]: { " << pos[0] << ", " << pos[1] << ", " << pos[2] << " }\n";

    double X, Y, Z;
    double t1 = GMST();
    ECI2ECEF(pos[0], pos[1], pos[2], t1, X, Y, Z, false);
    cout << "Position in ECEF [km]: { " << X << ", " << Y << ", " << Z << " }\n";

    cout << "distance to the ground: " << distant - 6378 << " km" << "\n";
    //cout << "Speed in ECI [km/s]: { " << vel[0] << ", " << vel[1] << ", " << vel[2] << " }\n";
    //cout << "speed (abs): " << speed << "\n";

    //коэффициент, преобразующий вектор спутника в вектор подспутниковой проекции на землю
    double k;
    k=sqrt((6378*6378)/((X*X)+(Y*Y)+(Z*Z)));
}

void readFromFile(string fileName, vector<TLE>& data){
    ifstream inputFile(fileName);
    if (inputFile.is_open()) {
        string name, line1, line2;
        while (getline(inputFile, name) &&
               getline(inputFile, line1) &&
               getline(inputFile, line2)) {
            TLE newTLE;
            newTLE.satelliteName = name;
            newTLE.line1 = line1;
            newTLE.line2 = line2;
            data.push_back(newTLE);

            // Вывод данных после считывания
            cout << endl;
            cout << "Read TLE:" << endl;
            cout << "Satellite Name: " << newTLE.satelliteName << endl;
            cout << "Line 1: " << newTLE.line1 << endl;
            cout << "Line 2: " << newTLE.line2 << endl;


            // Вызов функции TLE_decoding с TLE-данными
            TLE_decoding(newTLE.line1, newTLE.line2);
        }
        inputFile.close();
    } else {
        cerr << "Unable to open the file." << endl;
    }
}

int main() {
    vector<TLE> data;
    //Раскомментировать нужное
    //Запуски за последние 30 дней
    //string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=last-30-days&FORMAT=tle";
    //Космические станции
    string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=stations&FORMAT=tle";

    writeInFile(url, "output.txt");
    readFromFile("output.txt", data);

    return 0;
}