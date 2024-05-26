#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <vector>
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

void now(int &year, int &month, int &day, int &hour, int &min, int &sec, int offset) {
    auto currentTimePoint = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(currentTimePoint) + offset*60;

    std::tm *localTime = std::localtime(&currentTime);

    year = localTime->tm_year + 1900;
    month = localTime->tm_mon + 1;
    day = localTime->tm_mday;
    hour = localTime->tm_hour-3;
    min = localTime->tm_min;
    sec = localTime->tm_sec;

}

double GMST(int offset){
    int year, month, day, hour, min, sec;
    now(year, month, day, hour, min, sec, offset);

    double dwhole = 367.0*year-int(7*(year+int((month+9)/12))/4)+int(275*month/9)+day-730531.5;
    double dfrac = (hour+min/60.0+sec/3600.0)/24.0;
    double d = dwhole+dfrac;

    double GMST = 281.46061837+360.98564736629*d;
    GMST = GMST-360*int(GMST/360);
    if(GMST < 0)   GMST = 360.0 + GMST;

    double t = GMST*PI/180.0;
    return t;
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

double distance2MIEM(double x, double y, double z){
    double x_MIEM = 2840.88, y_MIEM = 2172.78, z_MIEM = 5263.38;

    double r=sqrt((x_MIEM-x)*(x_MIEM-x) + (y_MIEM-y)*(y_MIEM-y) + (z_MIEM-z)*(z_MIEM-z));
    return r;

}

double Earth_height_in_point(double x, double y, double z){
    double a = 6378.13649;
    double b = 6356.75175;
    double t = sqrt(1.0/((x*x + y*y)/(a*a)+(z*z)/(b*b)));
    double r = t*sqrt(x*x+y*y+z*z);
    return r;
}

void TLE_decoding(string TLE_1, string TLE_2 ){
    auto sat = Satellite::from_tle(TLE_1, TLE_2);

    int year, month, day, hour, minute, second;
    now(year, month, day, hour, minute, second, 0);
    const auto t = JulianDate(DateTime { year, month, day, hour, minute, (double)second });
    const double delta_days = t - sat.epoch();
    StateVector sv;
    const auto err = sat.propagate(t, sv);
    const auto &ECI_pos = sv.position, &vel = sv.velocity;

    double distant_to_center;
    distant_to_center = sqrt(ECI_pos[0] * ECI_pos[0] + ECI_pos[1] * ECI_pos[1] + ECI_pos[2] * ECI_pos[2]);


    double X, Y, Z;
    double sideralT = GMST(0);
    std::cout << "GMST= " << sideralT << std::endl;
    cout << "Position in  ECI [km]: { " << ECI_pos[0] << ", " << ECI_pos[1] << ", " << ECI_pos[2] << " }\n";

    ECI2ECEF(ECI_pos[0], ECI_pos[1], ECI_pos[2], sideralT, X, Y, Z, false);
    cout << "Position in ECEF [km]: { " << X << ", " << Y << ", " << Z << " }\n";

    double latitude, longitude;
    ECEF2LLA(X, Y, Z, latitude, longitude);
    double Earth_height = Earth_height_in_point(X, Y, Z);

    cout << "Position in LLA [deg]: { " << latitude*180/PI << ", " << longitude*180/PI << " }\n";

    for(int offset=-30; offset<30; offset+=1){

        const auto t1 = JulianDate(DateTime { year, month, day, hour, minute, (double)second }) - offset/1440.0;
        const double delta_days1 = t1 - sat.epoch();
        StateVector sv1;
        const auto err1 = sat.propagate(t1, sv1);
        const auto &ECI_pos1 = sv1.position, &vel1 = sv1.velocity;

        double X1, Y1, Z1;
        double sideralT1 = GMST(offset);
        ECI2ECEF(ECI_pos1[0], ECI_pos1[1], ECI_pos1[2], sideralT1, X1, Y1, Z1, false);
        double latitude1, longitude1;
        ECEF2LLA(X1, Y1, Z1, latitude1, longitude1);

        cout << "Trajectory in LLA: { " << latitude1*180/PI << ", " << longitude1*180/PI << " }\n";
    }

    double distance_to_the_ground = distant_to_center - Earth_height;
    cout << "Distance to the ground: " << distance_to_the_ground << " km" << "\n";
    //cout << "Speed in ECI [km/s]: { " << vel[0] << ", " << vel[1] << ", " << vel[2] << " }\n";
    //cout << "speed (abs): " << speed << "\n";
    double r = distance2MIEM(X, Y, Z);
    cout << "Distance to MIEM: " << r << " km" << endl;

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

            cout << endl;
            cout << "Name: " << newTLE.satelliteName << endl;
            cout << "Line 1: " << newTLE.line1 << endl;
            cout << "Line 2: " << newTLE.line2 << endl;

            TLE_decoding(newTLE.line1, newTLE.line2);
        }
        inputFile.close();
    } else {
        cerr << "Unable to open the file." << endl;
    }
}

int main(int argc, char *argv[]) {
    if ((string)argv[1] == "update"){
        writeInFile("https://celestrak.org/NORAD/elements/gp.php?GROUP=last-30-days&FORMAT=tle", "./Last_30_days.txt");
        writeInFile("https://celestrak.org/NORAD/elements/gp.php?GROUP=stations&FORMAT=tle", "./Space_stations.txt");
        writeInFile("https://celestrak.org/NORAD/elements/gp.php?GROUP=goes&FORMAT=tle", "./GOES.txt");
        writeInFile("https://celestrak.org/NORAD/elements/gp.php?GROUP=iridium-33-debris&FORMAT=tle", "./IRIDIUM.txt");
        writeInFile("https://r4uab.ru/satonline.txt", "./R4uab.txt");
    }else{
        vector<TLE> data;
        bool flag = false;
        if((string)argv[1] == "IRIDIUM"){
            readFromFile("./IRIDIUM.txt", data);
            flag = true;
        }
        if((string)argv[1] == "Last 30 days") {
            readFromFile("./Last_30_days.txt", data);
            flag = true;
        }
        if((string)argv[1] == "GOES") {
            readFromFile("./GOES.txt", data);
            flag = true;
        }
        if((string)argv[1] == "R4uab") {
            readFromFile("./R4uab.txt", data);
            flag = true;
        }
        if(!flag){
            readFromFile("./Space_stations.txt", data);
        }
    }
    return 0;
}