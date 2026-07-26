// A representative name from each family the tables cover, value-level and type-level. Not every
// entry — the point is that each *group* reaches a real standard component, so a name that quietly
// stopped resolving (a rename, a CPO that no longer applies) shows up here rather than in user code.
#include <tacit/_.hpp>

#include <atomic>
#include <bitset>
#include <cassert>
#include <chrono>
#include <complex>
#include <filesystem>
#include <forward_list>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <span>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

using tacit::_;

int main() {
  // ---- diagnostics ----
  {
    std::runtime_error e{"boom"};
    assert(std::string(_.what()(e)) == "boom");
    std::error_code ec = std::make_error_code(std::errc::invalid_argument);
    assert(!_.message()(ec).empty());
    assert(_.value()(ec) != 0);
    (void)_.category()(ec);
  }

  // ---- filesystem::path — the projection-shaped family ----
  {
    std::filesystem::path p{"/tmp/report.txt"};
    assert(_.extension()(p) == ".txt");
    assert(_.stem()(p) == "report");
    assert(_.filename()(p) == "report.txt");
    assert(_.parent_path()(p) == "/tmp");
    assert(_.is_absolute()(p));
    assert(!_.is_relative()(p));
    assert(_.has_extension()(p));
    assert(_.string()(p) == "/tmp/report.txt");
    (void)_.generic_string()(p);
    (void)_.lexically_normal()(p);
    (void)_.relative_path()(p);
    (void)_.root_path()(p);
  }

  // ---- string_view / string ----
  {
    std::string_view sv{"prefix-body"};
    auto trim = _.remove_prefix(7);
    trim(sv);
    assert(sv == "body");
  }

  // ---- associative: try_emplace ----
  {
    std::map<int, std::string> m;
    _.try_emplace(1, "one")(m);
    assert(m.at(1) == "one");
  }

  // ---- forward_list splicing family ----
  {
    std::forward_list<int> fl{2, 3};
    auto it = _.before_begin()(fl);
    fl.insert_after(it, 1);
    assert(fl.front() == 1);
  }

  // ---- bitset ----
  {
    std::bitset<8> b{0b1010};
    assert(_.test(1)(b));
    assert(_.any()(b));
    assert(!_.all()(b));
    assert(!_.none()(b));
    assert(_.count()(b) == 2);
    assert(_.to_string()(b).size() == 8);
  }

  // ---- numeric / chrono ----
  {
    std::complex<double> c{3.0, 4.0};
    assert(_.real()(c) == 3.0);
    assert(_.imag()(c) == 4.0);
    auto tp = std::chrono::steady_clock::now();
    (void)_.time_since_epoch()(tp);
    std::chrono::seconds s{5};
    assert(_.count()(s) == 5);
  }

  // ---- span ----
  {
    std::vector<int> v{1, 2, 3, 4};
    std::span sp{v};
    assert(_.size_bytes()(sp) == 4 * sizeof(int));
    assert(_.subspan(2)(sp).size() == 2);
  }

  // ---- streams ----
  {
    std::ostringstream os;
    os << "x";
    _.flush()(os);
    assert(_.good()(os) && !_.fail()(os) && !_.bad()(os) && !_.eof()(os));
    (void)_.rdbuf()(os);
  }

  // ---- concurrency ----
  {
    std::atomic<int> a{1};
    assert(_.load()(a) == 1);
    _.store(5)(a);
    assert(a.load() == 5);
    assert(_.fetch_add(2)(a) == 5 && a.load() == 7);
    assert(_.exchange(0)(a) == 7);
    std::thread t{[] {}};
    assert(_.joinable()(t));
    _.join()(t);
  }

  // ---- regex ----
  {
    std::string text{"ab12cd"};
    std::smatch m;
    std::regex_search(text, m, std::regex{"[0-9]+"});
    assert(_.ready()(m));
    assert(_.position(0)(m) == 2);
    assert(_.prefix()(m) == "ab");
    assert(_.suffix()(m) == "cd");
  }

  // ---- non-range containers still reach size/empty through the CPOs ----
  {
    std::stack<int> st;
    st.push(1);
    assert(_.size()(st) == 1);
    assert(!_.empty()(st));
  }

  // ---- type-level: the noun table ----
  {
    static_assert(std::is_same_v<_::rep::of<std::chrono::seconds>,
                                 std::chrono::seconds::rep>);  // rep is long on LP64, long long elsewhere
    static_assert(std::is_same_v<_::period::of<std::chrono::seconds>, std::ratio<1>>);
    static_assert(std::is_same_v<_::container_type::of<std::stack<int>>, std::deque<int>>);
    static_assert(std::is_same_v<_::deleter_type::of<std::unique_ptr<int>>,
                                 std::default_delete<int>>);
    static_assert(std::is_same_v<_::weak_type::of<std::shared_ptr<int>>, std::weak_ptr<int>>);
    static_assert(std::is_same_v<_::hasher::of<std::unordered_map<int, int>>, std::hash<int>>);
    static_assert(std::is_same_v<_::key_equal::of<std::unordered_map<int, int>>,
                                 std::equal_to<int>>);
    static_assert(std::is_same_v<_::iterator_category::of<std::vector<int>::iterator>,
                                 std::random_access_iterator_tag>);
    static_assert(std::is_same_v<_::string_type::of<std::filesystem::path>, std::string>);
    static_assert(std::is_same_v<_::pos_type::of<std::char_traits<char>>, std::streampos>);
    static_assert(std::is_same_v<_::node_type::of<std::map<int, int>>,
                                 std::map<int, int>::node_type>);
    static_assert(std::is_same_v<_::id::of<std::thread>, std::thread::id>);
    static_assert(std::is_same_v<_::clock::of<std::chrono::steady_clock::time_point>,
                                 std::chrono::steady_clock>);
  }

  return 0;
}
