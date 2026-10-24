#pragma once

#include <QString>

#include <utility>
#include <variant>

namespace FloStudio::Core {

// Something that went wrong, in a form the screens can show: `code` picks
// the translated message (stage 3), `detail` carries the original text (a
// path, a command's message). Codes are listed where they are returned.
struct Error
{
    QString code;
    QString detail;

    bool operator==(const Error &) const = default;
};

// A value or an Error. Header-only (a template), so nothing crosses the DLL
// boundary but the two types it holds.
template<typename T>
class Result
{
public:
    Result(T value) // NOLINT(google-explicit-constructor): return a value directly
        : m_data(std::move(value))
    {
    }
    Result(Error error) // NOLINT(google-explicit-constructor): or an Error
        : m_data(std::move(error))
    {
    }

    bool ok() const { return std::holds_alternative<T>(m_data); }
    explicit operator bool() const { return ok(); }

    // Only call these after checking ok().
    const T &value() const & { return std::get<T>(m_data); }
    T &value() & { return std::get<T>(m_data); }
    T &&value() && { return std::get<T>(std::move(m_data)); }
    const Error &error() const { return std::get<Error>(m_data); }

    const T *operator->() const { return &value(); }
    const T &operator*() const & { return value(); }

private:
    std::variant<T, Error> m_data;
};

// For functions that return nothing on success.
struct Done
{
    bool operator==(const Done &) const = default;
};

} // namespace FloStudio::Core
