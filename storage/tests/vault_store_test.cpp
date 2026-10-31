#include "oyama/storage/connection.h"
#include "oyama/storage/passphrase.h"
#include "oyama/storage/vault_store.h"

#include "oyama/vault.h"

#include <libpq-fe.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

const char* env_value(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : nullptr;
}

bool run_sql(PGconn* connection, const char* sql) {
    PGresult* result = PQexec(connection, sql);
    if (result == nullptr) {
        std::cerr << "SQL failed: " << PQerrorMessage(connection) << '\n';
        return false;
    }
    const ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::cerr << "SQL failed: " << sql << '\n'
                  << PQresultErrorMessage(result) << '\n';
        PQclear(result);
        return false;
    }
    PQclear(result);
    return true;
}

void test_passphrase() {
    const std::vector<std::string> words = oyama::storage::Passphrase::generate(7);
    check(words.size() == 7, "generate seven words");
    for (const std::string& word : words) {
        check(!word.empty(), "generated word is not empty");
        check(word == oyama::storage::Passphrase::normalize(word),
              "generated word is normalized");
    }
    check(oyama::storage::Passphrase::normalize("  Correct\tHORSE  battery ") ==
              "correct horse battery",
          "normalize phrase");
    check(oyama::storage::Passphrase::join(words) ==
              oyama::storage::Passphrase::normalize(
                  oyama::storage::Passphrase::join(words)),
          "joined phrase is normalized");

    const std::vector<std::size_t> positions =
        oyama::storage::Passphrase::challenge(7, 3);
    check(positions.size() == 3, "challenge has three positions");
    std::vector<std::size_t> sorted = positions;
    std::sort(sorted.begin(), sorted.end());
    check(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end(),
          "challenge positions are distinct");
    for (std::size_t position : positions) {
        check(position < 7, "challenge position in range");
    }

    const std::string phrase = "alpha beta gamma delta epsilon zeta eta";
    check(oyama::storage::Passphrase::verify_challenge(
              phrase, {0, 2, 6}, {"alpha", "gamma", "eta"}),
          "challenge answers accepted");
    check(!oyama::storage::Passphrase::verify_challenge(
              phrase, {0, 2, 6}, {"alpha", "wrong", "eta"}),
          "challenge wrong answer rejected");
}

void test_conninfo() {
    oyama::storage::ConnectionConfig config;
    config.socket_dir = "/tmp/a b";
    config.dbname = "oyama_hrt";
    config.user = "tester";
    config.port = 5433;
    config.application_name = "vault_store_test";
    config.connect_timeout_seconds = 5;

    const std::string conninfo = config.to_conninfo();
    check(conninfo.find("host='/tmp/a b'") != std::string::npos,
          "conninfo quotes socket path with space");
    check(conninfo.find("port='5433'") != std::string::npos,
          "conninfo includes port");
    check(conninfo.find("dbname='oyama_hrt'") != std::string::npos,
          "conninfo includes dbname");
    check(conninfo.find("user='tester'") != std::string::npos,
          "conninfo includes user");
}

}  // namespace

int main() {
    const char* socket_dir = env_value("OYAMA_TEST_PG_SOCKET");
    const char* dbname = env_value("OYAMA_TEST_PG_DB");
    const char* user = env_value("OYAMA_TEST_PG_USER");
    const char* port = env_value("OYAMA_TEST_PG_PORT");

    if (socket_dir == nullptr || dbname == nullptr || user == nullptr ||
        port == nullptr) {
        std::cout << "SKIP: OYAMA_TEST_PG_* is not set\n";
        return 77;
    }

    oyama::storage::ConnectionConfig config;
    config.socket_dir = socket_dir;
    config.dbname = dbname;
    config.user = user;
    config.port = std::stoi(port);
    config.application_name = "vault_store_test";
    config.connect_timeout_seconds = 5;

    PGconn* admin = PQconnectdb(config.to_conninfo().c_str());
    if (admin == nullptr || PQstatus(admin) != CONNECTION_OK) {
        std::cerr << "admin connection failed: " << PQerrorMessage(admin)
                  << '\n';
        if (admin != nullptr) {
            PQfinish(admin);
        }
        return 1;
    }
    if (!run_sql(admin, "TRUNCATE vault_meta, vault_records")) {
        PQfinish(admin);
        return 1;
    }

    test_conninfo();
    test_passphrase();

    const std::string phrase = "correct horse battery staple";
    oyama::storage::VaultStore store(config);
    store.open();
    check(store.state() == oyama::storage::VaultState::Uninitialized,
          "fresh store is uninitialized");
    store.setup(phrase, {1, 8 * 1024 * 1024});
    check(store.state() == oyama::storage::VaultState::Locked,
          "setup leaves store locked");

    try {
        store.unlock("wrong phrase");
        check(false, "wrong phrase throws");
    } catch (const oyama::storage::VaultError& error) {
        check(error.code() == OYAMA_VAULT_ERR_BAD_PASSPHRASE,
              "wrong phrase error code");
    }

    store.unlock(phrase);
    check(store.state() == oyama::storage::VaultState::Unlocked,
          "unlock leaves store unlocked");
    check(store.is_unlocked(), "is_unlocked");

    const oyama::storage::VaultRecord settings{
        "00000000-0000-0000-0000-000000000001", "settings", 7,
        R"({"weight":60})"};
    store.put(settings);
    const auto loaded = store.get(settings.id);
    check(loaded.has_value(), "get returns record");
    if (loaded.has_value()) {
        check(loaded->kind == settings.kind, "get kind");
        check(loaded->updated_at_ms == settings.updated_at_ms, "get stamp");
        check(loaded->payload == settings.payload, "get payload");
    }

    store.put({"00000000-0000-0000-0000-000000000002", "dose_event", 8,
               R"({"doseMG":2})"});
    const auto settings_only = store.scan("settings");
    check(settings_only.size() == 1, "scan filters kind");
    const auto all = store.scan();
    check(all.size() == 2, "scan returns all records");
    check(store.remove("00000000-0000-0000-0000-000000000002"),
          "remove existing");
    check(!store.remove("00000000-0000-0000-0000-000000000002"),
          "remove missing");

    store.lock();
    check(store.state() == oyama::storage::VaultState::Locked,
          "lock leaves store locked");
    try {
        (void)store.get(settings.id);
        check(false, "locked get throws");
    } catch (const oyama::storage::VaultError& error) {
        check(error.code() == OYAMA_VAULT_ERR_LOCKED, "locked get code");
    }

    store.unlock(phrase);
    store.clear();
    check(store.scan().empty(), "clear removes records");

    PQfinish(admin);

    if (failures != 0) {
        std::cerr << "vault_store_test: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "vault_store_test: ok\n";
    return 0;
}
