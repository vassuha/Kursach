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

    // Преобразование времени в структуру tm
    std::tm *localTime = std::localtime(&currentTime);

    year = localTime->tm_year + 1900;
    month = localTime->tm_mon + 1;
    day = localTime->tm_mday;
    hour = localTime->tm_hour-3;
    min = localTime->tm_min;
    sec = localTime->tm_sec;

    //Отладка!!!!!!!!!
//    year = 2000;
//    month = 5;
//    day = 3;
//    hour-=3;
//    min =10;
//    sec = 0;
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

double GMST2(double JD) {
    int year, month, day, hour, min, sec;
    now(year, month, day, hour, min, sec);
    double M = ((((hour-3)*3600.0) + (min*60.0) + sec)/(12.0*3600.0));
    double d = JD - 2451545.0;
    double T = d/36525;
    double S=1.7533685592 + 0.0172027918051 * d + 6.2831853072 * M + 6.7707139 * 0.000001 * T*T - 4.50876 * 0.0000000001 * T*T*T;
    return S;
}


void ECEF2LLA(double x, double y, double z, double &latitude, double &longitude) {
    latitude = atan(z/(sqrt(x*x + y*y)));
    longitude = atan(y/x);
    if(y<0 && x< 0){
        longitude-=PI;
    }
    if(y>0 && x< 0){
        longitude+=PI;
    }
}

void LLA2ECEF(double &x, double &y, double &z, double latitude, double longitude, double r) {
    x = r*cos(latitude)*cos(longitude);
    y = r*cos(latitude)*sin(longitude);
    z = r*sin(latitude);
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

void ECI2ECEF(double x, double y, double z,double S, double &X, double &Y, double &Z, bool CorV){
    double W = 0.000072921151467;

    double cos_s = cos(S);
    double sin_s = sin(S);

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

double distanceToMIEM(double x, double y, double z){
    double x_MIEM = 2847.32, y_MIEM = 2177.72, z_MIEM = 5275.34;

    double r=sqrt((x_MIEM-x)*(x_MIEM-x) + (y_MIEM-y)*(y_MIEM-y) + (z_MIEM-z)*(z_MIEM-z));
    return r;

}

void TLE_decoding(string ISS_TLE_1, string ISS_TLE_2 ){
    auto sat = Satellite::from_tle(ISS_TLE_1, ISS_TLE_2);
    assert(sat.last_error() == Sgp4Error::NONE);

    int year, month, day, hour, minute, second;
    now(year, month, day, hour, minute, second);
    const auto t = JulianDate(DateTime { year, month, day, hour, minute, (double)second });
    const double delta_days = t - sat.epoch();
    StateVector sv;
    const auto err = sat.propagate(t, sv);
    const auto &ECI_pos = sv.position, &vel = sv.velocity;

    double distant;
    distant = sqrt( ECI_pos[0] * ECI_pos[0] + ECI_pos[1] * ECI_pos[1] + ECI_pos[2] * ECI_pos[2]);

    double X, Y, Z;
    double sideralT = GMST();
    std::cout << "GMST= " << sideralT << std::endl;
    cout << "Position in  ECI [km]: { " << ECI_pos[0] << ", " << ECI_pos[1] << ", " << ECI_pos[2] << " }\n";

    ECI2ECEF(ECI_pos[0], ECI_pos[1], ECI_pos[2], sideralT, X, Y, Z, false);
    cout << "Position in ECEF [km]: { " << X << ", " << Y << ", " << Z << " }\n";

    double latitude, longitude;
    ECEF2LLA(X, Y, Z, latitude, longitude);

    cout << "Position in LLA [deg]: { " << latitude*180/PI << ", " << longitude*180/PI << " }\n";

    cout << "Distance to the ground: " << distant - 6378 << " km" << "\n";
    //cout << "Speed in ECI [km/s]: { " << vel[0] << ", " << vel[1] << ", " << vel[2] << " }\n";
    //cout << "speed (abs): " << speed << "\n";
    double r = distanceToMIEM(X, Y, Z);
    cout << "Distance to MIEM = " << r << "km" << endl;

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
            cout << newTLE.satelliteName << endl;
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

    //Раскомментировать нужное:
    //Запуски за последние 30 дней
    //string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=last-30-days&FORMAT=tle";
    //Космические станции
    //string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=stations&FORMAT=tle";
    //GOES
    //string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=goes&FORMAT=tle";
    //IRIDIUM
    string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=iridium-33-debris&FORMAT=tle";

    //writeInFile(url, "output.txt");
    readFromFile("Space_Stations.txt", data);

    return 0;
}