FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Запуск conan как раньше
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing

# Папка data больше не нужна
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www

# Копируем билд артефакт и данные
COPY --from=build /app/build/bin/game_server /app/game_server
COPY ./data /app/data
COPY ./static /app/static

# Устанавливаем владельца для скопированных файлов
RUN chown -R www:www /app

USER www

# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static"] 