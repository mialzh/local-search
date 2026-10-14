# Готовый фронт Local Search

В архиве обычные исходники, без патча:

- web/index.html — страница;
- web/styles.css — оформление;
- web/app.mjs — интерфейс, загрузка и поиск;
- web/api.mjs — запросы к твоим ручкам;
- web/favicon.svg — значок сайта.

Расширение .mjs означает обычный JavaScript-модуль. Для фронта не нужны
npm-пакеты, Node.js, React или отдельная сборка.

Скопируй содержимое архива в корень репозитория local-search:
файлы из web попадут в твою существующую папку web.

В архиве также есть src/main.cpp и configs/static_config.yaml. Они добавляют
компоненты FsCache и HttpHandlerStatic, чтобы userver отдавал web/ по адресу /.
Эти два файла подготовлены на основе main коммита c6989a2. Если ты изменял
их после этого коммита, перенеси только следующие добавления в свои файлы.

В src/main.cpp добавлены заголовки:

```cpp
#include <userver/components/fs_cache.hpp>
#include <userver/server/handlers/http_handler_static.hpp>
```

В конце цепочки component_list, после UploadHandler, добавлены:

```cpp
.Append<userver::components::FsCache>("fs-cache-web")
.Append<userver::server::handlers::HttpHandlerStatic>();
```

В секцию components YAML добавлены fs-cache-web, handler-static и
USERVER_FILES_CONTENT_TYPE_MAP внутри dynamic-config.defaults.
Полные настройки есть в configs/static_config.yaml из архива.

## Запуск

Команды выполнять из корня проекта в том же окружении, где ты собирал userver:

```bash
cmake -S . -B build-server -DBUILD_SERVER=ON -DBUILD_TESTING=OFF
cmake --build build-server --target local_search --parallel
./build-server/local_search -c configs/static_config.yaml
```

Предыдущий процесс на порту 8080 нужно остановить перед новым запуском.
Открой http://localhost:8080 в браузере. Не открывай HTML-файл напрямую.

## Что работает

Загрузка .txt через POST /documents; несколько файлов отправляются по очереди.
Поиск через GET /search, выбор лимита и подсветка слов. Индикатор соединения
использует /health. Результаты и документы берутся с настоящего сервера.

Список загрузок в интерфейсе показывает только текущую сессию страницы,
а сами документы сохраняются на сервере и участвуют в поиске после перезапуска.

## Проверки

13 тестов клиента API прошли. Для их запуска нужен только Node.js 20+:

```bash
node --test tests/web/api.test.mjs
```

Проверка настоящего сервера и выдачи фронта:

```bash
python3 tests/integration/http_smoke.py --binary build-server/local_search
```

Полная сборка userver, HTTP-тест и визуальная проверка в браузере в окружении
подготовки не выполнялись: userver и браузерный инструмент отсутствуют.
