# Calculator.app (terminal edition)

Функциональный аналог штатного macOS **Calculator.app**, работающий в терминале
(Termux/Android и любой POSIX tty). Это оригинальный код OpenMac — не порт и не
производная от Apple исходников; воспроизводится только пользовательское
поведение (Basic/Scientific режимы, история вычислений).

## Сборка

```sh
make            # -> ./calculator
make test       # self-tests
make install    # -> ~/.omac/bin/calculator (+ .app bundle layout)
```

## Использование

| Команда | Что делает |
|---|---|
| `calculator` | интерактивный REPL (Basic) |
| `calculator --sci` | REPL с научными функциями |
| `calculator -e "2+2*2"` | вычислить одно выражение → `6` |
| `calculator "2^10"` | то же, короткая форма → `1024` |
| `calculator --history [n]` | последние n вычислений (панель History) |
| `calculator --clear-history` | очистить историю |

В REPL: `_` подставляет предыдущий ответ (`2+3` → `5`, затем `_*2` → `10`),
команды `history`, `clear`, `quit`.

## Поддерживаемый синтаксис

- Операторы: `+ - * / % ^` (приоритет как в macOS: `^` правассоциативен)
- Функции Scientific-режима: `sqrt cbrt abs ln log log2 exp sin cos tan
  asin acos atan sinh cosh tanh round floor ceil fact`
- Константы: `pi`, `e`, `tau`
- Ошибки указываются кареткой на колонку проблемы, как в macOS при неверном вводе

История хранится в `~/.omac/Calculator/history` (TSV: дата, выражение, результат).
