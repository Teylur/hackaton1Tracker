# Kazaki Car Racing — MVP детектора (хакатон «Автогонки»)

Прототип CV-системы для кейса: определение болида, трассы и колёс на кадрах (YOLO → ONNX). Дальше — трекинг, траектория, выезд за пределы трассы, UI для судьи.

## Требования

- **CMake** ≥ 3.18, компилятор с **C++17**
- **OpenCV 4**
- **ONNX Runtime** в `third_party/onnxruntime/` (заголовки + `lib/`)

### ONNX Runtime по платформе

В `third_party/onnxruntime/lib/` должны лежать библиотеки **той ОС, на которой собираете**:

| ОС | Файлы в `third_party/onnxruntime/lib/` |
|----|----------------------------|
| Ubuntu | `libonnxruntime.so*` (CPU-пакет [releases](https://github.com/microsoft/onnxruntime/releases)) |
| Windows | `onnxruntime.lib`, `onnxruntime.dll` (пакет `win-x64`) |

Сейчас в репозитории может быть только Linux-набор `.so` — для сборки на Windows замените содержимое `lib/` на Windows-версию (версия 1.16.x совместима с заголовками в `include/`).

## Сборка

Запускайте команды из **корня репозитория**.

### Ubuntu

```bash
sudo apt install build-essential cmake pkg-config libopencv-dev libglfw3-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Windows (PowerShell, MSVC)

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

OpenCV: установите через [vcpkg](https://github.com/microsoft/vcpkg) или укажите `OpenCV_DIR` при конфигурации:

```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Запуск

1. Положите модель: `models/best.onnx`
2. Положите тестовые кадры: `data/images/test/*.jpg` (или `.png`)
3. Запуск из корня проекта:

```bash
./build/yolo_image_test
```

### GUI (ассистент судьи)

```bash
sudo apt install libglfw3-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/racing_gui data/videos/test.mp4 models/best.onnx
```

Аргументы (все опциональны):

```text
yolo_image_test [images_dir] [model_path] [output_dir]
```

Пример:

```bash
./build/yolo_image_test data/images/test models/best.onnx output/detections
```

Результаты — в `output/detections/*_detected.jpg`.

## Структура (текущая и план)

```
kazaki-car_racing/
├── src/                  # исходники
├── include/              # заголовки
├── config/               # конфиги
├── models/               # best.onnx (локально, не в git)
├── third_party/onnxruntime/
├── data/                 # кадры, видео, телеметрия
├── output/detections/    # результаты детекции
├── scripts/
├── docs/STRUCTURE.md     # отчёт по структуре
└── build/
```

Подробнее: [docs/STRUCTURE.md](docs/STRUCTURE.md).

Планируемое развитие под кейс:

- обработка **видео** и потоков;
- **трекинг** болида и восстановление **траектории**;
- детекция **выезда** за границы трассы (класс `track` + геометрия);
- интеграция **телеметрии**;
- **UI** для просмотра эпизодов судьёй;
- конфиги под разные трассы (Moscow Raceway, КазаньРинг и др.).

UI:
libs - https://vk.com/away.php?to=https%3A%2F%2Fgithub.com%2Focornut%2Fimgui%2Farchive%2Frefs%2Fheads%2Fmaster.zip&utf=1
glfw - 
sudo apt update
sudo apt install libglfw3-dev libglew-dev
