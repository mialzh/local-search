#include "lemmatizer.hpp"

#include <api.h>              // интерфейс и общие константы
#include <rus.h>              // создание русского анализатора
#include <eng.h>              // создание английского анализатора
#include <moonycode/codes.h>   // преобразование кодировок

#include <algorithm>          // sort, unique
#include <stdexcept>          // исключения