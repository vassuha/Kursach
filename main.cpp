#include <iostream>
#include <fstream>
#include <curl/curl.h>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

int main() {
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        // Укажите URL вашего веб-сайта
        curl_easy_setopt(curl, CURLOPT_URL, "https://celestrak.org/NORAD/elements/gp.php?GROUP=last-30-days&FORMAT=tle");

        // Отключите проверку SSL-сертификата
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        // Укажите функцию обратного вызова для записи данных в переменную
        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Выполните HTTP-запрос
        res = curl_easy_perform(curl);

        // Проверка на ошибки
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        else {
            // Сохраните данные в текстовый файл
            std::ofstream outFile("output.txt");
            outFile << response;
            outFile.close();
        }

        // Освободите ресурсы
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return 0;
}
