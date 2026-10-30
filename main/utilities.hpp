#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

template <typename type, std::size_t cap> struct inplace_vector {
    std::array<type, cap> storage{};
    std::size_t           count{};

    inplace_vector() = default;

    explicit inplace_vector(std::size_t const new_count) : count(new_count) {
        assert(new_count <= cap);
    }

    inplace_vector(std::size_t const new_count, type const &value) : count(new_count) {
        assert(new_count <= cap);
        for (std::size_t index{}; index < new_count; ++index)
            emplace_at_(index, value);
    }

    inplace_vector(std::initializer_list<type> init) {
        assert(init.size() <= cap);
        for (auto const &value : init)
            emplace_at_(count++, value);
    }

    auto push_back(type const &value) {
        assert(count < cap);
        emplace_at_(count, value);
        ++count;
    }

    // Removed slots are reset so resources are released and stale values never leak back.
    auto pop_back() {
        assert(count > 0);
        --count;
        emplace_at_(count);
    }

    template <typename... args> auto emplace_back(args &&...values) -> type & {
        assert(count < cap);
        emplace_at_(count, std::forward<args>(values)...);
        return storage[count++];
    }

    auto resize(std::size_t const new_count) {
        assert(new_count <= cap);

        for (std::size_t index{count}; index < new_count; ++index)
            emplace_at_(index);

        for (std::size_t index{new_count}; index < count; ++index)
            emplace_at_(index);

        count = new_count;
    }

    auto resize(std::size_t const new_count, type const &value) {
        assert(new_count <= cap);

        for (std::size_t index{count}; index < new_count; ++index)
            emplace_at_(index, value);

        for (std::size_t index{new_count}; index < count; ++index)
            emplace_at_(index);

        count = new_count;
    }

    auto clear() {
        for (std::size_t index{}; index < count; ++index)
            emplace_at_(index);

        count = 0;
    }

    [[nodiscard]] auto operator[](std::size_t const index) -> type & {
        assert(index < count);
        return storage[index];
    }

    [[nodiscard]] auto operator[](std::size_t const index) const -> type const & {
        assert(index < count);
        return storage[index];
    }

    [[nodiscard]] auto at(std::size_t const index) -> type & {
        assert(index < count);
        return storage[index];
    }

    [[nodiscard]] auto at(std::size_t const index) const -> type const & {
        assert(index < count);
        return storage[index];
    }

    [[nodiscard]] auto begin() -> type * {
        return storage.data();
    }

    [[nodiscard]] auto end() -> type * {
        return storage.data() + count;
    }

    [[nodiscard]] auto begin() const -> type const * {
        return storage.data();
    }

    [[nodiscard]] auto end() const -> type const * {
        return storage.data() + count;
    }

    [[nodiscard]] auto rbegin() {
        return std::reverse_iterator(end());
    }

    [[nodiscard]] auto rend() {
        return std::reverse_iterator(begin());
    }

    [[nodiscard]] auto rbegin() const {
        return std::reverse_iterator(end());
    }

    [[nodiscard]] auto rend() const {
        return std::reverse_iterator(begin());
    }

    [[nodiscard]] auto data() -> type * {
        return storage.data();
    }

    [[nodiscard]] auto data() const -> type const * {
        return storage.data();
    }

    [[nodiscard]] auto front() -> type & {
        assert(count > 0);
        return storage[0];
    }

    [[nodiscard]] auto front() const -> type const & {
        assert(count > 0);
        return storage[0];
    }

    [[nodiscard]] auto back() -> type & {
        assert(count > 0);
        return storage[count - 1];
    }

    [[nodiscard]] auto back() const -> type const & {
        assert(count > 0);
        return storage[count - 1];
    }

    [[nodiscard]] auto size() const {
        return count;
    }

    [[nodiscard]] static constexpr auto capacity() {
        return cap;
    }

    [[nodiscard]] auto empty() const {
        return count == 0;
    }

    [[nodiscard]] auto full() const {
        return count == cap;
    }

    [[nodiscard]] auto remaining() const {
        return cap - count;
    }

private:
    // Re-creates the element in place. Works for types that are not assignable
    // (atomics, mutexes, ...). Falls back to assignment when construction may throw,
    // because a throw after destroy_at would double-destroy the element.
    template <typename... args> auto emplace_at_(std::size_t const index, args &&...values) {
        auto &slot = storage[index];

        if constexpr (std::is_nothrow_constructible_v<type, args...>) {
            std::destroy_at(std::addressof(slot));
            std::construct_at(std::addressof(slot), std::forward<args>(values)...);
        } else {
            slot = type(std::forward<args>(values)...);
        }
    }
};

template <typename type, typename... rest> inplace_vector(type, rest...) -> inplace_vector<type, 1 + sizeof...(rest)>;

template <typename type, std::size_t cap1, std::size_t cap2>
auto operator==(inplace_vector<type, cap1> const &a, inplace_vector<type, cap2> const &b) {
    if (a.count != b.count)
        return false;

    for (std::size_t index{}; index < a.count; ++index)
        if (!(a[index] == b[index]))
            return false;

    return true;
}

template <std::size_t cap, typename char_type = char> struct inplace_basic_string {
    using traits = std::char_traits<char_type>;
    using view   = std::basic_string_view<char_type>;

    static constexpr std::size_t npos{view::npos};

    std::array<char_type, cap + 1> storage{};
    std::size_t                    count{};

    inplace_basic_string() = default;

    inplace_basic_string(std::size_t const new_count, char_type const value) {
        assign(new_count, value);
    }

    explicit inplace_basic_string(char_type const *text) {
        assign(view{text});
    }

    explicit inplace_basic_string(view const text) {
        assign(text);
    }

    inplace_basic_string(std::initializer_list<char_type> init) {
        assign(view{init.begin(), init.size()});
    }

    template <std::input_iterator iterator> inplace_basic_string(iterator first, iterator last) {
        for (; first != last; ++first)
            push_back(*first);
    }

    explicit operator view() const noexcept {
        return view{storage.data(), count};
    }

    auto assign(view const text) -> inplace_basic_string & {
        assert(text.size() <= cap);
        traits::move(storage.data(), text.data(), text.size());
        count = text.size();
        terminate_();
        return *this;
    }

    auto assign(std::size_t const new_count, char_type const value) -> inplace_basic_string & {
        assert(new_count <= cap);
        traits::assign(storage.data(), new_count, value);
        count = new_count;
        terminate_();
        return *this;
    }

    auto operator=(view const text) -> inplace_basic_string & {
        assign(text);
        return *this;
    }

    auto operator=(char_type const *text) -> inplace_basic_string & {
        assign(view{text});
        return *this;
    }

    auto operator=(char_type const value) -> inplace_basic_string & {
        assign(1, value);
        return *this;
    }

    // Like std::string, operator[] permits index == size() (the terminator).
    [[nodiscard]] auto operator[](std::size_t const index) -> char_type & {
        assert(index <= count);
        return storage[index];
    }

    [[nodiscard]] auto operator[](std::size_t const index) const -> char_type const & {
        assert(index <= count);
        return storage[index];
    }

    [[nodiscard]] auto at(std::size_t const index) -> char_type & {
        assert(index < count);
        return storage[index];
    }

    [[nodiscard]] auto at(std::size_t const index) const -> char_type const & {
        assert(index < count);
        return storage[index];
    }

    [[nodiscard]] auto front() -> char_type & {
        assert(count > 0);
        return storage[0];
    }

    [[nodiscard]] auto front() const -> char_type const & {
        assert(count > 0);
        return storage[0];
    }

    [[nodiscard]] auto back() -> char_type & {
        assert(count > 0);
        return storage[count - 1];
    }

    [[nodiscard]] auto back() const -> char_type const & {
        assert(count > 0);
        return storage[count - 1];
    }

    [[nodiscard]] auto data() -> char_type * {
        return storage.data();
    }

    [[nodiscard]] auto data() const -> char_type const * {
        return storage.data();
    }

    [[nodiscard]] auto c_str() const -> char_type const * {
        return storage.data();
    }

    [[nodiscard]] auto begin() -> char_type * {
        return storage.data();
    }

    [[nodiscard]] auto end() -> char_type * {
        return storage.data() + count;
    }

    [[nodiscard]] auto begin() const -> char_type const * {
        return storage.data();
    }

    [[nodiscard]] auto end() const -> char_type const * {
        return storage.data() + count;
    }

    [[nodiscard]] auto cbegin() const -> char_type const * {
        return begin();
    }

    [[nodiscard]] auto cend() const -> char_type const * {
        return end();
    }

    [[nodiscard]] auto rbegin() {
        return std::reverse_iterator(end());
    }

    [[nodiscard]] auto rend() {
        return std::reverse_iterator(begin());
    }

    [[nodiscard]] auto rbegin() const {
        return std::reverse_iterator(end());
    }

    [[nodiscard]] auto rend() const {
        return std::reverse_iterator(begin());
    }

    [[nodiscard]] auto size() const {
        return count;
    }

    [[nodiscard]] auto length() const {
        return count;
    }

    [[nodiscard]] static constexpr auto capacity() {
        return cap;
    }

    [[nodiscard]] static constexpr auto max_size() {
        return cap;
    }

    [[nodiscard]] auto empty() const {
        return count == 0;
    }

    [[nodiscard]] auto full() const {
        return count == cap;
    }

    [[nodiscard]] auto remaining() const {
        return cap - count;
    }

    auto clear() {
        count = 0;
        terminate_();
    }

    auto push_back(char_type const value) {
        assert(count < cap);
        storage[count++] = value;
        terminate_();
    }

    auto pop_back() {
        assert(count > 0);
        --count;
        terminate_();
    }

    template <typename... args> auto emplace_back(args &&...values) -> char_type & {
        assert(count < cap);
        auto &slot = storage[count++];
        slot       = char_type(std::forward<args>(values)...);
        terminate_();
        return slot;
    }

    auto resize(std::size_t const new_count, char_type const value = char_type{}) {
        assert(new_count <= cap);

        if (new_count > count)
            traits::assign(storage.data() + count, new_count - count, value);

        count = new_count;
        terminate_();
    }

    auto append(view const text) -> inplace_basic_string & {
        assert(text.size() <= remaining());
        traits::move(storage.data() + count, text.data(), text.size());
        count += text.size();
        terminate_();
        return *this;
    }

    auto append(std::size_t const append_count, char_type const value) -> inplace_basic_string & {
        assert(append_count <= remaining());
        traits::assign(storage.data() + count, append_count, value);
        count += append_count;
        terminate_();
        return *this;
    }

    auto operator+=(view const text) -> inplace_basic_string & {
        return append(text);
    }

    auto operator+=(char_type const *text) -> inplace_basic_string & {
        return append(view{text});
    }

    auto operator+=(char_type const value) -> inplace_basic_string & {
        return append(1, value);
    }

    // Note: `text` must not alias this string's own storage.
    auto insert(std::size_t const index, view const text) -> inplace_basic_string & {
        assert(index <= count);
        assert(text.size() <= remaining());
        traits::move(storage.data() + index + text.size(), storage.data() + index, count - index);
        traits::copy(storage.data() + index, text.data(), text.size());
        count += text.size();
        terminate_();
        return *this;
    }

    auto insert(std::size_t const index, std::size_t const insert_count, char_type const value)
        -> inplace_basic_string & {
        assert(index <= count);
        assert(insert_count <= remaining());
        traits::move(storage.data() + index + insert_count, storage.data() + index, count - index);
        traits::assign(storage.data() + index, insert_count, value);
        count += insert_count;
        terminate_();
        return *this;
    }

    auto erase(std::size_t const index = 0, std::size_t erase_count = npos) -> inplace_basic_string & {
        assert(index <= count);
        erase_count = std::min(erase_count, count - index);
        traits::move(storage.data() + index, storage.data() + index + erase_count, count - index - erase_count);
        count -= erase_count;
        terminate_();
        return *this;
    }

    // Note: `text` must not alias this string's own storage.
    auto replace(std::size_t const index, std::size_t replace_count, view const text) -> inplace_basic_string & {
        assert(index <= count);
        replace_count = std::min(replace_count, count - index);
        assert(count - replace_count + text.size() <= cap);
        traits::move(storage.data() + index + text.size(), storage.data() + index + replace_count,
            count - index - replace_count);
        traits::copy(storage.data() + index, text.data(), text.size());
        count = count - replace_count + text.size();
        terminate_();
        return *this;
    }

    [[nodiscard]] auto substr(std::size_t const position = 0, std::size_t const substr_count = npos) const
        -> inplace_basic_string {
        assert(position <= count);
        return inplace_basic_string{view{*this}.substr(position, substr_count)};
    }

    [[nodiscard]] auto compare(view const text) const -> int {
        return view{*this}.compare(text);
    }

    [[nodiscard]] auto starts_with(view const text) const {
        return view{*this}.starts_with(text);
    }

    [[nodiscard]] auto starts_with(char_type const value) const {
        return view{*this}.starts_with(value);
    }

    [[nodiscard]] auto ends_with(view const text) const {
        return view{*this}.ends_with(text);
    }

    [[nodiscard]] auto ends_with(char_type const value) const {
        return view{*this}.ends_with(value);
    }

    [[nodiscard]] auto contains(view const text) const {
        return view{*this}.contains(text);
    }

    [[nodiscard]] auto contains(char_type const value) const {
        return view{*this}.contains(value);
    }

    [[nodiscard]] auto find(view const text, std::size_t const position = 0) const {
        return view{*this}.find(text, position);
    }

    [[nodiscard]] auto find(char_type const value, std::size_t const position = 0) const {
        return view{*this}.find(value, position);
    }

    [[nodiscard]] auto rfind(view const text, std::size_t const position = npos) const {
        return view{*this}.rfind(text, position);
    }

    [[nodiscard]] auto rfind(char_type const value, std::size_t const position = npos) const {
        return view{*this}.rfind(value, position);
    }

    [[nodiscard]] auto find_first_of(view const characters, std::size_t const position = 0) const {
        return view{*this}.find_first_of(characters, position);
    }

    [[nodiscard]] auto find_last_of(view const characters, std::size_t const position = npos) const {
        return view{*this}.find_last_of(characters, position);
    }

    [[nodiscard]] auto find_first_not_of(view const characters, std::size_t const position = 0) const {
        return view{*this}.find_first_not_of(characters, position);
    }

    [[nodiscard]] auto find_last_not_of(view const characters, std::size_t const position = npos) const {
        return view{*this}.find_last_not_of(characters, position);
    }

    [[nodiscard]] friend auto operator==(inplace_basic_string const &a, view const b) {
        return view{a} == b;
    }

    [[nodiscard]] friend auto operator<=>(inplace_basic_string const &a, view const b) {
        return view{a} <=> b;
    }

private:
    auto terminate_() {
        storage[count] = char_type{};
    }
};

template <std::size_t cap1, std::size_t cap2, typename char_type>
[[nodiscard]] auto operator==(
    inplace_basic_string<cap1, char_type> const &a, inplace_basic_string<cap2, char_type> const &b) {
    return std::basic_string_view<char_type>{a} == std::basic_string_view<char_type>{b};
}

template <std::size_t cap1, std::size_t cap2, typename char_type>
[[nodiscard]] auto operator<=>(
    inplace_basic_string<cap1, char_type> const &a, inplace_basic_string<cap2, char_type> const &b) {
    return std::basic_string_view<char_type>{a} <=> std::basic_string_view<char_type>{b};
}

template <std::size_t cap1, std::size_t cap2, typename char_type>
[[nodiscard]] auto operator+(
    inplace_basic_string<cap1, char_type> const &a, inplace_basic_string<cap2, char_type> const &b) {
    inplace_basic_string<cap1 + cap2, char_type> result{std::basic_string_view<char_type>{a}};
    result += std::basic_string_view<char_type>{b};
    return result;
}

template <std::size_t cap> using inplace_string = inplace_basic_string<cap>;

template <std::size_t cap> using inplace_wstring = inplace_basic_string<cap, wchar_t>;

template <std::size_t cap> using inplace_u8string = inplace_basic_string<cap, char8_t>;

template <typename signature, std::size_t cap = sizeof(void *) * 4, std::size_t align = alignof(std::max_align_t)>
struct inplace_function;

template <typename ret, typename... args, std::size_t cap, std::size_t align>
struct inplace_function<ret(args...), cap, align> {
    using invoke_fn  = ret (*)(void *, args...);
    using copy_fn    = void (*)(void *, void const *);
    using move_fn    = void (*)(void *, void *) noexcept;
    using destroy_fn = void (*)(void *) noexcept;

    struct vtable {
        invoke_fn  invoke{};
        copy_fn    copy{};
        move_fn    move{};
        destroy_fn destroy{};
    };

    alignas(align) mutable std::array<std::byte, cap> storage{};
    vtable const *table{};

    template <typename callable>
    static constexpr bool fits{sizeof(callable) <= cap && alignof(callable) <= align && align % alignof(callable) == 0};

    inplace_function() = default;

    explicit inplace_function(std::nullptr_t) {}

    inplace_function(inplace_function const &other) {
        copy_from_(other);
    }

    inplace_function(inplace_function &&other) noexcept {
        move_from_(other);
    }

    template <typename callable, typename decayed = std::decay_t<callable>>
        requires(!std::is_same_v<decayed, inplace_function> && std::is_invocable_r_v<ret, decayed &, args...>)
    explicit inplace_function(callable &&value) {
        emplace<decayed>(std::forward<callable>(value));
    }

    ~inplace_function() {
        reset();
    }

    auto operator=(inplace_function const &other) -> inplace_function & {
        if (this != &other) {
            reset();
            copy_from_(other);
        }
        return *this;
    }

    auto operator=(inplace_function &&other) noexcept -> inplace_function & {
        if (this != &other) {
            reset();
            move_from_(other);
        }
        return *this;
    }

    auto operator=(std::nullptr_t) noexcept -> inplace_function & {
        reset();
        return *this;
    }

    template <typename callable, typename decayed = std::decay_t<callable>>
        requires(!std::is_same_v<decayed, inplace_function> && std::is_invocable_r_v<ret, decayed &, args...>)
    auto operator=(callable &&value) -> inplace_function & {
        emplace<decayed>(std::forward<callable>(value));
        return *this;
    }

    template <typename callable, typename... params> auto emplace(params &&...values) -> callable & {
        static_assert(fits<callable>, "callable does not fit in inplace_function storage");
        static_assert(std::is_nothrow_move_constructible_v<callable>, "callable must be nothrow move constructible");
        static_assert(std::is_invocable_r_v<ret, callable &, args...>, "callable is not invocable with this signature");

        // Build first, then reset: arguments may refer to the currently stored callable.
        callable temporary(std::forward<params>(values)...);

        reset();
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        auto *object = ::new (static_cast<void *>(storage.data())) callable(std::move(temporary));
        table        = table_for_<callable>();
        return *object;
    }

    auto reset() noexcept {
        if (table) {
            table->destroy(storage.data());
            table = nullptr;
        }
    }

    auto operator()(args... values) const -> ret {
        assert(table);
        return table->invoke(storage.data(), std::forward<args>(values)...);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return table != nullptr;
    }

    [[nodiscard]] auto empty() const noexcept {
        return table == nullptr;
    }

    [[nodiscard]] static constexpr auto capacity() {
        return cap;
    }

    [[nodiscard]] friend auto operator==(inplace_function const &f, std::nullptr_t) noexcept {
        return f.empty();
    }

private:
    template <typename callable> static auto object_(void *self) noexcept -> callable * {
        return std::launder(static_cast<callable *>(self));
    }

    template <typename callable> static auto invoke_impl_(void *self, args... values) -> ret {
        return std::invoke(*object_<callable>(self), std::forward<args>(values)...);
    }

    template <typename callable> static auto copy_impl_(void *destination, void const *source) {
        ::new (destination) callable(*std::launder(static_cast<callable const *>(source)));
    }

    template <typename callable> static auto move_impl_(void *destination, void *source) noexcept {
        ::new (destination) callable(std::move(*object_<callable>(source)));
    }

    template <typename callable> static auto destroy_impl_(void *self) noexcept {
        object_<callable>(self)->~callable();
    }

    template <typename callable> static constexpr auto copy_for_() -> copy_fn {
        if constexpr (std::is_copy_constructible_v<callable>)
            return &copy_impl_<callable>;
        else
            return nullptr;
    }

    template <typename callable> static auto table_for_() -> vtable const * {
        static constexpr vtable value{
            &invoke_impl_<callable>,
            copy_for_<callable>(),
            &move_impl_<callable>,
            &destroy_impl_<callable>,
        };
        return &value;
    }

    auto copy_from_(inplace_function const &other) {
        if (other.table == nullptr)
            return;

        // Copying a move-only callable is a logic error; fail loudly in release builds too
        // instead of calling a null function pointer.
        if (other.table->copy == nullptr)
            std::terminate();

        other.table->copy(storage.data(), other.storage.data());
        table = other.table;
    }

    auto move_from_(inplace_function &other) noexcept {
        if (other.table == nullptr)
            return;

        other.table->move(storage.data(), other.storage.data());
        table = other.table;
        other.reset();
    }
};