#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <vector>
#include <cassert>
#include <perturb/perturb.hpp>
#define lmax 100

using namespace perturb;

struct TLE{
    std::string name; //Название спутника
    int stringNumber;   //Номер строки	(1)
    int NORADNumber;    //Номер спутника в базе данных NORAD	(25544)
    char classification;    //Классификация (U=Unclassified — не секретный)	(U)
    int lastTwoNubersOfLaunchYear;  //Последние две цифры года запуска	(98)
    int numberOfLaunch; //Номер запуска в этом году	(067)
    char partOfLaunch;  //Часть запуска	(A)
    int epochYear;	//Год эпохи (последние две цифры) (08)
    float EpochTime;	//Время эпохи (целая часть — номер дня в году, дробная — часть дня)	(264.51782528)
    float firstDerivative;	//Первая производная от среднего движения (ускорение), делённая на два [виток/день^2]	(-.00002182)
    float SecondDerivative;	//Вторая производная от среднего движения, делённая на шесть (подразумевается, что число начинается с десятичного разделителя) [виток/день^3]	(00000-0)
    float slowdown;	//Коэффициент торможения B* (подразумевается, что число начинается с десятичного разделителя)	(-11606-4)
    int enefirid;	//Изначально — типы эфемерид, сейчас — всегда число 0	(0)
    int elementVersion; //Номер (версия) элемента	(292)
    int Checksum;	//Контрольная сумма по модулю 10	(7)
    float inclination;    //Наклонение в градусах	(51.6416)
    float ascension;   //Долгота восходящего узла в градусах	(247.4627)
    float Eccentricity;    //Эксцентриситет (подразумевается, что число начинается с десятичного разделителя)	(0006703)
    float ArgumentOfPerigee;    //Аргумент перицентра в градусах	(130.5360)
    float anomaly;  //Средняя аномалия в градусах	(325.0288)
    float motion;   //Частота обращения (оборотов в день) (среднее движение) [виток/день]	(15.72125391)
    int RevolutionNumber;   //Номер витка на момент эпохи	(56353)
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

void readFromFile(std::string fileName, std::vector<TLE>& data){
    int i = 0;
    std::ifstream inputFile(fileName);
    if (inputFile.is_open()) {
        // Создаем строку для хранения считанной строки
        std::string line;
        struct TLE newTLE;
        // Считываем данные построчно из файла
        while (std::getline(inputFile, line)) {
            if(i %3 == 0){
                struct TLE newTLE;
                newTLE.name = line;
                data.push_back(newTLE);
                std::cout << newTLE.name << std::endl;
            }
            if(i%3 == 1){
                newTLE.stringNumber = line[0];

            }
            if(i%3 == 2){

            }
            i++;
        }

        // Закрываем файл
        inputFile.close();
    } else {
        std::cerr << "Unable to open the file." << std::endl;
    }
}

void ISS_test(void){
    // Let try simulating the orbit of the International Space Station
    // Got TLE from Celestrak sometime around 2022-03-12
    std::string ISS_TLE_1 = "1 25544U 98067A   22071.78032407  .00021395  00000-0  39008-3 0  9996";
    std::string ISS_TLE_2 = "2 25544  51.6424  94.0370 0004047 256.5103  89.8846 15.49386383330227";

    // Create and initialize a satellite object from the TLE
    auto sat = Satellite::from_tle(ISS_TLE_1, ISS_TLE_2);
    assert(sat.last_error() == Sgp4Error::NONE);
    assert(sat.epoch().to_datetime().day == 12);

    // Let's see what the ISS is doing on Pi Day
    const auto t = JulianDate(DateTime { 2022, 3, 14, 1, 59, 26.535 });
    const double delta_days = t - sat.epoch();
    assert(1 < delta_days && delta_days < 3);  // It's been ~2 days since the epoch

    // Calculate the position and velocity at the chosen time
    StateVector sv;
    const auto err = sat.propagate(t, sv);
    assert(err == Sgp4Error::NONE);
    const auto &pos = sv.position, &vel = sv.velocity;

    // Вывод: МКС движется довольно быстро (~ 8 км / с)
    std::cout << "Position [km]: { " << pos[0] << ", " << pos[1] << ", " << pos[2] << " }\n";
    std::cout << "Speed [km/s]: { " << vel[0] << ", " << vel[1] << ", " << vel[2] << " }\n";
}

int main() {
    std::vector<TLE> data;
    std::string url = "https://celestrak.org/NORAD/elements/gp.php?GROUP=last-30-days&FORMAT=tle";

    //std::string url = "https://www.google.com";
    //char url[] = "http://r4uab.ru/satonline.txt";
    writeInFile(url, "output.txt");
    readFromFile("output.txt", data);

    ISS_test();

    return 0;
}
