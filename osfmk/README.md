# osfmk/ — Darwin-shim layer (NOT a kernel)

Здесь находится **пользовательский shim**, воспроизводящий *форму API* Mach/XNU
(`host_self`, `host_info`, `vm_allocate`, `task_for_pid`) поверх Linux-системных
вызовов Android. Компилируется в `libopenmach.so`, работает в Termux без root.

## Честно о происхождении кода
- В этом репозитории **нет ни одной строки** из ядра XNU (`apple/darwin-xnu`).
  Копировать его код нельзя: XNU распространяется по APSL 2.0, которая
  несовместима с проприетарной лицензией OpenMac и запрещает relicensing производных работ.
- Воспроизведён только публичный API-контракт (имена типов/функций), что
  является функциональным интерфейсом, а не объектом авторского права.
- Файл `LICENSE.APSL` — эталонная копия Apple Public Source License v2.0,
  скачана из официального репозитория `apple/darwin-xnu`. Она приложена для
  справки и покроет любые будущие изменения, которые реально будут использовать
  APSL-код Apple. Текущий код репозитория — оригинальный, под проприетарной лицензией OpenMac.

## Сборка и проверка
```sh
gcc -shared -fPIC osfmk/mach_shim.c -o libopenmach.so
tools/sysctl_darwin.sh -a   # darwin-style sysctl поверх /proc
```
