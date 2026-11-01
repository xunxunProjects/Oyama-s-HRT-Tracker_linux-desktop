#include <oyama/compat/optional.h>
#include <oyama/compat/string_view.h>
#include <oyama/compat/filesystem.h>
#include <oyama/compat/attributes.h>
#include <oyama/compat/make_unique.h>
#include <oyama/compat/algorithm.h>

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using oyama::optional;
using oyama::nullopt;
using oyama::string_view;
namespace fs = oyama::fs;

OYAMA_NODISCARD static int checked_value(int v) { return v; }

int main() {
    // optional: empty + value_or
    optional<int> o;
    assert(!o.has_value());
    assert(o.value_or(42) == 42);
    o = 7;
    assert(o.has_value() && *o == 7);
    assert(o.value() == 7);
    o.reset();
    assert(!o);

    // move-only optional
    optional<std::unique_ptr<int>> mo = oyama::make_unique<int>(5);
    assert(mo.has_value() && mo->get() && **mo == 5);
    optional<std::unique_ptr<int>> mo2 = std::move(mo);
    assert(mo2.has_value() && mo2->get() && **mo2 == 5);

    // nullopt assignment
    mo2 = nullopt;
    assert(!mo2.has_value());

    // emplace
    optional<std::string> os;
    os.emplace(3, 'a');
    assert(os.has_value() && *os == "aaa");

    // string_view
    string_view sv("hello world");
    assert(sv.size() == 11);
    assert(sv.substr(6) == string_view("world"));
    assert(sv.substr(0, 5) == string_view("hello"));
    assert(sv < string_view("zzz"));
    assert(string_view("abc") == string_view("abc"));
    assert(!string_view("abc").empty());

    // fs::path
    fs::path p = fs::temp_directory_path() / "oyama_compat_test";
    fs::remove_all(p);
    assert(!fs::exists(p));
    fs::create_directories(p);
    assert(fs::is_directory(p));
    fs::path child = p / "sub" / "file.txt";
    fs::create_directories(child.parent_path());
    assert(child.filename() == fs::path("file.txt"));
    assert(child.parent_path() == (p / "sub"));
    // directory_iterator
    {
        std::vector<std::string> names;
        for (auto e : fs::directory_iterator(p / "sub")) {
            (void)e;
        }
        (void)names;
    }
    fs::remove_all(p);
    assert(!fs::exists(p));

    // make_unique
    auto u = oyama::make_unique<std::vector<int>>(3, 1);
    assert(u->size() == 3 && (*u)[0] == 1);

    // clamp
    assert(oyama::clamp(5, 1, 3) == 3);
    assert(oyama::clamp(0, 1, 3) == 1);
    assert(oyama::clamp(2, 1, 3) == 2);

    // size / data / as_const
    int arr[4] = {1, 2, 3, 4};
    assert(oyama::size(arr) == 4);
    assert(oyama::data(arr) == arr);
    std::vector<int> v{10, 20};
    assert(oyama::size(v) == 2);
    const int& c = oyama::as_const(arr[0]);
    (void)c;

    // attributes
    OYAMA_MAYBE_UNUSED int x = 0;
    (void)x;
    assert(checked_value(1) == 1);

    std::cout << "compat_test: PASS\n";
    return 0;
}
