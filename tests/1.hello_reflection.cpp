#include <experimental/meta>
#include <print>
#include <string_view>

struct Point {
    int    x;
    int    y;
    double z;
};

int main() {
    constexpr auto ctx = std::meta::access_context::current();
    std::println("members of Point:");
    template for (constexpr auto member :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^Point, ctx))) {
        std::println("  {} : {}", std::meta::identifier_of(member),
                     std::meta::display_string_of(std::meta::type_of(member)));
    }

    // ---------------------------------

    constexpr std::meta::info info = ^^Point;
    typename[:info:] p             = {.x = 1, .y = 2, .z = 2.1};

    return 0;
}
