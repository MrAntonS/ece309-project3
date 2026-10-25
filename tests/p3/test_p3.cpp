// ECE 309 P3 tests: hash registry, workspace containment, stack calculator.
#include "../mini_test.h"
#include "../test_helpers.h"

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue schema_;
};

class CountingTool : public NamedTool {
public:
    CountingTool(std::string name, int* counter) : NamedTool(std::move(name)), counter_(counter) {}
    ~CountingTool() override { ++*counter_; }
private:
    int* counter_;
};

bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

void check_expression(const std::string& expr, double expected) {
    double v = 0;
    std::string error;
    REQUIRE(eval(expr, &v, &error));
    CHECK(std::abs(v - expected) < 1e-9);
}

void accepted(const Workspace& ws, std::string_view input, const fs::path& suffix) {
    fs::path out;
    std::string why = "preexisting error";
    REQUIRE(ws.resolve(input, &out, &why));
    CHECK(out == (suffix.empty() ? ws.root() : ws.root() / suffix));
    CHECK(why.empty());
}

void rejected(const Workspace& ws, std::string_view input) {
    fs::path out;
    std::string why;
    CHECK(!ws.resolve(input, &out, &why));
    CHECK(!why.empty());
}
}  // namespace

// ---------------------------------------------------------------- Registry
TEST(reg_empty_and_missing) {
    ToolRegistry r;
    CHECK_EQ(r.size(), std::size_t{0});
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK_EQ(r.max_probe_length(), std::size_t{0});
    CHECK_EQ(r.load_factor(), 0.0);
    CHECK(r.find("not_there") == nullptr);
}

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();
    REQUIRE(r.add(std::move(t)));
    CHECK_EQ(r.size(), std::size_t{1});
    CHECK(r.find("alpha") == raw);
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK_EQ(r.size(), std::size_t{1});
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK_EQ(r.size(), std::size_t{0});
    CHECK_EQ(r.max_probe_length(), std::size_t{0});
}

TEST(reg_ownership_and_duplicate_cleanup) {
    int destroyed = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountingTool>("one", &destroyed)));
        REQUIRE(r.add(std::make_unique<CountingTool>("two", &destroyed)));
        CHECK(!r.add(std::make_unique<CountingTool>("one", &destroyed)));
        CHECK_EQ(destroyed, 1);  // Rejected duplicate destroyed immediately.
        CHECK_EQ(r.size(), std::size_t{2});
    }
    CHECK_EQ(destroyed, 3); // Both stored tools destroyed by registry.
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(r.add(std::make_unique<NamedTool>("gamma")));
    int a = 0, b = 0, c = 0, total = 0;
    r.for_each([&](const Tool& tool) {
        ++total;
        if (tool.name() == "alpha") ++a;
        if (tool.name() == "beta") ++b;
        if (tool.name() == "gamma") ++c;
    });
    CHECK_EQ(total, 3);
    CHECK_EQ(a, 1);
    CHECK_EQ(b, 1);
    CHECK_EQ(c, 1);
}

TEST(reg_hash_order_sensitive) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1") != ToolRegistry::hash("tool_2"));
    CHECK(ToolRegistry::hash("abcXdef") != ToolRegistry::hash("abcYdef"));
}

TEST(reg_rehash_pointer_stability) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("original");
    Tool* raw = t.get();
    REQUIRE(r.add(std::move(t)));
    const auto first_buckets = r.bucket_count();
    for (int i = 0; i < 300; ++i)
        REQUIRE(r.add(std::make_unique<NamedTool>("extra_" + std::to_string(i))));
    CHECK(r.bucket_count() > first_buckets);
    CHECK(r.find("original") == raw);
    CHECK_EQ(r.size(), std::size_t{301});
    CHECK(r.load_factor() <= 0.75);
}

TEST(reg_10000_tools_and_alternative_hash) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("tool_0");
    Tool* raw = first.get();
    REQUIRE(r.add(std::move(first)));
    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
        CHECK(r.load_factor() <= 1.0);
    }
    CHECK_EQ(r.size(), std::size_t{10000});
    CHECK(r.find("tool_0") == raw);
    for (int i = 0; i < 10000; ++i)
        CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    CHECK(r.max_probe_length() <= 64);
    CHECK(r.load_factor() <= 0.75);

    // Same names, same final bucket count, but intentionally weak hash.
    std::vector<int> weak_chains(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        const std::size_t weak_hash = static_cast<unsigned char>(name[0]) + name.size();
        ++weak_chains[weak_hash % weak_chains.size()];
    }
    int weak_longest = 0;
    for (int len : weak_chains)
        if (len > weak_longest) weak_longest = len;
    CHECK(weak_longest > static_cast<int>(r.max_probe_length()));
    std::printf("10,000 tools | FNV-1a chaining: buckets=%zu, load=%.6f, max_chain=%zu | weak hash max_chain=%d\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), weak_longest);
}

// -------------------------------------------------------------- Workspace
TEST(ws_accepts_relative_file) {
    TempDir d;
    Workspace ws(d.path());
    accepted(ws, "notes/todo.txt", fs::path("notes/todo.txt"));
}
TEST(ws_accepts_normalized_slashes) {
    TempDir d;
    Workspace ws(d.path());
    accepted(ws, "./notes//todo.txt", fs::path("notes/todo.txt"));
}
TEST(ws_accepts_dot_as_root) {
    TempDir d;
    Workspace ws(d.path());
    accepted(ws, ".", fs::path{});
}
TEST(ws_accepts_middle_dotdot) {
    TempDir d;
    Workspace ws(d.path());
    accepted(ws, "a/../b", fs::path("b"));
}
TEST(ws_accepts_collapsing_to_root) {
    TempDir d;
    Workspace ws(d.path());
    accepted(ws, "a/..", fs::path{});
}
TEST(ws_accepts_nonexisting_path_without_creating_it) {
    TempDir d;
    Workspace ws(d.path());
    const fs::path suffix("new/dir/file.txt");
    CHECK(!fs::exists(ws.root() / suffix));
    accepted(ws, "new/dir/file.txt", suffix);
    CHECK(!fs::exists(ws.root() / suffix));
}
TEST(ws_accepts_dots_in_filename) {
    TempDir d;
    Workspace ws(d.path());
    accepted(ws, "my..notes.txt", fs::path("my..notes.txt"));
}
TEST(ws_rejects_empty) {
    TempDir d;
    Workspace ws(d.path());
    rejected(ws, "");
}
TEST(ws_rejects_nul) {
    TempDir d;
    Workspace ws(d.path());
    const std::string with_nul = std::string("notes") + '\0' + "/x";
    rejected(ws, with_nul);
}
TEST(ws_rejects_absolute_path) {
    TempDir d;
    Workspace ws(d.path());
    rejected(ws, "/etc/passwd");
    rejected(ws, ws.root().string() + "/inside.txt");
}
TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    accepted(ws, "notes/todo.txt", fs::path("notes/todo.txt"));
    rejected(ws, "../etc/passwd");
}
TEST(ws_rejects_parent_only) {
    TempDir d;
    Workspace ws(d.path());
    rejected(ws, "..");
}
TEST(ws_rejects_double_parent) {
    TempDir d;
    Workspace ws(d.path());
    rejected(ws, "a/../../b");
}
TEST(ws_rejects_escape_and_reentry) {
    TempDir d;
    Workspace ws(d.path());
    rejected(ws, "notes/../../ws/notes/todo.txt");
}

// ------------------------------------------------------------- Calculator
TEST(calc_precedence) { check_expression("2 + 3 * 4", 14.0); }
TEST(calc_left_to_right_minus) { check_expression("10 - 4 - 3", 3.0); }
TEST(calc_left_to_right_division) { check_expression("100 / 10 / 5", 2.0); }
TEST(calc_parentheses) { check_expression("(2 + 3) * 4", 20.0); }
TEST(calc_mixed_parentheses) { check_expression("2 * (3 + 4) - 5", 9.0); }
TEST(calc_negative_numbers) { check_expression("-3 * -3", 9.0); }
TEST(calc_decimal_division) { check_expression("7 / 2", 3.5); }
TEST(calc_nested_precedence) { check_expression("(9 - 3) / (1 + 2) + 5 * 2", 12.0); }
TEST(calc_extra_expression) { check_expression("8 / 4 * 3 + (2 - 7)", 1.0); }
TEST(calc_division_by_zero) {
    double value = 42.0;
    std::string error;
    CHECK(!eval("1 / (2 - 2)", &value, &error));
    CHECK_EQ(error, std::string("division by zero"));
}
TEST(calc_long_expression_iterative) {
    std::string expr = "1";
    for (int i = 1; i < 20000; ++i) expr += "+1";
    check_expression(expr, 20000.0);
}
TEST(calc_deeply_nested_parentheses_iterative) {
    std::string expr(5000, '(');
    expr += "42";
    expr += std::string(5000, ')');
    check_expression(expr, 42.0);
}
TEST(calc_empty_token_stack_error) {
    std::vector<CalcToken> tokens;
    double value = 0;
    std::string error;
    CHECK(!calc_evaluate(tokens, &value, &error));
    CHECK(!error.empty());
}
TEST(calc_malformed_token_stack_error) {
    std::vector<CalcToken> tokens{{CalcToken::Kind::Plus, 0.0}};
    double value = 0;
    std::string error;
    CHECK(!calc_evaluate(tokens, &value, &error));
    CHECK(!error.empty());
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
