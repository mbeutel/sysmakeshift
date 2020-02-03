
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_BUFFER_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_BUFFER_HPP_


#include <limits>
#include <memory>       // for allocator_traits<>
#include <cstddef>      // for size_t, ptrdiff_t
#include <iterator>     // for input_iterator_tag, output_iterator_tag, random_access_iterator_tag
#include <type_traits>  // for common_type<>, integral_constant<>, enable_if<>, is_const<>, is_same<>, is_nothrow_default_constructible<>
#include <system_error> // for errc

#include <gsl-lite/gsl-lite.hpp> // for gsl_Expects()


namespace sysmakeshift {


namespace gsl = ::gsl_lite;


template <typename T, std::size_t Alignment, typename A>
class aligned_buffer;

template <typename T, std::size_t Alignment, typename A>
class aligned_row_buffer;


namespace detail {


template <typename T>
struct arithmetic_result
{
    T value;
    std::errc ec;
};

template <typename A, typename B>
constexpr arithmetic_result<std::common_type_t<A, B>> multiply_unsigned(A a, B b)
{
    // Borrowed from slowmath.

    using V = std::common_type_t<A, B>;

    if (b > 0 && a > std::numeric_limits<V>::max() / b) return { { }, std::errc::value_too_large };
    return { a * b, { } };
}

    // Computes ⌈x ÷ d⌉ ∙ d for x ∊ ℕ₀, d ∊ ℕ, d ≠ 0.
template <typename X, typename D>
constexpr arithmetic_result<std::common_type_t<X, D>> ceili(X x, D d)
{
    // Borrowed from slowmath.

    using V = std::common_type_t<X, D>;

        // We have the following identities:
        //
        //     x = ⌊x ÷ d⌋ ∙ d + x mod d
        //     ⌈x ÷ d⌉ = ⌊(x + d - 1) ÷ d⌋ = ⌊(x - 1) ÷ d⌋ + 1
        //
        // Assuming x ≠ 0, we can derive the form
        //
        //     ⌈x ÷ d⌉ ∙ d = x + d - (x - 1) mod d - 1 .

    if (x == 0) return { 0, { } };
    V dx = d - (x - 1) % d - 1;
    if (x > std::numeric_limits<V>::max() - dx) return { { }, std::errc::value_too_large };
    return { x + dx, { } };
}


template <typename T, typename A, typename... Ts>
void
construct_aligned_buffer(char* data, A alloc, std::size_t& numElementsConstructed, std::size_t size, std::size_t bytesPerElement,
    std::true_type /*isNothrowConstructible*/,
    Ts&&... args)
noexcept
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerElement), e = std::ptrdiff_t(size * bytesPerElement); i != e; i += d)
    {
        std::allocator_traits<A>::construct(alloc, reinterpret_cast<T*>(&data[i]), args...);
    }
    numElementsConstructed = size;
}
template <typename T, typename A, typename... Ts>
void
construct_aligned_buffer(char* data, A alloc, std::size_t& numElementsConstructed, std::size_t size, std::size_t bytesPerElement,
    std::false_type /*isNothrowConstructible*/,
    Ts&&... args)
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerElement), e = std::ptrdiff_t(size * bytesPerElement); i != e; i += d)
    {
        std::allocator_traits<A>::construct(alloc, reinterpret_cast<T*>(&data[i]), args...);
        ++numElementsConstructed;
    }
}

template <typename T, typename A>
void
destroy_aligned_buffer(char* data, A alloc, std::size_t size, std::size_t bytesPerElement)
noexcept
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerElement), e = std::ptrdiff_t(size * bytesPerElement); i != e; i += d)
    {
        std::allocator_traits<A>::destroy(alloc, reinterpret_cast<T*>(&data[i]));
    }
}


template <typename T, typename A, typename... Ts>
void
construct_aligned_row_buffer(char* data, A alloc, std::size_t& numElementsConstructed, std::size_t rows, std::size_t cols, std::size_t bytesPerRow,
    std::true_type /*isNothrowConstructible*/,
    Ts&&... args)
noexcept
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerRow), e = std::ptrdiff_t(rows * bytesPerRow), ncols = std::ptrdiff_t(cols); i != e; i += d)
    {
        for (std::ptrdiff_t j = 0; j != ncols; ++j)
        {
            std::allocator_traits<A>::construct(alloc, reinterpret_cast<T*>(&data[i + j * sizeof(T)]), args...);
        }
    }
    numElementsConstructed = rows * cols;
}
template <typename T, typename A, typename... Ts>
void
construct_aligned_row_buffer(char* data, A alloc, std::size_t& numElementsConstructed, std::size_t rows, std::size_t cols, std::size_t bytesPerRow,
    std::false_type /*isNothrowConstructible*/,
    Ts&&... args)
{
    numElementsConstructed = 0;
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerRow), e = std::ptrdiff_t(rows * bytesPerRow), ncols = std::ptrdiff_t(cols); i != e; i += d)
    {
        for (std::ptrdiff_t j = 0; j != ncols; ++j)
        {
            std::allocator_traits<A>::construct(alloc, reinterpret_cast<T*>(&data[i + j * sizeof(T)]), args...);
            ++numElementsConstructed;
        }
    }
}

template <typename T, typename A>
void
destroy_aligned_row_buffer(char* data, A alloc, std::size_t rows, std::size_t cols, std::size_t bytesPerRow)
noexcept
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerRow), e = std::ptrdiff_t(rows * bytesPerRow), ncols = std::ptrdiff_t(cols); i != e; i += d)
    {
        for (std::ptrdiff_t j = 0; j != ncols; ++j)
        {
            std::allocator_traits<A>::destroy(alloc, reinterpret_cast<T*>(&data[i + j * sizeof(T)]));
        }
    }
}
template <typename T, typename A>
void
destroy_aligned_row_buffer(char* data, A alloc, std::size_t rows, std::size_t cols, std::size_t bytesPerRow, std::size_t numElementsConstructed)
noexcept
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerRow), e = std::ptrdiff_t(rows * bytesPerRow), ncols = std::ptrdiff_t(cols); i != e; i += d)
    {
        for (std::ptrdiff_t j = 0; j != ncols; ++j)
        {
            if (numElementsConstructed == 0) return;
            --numElementsConstructed;
            std::allocator_traits<A>::destroy(alloc, reinterpret_cast<T*>(&data[i + j * sizeof(T)]));
        }
    }
}


template <typename T>
class aligned_buffer_iterator
{
    template <typename, std::size_t, typename> friend class sysmakeshift::aligned_buffer;

private:
    char* data_;
    std::size_t index_;
    std::size_t bytesPerElement_;

    aligned_buffer_iterator(char* _data, std::size_t _index, std::size_t _bytesPerElement)
        : data_(_data), index_(_index), bytesPerElement_(_bytesPerElement)
    {
    }

public:
    constexpr aligned_buffer_iterator(void) noexcept
        : data_(nullptr), index_(0), bytesPerElement_(0)
    {
    }

    aligned_buffer_iterator(aligned_buffer_iterator const&) = default;
    aligned_buffer_iterator& operator =(aligned_buffer_iterator const&) = default;

    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_buffer_iterator(aligned_buffer_iterator const& rhs) noexcept
        : data_(rhs.data_), index_(rhs.index_), bytesPerElement_(rhs.bytesPerElement_)
    {
    }
    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_buffer_iterator& operator =(aligned_buffer_iterator const& rhs) noexcept
    {
        data_ = rhs.data_;
        index_ = rhs.index_;
        bytesPerElement_ = rhs.bytesPerElement_;
    }

#ifdef __cpp_lib_concepts
    using iterator_concept  = std::random_access_iterator_tag;
#endif // __cpp_lib_concepts
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    friend bool operator ==(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        gsl_Expects(lhs.data_ == rhs.data_);

        return lhs.index_ == rhs.index_;
    }
    friend bool operator !=(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator <(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        gsl_Expects(lhs.data_ == rhs.data_);

        return lhs.index_ < rhs.index_;
    }
    friend bool operator >(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return rhs < lhs;
    }
    friend bool operator <=(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return !(rhs < lhs);
    }
    friend bool operator >=(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return !(lhs < rhs);
    }

    gsl_NODISCARD reference operator *(void) const
    {
        return *const_cast<T*>(reinterpret_cast<T const*>(data_ + index_ * bytesPerElement_));
    }
    gsl_NODISCARD pointer operator ->(void) const
    {
        return &**this;
    }
    aligned_buffer_iterator& operator ++(void)
    {
        ++index_;
        return *this;
    }
    aligned_buffer_iterator& operator --(void)
    {
        --index_;
        return *this;
    }
    aligned_buffer_iterator operator ++(int)
    {
        aligned_buffer_iterator result = *this;
        ++index_;
        return result;
    }
    aligned_buffer_iterator operator --(int)
    {
        aligned_buffer_iterator result = *this;
        --index_;
        return result;
    }
    aligned_buffer_iterator& operator +=(difference_type d)
    {
        index_ += d;
        return *this;
    }
    aligned_buffer_iterator operator +(difference_type d) const
    {
        aligned_buffer_iterator result = *this;
        return result += d;
    }
    friend aligned_buffer_iterator operator +(difference_type d, aligned_buffer_iterator const& self)
    {
        aligned_buffer_iterator result = self;
        return result += d;
    }
    aligned_buffer_iterator& operator -=(difference_type d)
    {
        index_ -= d;
        return *this;
    }
    aligned_buffer_iterator operator -(difference_type d) const
    {
        aligned_buffer_iterator result = *this;
        return result -= d;
    }
    difference_type operator -(aligned_buffer_iterator const& rhs) const
    {
        return index_ - rhs.index_;
    }
    reference operator [](difference_type d) const
    {
        return *const_cast<T*>(reinterpret_cast<T const*>(data_ + (index_ + d) * bytesPerElement_));
    }
};


struct input_output_iterator_tag : std::input_iterator_tag, std::output_iterator_tag { };

template <typename T>
class aligned_row_buffer_iterator
{
    template <typename, std::size_t, typename> friend class sysmakeshift::aligned_row_buffer;

private:
    char* data_;
    std::size_t index_;
    std::size_t cols_;
    std::size_t bytesPerRow_;

    aligned_row_buffer_iterator(char* _data, std::size_t _index, std::size_t _cols, std::size_t _bytesPerRow)
        : data_(_data), index_(_index), cols_(_cols), bytesPerRow_(_bytesPerRow)
    {
    }

public:
    constexpr aligned_row_buffer_iterator(void) noexcept
        : data_(nullptr), index_(0), cols_(0), bytesPerRow_(0)
    {
    }

    aligned_row_buffer_iterator(aligned_row_buffer_iterator const&) = default;
    aligned_row_buffer_iterator& operator =(aligned_row_buffer_iterator const&) = default;

    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_row_buffer_iterator(aligned_row_buffer_iterator const& rhs) noexcept
        : data_(rhs.data_), index_(rhs.index_), cols_(rhs.cols_), bytesPerRow_(rhs.bytesPerRow_)
    {
    }
    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_row_buffer_iterator& operator =(aligned_row_buffer_iterator const& rhs) noexcept
    {
        data_ = rhs.data_;
        index_ = rhs.index_;
        cols_ = rhs.cols_;
        bytesPerRow_ = rhs.bytesPerRow_;
    }

#ifdef __cpp_lib_concepts
    using iterator_concept  = std::random_access_iterator_tag; // TODO: is this true?
#endif // __cpp_lib_concepts
    using iterator_category = input_output_iterator_tag;
    using value_type        = gsl::span<T>;
    using difference_type   = std::ptrdiff_t;
    using reference         = gsl::span<T>;

    friend bool operator ==(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        gsl_Expects(lhs.data_ == rhs.data_);

        return lhs.index_ == rhs.index_;
    }
    friend bool operator !=(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator <(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        gsl_Expects(lhs.data_ == rhs.data_);

        return lhs.index_ < rhs.index_;
    }
    friend bool operator >(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return rhs < lhs;
    }
    friend bool operator <=(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return !(rhs < lhs);
    }
    friend bool operator >=(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return !(lhs < rhs);
    }

    gsl_NODISCARD reference operator *(void) const
    {
        return { const_cast<T*>(reinterpret_cast<T const*>(data_ + index_ * bytesPerRow_)), cols_ };
    }
    aligned_row_buffer_iterator& operator ++(void)
    {
        ++index_;
        return *this;
    }
    aligned_row_buffer_iterator& operator --(void)
    {
        --index_;
        return *this;
    }
    aligned_row_buffer_iterator operator ++(int)
    {
        aligned_row_buffer_iterator result = *this;
        ++index_;
        return result;
    }
    aligned_row_buffer_iterator operator --(int)
    {
        aligned_row_buffer_iterator result = *this;
        --index_;
        return result;
    }
    aligned_row_buffer_iterator& operator +=(difference_type d)
    {
        index_ += d;
        return *this;
    }
    aligned_row_buffer_iterator operator +(difference_type d) const
    {
        aligned_row_buffer_iterator result = *this;
        return result += d;
    }
    friend aligned_row_buffer_iterator operator +(difference_type d, aligned_row_buffer_iterator const& self)
    {
        aligned_row_buffer_iterator result = self;
        return result += d;
    }
    aligned_row_buffer_iterator& operator -=(difference_type d)
    {
        index_ -= d;
        return *this;
    }
    aligned_row_buffer_iterator operator -(difference_type d) const
    {
        aligned_row_buffer_iterator result = *this;
        return result -= d;
    }
    difference_type operator -(aligned_row_buffer_iterator const& rhs) const
    {
        return index_ - rhs.index_;
    }
    reference operator [](difference_type d) const
    {
        return { const_cast<T*>(reinterpret_cast<T const*>(data_ + (index_ + d) * bytesPerRow_)), cols_ };
    }
};


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_BUFFER_HPP_
