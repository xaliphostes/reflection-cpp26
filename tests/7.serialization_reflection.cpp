#include <experimental/meta>
#include <print>
#include <string_view>

template <typename T> std::string to_json(const T &obj) {
    constexpr auto ctx = std::meta::access_context::current();

    std::string out   = "{";
    bool        first = true;
    template for (constexpr auto m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if (!first)
            out += ",";
        first = false;
        out += '"';
        out += std::meta::identifier_of(m);
        out += "\":";
        out += serialize_value(obj.[:m:]); // your per-type value formatter
    }
    out += "}";
    return out;
}

struct Point {
    int    x;
    int    y;
    double z;
    double norm() const { return 0; }
    Point  scale(double) const { return Point(); }
};

int main() {

    auto json = to_json(Point{.x = 1, .y = 2, .z = 1.9});

    return 0;
}