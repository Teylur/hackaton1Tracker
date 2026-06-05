# Сторонние зависимости

## onnxruntime/

Заголовки (`include/`) можно держать в репозитории. Бинарники (`lib/*.so`, `lib/*.dll`) — **локально**, в git не попадают.

Скачайте [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases) под свою ОС и распакуйте `lib/` в `third_party/onnxruntime/lib/`.
