# Название исполняемого файла
TARGET = log_parser

# Компилятор
CC = gcc

# Флаги компиляции
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -pthread

# Исходный файл
SRC = log_parser.c

# Исполняемый файл
BIN = $(TARGET)

# Цель по умолчанию
all: $(BIN)

# Сборка программы
$(BIN): $(SRC)
    $(CC) $(CFLAGS) -o $(BIN) $(SRC)

# Очистка временных файлов и исполняемого файла
clean:
    rm -f $(BIN)

# Установка программы (копирование в /usr/local/bin)
install: $(BIN)
    cp $(BIN) /usr/local/bin/

# Удаление установленной программы
uninstall:
    rm -f /usr/local/bin/$(BIN)

.PHONY: all clean install uninstall