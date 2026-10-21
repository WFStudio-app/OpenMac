# Stickies.app (OpenMac, terminal edition)

Терминальный порт классического macOS **Stickies** — жёлтых стикеров на рабочем
столе. Каждое «стикер-окно» хранится как заметка в `~/.omac/Stickies/notes/`:

- `NAME.text` — содержимое стикера;
- `NAME.meta` — цвет, «пришпиленная» позиция (x/y), дата создания.

## Сборка и установка

```sh
make            # собирает ./stickies
make test       # 24 unit-теста
make install    # кладёт бинарь в ~/.omac/bin и создаёт bundle Stickies.app
```

Либо через общий инсталлер OpenMac: `bash ../../install.sh`.

## Использование

```sh
stickies new milk -c pink -t "Купить молоко"   # создать стикер
echo "заметка из stdin" | stickies new todo     # создать из stdin
stickies add milk "и яйца"                      # дописать строку
stickies show milk                              # показать цветной стикер
stickies list                                   # все стикеры
stickies open                                   # интерактивный просмотрщик
stickies edit milk < newtext.txt                # заменить содержимое
stickies color milk blue                        # сменить цвет
stickies move milk 100 200                      # «передвинуть» стикер
stickies rm milk                                # удалить
stickies clear                                  # удалить все
```

Цвета как в оригинале: `yellow pink blue green orange white purple`.

Без цвета (для pipe/тестов): `STICKIES_NO_COLOR=1` или вывод в pipe.
Корень хранения можно переопределить: `STICKIES_ROOT=/path`.

## Лицензия

Проприетарная (см. `../../LICENSE`). Код написан с нуля для OpenMac;
название и идея принадлежат Apple, это не аффилированный порт.
