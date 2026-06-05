# Отчёт: базовая структура проекта

**Дата:** 2026-06-04  
**Задача:** разнести код, модели и тяжёлые файлы; подготовить каркас под кейс «Автогонки».

## Что сделано

### 1. Каркас каталогов

```
kazaki-car_racing/
├── src/                    # исходный код (сейчас main.cpp)
├── include/                # заголовки (будущие модули)
├── config/                 # конфиги трасс, пороги детекции
├── models/                 # ML-модели (gitignore)
├── third_party/            # внешние зависимости (onnxruntime)
├── data/                   # входные данные (gitignore)
│   ├── images/test/
│   ├── videos/
│   └── telemetry/
├── output/                 # результаты работы (gitignore)
│   ├── detections/
│   └── logs/
├── scripts/                # скрипты сборки/запуска
├── docs/                   # документация
├── build/                  # cmake-сборка (gitignore)
├── CMakeLists.txt
└── README.md
```

### 2. Модель отделена от кода

- Перенесено: `src/weights/best.onnx` → `models/best.onnx`
- Удалены пустые `src/weights/` и корневая `weights/`
- Дефолтный путь в `main.cpp`: `models/best.onnx`
- Добавлен `models/README.md` с инструкцией

### 3. Тяжёлые файлы вынесены из git

| Что | Где лежит | В git |
|-----|-----------|-------|
| ONNX-модель | `models/` | нет (`*.onnx`) |
| ORT бинарники | `third_party/onnxruntime/lib/` | нет (`.so`, `.dll`) |
| Датасеты, видео | `data/` | нет |
| Результаты | `output/` | нет |
| Сборка | `build/` | нет |

Заголовки ONNX Runtime (`third_party/onnxruntime/include/`) остаются в репозитории — они лёгкие и нужны для сборки/IntelliSense.

### 4. Обновлены пути в проекте

- `CMakeLists.txt` → `third_party/onnxruntime`
- `main.cpp` → вывод в `output/detections/` вместо `results/`
- `.gitignore` — правила с сохранением `.gitkeep` и `README.md` в пустых папках

## Что положить локально после clone

1. `models/best.onnx` — веса YOLO (уже на месте у разработчика)
2. `third_party/onnxruntime/lib/` — библиотеки ORT под свою ОС
3. `data/images/test/` — тестовые кадры
4. При необходимости: `data/videos/`, `data/telemetry/`

## Следующие шаги (не делались в этой задаче)

- Разбить `main.cpp` на модули в `src/` + `include/`
- Видеопайплайн, трекинг, детекция выезда за трассу
- Конфиги трасс в `config/`
- UI для судьи
