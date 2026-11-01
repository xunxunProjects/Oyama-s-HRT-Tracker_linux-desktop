# Patches

Pinned patches live here and are applied by
`scripts/bootstrap-third-party.sh` with `patch -p1 --forward`. A patch path is
declared by `OYAMA_<DEP>_PATCHES` in `third_party/versions.cmake`.

## Qt 5.15.2 KDE patch collection

- Source: KDE Qt5 patch collection (https://invent.kde.org/qt/qt/qt5)
- Version: 5.15.2
- Upstream commit: to be pinned in Phase A (Task A.5)
- Apply: `patch -p1 --forward`

The default build uses Qt 6.4.3 and does not need this collection. The Qt 5.15.2
compatibility build (`OYAMA_QT_MAJOR=5`) does. Do not describe Qt 5.15.2 as
receiving upstream open-source security fixes; the patch collection is a
downstream maintenance path, not an upstream LTS.
