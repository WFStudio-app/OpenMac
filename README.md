# OpenMac

**Перенос MacOS в Termux.**

OpenMac — это проект, цель которого принести максимальное количество функций и ощущений MacOS на Android-устройство через [Termux](https://termux.dev/). Проект **полностью переписывает** часть компонентов (оболочка, пакеты, конфигурации) под окружение Termux (Android / Linux / `bionic` libc), но местами **копирует кучу кода** из upstream-проектов с необходимыми адаптациями.

> ⚠️ Это не настоящий MacOS. Здесь нет ядра XNU, бинарных драйверов Apple или закрытых фреймворков. OpenMac воссоздаёт пользовательский опыт (CLI-инструменты, структуру файловой системы, стиль конфигураций, утилиты вроде `mdfind`, `pbcopy/pbpaste`, `open`, `say` и т.д.) средствами Termux.

## Зачем

- Termux даёт полноценную POSIX-среду на Android, но в ней нет привычных macOS-инструментов.
- Многие скрипты и туториалы написаны под macOS (`brew`, `switchover`, `launchctl`, `defaults`...).
- OpenMac позволяет запускать эти workflows на телефоне/планшете без root.

## Архитектура

```
OpenMac/
├── bin/            # Переписанные CLI-утилиты (brew-совместимый менеджер, say, open...)
├── lib/            # Общие библиотеки для Termux (bionic, proot, pkg)
├── etc/            # Конфиги в стиле macOS (defaults, launchd-lite)
├── packages/       # Рецепты "формулы" для установки пакетов в Termux
├── scripts/        # Инсталлеры и хуки окружения
└── docs/           # Документация
```

### Что переписано с нуля
- Оболочка окружения и профили (`.zshrc`, `.profile`) в стиле macOS.
- Менеджер пакетов `omac` (вдохновлён Homebrew, работает поверх `pkg`/`apt` Termux).
- Утилита `defaults` (эмуляция `~/Library/Preferences` поверх JSON).
- `launchd-lite` — простой планировщик задач на `cron`/`run-parts`.

### Что скопировано/адаптировано
- Часть скриптов и формул взята из открытых проектов (Homebrew Cask, macos-cli-tools и др.) и переработана под Termux. См. секцию **Атрибуция**.

## Установка (для Termux)

```bash
pkg update -y
pkg install git curl -y
git clone https://github.com/WFStudio-app/OpenMac.git
cd OpenMac
./install.sh
```

После установки:

```bash
source ~/.omac/env
omac --version
```

## Использование

| Команда | Назначение | Аналог macOS |
|---------|-----------|--------------|
| `omac install <formula>` | Установить пакет | `brew install` |
| `omac list` | Список установленных | `brew list` |
| `defaults write <domain> <key> <value>` | Сохранить настройку | `defaults` |
| `say "текст"` | Озвучить текст (TTS Android) | `say` |
| `open <файл/url>` | Открыть через Intent Android | `open` |
| `pbcopy` / `pbpaste` | Буфер обмена через Termux-api | `pbcopy/pbpaste` |

## Требования

- Android 7.0+
- Termux (из F-Droid, не из Play Store)
- Termux:API (опционально, для `say`, `open`, буфера обмена)

## Статус проекта

🚧 Активная разработка. Userspace-shim (Mach-style API поверх Linux) собирается и проходит смоук- и стресс-тесты; продолжается портирование формул.

## Документация

- [CHANGELOG.md](CHANGELOG.md) — история изменений
- [docs/architecture.md](docs/architecture.md) — архитектура шима
- [docs/building.md](docs/building.md) — сборка и установка
- [docs/api.md](docs/api.md) — справочник по `mach.h` и C++ слою
- [docs/usage.md](docs/usage.md) — использование CLI-утилит

## Атрибуция

Проект использует фрагменты кода из следующих открытых источников (лицензии сохранены):
- [Homebrew](https://brew.sh) — BSD 2-clause
- [Termux packages](https://github.com/termux/termux-packages) — Apache 2.0
- Другие — см. заголовки файлов в `packages/`.

Если ваш код попал сюда без указания авторства — откройте issue, мы исправим.

## Лицензия

Проприетарная лицензия — All Rights Reserved (см. `LICENSE`). Копирование, модификация и распространение без письменного разрешения правообладателя запрещены.

## Лицензии

- Основной код проекта: проприетарный, All Rights Reserved (`LICENSE`).
- `LICENSE.APSL`: текст Apple Public Source License 2.0 (из официального `apple/darwin-xnu`) — приложен как справочный, покрывает потенциальные будущие компоненты на базе APSL-кода.
- `osfmk/`: userspace Darwin-shim, оригинальный код (проприетарная лицензия). Это НЕ ядро XNU и не содержит кода Apple.
