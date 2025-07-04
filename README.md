# ☁️ Cloud IaaS Service

Модульная облачная инфраструктурная платформа на C++20 для высокопроизводительных вычислений, балансировки нагрузки, кэширования, восстановления и безопасности.

---

## 🚀 Быстрый старт

```sh
# Сборка
cmake -B build
cmake --build build -j

# Запуск
./build/bin/CloudIaaS
```

- Конфигурация: `config/cloud_service.json`
- Скрипт запуска: `scripts/start_service.sh`

---

## 🧩 Архитектура

- **Ядра**: CoreKernel, MicroKernel, SmartKernel, ParentKernel, OrchestrationKernel, ComputationalKernel, ArchitecturalKernel, CryptoMicroKernel
- **Балансировка**: гибридные стратегии, метрики
- **Кэш**: динамический, предзагрузка, адаптация
- **Восстановление**: контрольные точки, сериализация, асинхронность
- **Безопасность**: политики, аудит, криптография
- **Потоки**: ThreadPool с оптимизациями под платформу

<details>
<summary>Подробнее об архитектуре</summary>

- [Общее описание](./PROJECT_OVERVIEW.md)
- [Ядра и компоненты](./KERNELS_AND_COMPONENTS.md)
- [Безопасность](./SECURITY_DESIGN.md)
- [Процессы и потоки](./PROCESS_FLOW.md)

</details>

---

## 🛠️ Технологии

- **C++20**, CMake
- spdlog, OpenSSL, Boost, nlohmann/json, zlib
- Поддержка: macOS ARM, Linux x64

---

## 📦 Структура

```
include/    # Заголовочные файлы (ядра, компоненты)
src/        # Исходный код
config/     # Конфигурация (JSON)
logs/       # Логи
recovery_points/ # Контрольные точки
scripts/    # Скрипты запуска и обслуживания
tests/      # Тесты
```

---

## 📝 Пример использования

```cpp
#include <core/kernel/base/MicroKernel.hpp>

cloud::core::kernel::MicroKernel kernel("example");
kernel.initialize();
// ... постановка задач, интеграция с кэшем и балансировщиком ...
kernel.shutdown();
```

---

## 📚 Документация

- [Архитектура и описание](./PROJECT_OVERVIEW.md)
- [Ядра и компоненты](./KERNELS_AND_COMPONENTS.md)
- [Безопасность](./SECURITY_DESIGN.md)
- [Процессы и потоки](./PROCESS_FLOW.md)

---

## ⚡ Контакты и вклад

- Вопросы и предложения — через Issues/Pull Requests
- Добро пожаловать к развитию проекта! 

---

<p align="center">
<pre>
             `         '
 ;,,,             `       '             ,,,;
 `YES8888bo.       :     :       .od8888YES'
   888IO8DO88b.     :   :     .d8888I8DO88
   8LOVEY'  `Y8b.   `   '   .d8Y'  `YLOVE8
  jTHEE!  .db.  Yb. '   ' .dY  .db.  8THEE!
    `888  Y88Y    `b ( ) d'    Y88Y  888'
     8MYb  '"        ,',        "'  dMY8
    j8prECIOUSgf"'   ':'   `"?g8prECIOUSk
      'Y'   .8'     d' 'b     '8.   'Y'
       !   .8' db  d'; ;`b  db '8.   !
          d88  `'  8 ; ; 8  `'  88b
         d88Ib   .g8 ',' 8g.   dI88b
        :888LOVE88Y'     'Y88LOVE888:
        '! THEE888'       `888THEE !'
           '8Y  `Y         Y'  Y8'
            Y                   Y
            !                   !
</pre>
</p> 