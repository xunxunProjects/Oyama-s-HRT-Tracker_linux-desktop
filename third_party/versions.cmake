# Pinned third-party sources. Update a version and its SHA-256 together.
# The hashes are filled from the official checksum files (or computed once
# from the official URL) and must never be edited by hand without a version
# change.

set(OYAMA_QT6_VERSION "6.4.3")
set(OYAMA_QT6_URL "https://download.qt.io/archive/qt/6.4/6.4.3/single/qt-everywhere-src-6.4.3.tar.xz")
set(OYAMA_QT6_SHA256 "29a7eebdbba0ea57978dea6083709c93593a60f0f3133a3de08b9571ee8eaab4")
set(OYAMA_QT6_PATCHES "")

set(OYAMA_QT5_VERSION "5.15.2")
set(OYAMA_QT5_URL "https://download.qt.io/archive/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.tar.xz")
set(OYAMA_QT5_SHA256 "3a530d1b243b5dec00bc54937455471aaa3e56849d2593edb8ded07228202240")
# The KDE Qt5 patch collection is optional and is pinned by Task 0.2.
set(OYAMA_QT5_PATCHES "patches/qt5-kde-5.15.2.tar.gz")

set(OYAMA_PG_VERSION "14.18")
set(OYAMA_PG_URL "https://ftp.postgresql.org/pub/source/v14.18/postgresql-14.18.tar.bz2")
set(OYAMA_PG_SHA256 "83ab29d6bfc3dc58b2ed3c664114fdfbeb6a0450c4b8d7fa69aee91e3ca14f8e")

set(OYAMA_SODIUM_VERSION "1.0.20")
set(OYAMA_SODIUM_URL "https://download.libsodium.org/libsodium/releases/libsodium-1.0.20.tar.gz")
set(OYAMA_SODIUM_SHA256 "ebb65ef6ca439333c2bb41a0c1990587288da07f6c7fd07cb3a18cc18d30ce19")

set(OYAMA_JSON_VERSION "3.11.3")
set(OYAMA_JSON_URL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz")
set(OYAMA_JSON_SHA256 "d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d")

set(OYAMA_XEN_VERSION "4.17.5")
set(OYAMA_XEN_URL "https://downloads.xenproject.org/release/xen/4.17.5/xen-4.17.5.tar.gz")
set(OYAMA_XEN_SHA256 "b1224df08cbe5706bd721be95e6d617e53c4a9bd5b61f42354561146375b722c")
