# Vendored Unity

`unity.c`, `unity.h`, `unity_internals.h` are the [ThrowTheSwitch/Unity]
test framework, vendored here so host unit tests build with no ESP-IDF
dependency. Sourced from `$IDF_PATH/components/unity/unity/src/` (ESP-IDF v5.5).

Do not edit these files. To update, re-copy from a newer ESP-IDF checkout.

> This is an independent copy from the one in `firmware-c6/tests/host/unity/`;
> each firmware project keeps its host suite self-contained. Keep both in sync
> when bumping the Unity version.
