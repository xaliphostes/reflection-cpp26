#include <experimental/meta>
#include <print>
#include <string_view>

struct Point {
    int    x;
    int    y;
    double z;
    double norm() const { return 0; }
    Point scale(double) const { return Point(); }
};

// Keep regular and static methods; drop constructors/destructors and
// the compiler-generated copy/move specials.
consteval bool is_exportable_member_function(std::meta::info fn) {
    return std::meta::is_function(fn) && !std::meta::is_constructor(fn) &&
           !std::meta::is_destructor(fn) && !std::meta::is_special_member_function(fn);
}

int main() {
    constexpr auto ctx = std::meta::access_context::current();

    // -------------------------------------------------------------------------------------

    constexpr auto fields =
        std::define_static_array(std::meta::nonstatic_data_members_of(^^Point, ctx));

    template for (constexpr auto f : fields) {
        std::println("  {} : {}", std::meta::identifier_of(f),
                     std::meta::display_string_of(std::meta::type_of(f)));
    }

    // -------------------------------------------------------------------------------------

    constexpr auto members = std::define_static_array(std::meta::members_of(^^Point, ctx));

    template for (constexpr auto m : members) {
        if constexpr (is_exportable_member_function(m)) {
            std::print("  {} {}(", std::meta::display_string_of(std::meta::return_type_of(m)),
                       std::meta::identifier_of(m));

            bool first = true;
            template for (constexpr auto param :
                          std::define_static_array(std::meta::parameters_of(m))) {
                if (!first) {
                    std::print(", ");
                }
                first = false;
                std::print("{}", std::meta::display_string_of(std::meta::type_of(param)));
            }
            std::println(")");
        }
    }
}
