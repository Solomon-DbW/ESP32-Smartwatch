# include/app_state.h

## Purpose

This is a convenience header that pulls in the shared state modules used by the watch UI and runtime logic.

## Syntax overview

```c
#pragma once

#include "config.h"
#include "stopwatch.h"
#include "music_remote.h"
```

## What it provides

It brings together header declarations for:

- global display state from `config.h`
- stopwatch state from `stopwatch.h`
- music remote state from `music_remote.h`

This reduces the need to include many headers separately in each file.

## Why it exists

The codebase is modular, and the app uses global runtime variables across multiple modules. Instead of each file including every dependency manually, `app_state.h` acts as a central umbrella header for shared state.
