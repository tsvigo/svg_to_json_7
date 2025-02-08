#include "dialog.h"
#include "ui_dialog.h"
#//---------------------------------------------------------------------------------------------------------------
#include <sstream>
#include <RoundedDouble.h>
#include <cctype>
#include <cstdlib>
//#include <iostream>
#include <fstream>


#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#//----------------------------------------------------------------------------------------------------
#//-----------------------------------------------------------------------------------------------------------------
QString stroka="M 2.5705738,0.29890394 V 5.2607093 l 5.0215861,0.239123 0.1793425,-5.14114758 z";

#//---------------------------------------------------------------------------------------------------------------- \ \
    // Функция для преобразования строки в double (можно заменить std::stod)
RoundedDouble parseDouble(const std::string &s) {
    return std::stod(s);
}
#//--------------------------------------------------------------------------------------------------------------

Dialog::Dialog(
    QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    ui->setupUi(this);
}

Dialog::~Dialog()
{
    delete ui;
}

void Dialog::on_pushButton_clicked()
{
#//----------------------------------------------------------------------------------------------------------

    // Диалог выбора файла с фильтром SVG
    QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                    "Выберите SVG файл",
                                                    "",
                                                    "SVG файлы (*.svg)");
    if (fileName.isEmpty()) {
        qDebug() << "Файл не выбран.";
      //  return 0;
    }

    // Открываем файл для чтения
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Не удалось открыть файл:" << fileName;
      //  return 1;
    }

    // путь к файлу, полученный из диалога
    QFileInfo fileInfo(fileName);
    QString folderPath = fileInfo.absolutePath(); // полный путь к папке



    // Считываем всё содержимое файла
    QTextStream in(&file);
    QString fileContent = in.readAll();
    file.close();

    // Используем регулярное выражение для поиска атрибута d внутри тега <path ...>
    // Шаблон ищет тег <path ... d="..." ...>
    QRegularExpression regex("<path[^>]*\\sd\\s*=\\s*\"([^\"]+)\"");
    QRegularExpressionMatch match = regex.match(fileContent);

    QString stroka;  // переменная для сохранения извлечённого пути
    if (match.hasMatch()) {
        stroka = match.captured(1);
        qDebug() << "Извлечённый путь:" << stroka;
    } else {
        qDebug() << "Атрибут d в теге <path> не найден.";
    }

#//------------------------------------------------------------------------------------------------------- \
    // запись иксов:
    std::string path = stroka.toStdString();
    std::istringstream iss(path);
    std::string token;
    char currentCommand = '\0';
    // Абсолютные текущие координаты
    RoundedDouble last_x = 0.0, last_y = 0.0;
    // Для команды Z (закрытия контура) можно запомнить начальную точку подконтура
    RoundedDouble start_x = 0.0, start_y = 0.0;
    bool firstMove = true; // для команды M/m первая координата – стартовая

    std::cout << "Абсолютные X-координаты:" << std::endl;

    // Поток для записи в файл
    std::ofstream outFile(folderPath.toStdString()+"/x_coordinates.txt");
    if (!outFile) {
        std::cerr << "Не удалось открыть файл для записи." << std::endl;
        //  return 1;
    }


    // Основной цикл: считываем токен за токеном (токен – слово, разделённое пробелами)
    while (iss >> token) {
        // Если токен начинается с буквы, это команда
        if (!token.empty() && std::isalpha(token[0])) {
            currentCommand = token[0];
            // Если команда Z или z, то возвращаемся к начальному положению подконтура
            if (currentCommand == 'Z' || currentCommand == 'z') {
                last_x = start_x;
                last_y = start_y;
                std::cout << last_x << " ";
                outFile << last_x << std::endl;
            }
            continue; // далее переходим к следующему токену
        }

        // Обработка координат в зависимости от команды
        if (currentCommand == 'M' || currentCommand == 'm' ||
            currentCommand == 'L' || currentCommand == 'l') {
            // Эти команды ожидают пару координат (x,y)
            // Токен может выглядеть как "18.196295,7.9448611"
            size_t commaPos = token.find(',');
            RoundedDouble x, y;
            if (commaPos != std::string::npos) {
                std::string xs = token.substr(0, commaPos);
                std::string ys = token.substr(commaPos + 1);
                x = parseDouble(xs);
                y = parseDouble(ys);
            } else {
                // Если запятой нет, считаем, что это x, а y – следующий токен
                x = parseDouble(token);
                if (!(iss >> token)) break; // если нет y, выходим
                y = parseDouble(token);
            }
            // Если команда относительная (строчная буква), прибавляем предыдущие координаты
            if (std::islower(currentCommand)) {
                x += last_x;
                y += last_y;
            }
            last_x = x;
            last_y = y;
            // Если это первая команда перемещения, запоминаем стартовую точку подконтура
            if (firstMove && (currentCommand == 'M' || currentCommand == 'm')) {
                start_x = x;
                start_y = y;
                firstMove = false;
            }
            // Выводим абсолютное значение X
            std::cout << last_x << " ";
            outFile << last_x << std::endl;
        }
        else if (currentCommand == 'H' || currentCommand == 'h') {
            // Команда H/h: только одна координата для X
            RoundedDouble x = parseDouble(token);
            if (std::islower(currentCommand)) {
                x += last_x;
            }
            last_x = x;
            std::cout << last_x << " ";
            outFile << last_x << std::endl;
        }
        else if (currentCommand == 'V' || currentCommand == 'v') {
            // Команда V/v: задаётся только Y, X остаётся прежним.
            RoundedDouble y = parseDouble(token);
            if (std::islower(currentCommand)) {
                y += last_y;
            }
            last_y = y;
            // Выводим текущее (не изменённое) значение X
            std::cout << last_x << " ";
            outFile << last_x << std::endl;
        }
        else {
            // Для других команд (например, C, S, Q, A и т.д.) можно добавить соответствующую обработку,
            // либо пропустить их, если нас интересуют только команды M, L, H, V.
            // Для простоты пропускаем неизвестные команды.
        }
    }
    outFile.close();
    std::cout << std::endl;
    std::cout << "X-координаты записаны в файл x_coordinates.txt" << std::endl;
    //                    return 0;
    //                }
    ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////
    // запись игреков:
    // Поток для чтения из строки
    std::istringstream iss2(path);
    // Поток для записи в файл y-координат
    std::ofstream outFile2(folderPath.toStdString()+"/y_coordinates.txt");
    if (!outFile2) {
        std::cerr << "Не удалось открыть файл для записи." << std::endl;
        // return 1;
    }

    std::string token2;
    char currentCommand2 = '\0';
    // Абсолютные текущие координаты
    RoundedDouble last_x2 = 0.0, last_y2 = 0.0;
    // Для команды Z запоминаем начальную точку подконтура
    RoundedDouble start_x2 = 0.0, start_y2 = 0.0;
    bool firstMove2 = true; // для команды M/m первая координата – стартовая
    std::cout << "Абсолютные Y-координаты:" << std::endl;
    // Основной цикл: считываем токен за токеном
    while (iss2 >> token2) {
        // Если токен начинается с буквы, это команда
        if (!token2.empty() && std::isalpha(token2[0])) {
            currentCommand2 = token2[0];
            // Для команды Z/z возвращаемся к начальному положению подконтура
            if (currentCommand2 == 'Z' || currentCommand2 == 'z') {
                last_x2 = start_x2;
                last_y2 = start_y2;
                outFile2 << last_y2 << std::endl;
                std::cout << last_y2 << " ";
            }
            continue;
        }

        // Обработка координатных пар (M, m, L, l)
        if (currentCommand2 == 'M' || currentCommand2 == 'm' ||
            currentCommand2 == 'L' || currentCommand2 == 'l') {
            size_t commaPos = token2.find(',');
            RoundedDouble x, y;
            if (commaPos != std::string::npos) {
                // Токен вида "x,y"
                std::string xs = token2.substr(0, commaPos);
                std::string ys = token2.substr(commaPos + 1);
                x = parseDouble(xs);
                y = parseDouble(ys);
            } else {
                // Если запятой нет, предполагаем, что это x, а y – следующий токен
                x = parseDouble(token2);
                if (!(iss2 >> token2)) break;
                y = parseDouble(token2);
            }
            // Если команда относительная (строчная буква) – прибавляем предыдущие координаты
            if (std::islower(currentCommand2)) {
                x += last_x2;
                y += last_y2;
            }
            last_x2 = x;
            last_y2 = y;
            // Если это первая команда перемещения, запоминаем стартовую точку подконтура
            if (firstMove2 && (currentCommand2 == 'M' || currentCommand2 == 'm')) {
                start_x2 = x;
                start_y2 = y;
                firstMove2 = false;
            }
            // Записываем абсолютное значение Y в файл
            outFile2 << last_y2 << std::endl;
            std::cout << last_y2 << " ";
        }
        else if (currentCommand2 == 'H' || currentCommand2 == 'h') {
            // Команда H/h: задаётся только X‑координата, Y остаётся прежним
            RoundedDouble x = parseDouble(token2);
            if (std::islower(currentCommand2)) {
                x += last_x2;
            }
            last_x2 = x;
            // Записываем текущее значение Y (оно не менялось)
            outFile2 << last_y2 << std::endl;
            std::cout << last_y2 << " ";
        }
        else if (currentCommand2 == 'V' || currentCommand2 == 'v') {
            // Команда V/v: задаётся только Y‑координата
            RoundedDouble y = parseDouble(token2);
            if (std::islower(currentCommand2)) {
                y += last_y2;
            }
            last_y2 = y;
            outFile2 << last_y2 << std::endl;
            std::cout << last_y2 << " ";

        }
        else {
            // Для других команд (например, C, S, Q, A и т.д.) можно добавить обработку,
            // либо пропустить, если нас интересуют только координаты Y.
        }
    }

    outFile2.close();
    std::cout << "\n"<< "Y-координаты записаны в файл y_coordinates.txt" << std::endl;

#//------------------------------------------------------------------------------------------------------

}

